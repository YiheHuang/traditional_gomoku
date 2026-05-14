"""
Gomoku online matchmaking & relay server.
Pure Python, no third-party dependencies. Deploy on CentOS.

Usage:
    python server.py --host 0.0.0.0 --port 9999
"""

import socket
import selectors
import json
import time
import sys
import argparse
import random

# ── board constants ──────────────────────────────────────────
EMPTY, BLACK, WHITE = 0, 1, 2
SIZE = 15
DIRS = [(0,1), (1,0), (1,1), (1,-1)]  # 4 directions for win check


class PureBoard:
    """Server-side board for move validation and win detection (no C++ dep)."""
    def __init__(self):
        self.grid = [[EMPTY] * SIZE for _ in range(SIZE)]
        self.turn = BLACK
        self.move_count = 0

    def is_empty(self, r, c):
        return self.grid[r][c] == EMPTY

    def make_move(self, r, c):
        """Apply a move. Returns True if valid."""
        if not (0 <= r < SIZE and 0 <= c < SIZE):
            return False
        if self.grid[r][c] != EMPTY:
            return False
        self.grid[r][c] = self.turn
        self.turn = WHITE if self.turn == BLACK else BLACK
        self.move_count += 1
        return True

    def check_winner(self, r, c):
        """Check if the move at (r,c) created five-in-a-row. Returns winner or None."""
        color = self.grid[r][c]
        if color == EMPTY:
            return None
        for dr, dc in DIRS:
            count = 1
            for sign in (1, -1):
                for step in range(1, 5):
                    nr, nc = r + sign * step * dr, c + sign * step * dc
                    if 0 <= nr < SIZE and 0 <= nc < SIZE and self.grid[nr][nc] == color:
                        count += 1
                    else:
                        break
            if count >= 5:
                return color
        return None

    def is_full(self):
        return self.move_count >= SIZE * SIZE

    def reset(self):
        self.__init__()


# ── server ────────────────────────────────────────────────────
MAX_MESSAGE = 4096
HEARTBEAT_INTERVAL = 30
HEARTBEAT_TIMEOUT = 90


class Player:
    __slots__ = ("sock", "addr", "state", "opponent", "color",
                 "recv_buf", "last_beat", "name")

    def __init__(self, sock, addr):
        self.sock = sock
        self.addr = addr
        self.state = "idle"       # idle | matching | playing
        self.opponent = None
        self.color = None
        self.recv_buf = b""
        self.last_beat = time.time()
        self.name = f"{addr[0]}:{addr[1]}"


