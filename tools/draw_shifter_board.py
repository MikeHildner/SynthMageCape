"""Draw the ELEGOO 5x7 cm perfboard and the shifter/distribution layout on it.

Orientation matches the board held with the numbers 01..18 across the top and
the letters A..X down the sides (24 rows, Y and Z are not printed on the board).
Columns 01 and 02 have plain round pads; 03..18 have oval pads. Four corner
mounting holes. Component-side view.

Usage: python tools/draw_shifter_board.py <stage> <out.png>
  stage 0 = bare board with the grid
  stage 1 = socket, rails, capacitor
  stage 2 = everything
"""
import sys
from PIL import Image, ImageDraw, ImageFont

COLS = list(range(1, 19))                        # 01..18 left to right
ROWS = [chr(ord("A") + i) for i in range(24)]    # A..X top to bottom
P = 40                                           # pixel pitch
MX, MY = 300, 110
W = MX * 2 + P * 17
H = MY * 2 + P * 23 + 170

GREEN = "#1f6b3a"; RING = "#c9a86a"; EDGE = "#8a6d3b"
RED = "#d62b1a"; BLUE = "#1a4fd6"; PURPLE = "#6a1b9a"; TEAL = "#1d7a4a"; GREY = "#444"

def xy(col, row):
    return MX + COLS.index(col) * P, MY + ROWS.index(row) * P

def font(sz, bold=False):
    for name in (["arialbd.ttf"] if bold else ["arial.ttf"]):
        try:
            return ImageFont.truetype(name, sz)
        except Exception:
            pass
    return ImageFont.load_default()

stage = int(sys.argv[1]); out = sys.argv[2]
img = Image.new("RGB", (W, H), "#f4f4f0")
d = ImageDraw.Draw(img)
f12 = font(15); f10 = font(13); fb = font(18, True); fh = font(22, True)

# board outline
bx0, by0 = MX - P * 1.3, MY - P * 1.3
bx1, by1 = MX + P * 17 + P * 1.3, MY + P * 23 + P * 1.3
d.rounded_rectangle([bx0, by0, bx1, by1], radius=10, fill=GREEN)
for cx, cy in ((bx0 + 22, by0 + 22), (bx1 - 22, by0 + 22), (bx0 + 22, by1 - 22), (bx1 - 22, by1 - 22)):
    d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill="#f4f4f0", outline="#111")

for c in COLS:
    x, _ = xy(c, "A")
    d.text((x, MY - P * 0.85), f"{c:02d}", fill="white", font=f12, anchor="mm")
    d.text((x, MY + P * 23 + P * 0.85), f"{c:02d}", fill="white", font=f12, anchor="mm")
for r in ROWS:
    _, y = xy(1, r)
    d.text((MX - P * 0.9, y), r, fill="white", font=f12, anchor="mm")
    d.text((MX + P * 17 + P * 0.9, y), r, fill="white", font=f12, anchor="mm")

for c in COLS:
    for r in ROWS:
        x, y = xy(c, r)
        if c <= 2:
            d.ellipse([x - 9, y - 9, x + 9, y + 9], fill=RING, outline=EDGE)
        else:
            d.ellipse([x - 9, y - 12, x + 9, y + 12], fill=RING, outline=EDGE)
        d.ellipse([x - 3.5, y - 3.5, x + 3.5, y + 3.5], fill=GREEN)

def ring(c, r, color, rad=13, w=3):
    x, y = xy(c, r)
    d.ellipse([x - rad, y - rad, x + rad, y + rad], outline=color, width=w)

def tag(x, y, text, color, anchor="mm", f=None):
    """Text on a light box so it reads on the green."""
    f = f or f10
    l, t, r, b = d.textbbox((x, y), text, font=f, anchor=anchor)
    d.rounded_rectangle([l - 5, t - 3, r + 5, b + 3], radius=4, fill="#fbfbf6", outline=color)
    d.text((x, y), text, fill=color, font=f, anchor=anchor)

