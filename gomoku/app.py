"""
Gomoku AI — tkinter GUI application with precise pixel-aligned board.
"""

import tkinter as tk
from tkinter import messagebox, filedialog
import threading
import re, math
from gomoku import (
    EMPTY, BLACK, WHITE, BOARD_SIZE,
    Board, Move, Game, SearchEngine, SearchConfig,
    GameResult, analyze_root,
)

# ── layout constants (all in pixels, fixed and precise) ────────
CELL   = 38          # grid cell size
LEFT   = 54          # left margin (room for row labels)
TOP    = 74          # top margin (room for title + col labels)
RADIUS = 16          # stone radius
BOARD_PX = CELL * 14 # total grid span (532 px)

# canvas size derived from layout
CANVAS_W = LEFT + BOARD_PX + LEFT      # 54+532+54 = 640
CANVAS_H = TOP  + BOARD_PX + (LEFT-4)  # 74+532+50 = 656

# intersection pixel position
def ix(col): return LEFT + col * CELL
def iy(row): return TOP  + row * CELL

# ── colors ────────────────────────────────────────────────────
BG         = "#DCB478"
BOARD_BG   = "#D2A564"
GRID       = "#3C2814"
BLACK_FILL = "#141414"
WHITE_FILL = "#EBEBEB"
WHITE_OUT  = "#1E1E1E"
STATUS_BG  = "#B48C50"
TEXT_CLR   = "#281400"
LAST_RED   = "#DC3232"