class GomokuServer:
    def __init__(self, host="0.0.0.0", port=9999, max_clients=50):
        self.host = host
        self.port = port
        self.max_clients = max_clients
        self.sel = selectors.DefaultSelector()
        self.match_queue = []          # list of Player
        self.connections = {}          # sock → Player
        self.games = {}                # (p1, p2) → PureBoard
        self._running = False

    # ── message helpers ──────────────────────────────────────
    @staticmethod
    def _make_msg(msg_type, **kwargs):
        obj = {"type": msg_type}
        obj.update(kwargs)
        return (json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8")

    @staticmethod
    def _send(player, msg_type, **kwargs):
        try:
            player.sock.sendall(GomokuServer._make_msg(msg_type, **kwargs))
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    # ── event loop ───────────────────────────────────────────
    def start(self):
        listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listen_sock.bind((self.host, self.port))
        listen_sock.listen(16)
        listen_sock.setblocking(False)
        self.sel.register(listen_sock, selectors.EVENT_READ, data=None)
        self._running = True
        print(f"[server] listening on {self.host}:{self.port}")
        print(f"[server] max clients: {self.max_clients}")

        try:
            while self._running:
                events = self.sel.select(timeout=1.0)
                for key, mask in events:
                    if key.data is None:
                        self._accept(key.fileobj)
                    else:
                        self._handle_client(key.fileobj, key.data)
                self._check_heartbeats()
        except KeyboardInterrupt:
            print("\n[server] shutting down...")
        finally:
            self._shutdown()

    def stop(self):
        self._running = False

    # ── accept ───────────────────────────────────────────────
    def _accept(self, sock):
        conn, addr = sock.accept()
        if len(self.connections) >= self.max_clients:
            try:
                conn.sendall(self._make_msg("error", message="服务器已满，请稍后再试。"))
            except OSError:
                pass
            conn.close()
            print(f"[server] rejected {addr}: server full")
            return
        conn.setblocking(False)
        player = Player(conn, addr)
        self.connections[conn] = player
        self.sel.register(conn, selectors.EVENT_READ, data=player)
        self._send(player, "info", message=f"已连接到服务器。在线人数: {len(self.connections)}")
        print(f"[server] {player.name} connected ({len(self.connections)} online)")

    # ── client handler ───────────────────────────────────────
    def _handle_client(self, sock, player):
        try:
            data = sock.recv(4096)
        except (ConnectionResetError, OSError):
            data = b""
        if not data:
            self._disconnect(player)
            return

        player.last_beat = time.time()
        player.recv_buf += data

        while b"\n" in player.recv_buf:
            line, player.recv_buf = player.recv_buf.split(b"\n", 1)
            if not line.strip():
                continue
            try:
                msg = json.loads(line.decode("utf-8"))
            except (json.JSONDecodeError, UnicodeDecodeError):
                self._send(player, "error", message="消息格式错误。")
                continue
            self._dispatch(player, msg)

    # ── dispatch ─────────────────────────────────────────────
    def _dispatch(self, player, msg):
        msg_type = msg.get("type", "")

        if msg_type == "match":
            if player.state != "idle":
                self._send(player, "error", message="你已在匹配或游戏中。")
                return
            player.state = "matching"
            self.match_queue.append(player)
            self._send(player, "info", message="正在寻找对手...")
            print(f"[server] {player.name} looking for match")
            self._try_pair()

        elif msg_type == "cancel_match":
            if player.state != "matching":
                return
            if player in self.match_queue:
                self.match_queue.remove(player)
            player.state = "idle"
            self._send(player, "info", message="已取消匹配。")

        elif msg_type == "move":
            self._handle_move(player, msg)

        elif msg_type == "resign":
            self._handle_resign(player)

        elif msg_type == "ping":
            self._send(player, "pong")

        else:
            self._send(player, "error", message=f"未知消息类型: {msg_type}")

    # ── matchmaking ──────────────────────────────────────────
    def _try_pair(self):
        while len(self.match_queue) >= 2:
            p1 = self.match_queue.pop(0)
            p2 = self.match_queue.pop(0)

            # Randomly assign colors
            if random.random() < 0.5:
                p_black, p_white = p1, p2
            else:
                p_black, p_white = p2, p1

            p_black.state = "playing"
            p_black.color = BLACK
            p_black.opponent = p_white

            p_white.state = "playing"
            p_white.color = WHITE
            p_white.opponent = p_black

            board = PureBoard()
            self.games[(p_black, p_white)] = board

            self._send(p_black, "matched", color="black")
            self._send(p_white, "matched", color="white")
            self._send(p_black, "info", message="对局开始！你执黑 (●)，请先落子。")
            self._send(p_white, "info", message="对局开始！你执白 (○)，等待对手落子。")
            print(f"[server] matched {p_black.name}(黑) vs {p_white.name}(白)")

    # ── move ─────────────────────────────────────────────────
    def _handle_move(self, player, msg):
        if player.state != "playing":
            self._send(player, "error", message="你不在游戏中。")
            return
        opponent = player.opponent
        if opponent is None:
            self._send(player, "error", message="对手已离开。")
            return

        board = self.games.get((player, opponent)) or self.games.get((opponent, player))
        if board is None:
            self._send(player, "error", message="游戏状态异常。")
            return

        if board.turn != player.color:
            self._send(player, "error", message="还没轮到你。")
            return

        r = msg.get("row")
        c = msg.get("col")
        if r is None or c is None:
            self._send(player, "error", message="无效的着法。")
            return
        try:
            r, c = int(r), int(c)
        except (ValueError, TypeError):
            self._send(player, "error", message="无效的着法坐标。")
            return

        if not board.make_move(r, c):
            self._send(player, "error", message="此处不能落子。")
            return

        # Forward move to opponent
        self._send(opponent, "move", row=r, col=c)

        # Check game end
        winner = board.check_winner(r, c)
        if winner is not None:
            win_color = "black" if winner == BLACK else "white"
            self._send(player, "game_over", result="win", reason="five")
            self._send(opponent, "game_over", result="loss", reason="five")
            self._send(player, "info", message=f"你赢了！(五连)")
            self._send(opponent, "info", message=f"你输了，对手形成五连。")
            print(f"[server] game over: {player.name}({'黑' if player.color==BLACK else '白'}) wins by five")
            self._end_game(player, opponent)
        elif board.is_full():
            self._send(player, "game_over", result="draw", reason="full")
            self._send(opponent, "game_over", result="draw", reason="full")
            self._send(player, "info", message="平局！棋盘已满。")
            self._send(opponent, "info", message="平局！棋盘已满。")
            print(f"[server] game over: draw (board full)")
            self._end_game(player, opponent)

    # ── resign ───────────────────────────────────────────────
    def _handle_resign(self, player):
        if player.state != "playing":
            return
        opponent = player.opponent
        if opponent is None:
            return
        self._send(player, "game_over", result="loss", reason="resign")
        self._send(opponent, "game_over", result="win", reason="resign")
        self._send(player, "info", message="你认输了。")
        self._send(opponent, "info", message="对手认输，你赢了！")
        print(f"[server] {player.name} resigned")
        self._end_game(player, opponent)

    # ── game end cleanup ─────────────────────────────────────
    def _end_game(self, p1, p2):
        key = (p1, p2) if (p1, p2) in self.games else (p2, p1)
        self.games.pop(key, None)

        p1.state = "idle"
        p1.opponent = None
        p1.color = None

        p2.state = "idle"
        p2.opponent = None
        p2.color = None

    # ── disconnect ───────────────────────────────────────────
    def _disconnect(self, player):
        # Remove from match queue
        if player in self.match_queue:
            self.match_queue.remove(player)

        # If playing, notify opponent
        if player.state == "playing" and player.opponent:
            opp = player.opponent
            self._send(opp, "opponent_disconnected")
            self._send(opp, "info", message="对手断线，你获胜！")
            self._send(opp, "game_over", result="win", reason="disconnect")
            # Cleanup opponent too
            for key in list(self.games.keys()):
                if player in key or opp in key:
                    self.games.pop(key, None)
            opp.state = "idle"
            opp.opponent = None
            opp.color = None

        if player.sock in self.connections:
            self.sel.unregister(player.sock)
            del self.connections[player.sock]
        try:
            player.sock.close()
        except OSError:
            pass
        print(f"[server] {player.name} disconnected ({len(self.connections)} online)")

    # ── heartbeat ────────────────────────────────────────────
    def _check_heartbeats(self):
        now = time.time()
        to_kick = []
        for sock, player in self.connections.items():
            if now - player.last_beat > HEARTBEAT_TIMEOUT:
                to_kick.append(player)
        for player in to_kick:
            self._send(player, "error", message="心跳超时，你已被断开连接。")
            print(f"[server] {player.name} heartbeat timeout")
            self._disconnect(player)

    # ── shutdown ─────────────────────────────────────────────
    def _shutdown(self):
        for sock in list(self.connections.keys()):
            player = self.connections[sock]
            try:
                self._send(player, "info", message="服务器正在关闭。")
            except OSError:
                pass
            self._disconnect(player)
        self.sel.close()
        print("[server] stopped.")


# ── entry point ───────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Gomoku online match server")
    parser.add_argument("--host", default="0.0.0.0", help="listen host")
    parser.add_argument("--port", type=int, default=9999, help="listen port")
    parser.add_argument("--max-clients", type=int, default=50, help="max connections")
    args = parser.parse_args()

    server = GomokuServer(host=args.host, port=args.port, max_clients=args.max_clients)
    server.start()


if __name__ == "__main__":
    main()