def side_label(c, r, text, color, side):
    """Label outside the board edge with a leader line to the hole."""
    x, y = xy(c, r)
    if side == "left":
        lx = bx0 - 12
        d.line([x - 13, y, lx, y], fill=color, width=2)
        tag(lx - 4, y, text, color, anchor="rm")
    else:
        lx = bx1 + 12
        d.line([x + 13, y, lx, y], fill=color, width=2)
        tag(lx + 4, y, text, color, anchor="lm")

if stage >= 1:
    x0, y0 = xy(9, "J"); x1, y1 = xy(12, "P")
    d.rounded_rectangle([x0 - P * 0.45, y0 - P * 0.45, x1 + P * 0.45, y1 + P * 0.45], radius=5, fill="#222")
    d.ellipse([(x0 + x1) / 2 - 9, y1 + P * 0.45 - 9, (x0 + x1) / 2 + 9, y1 + P * 0.45 + 9], fill="#f4f4f0")
    d.text(((x0 + x1) / 2, y1 + P * 0.45 - 20), "notch", fill="#aaa", font=f10, anchor="mm")
    pins = {1: (12, "P"), 2: (12, "O"), 3: (12, "N"), 4: (12, "M"), 5: (12, "L"), 6: (12, "K"), 7: (12, "J"),
            8: (9, "J"), 9: (9, "K"), 10: (9, "L"), 11: (9, "M"), 12: (9, "N"), 13: (9, "O"), 14: (9, "P")}
    for n, (c, r) in pins.items():
        x, y = xy(c, r)
        d.ellipse([x - 6, y - 6, x + 6, y + 6], fill="#ddd")
        d.text((x + (16 if c == 12 else -16), y), str(n), fill="white", font=f10, anchor="mm")
    d.text(((x0 + x1) / 2, (y0 + y1) / 2 - 10), "DIP-14", fill="#888", font=f10, anchor="mm")
    d.text(((x0 + x1) / 2, (y0 + y1) / 2 + 8), "socket", fill="#888", font=f10, anchor="mm")

    def rail(col, color, text):
        x, ya = xy(col, "E"); _, yb = xy(col, "S")
        y = ya
        while y < yb:
            d.line([x, y, x, min(y + 12, yb)], fill=color, width=7)
            y += 19
        ring(col, "E", color); ring(col, "S", color)
        tag(x, yb + P * 0.75, text, color, f=fb)
    rail(1, BLUE, "GND")
    rail(3, RED, "5V")
    side_label(1, "T", "rails E to S, wire on the BACK", GREY, "left")

    xa, y = xy(1, "G"); xb, _ = xy(3, "G")
    d.rounded_rectangle([xa - 14, y - 15, xb + 14, y + 15], radius=8, fill="#2a2a5a")
    d.rectangle([xa - 14, y - 15, xa - 4, y + 15], fill="#ddd")
    d.text((xb, y), "+", fill="white", font=fb, anchor="mm")
    side_label(1, "G", "1000uF: stripe G01, long leg G03", GREY, "left")

