"""
Gomoku online battle client — standalone tkinter window.
Launched as a subprocess by the main app, or run independently.

Usage:
    python -m gomoku.client_online
    python gomoku/client_online.py --host 192.144.228.237 --port 9999
"""

import tkinter as tk
from tkinter import messagebox
import threading
import socket
import json
import queue
import time
import argparse
import sys
import os

# ── try to use C++ Board; fall back to pure-Python SimpleBoard ─
try:
    from gomoku import Board as CppBoard, Move as CppMove, GameResult, BLACK, WHITE, EMPTY, BOARD_SIZE
    _HAS_CPP = True
except ImportError:
    _HAS_CPP = False
    EMPTY, BLACK, WHITE = 0, 1, 2
    BOARD_SIZE = 15

from gomoku.board_ui import (
    CELL, LEFT, TOP, RADIUS, CANVAS_W, CANVAS_H,
    ix, iy, to_board,
    BG, STATUS_BG, TEXT_CLR, GRID,
    draw_stone, draw_board_background, draw_stones_on_board,
)

# ── server config ──────────────────────────────────────────────
SERVER_HOST = "192.144.228.237"
SERVER_PORT = 9999
HEARTBEAT_INTERVAL = 30  # seconds


class SimpleBoard:
    """Pure-Python board for client-side display (used when C++ extension absent)."""
    def __init__(self):
        self.grid = [[EMPTY] * BOARD_SIZE for _ in range(BOARD_SIZE)]

    def reset(self):
        self.__init__()

    def get(self, r, c):
        return self.grid[r][c]

    def make_move(self, r, c, color):
        self.grid[r][c] = color

    def is_empty(self, r, c):
        return self.grid[r][c] == EMPTY


