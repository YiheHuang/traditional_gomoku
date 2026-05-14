"""
Shared board-drawing constants, helpers and functions.
Used by both the main app (GomokuApp) and the online client.
"""

# ── layout constants (all in pixels, fixed and precise) ────────
CELL   = 38          # grid cell size
LEFT   = 54          # left margin (room for row labels)
TOP    = 74          # top margin (room for title + col labels)
RADIUS = 16          # stone radius
BOARD_PX = CELL * 14 # total grid span (532 px)

# canvas size derived from layout
CANVAS_W = LEFT + BOARD_PX + LEFT      # 54+532+54 = 640
CANVAS_H = TOP  + BOARD_PX + (LEFT+16) # 74+532+70 = 676

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

# ── coordinate helpers ─────────────────────────────────────────
def ix(col):
    """Column index → canvas x pixel."""
    return LEFT + col * CELL

def iy(row):
    """Row index → canvas y pixel."""
    return TOP + row * CELL

def to_board(mx, my):
    """Canvas pixel → (row, col), or (-1,-1) if too far from intersection."""
    col = int(round((mx - LEFT) / CELL))
    row = int(round((my - TOP)  / CELL))
    if not (0 <= col < 15 and 0 <= row < 15):
        return -1, -1
    dx = mx - ix(col)
    dy = my - iy(row)
    if dx*dx + dy*dy > RADIUS*RADIUS:
        return -1, -1
    return row, col

# ── drawing primitives ─────────────────────────────────────────
def draw_stone(canvas, color, is_last, px, py):
    """Draw a single stone at (px, py).  color: 1=black, 2=white."""
    r = RADIUS
    if color == 1:  # BLACK
        canvas.create_oval(px-r, py-r, px+r, py+r,
                           fill=BLACK_FILL, outline="#0A0A0A", width=1)
    else:           # WHITE
        canvas.create_oval(px-r, py-r, px+r, py+r,
                           fill=WHITE_FILL, outline=WHITE_OUT, width=2)
    if is_last:
        d = 4
        canvas.create_oval(px-d, py-d, px+d, py+d,
                           fill=LAST_RED, outline="")

def draw_board_background(canvas, title_text=""):
    """Draw board background, grid lines, star points, labels and title."""
    margin = 24
    canvas.create_rectangle(
        ix(0) - margin, iy(0) - margin,
        ix(14) + margin, iy(14) + margin,
        fill=BOARD_BG, outline="")

    # grid lines
    for i in range(15):
        canvas.create_line(ix(i), iy(0), ix(i), iy(14), fill=GRID, width=1)
        canvas.create_line(ix(0), iy(i), ix(14), iy(i), fill=GRID, width=1)

    # star points
    stars = [(3,3),(3,7),(3,11),(7,3),(7,7),(7,11),(11,3),(11,7),(11,11)]
    for sr, sc in stars:
        x, y = ix(sc), iy(sr)
        canvas.create_oval(x-3, y-3, x+4, y+4, fill=GRID, outline="")

    # row/col labels
    fsize = 10
    for i in range(15):
        lb = str(i) if i < 10 else chr(ord('A') + i - 10)
        canvas.create_text(ix(i), iy(0) - CELL//2 - 4, text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
        canvas.create_text(ix(i), iy(14) + CELL//2 + 4, text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
        canvas.create_text(ix(0) - CELL//2 - 8, iy(i), text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))
        canvas.create_text(ix(14) + CELL//2 + 8, iy(i), text=lb,
                          fill=TEXT_CLR, font=("Consolas", fsize))

    # title
    if title_text:
        canvas.create_text(CANVAS_W//2, 22,
                          text=title_text, fill=TEXT_CLR,
                          font=("Microsoft YaHei", 13, "bold"))

def draw_stones_on_board(canvas, board_getter, last_move_row=-1, last_move_col=-1):
    """Iterate board_getter(r,c)→0/1/2, draw all stones.
    board_getter returns EMPTY(0), BLACK(1), or WHITE(2)."""
    for r in range(15):
        for col in range(15):
            stone = board_getter(r, col)
            if stone != 0:
                is_last = (last_move_row == r and last_move_col == col)
                draw_stone(canvas, stone, is_last, ix(col), iy(r))

def draw_hover_preview(canvas, row, col, side):
    """Draw a dashed transparent preview stone at (row, col)."""
    px, py = ix(col), iy(row)
    color = "#505050" if side == 1 else "#F0F0F0"  # 1=BLACK
    canvas.create_oval(px-RADIUS, py-RADIUS, px+RADIUS, py+RADIUS,
                      fill=color, outline="", dash=(3, 3))