if stage >= 2:
    def wire(c1, r1, c2, r2, color, text=None, frac=0.5, side=1):
        xa, ya = xy(c1, r1); xb, yb = xy(c2, r2)
        d.line([xa, ya, xb, yb], fill=color, width=4)
        if text:
            tag(xa + (xb - xa) * frac, ya + (yb - ya) * frac - 14 * side, text, color)
    wire(9, "P", 3, "Q", RED, "pin 14 to 5V", frac=0.45, side=-1)
    wire(9, "O", 1, "O", BLUE, "pin 13", frac=0.4)
    wire(9, "L", 1, "L", BLUE, "pin 10", frac=0.4)
    wire(12, "J", 1, "F", BLUE, "pin 7", frac=0.35)
    # pin 4 goes round the socket: M12 right to M14, down to T14, left to T02, up onto S01
    pts = [xy(12, "M"), xy(14, "M"), xy(14, "T"), xy(2, "T"), xy(1, "S")]
    d.line(pts, fill=BLUE, width=4, joint="curve")
    tag(xy(14, "P")[0] + 16, xy(14, "P")[1], "pin 4 to GND", BLUE, anchor="lm")
    wire(12, "P", 1, "R", BLUE, "pin 1", frac=0.3, side=-1)

    def resistor(ca, cb, r, text, bridge_to):
        xa, y = xy(ca, r); xb, _ = xy(cb, r)
        d.line([xa, y, xb, y], fill="#555", width=3)
        left, right = min(xa, xb) + P * 0.8, max(xa, xb) - P * 0.8
        d.rounded_rectangle([left, y - 9, right, y + 9], radius=4, fill="#d9b36a", outline="#555")
        tag((xa + xb) / 2, y - 20, text, GREY)
        xb2, _ = xy(bridge_to, r)
        d.line([xa, y + 6, xb2, y + 6], fill="#bbb", width=4)
    resistor(13, 17, "N", "220R N13-N17, pin 3, out A", 12)
    resistor(13, 17, "K", "220R K13-K17, pin 6, out B", 12)
    resistor(8, 4, "J", "220R J08-J04, pin 8, out C", 9)
    resistor(8, 4, "M", "220R M08-M04, pin 11, out D", 9)
    d.line([xy(17, "N")[0], xy(17, "N")[1] + 6, xy(18, "N")[0], xy(18, "N")[1] + 6], fill="#bbb", width=4)

    ring(18, "N", TEAL); side_label(18, "N", "pigtail A data (green) N18", TEAL, "right")
    ring(3, "S", RED); side_label(3, "S", "pigtail A red S03", RED, "left")
    ring(1, "K", BLUE); side_label(1, "K", "pigtail A black K01", BLUE, "left")
    side_label(12, "O", "GPIO 4 lead on pin 2 ring, O12", PURPLE, "right")
    side_label(12, "L", "GPIO 5 lead on pin 5 ring, L12", PURPLE, "right")
    ring(3, "I", RED); side_label(3, "I", "ESP 5V lead I03", RED, "left")
    ring(1, "I", BLUE); side_label(1, "I", "ESP GND lead I01", BLUE, "left")
    ring(3, "H", RED); side_label(3, "H", "USB red H03", RED, "left")
    ring(1, "H", BLUE); side_label(1, "H", "USB black H01", BLUE, "left")

ly = by1 + 30
title = {0: "ELEGOO 5 x 7 cm perfboard, component side, numbers across the top",
         1: "Stage 1: socket, rails, capacitor",
         2: "Stage 2: full shifter and distribution board"}[stage]
d.text((bx0, ly), title, fill="#222", font=fh, anchor="lm")
lines = ["Address a hole by letter then number: P12 = row P, column 12.",
         "Columns 01 and 02 have round pads, 03 to 18 oval. All 432 holes are isolated and plated through.",
         "Corner holes are 2 to 3 mm mounting holes, connected to nothing."]
if stage >= 1:
    lines += ["Dashed line = bare bus wire lying on the BACK, soldered to every ring it crosses. Column 02 stays empty.",
              "Socket spans J to P with pins in columns 09 and 12, notch toward P. Chip goes in last."]
if stage >= 2:
    lines += ["Light grey bar beside a resistor = solder bridge joining its near end to the ring next to it.",
              "Circled holes = where a wire joins a rail: lay the bare end on the rail joint on the back and melt it in.",
              "Pin wires: same at the pin end, onto the pin solder cone. Nothing is pushed through a filled hole."]
for i, t in enumerate(lines):
    d.text((bx0, ly + 30 + i * 22), t, fill="#222", font=f12, anchor="lm")
img.save(out)
print("wrote", out)