class GomokuApp:
    def __init__(self):
        self._game = Game()
        self._mode = "pve"             # "pve" = human vs AI, "pvp" = local 2-player
        self._player_is_black = True
        self._ai_busy = False
        self._game_over = False
        self._last_ai_move = Move()
        self._hover = (-1, -1)
        self._move_history = []       # (row, col) for SGF export

        # analysis state
        self._ana_idx = -1            # -1 = no moves; 0..N-1 = viewing move N
        self._ana_cands = []          # [{row,col,wr}, ...]
        self._ana_score = 0           # search score of current position
        self._ana_cache = {}          # board hash → (score, cands) to avoid re-search

        self._build_ui()
        self._update_status()

        if self._mode == "pve" and not self._player_is_black:
            self._start_ai()

    # ── UI ────────────────────────────────────────────────────
    def _build_ui(self):
        self._root = tk.Tk()
        self._root.title("五子棋 AI — Alpha-Beta")
        self._root.resizable(False, False)

        # menubar
        mb = tk.Menu(self._root)

        gm = tk.Menu(mb, tearoff=0)
        gm.add_command(label="新游戏(N)", command=self._new_game, accelerator="Ctrl+N")

        # side sub-menu: "人类执黑" / "人类执白"
        side_menu = tk.Menu(gm, tearoff=0)
        self._side_var = tk.StringVar(value="black" if self._player_is_black else "white")
        side_menu.add_radiobutton(label="人类执黑 (●)", variable=self._side_var,
                                  value="black", command=self._switch_side)
        side_menu.add_radiobutton(label="人类执白 (○)", variable=self._side_var,
                                  value="white", command=self._switch_side)
        gm.add_cascade(label="选择阵营", menu=side_menu)

        gm.add_separator()
        gm.add_command(label="导入 SGF...", command=self._import_sgf)
        gm.add_command(label="导出 SGF...", command=self._export_sgf)
        gm.add_separator()
        gm.add_command(label="退出(X)", command=self._root.destroy)
        mb.add_cascade(label="游戏", menu=gm)

        # mode menu
        mm = tk.Menu(mb, tearoff=0)
        self._mode_var = tk.StringVar(value="pve")
        mm.add_radiobutton(label="人机对战", variable=self._mode_var,
                           value="pve", command=self._switch_mode)
        mm.add_radiobutton(label="本地对战", variable=self._mode_var,
                           value="pvp", command=self._switch_mode)
        mm.add_radiobutton(label="棋谱分析", variable=self._mode_var,
                           value="analysis", command=self._switch_mode)
        mb.add_cascade(label="模式", menu=mm)

        sm = tk.Menu(mb, tearoff=0)
        self._time_var = tk.IntVar(value=5000)
        for sec, ms in [("1 秒", 1000), ("3 秒", 3000), ("5 秒", 5000), ("10 秒", 10000)]:
            sm.add_radiobutton(label=sec, variable=self._time_var, value=ms,
                               command=self._set_time)
        mb.add_cascade(label="设置", menu=sm)
        self._root.config(menu=mb)
        self._root.bind_all("<Control-n>", lambda e: self._new_game())
        self._root.bind_all("<Left>",      lambda e: self._navigate(-1))
        self._root.bind_all("<Right>",     lambda e: self._navigate(+1))

        # canvas
        self._canvas = tk.Canvas(
            self._root, width=CANVAS_W, height=CANVAS_H,
            bg=BG, highlightthickness=0)
        self._canvas.pack(side=tk.TOP)
        self._canvas.bind("<Button-1>", self._on_click)
        self._canvas.bind("<Motion>",   self._on_move)

        # status bar
        self._status_var = tk.StringVar(value="")
        tk.Label(self._root, textvariable=self._status_var,
                 bg=STATUS_BG, fg=TEXT_CLR, font=("Microsoft YaHei", 12),
                 anchor="center", height=2).pack(side=tk.BOTTOM, fill=tk.X)

    # ── coordinate conversion ─────────────────────────────────
    def _to_board(self, mx, my):
        """canvas pixel → (row, col), or (-1,-1) if too far from intersection."""
        col = int(round((mx - LEFT) / CELL))
        row = int(round((my - TOP)  / CELL))
        if not (0 <= col < BOARD_SIZE and 0 <= row < BOARD_SIZE):
            return -1, -1
        dx = mx - ix(col)
        dy = my - iy(row)
        if dx*dx + dy*dy > RADIUS*RADIUS:
            return -1, -1
        return row, col

    # ── drawing ───────────────────────────────────────────────
    def _draw(self):
        c = self._canvas
        c.delete("all")

        # board background
        margin = 24
        c.create_rectangle(
            ix(0) - margin, iy(0) - margin,
            ix(14) + margin, iy(14) + margin,
            fill=BOARD_BG, outline="")

        # grid lines
        for i in range(BOARD_SIZE):
            c.create_line(ix(i), iy(0), ix(i), iy(14), fill=GRID, width=1)
            c.create_line(ix(0), iy(i), ix(14), iy(i), fill=GRID, width=1)

        # star points
        stars = [(3,3),(3,7),(3,11),(7,3),(7,7),(7,11),(11,3),(11,7),(11,11)]
        for sr, sc in stars:
            x, y = ix(sc), iy(sr)
            c.create_oval(x-3, y-3, x+4, y+4, fill=GRID, outline="")

        # row/col labels
        fsize = 10
        for i in range(BOARD_SIZE):
            lb = str(i) if i < 10 else chr(ord('A') + i - 10)
            c.create_text(ix(i), iy(0) - CELL//2 - 4, text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
            c.create_text(ix(i), iy(14) + CELL//2 + 4, text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
            c.create_text(ix(0) - CELL//2 - 8, iy(i), text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
            c.create_text(ix(14) + CELL//2 + 8, iy(i), text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))

        # stones
        board = self._game.getBoard()
        for r in range(BOARD_SIZE):
            for col in range(BOARD_SIZE):
                stone = board.get(r, col)
                if stone != EMPTY:
                    is_last = (self._last_ai_move.row == r and
                               self._last_ai_move.col == col)
                    self._draw_stone(stone, is_last, ix(col), iy(r))

        # hover preview
        hr, hc = self._hover
        if hr >= 0 and hc >= 0 and not self._game_over and not self._ai_busy:
            my_turn = self._is_my_turn()
            if my_turn and board.isEmpty(hr, hc):
                px, py = ix(hc), iy(hr)
                color = "#505050" if board.side == BLACK else "#F0F0F0"
                c.create_oval(px-RADIUS, py-RADIUS, px+RADIUS, py+RADIUS,
                              fill=color, outline="", dash=(3, 3))

        # ── analysis heatmap ──────────────────────────────────
        if self._mode == "analysis" and self._ana_cands:
            for cd in self._ana_cands:
                if not board.isEmpty(cd["row"], cd["col"]):
                    continue
                px, py = ix(cd["col"]), iy(cd["row"])
                wr = cd["wr"]
                # color: red(0%) → yellow(50%) → green(100%)
                ratio = (wr - 25.0) / 50.0  # map 25-75% → 0-1
                if ratio < 0: ratio = 0
                if ratio > 1: ratio = 1
                rc = int(220 * (1 - ratio))
                gc = int(220 * ratio)
                bc = min(rc, gc) // 3
                hcolor = f"#{rc:02x}{gc:02x}{bc:02x}"
                # circle: radius 12–18, larger and more readable
                sz = 12 + int(6 * abs(wr - 50) / 50)
                if sz < 12: sz = 12
                if sz > 18: sz = 18
                c.create_oval(px - sz, py - sz, px + sz, py + sz,
                              fill=hcolor, outline=GRID, width=1)
                fsize = max(9, sz - 4)
                c.create_text(px, py, text=str(wr), fill="#FFFFFF" if abs(wr-50) > 20 else TEXT_CLR,
                              font=("Consolas", fsize, "bold"))
            # best-move star
            best = self._ana_cands[0]
            bpx, bpy = ix(best["col"]), iy(best["row"])
            if board.isEmpty(best["row"], best["col"]):
                c.create_text(bpx, bpy - RADIUS - 12, text="★",
                              fill="#CC0000", font=("", 14, "bold"))

        # title
        mode_names = {"pve": "人机对战", "pvp": "本地对战", "analysis": "棋谱分析"}
        self._root.title(f"五子棋 AI — {mode_names.get(self._mode, self._mode)}")
        title_text = f"五子棋 AI — {mode_names.get(self._mode, '')}"
        c.create_text(CANVAS_W//2, 22,
                      text=title_text, fill=TEXT_CLR,
                      font=("Microsoft YaHei", 13, "bold"))

        # ── analysis progress bar ───────────────────────────────
        if self._mode == "analysis":
            wr_cur = self._score_to_winrate(self._ana_score)
            # convert to black's perspective
            side = board.side
            black_wr = wr_cur if side == BLACK else (100.0 - wr_cur)
            white_wr = 100.0 - black_wr

            bar_w, bar_h = 300, 18
            bar_x = (CANVAS_W - bar_w) // 2
            bar_y = iy(14) + 22   # just below the board

            # background
            c.create_rectangle(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h,
                               fill="#DDDDDD", outline=GRID, width=1)
            # black portion (left)
            bw = int(bar_w * black_wr / 100.0)
            if bw > 0:
                c.create_rectangle(bar_x, bar_y, bar_x + bw, bar_y + bar_h,
                                   fill="#222222", outline="")
            # white portion (right)
            if bw < bar_w:
                c.create_rectangle(bar_x + bw, bar_y, bar_x + bar_w, bar_y + bar_h,
                                   fill="#E8E8E8", outline="")
            # divider at 50%
            mid_x = bar_x + bar_w // 2
            c.create_line(mid_x, bar_y, mid_x, bar_y + bar_h, fill="#999999", width=1)
            # labels
            fsize2 = 10
            c.create_text(bar_x - 6, bar_y + bar_h // 2,
                          text="黑", fill=TEXT_CLR, anchor="e",
                          font=("Microsoft YaHei", fsize2, "bold"))
            c.create_text(bar_x + bar_w + 6, bar_y + bar_h // 2,
                          text="白", fill=TEXT_CLR, anchor="w",
                          font=("Microsoft YaHei", fsize2, "bold"))
            c.create_text(bar_x + bar_w // 2, bar_y + bar_h // 2,
                          text=f"{black_wr:.0f}% : {white_wr:.0f}%",
                          fill="#FFFFFF" if 25 < black_wr < 75 else TEXT_CLR,
                          font=("Consolas", fsize2, "bold"))

    def _draw_stone(self, color, is_last, px, py):
        r = RADIUS
        if color == BLACK:
            self._canvas.create_oval(px-r, py-r, px+r, py+r,
                                     fill=BLACK_FILL, outline="#0A0A0A", width=1)
        else:
            self._canvas.create_oval(px-r, py-r, px+r, py+r,
                                     fill=WHITE_FILL, outline=WHITE_OUT, width=2)
        if is_last:
            d = 4
            self._canvas.create_oval(px-d, py-d, px+d, py+d,
                                     fill=LAST_RED, outline="")

    # ── helpers ───────────────────────────────────────────────
    def _is_my_turn(self):
        if self._mode == "pvp":
            return True
        side = self._game.getBoard().side
        return (self._player_is_black and side == BLACK) or \
               (not self._player_is_black and side == WHITE)

    def _is_ai_turn(self):
        return self._mode == "pve" and not self._is_my_turn()

    def _update_status(self):
        if self._game_over:
            r = self._game.result()
            if r == GameResult.BLACK_WIN:
                if self._mode == "pvp":
                    s = "黑棋获胜！(●)"
                else:
                    s = "你赢了！(● 黑棋)" if self._player_is_black else "AI 赢了！(● 黑棋)"
            elif r == GameResult.WHITE_WIN:
                if self._mode == "pvp":
                    s = "白棋获胜！(○)"
                else:
                    s = "AI 赢了！(○ 白棋)" if self._player_is_black else "你赢了！(○ 白棋)"
            else:
                s = "平局！"
            self._status_var.set(s)
            return
        side = self._game.getBoard().side
        stone = "● 黑棋" if side == BLACK else "○ 白棋"
        if self._mode == "analysis":
            move_n = f"第 {self._ana_idx+1}/{len(self._move_history)} 步" if self._ana_idx >= 0 else "空棋盘"
            busy = "分析中..." if self._ai_busy else ""
            self._status_var.set(
                f"棋谱分析 — {move_n} — {stone} 回合  {busy}")
        elif self._mode == "pvp":
            self._status_var.set(f"本地对战 — {stone} 回合")
        else:
            self._status_var.set(f"你的回合 ({stone})" if self._is_my_turn()
                                 else "AI 思考中...")

    # ── actions ───────────────────────────────────────────────
    def _new_game(self):
        if self._ai_busy: return
        self._game.reset()
        self._last_ai_move = Move()
        self._game_over = False
        self._move_history = []
        self._ana_idx = -1
        self._ana_cands = []
        self._ana_score = 0
        self._ana_cache = {}
        self._update_status()
        self._draw()
        if self._mode == "analysis":
            self._start_analysis()
        elif self._mode == "pve" and not self._player_is_black:
            self._start_ai()

    def _switch_mode(self):
        if self._ai_busy:
            self._game.engine().stop()
            self._ai_busy = False
        self._mode = self._mode_var.get()
        if self._mode in ("pvp", "analysis"):
            self._game.aiIsBlack = True
        self._new_game()

    def _switch_side(self):
        if self._ai_busy or self._mode == "pvp":
            self._side_var.set("black" if self._player_is_black else "white")
            return
        new_black = (self._side_var.get() == "black")
        if new_black == self._player_is_black:
            return  # no change
        self._player_is_black = new_black
        self._game.aiIsBlack = not self._player_is_black
        self._new_game()

    def _set_time(self):
        c = self._game.engine().getConfig()
        c.timeMs = self._time_var.get()
        self._game.engine().setConfig(c)

    # ── analysis mode ─────────────────────────────────────────
    def _score_to_winrate(self, score):
        """Convert engine score to win-rate percentage (0-100)."""
        return round(50.0 + 50.0 * math.tanh(score / 15000.0), 1)

    def _navigate(self, delta):
        """Arrow-key navigation through move history."""
        if self._mode != "analysis" or self._ai_busy:
            return
        if not self._move_history:
            return
        new_idx = self._ana_idx + delta
        if new_idx < -1 or new_idx >= len(self._move_history):
            return

        # stop analysis thread if busy
        if self._ai_busy:
            self._game.engine().stop()
            self._ai_busy = False

        if delta < 0:
            # go backward: undo last move
            self._game.getBoard().undoMove()
            self._ana_idx = new_idx
        else:
            # go forward: redo next move
            r, c = self._move_history[new_idx]
            self._game.getBoard().makeMove(r, c)
            self._ana_idx = new_idx

        self._game_over = (self._game.result() != GameResult.ONGOING)
        self._last_ai_move = Move()
        self._hover = (-1, -1)
        self._start_analysis()
        self._draw()

    def _start_analysis(self):
        if self._ai_busy:
            return
        self._ai_busy = True
        self._update_status()
        # check cache
        board = self._game.getBoard()
        h = board.hash()
        if h in self._ana_cache:
            self._ana_score, self._ana_cands = self._ana_cache[h]
            self._ai_busy = False
            self._update_status()
            self._draw()
            return
        threading.Thread(target=self._analysis_thread, args=(h,), daemon=True).start()

    def _analysis_thread(self, hash_key):
        engine = self._game.engine()
        cfg = engine.getConfig()
        board_ref = self._game.getBoard()

        # ONE search → get scores for all root candidates
        # Scores are already from current player's perspective
        try:
            raw = list(analyze_root(board_ref, cfg.timeMs))
        except Exception:
            raw = []

        # Filter to empty cells only, convert to win rate
        cands = []
        best_score = -999999999
        for cd in raw:
            r, c = cd["row"], cd["col"]
            if not board_ref.isEmpty(r, c):
                continue
            wr = self._score_to_winrate(cd["score"])
            cands.append({"row": r, "col": c, "wr": wr, "score": cd["score"]})
            if cd["score"] > best_score:
                best_score = cd["score"]

        cands.sort(key=lambda x: x["score"], reverse=True)
        self._ana_score = best_score if cands else 0
        self._ana_cands = cands
        self._ana_cache[hash_key] = (self._ana_score, cands)
        self._ai_busy = False
        self._root.after(0, self._analysis_done)

    def _analysis_done(self):
        self._update_status()
        self._draw()

    # ── SGF import / export ───────────────────────────────────
    _SGF_COLS = "abcdefghijklmno"   # 0..14 → a..o

    @classmethod
    def _sgf_coord(cls, row, col):
        return cls._SGF_COLS[col] + cls._SGF_COLS[row]

    @classmethod
    def _parse_sgf_coord(cls, s):
        """'hh' → (7, 7).  Returns (-1,-1) on failure."""
        if len(s) != 2: return -1, -1
        c = cls._SGF_COLS.find(s[0])
        r = cls._SGF_COLS.find(s[1])
        if c < 0 or r < 0: return -1, -1
        return r, c

    def _export_sgf(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".sgf", filetypes=[("SGF 文件", "*.sgf"), ("全部", "*.*")],
            title="导出 SGF")
        if not path:
            return
        lines = ["(;GM[11]SZ[15]"]
        if self._mode in ("pvp", "analysis"):
            lines.append("PB[黑棋]PW[白棋]")
        else:
            pb = "玩家" if self._player_is_black else "AI"
            pw = "AI" if self._player_is_black else "玩家"
            lines.append(f"PB[{pb}]PW[{pw}]")

        for i, (r, c) in enumerate(self._move_history):
            tag = "B" if i % 2 == 0 else "W"
            lines.append(f";{tag}[{self._sgf_coord(r, c)}]")

        # result
        if self._game_over:
            res = self._game.result()
            if res == GameResult.BLACK_WIN:
                lines.append("RE[B+]")
            elif res == GameResult.WHITE_WIN:
                lines.append("RE[W+]")
            elif res == GameResult.DRAW:
                lines.append("RE[Draw]")

        lines.append(")")
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        messagebox.showinfo("导出成功", f"已保存到:\n{path}")

    def _import_sgf(self):
        path = filedialog.askopenfilename(
            filetypes=[("SGF 文件", "*.sgf"), ("全部", "*.*")],
            title="导入 SGF")
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
        except Exception as e:
            messagebox.showerror("读取失败", str(e))
            return

        # extract moves: ;B[cc] ;W[dd] ...
        moves = re.findall(r';(B|W)\[([a-o]{2})\]', text)
        if not moves:
            messagebox.showerror("解析失败", "未在 SGF 中找到有效着法。")
            return

        # stop current AI if running
        if self._ai_busy:
            self._game.engine().stop()
            self._ai_busy = False

        self._game.reset()
        self._move_history = []
        for tag, coord in moves:
            r, c = self._parse_sgf_coord(coord)
            if r < 0:
                continue
            self._game.makePlayerMove(r, c)
            self._move_history.append((r, c))

        self._game_over = (self._game.result() != GameResult.ONGOING)
        self._last_ai_move = Move()
        self._hover = (-1, -1)

        if self._mode == "analysis":
            # show all moves and run analysis on final position
            self._ana_idx = len(self._move_history) - 1
            self._update_status()
            self._draw()
            self._start_analysis()
            messagebox.showinfo("导入成功",
                f"已载入 {len(self._move_history)} 步着法。\n使用 ← → 方向键浏览。")
            return

        self._update_status()
        self._draw()
        if self._mode == "pve" and not self._game_over and not self._is_my_turn():
            self._start_ai()

        if not self._game_over:
            messagebox.showinfo("导入成功", f"已载入 {len(self._move_history)} 步着法。")

    def _start_ai(self):
        if self._ai_busy or self._game_over: return
        self._ai_busy = True
        self._update_status()
        threading.Thread(target=self._ai_thread, daemon=True).start()

    def _ai_thread(self):
        result = self._game.engine().search(self._game.getBoard())
        self._game.applyMove(result.bestMove)
        self._last_ai_move = result.bestMove
        self._ai_busy = False
        self._root.after(0, self._ai_done)

    def _ai_done(self):
        if self._last_ai_move.valid():
            self._move_history.append((self._last_ai_move.row, self._last_ai_move.col))
        if self._game.result() != GameResult.ONGOING:
            self._game_over = True
        self._update_status()
        self._draw()
        if self._game_over:
            messagebox.showinfo("对局结束", self._status_var.get())

    # ── input ─────────────────────────────────────────────────
    def _on_click(self, event):
        r, c = self._to_board(event.x, event.y)
        if r < 0: return

        # ── analysis mode: free stone placement ─────────────
        if self._mode == "analysis":
            if self._ai_busy:
                return
            board = self._game.getBoard()
            if not board.isEmpty(r, c):
                return
            # truncate history at current position, then append
            self._move_history = self._move_history[:self._ana_idx + 1]
            self._move_history.append((r, c))
            board.makeMove(r, c)
            self._ana_idx += 1
            self._game_over = (self._game.result() != GameResult.ONGOING)
            self._last_ai_move = Move()
            self._hover = (-1, -1)
            self._update_status()
            self._draw()
            self._start_analysis()
            return

        # ── PvE / PvP modes ─────────────────────────────────
        if self._game_over or self._ai_busy or not self._is_my_turn():
            return
        if not self._game.makePlayerMove(r, c):
            return

        self._move_history.append((r, c))
        self._hover = (-1, -1)
        self._update_status()
        self._draw()

        if self._game.result() != GameResult.ONGOING:
            self._game_over = True
            self._update_status()
            self._draw()
            messagebox.showinfo("对局结束", self._status_var.get())
        elif self._mode == "pve":
            self._start_ai()
        else:
            # PvP: just switch sides, let the other player click
            self._update_status()
            self._draw()

    def _on_move(self, event):
        r, c = self._to_board(event.x, event.y)
        if r != self._hover[0] or c != self._hover[1]:
            self._hover = (r, c)
            self._draw()

    # ── main ──────────────────────────────────────────────────
    def run(self):
        self._root.after(50, self._draw)
        self._root.mainloop()