class OnlineClient:
    def __init__(self, host=SERVER_HOST, port=SERVER_PORT):
        self._host = host
        self._port = port

        # State
        self._state = "idle"         # idle | matching | playing
        self._my_color = None        # BLACK or WHITE
        self._my_turn = False
        self._game_over = False
        self._hover = (-1, -1)
        self._last_move = (-1, -1)   # last stone position for red-dot highlight
        self._move_count = 0

        # Board
        if _HAS_CPP:
            self._board = CppBoard()
        else:
            self._board = SimpleBoard()

        # Network
        self._sock = None
        self._msg_queue = queue.Queue()
        self._net_thread = None
        self._connected = False
        self._closing = False

        # UI
        self._root = tk.Tk()
        self._root.title("五子棋 — 线上对战")
        self._root.resizable(False, False)
        self._root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._build_ui()
        self._auto_connect()

    # ── UI build ──────────────────────────────────────────────
    def _build_ui(self):
        self._canvas = tk.Canvas(
            self._root, width=CANVAS_W, height=CANVAS_H,
            bg=BG, highlightthickness=0)
        self._canvas.pack(side=tk.TOP)
        self._canvas.bind("<Button-1>", self._on_click)
        self._canvas.bind("<Motion>",   self._on_move)

        # Status bar
        self._status_var = tk.StringVar(value="正在连接服务器...")
        tk.Label(self._root, textvariable=self._status_var,
                 bg=STATUS_BG, fg=TEXT_CLR, font=("Microsoft YaHei", 12),
                 anchor="center", height=2).pack(side=tk.BOTTOM, fill=tk.X)

        # Control buttons
        btn_frame = tk.Frame(self._root, bg=STATUS_BG)
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=(0, 2))

        self._match_btn = tk.Button(btn_frame, text="寻找对手", font=("Microsoft YaHei", 11),
                                     command=self._request_match, state=tk.DISABLED)
        self._match_btn.pack(side=tk.LEFT, padx=(8, 4), pady=4)

        self._resign_btn = tk.Button(btn_frame, text="认输", font=("Microsoft YaHei", 11),
                                      command=self._resign, state=tk.DISABLED)
        self._resign_btn.pack(side=tk.LEFT, padx=4, pady=4)

        self._reconnect_btn = tk.Button(btn_frame, text="重新连接", font=("Microsoft YaHei", 11),
                                         command=self._reconnect)
        self._reconnect_btn.pack(side=tk.RIGHT, padx=(4, 8), pady=4)

    # ── connection ────────────────────────────────────────────
    def _auto_connect(self):
        """Connect on startup in a background thread."""
        self._set_status("正在连接服务器...")
        threading.Thread(target=self._connect_thread, daemon=True).start()

    def _connect_thread(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((self._host, self._port))
            sock.settimeout(None)
            self._sock = sock
            self._connected = True
            self._root.after(0, self._on_connected)
        except Exception as e:
            self._root.after(0, lambda: self._set_status(f"连接失败: {e}"))

    def _on_connected(self):
        self._set_status("已连接到服务器")
        self._match_btn.config(state=tk.NORMAL)
        self._start_net_thread()
        self._start_heartbeat()

    def _reconnect(self):
        if self._net_thread and self._net_thread.is_alive():
            return
        self._auto_connect()

    # ── network thread ────────────────────────────────────────
    def _start_net_thread(self):
        if self._net_thread and self._net_thread.is_alive():
            return
        self._net_thread = threading.Thread(target=self._net_loop, daemon=True)
        self._net_thread.start()
        self._root.after(100, self._poll_messages)

    def _net_loop(self):
        buf = b""
        while self._connected and not self._closing:
            try:
                data = self._sock.recv(4096)
            except (ConnectionResetError, OSError, socket.timeout):
                data = b""
            if not data:
                self._msg_queue.put(None)  # sentinel: disconnected
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if line.strip():
                    try:
                        msg = json.loads(line.decode("utf-8"))
                        self._msg_queue.put(msg)
                    except (json.JSONDecodeError, UnicodeDecodeError):
                        pass
        self._msg_queue.put(None)

    # ── message polling (main thread) ─────────────────────────
    def _poll_messages(self):
        try:
            while True:
                msg = self._msg_queue.get_nowait()
                if msg is None:  # disconnection sentinel
                    self._handle_disconnect()
                    return
                self._handle_message(msg)
        except queue.Empty:
            pass
        if self._connected:
            self._root.after(100, self._poll_messages)

    # ── message dispatch ──────────────────────────────────────
    def _handle_message(self, msg):
        msg_type = msg.get("type", "")

        if msg_type == "info":
            self._set_status(msg.get("message", ""))

        elif msg_type == "matched":
            color_str = msg.get("color", "black")
            self._my_color = BLACK if color_str == "black" else WHITE
            self._state = "playing"
            self._game_over = False
            self._move_count = 0
            self._last_move = (-1, -1)
            self._my_turn = (self._my_color == BLACK)
            if _HAS_CPP:
                self._board = CppBoard()
            else:
                self._board = SimpleBoard()
            self._match_btn.config(state=tk.DISABLED)
            self._resign_btn.config(state=tk.NORMAL)
            piece = "● 黑棋" if self._my_color == BLACK else "○ 白棋"
            self._set_status(f"对局开始！你执{piece}")
            self._draw()

        elif msg_type == "move":
            r, c = msg.get("row"), msg.get("col")
            if r is not None and c is not None:
                self._apply_opponent_move(r, c)

        elif msg_type == "game_over":
            result = msg.get("result", "")
            reason = msg.get("reason", "")
            self._state = "idle"
            self._game_over = True
            self._my_turn = False
            self._match_btn.config(state=tk.NORMAL)
            self._resign_btn.config(state=tk.DISABLED)
            self._draw()
            msgs = {
                ("win", "five"): "你赢了！五连获胜！",
                ("win", "resign"): "你赢了！对手认输。",
                ("win", "disconnect"): "你赢了！对手断线。",
                ("loss", "five"): "你输了，对手形成五连。",
                ("loss", "resign"): "你认输了。",
                ("loss", "disconnect"): "",
                ("draw", "full"): "平局！棋盘已满。",
            }
            info = msgs.get((result, reason), f"游戏结束: {result}")
            if info:
                self._set_status(info)
                self._root.after(200, lambda: messagebox.showinfo("对局结束", info))

        elif msg_type == "opponent_disconnected":
            if self._state == "playing":
                self._state = "idle"
                self._game_over = True
                self._my_turn = False
                self._match_btn.config(state=tk.NORMAL)
                self._resign_btn.config(state=tk.DISABLED)
                self._draw()
                self._set_status("对手断线，你获胜！")
                messagebox.showinfo("对局结束", "对手断线，你获胜！")

        elif msg_type == "error":
            pass  # Error messages are mostly for debugging

        elif msg_type == "pong":
            pass

    def _apply_opponent_move(self, r, c):
        if self._state != "playing":
            return
        color = WHITE if self._my_color == BLACK else BLACK
        if _HAS_CPP:
            self._board.makeMove(r, c)
        else:
            self._board.make_move(r, c, color)
        self._last_move = (r, c)
        self._move_count += 1
        self._my_turn = True
        self._set_status("轮到你落子了")
        self._draw()

    # ── disconnect handler ────────────────────────────────────
    def _handle_disconnect(self):
        self._connected = False
        self._state = "idle"
        self._my_turn = False
        self._match_btn.config(state=tk.DISABLED)
        self._resign_btn.config(state=tk.DISABLED)
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None
        self._draw()
        self._set_status("与服务器断开连接")
        if not self._closing:
            self._root.after(100, lambda: messagebox.showwarning("连接断开", "与服务器断开连接。\n请点击「重新连接」按钮。"))

    # ── actions ───────────────────────────────────────────────
    def _request_match(self):
        if not self._connected or self._state != "idle":
            return
        self._state = "matching"
        self._set_status("正在寻找对手...")
        self._match_btn.config(state=tk.DISABLED)
        self._game_over = False
        self._move_count = 0
        self._last_move = (-1, -1)
        if _HAS_CPP:
            self._board = CppBoard()
        else:
            self._board = SimpleBoard()
        self._draw()
        self._send("match")

    def _resign(self):
        if self._state != "playing":
            return
        self._send("resign")

    # ── send ──────────────────────────────────────────────────
    def _send(self, msg_type, **kwargs):
        if not self._connected or not self._sock:
            return
        obj = {"type": msg_type}
        obj.update(kwargs)
        try:
            data = (json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8")
            self._sock.sendall(data)
        except (BrokenPipeError, ConnectionResetError, OSError):
            self._msg_queue.put(None)

    # ── heartbeat ─────────────────────────────────────────────
    def _start_heartbeat(self):
        if self._connected:
            self._send("ping")
            self._root.after(HEARTBEAT_INTERVAL * 1000, self._start_heartbeat)

    # ── drawing ───────────────────────────────────────────────
    def _draw(self):
        c = self._canvas
        c.delete("all")

        draw_board_background(c, "五子棋 AI — 线上对战")

        # stones
        if _HAS_CPP:
            draw_stones_on_board(c, lambda r, col: self._board.get(r, col),
                                 self._last_move[0], self._last_move[1])
        else:
            def _stone_getter(r, col):
                return self._board.grid[r][col]
            draw_stones_on_board(c, _stone_getter,
                                 self._last_move[0], self._last_move[1])

        # hover preview
        hr, hc = self._hover
        if (hr >= 0 and hc >= 0 and self._state == "playing"
                and self._my_turn and not self._game_over):
            if _HAS_CPP:
                if self._board.isEmpty(hr, hc):
                    board_side = self._my_color
                    color = "#505050" if board_side == BLACK else "#F0F0F0"
                    px, py = ix(hc), iy(hr)
                    c.create_oval(px-RADIUS, py-RADIUS, px+RADIUS, py+RADIUS,
                                  fill=color, outline="", dash=(3, 3))
            else:
                if self._board.is_empty(hr, hc):
                    color = "#505050" if self._my_color == BLACK else "#F0F0F0"
                    px, py = ix(hc), iy(hr)
                    c.create_oval(px-RADIUS, py-RADIUS, px+RADIUS, py+RADIUS,
                                  fill=color, outline="", dash=(3, 3))

    # ── input ─────────────────────────────────────────────────
    def _on_click(self, event):
        if not self._connected or self._state != "playing":
            return
        if not self._my_turn or self._game_over:
            return
        r, c = to_board(event.x, event.y)
        if r < 0:
            return
        if _HAS_CPP:
            if not self._board.isEmpty(r, c):
                return
        else:
            if not self._board.is_empty(r, c):
                return

        # Apply locally
        color = self._my_color
        if _HAS_CPP:
            self._board.makeMove(r, c)
        else:
            self._board.make_move(r, c, color)
        self._last_move = (r, c)
        self._move_count += 1
        self._my_turn = False
        self._hover = (-1, -1)
        self._set_status("等待对手落子...")
        self._draw()

        # Send to server
        self._send("move", row=r, col=c)

    def _on_move(self, event):
        r, c = to_board(event.x, event.y)
        if r != self._hover[0] or c != self._hover[1]:
            self._hover = (r, c)
            self._draw()

    # ── helpers ───────────────────────────────────────────────
    def _set_status(self, text):
        self._status_var.set(text)

    def _on_close(self):
        self._closing = True
        self._connected = False
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None
        self._root.destroy()

    # ── main ──────────────────────────────────────────────────
    def run(self):
        self._root.after(50, self._draw)
        self._root.mainloop()


# ── entry point ───────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Gomoku online battle client")
    parser.add_argument("--host", default=SERVER_HOST, help="server host")
    parser.add_argument("--port", type=int, default=SERVER_PORT, help="server port")
    args = parser.parse_args()

    client = OnlineClient(host=args.host, port=args.port)
    client.run()


if __name__ == "__main__":
    main()
