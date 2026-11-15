"""Perfboard four: the shifter / distribution circuit planned hole by hole on the
ELEGOO 5 x 7 cm board, as a KiCad PCB so KiCad can check it against the schematic.

Run with KiCad's own Python so the pcbnew module and footprint libraries are found:

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" tools\\make_perfboard_plan.py

Writes hardware/perfboard-4/perfboard-4.kicad_pcb. The schematic for this project is
written by  python tools/make_shifter_schematic.py --perfboard  and the two are checked
against each other with

  kicad-cli pcb drc --schematic-parity --severity-all --format report -o drc.txt hardware\\perfboard-4\\perfboard-4.kicad_pcb

How to read the plan (this is a model of hand wiring, not a board to manufacture):

  * The board is held with the numbers 01..18 across the top and the letters A..X
    down the sides. A hole is letter then number: P12 is row P, column 12.
  * A pad is a hole with a component lead or a loose wire (pigtail, USB, Dupont) in it.
  * A via is a plain hole with the end of an insulated jumper pushed through it.
  * F.Cu (red) track   = insulated solid-core jumper lying on the FRONT.
  * B.Cu (blue) track  = bare conductor on the BACK: a lead bent over to the next
    hole, or the bare bus wire of a rail.
  * Silkscreen circles = holes that stay empty.

The project file turns five DRC checks down to "ignore" because they are about
manufactured boards, not hand wiring: courtyards_overlap and pth_inside_courtyard
(parts sit shoulder to shoulder on perfboard, the holes decide), silk_overlap
(there is no silkscreen), lib_footprint_issues and lib_footprint_mismatch (the
wire "footprints" are one-off hole sets that live only in this file).

Layout: chip notch at the top, pins 1..7 down column 09 (rows H..N), 14..8 down
column 12. GND rail is bare wire along column 01 on the back, +5V rail along
column 18, both rows D..V. Column 02 and column 17 carry no joints next to a rail.
Every socket pin that needs a connection gets it through the hole beside it
(column 08 or 13), with the lead's tail bent over to the pin on the back.
"""
import os
import pcbnew

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "hardware", "perfboard-4", "perfboard-4.kicad_pcb")
FP = r"C:\Program Files\KiCad\10.0\share\kicad\footprints"

OX, OY = 100.0, 100.0        # board origin on the KiCad sheet
BW, BH = 50.0, 70.0          # ELEGOO 5 x 7 cm board
PITCH = 2.54
COLS, NROWS = 18, 24
ROWS = "ABCDEFGHIJKLMNOPQRSTUVWX"
X0 = (BW - (COLS - 1) * PITCH) / 2
Y0 = (BH - (NROWS - 1) * PITCH) / 2


def hole(name):
    """'J04' -> board-local mm of that hole's centre."""
    return (X0 + (int(name[1:]) - 1) * PITCH, Y0 + ROWS.index(name[0]) * PITCH)


def P(x, y):
    return pcbnew.VECTOR2I_MM(OX + x, OY + y)


def mm(v):
    return (round(v.x / 1e6 - OX, 3), round(v.y / 1e6 - OY, 3))


board = pcbnew.BOARD()
board.GetDesignSettings().SetCopperLayerCount(2)
F, B = pcbnew.F_Cu, pcbnew.B_Cu

# ------------------------------------------------------------------ nets (from the schematic)
NET_NAMES = ["+5V", "GND", "/GPIO4", "/GPIO5", "/GPIO6", "/GPIO7",
             "Net-(J1-Pin_2)", "Net-(J2-Pin_2)", "Net-(J3-Pin_2)", "Net-(J4-Pin_2)",
             "Net-(R1-Pad1)", "Net-(R2-Pad1)", "Net-(R3-Pad1)", "Net-(R4-Pad1)"]
nets = {}
for n in NET_NAMES:
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    nets[n] = ni

PAD_NETS = {
    ("U1", "14"): "+5V", ("U1", "7"): "GND", ("U1", "1"): "GND", ("U1", "4"): "GND",
    ("U1", "10"): "GND", ("U1", "13"): "GND",
    ("U1", "2"): "/GPIO4", ("U1", "5"): "/GPIO5", ("U1", "12"): "/GPIO6", ("U1", "9"): "/GPIO7",
    ("U1", "3"): "Net-(R1-Pad1)", ("U1", "6"): "Net-(R2-Pad1)", ("U1", "11"): "Net-(R3-Pad1)", ("U1", "8"): "Net-(R4-Pad1)",
    ("R1", "1"): "Net-(R1-Pad1)", ("R1", "2"): "Net-(J1-Pin_2)",
    ("R2", "1"): "Net-(R2-Pad1)", ("R2", "2"): "Net-(J2-Pin_2)",
    ("R3", "1"): "Net-(R3-Pad1)", ("R3", "2"): "Net-(J3-Pin_2)",
    ("R4", "1"): "Net-(R4-Pad1)", ("R4", "2"): "Net-(J4-Pin_2)",
    ("J1", "1"): "+5V", ("J1", "2"): "Net-(J1-Pin_2)", ("J1", "3"): "GND",
    ("J2", "1"): "+5V", ("J2", "2"): "Net-(J2-Pin_2)", ("J2", "3"): "GND",
    ("J3", "1"): "+5V", ("J3", "2"): "Net-(J3-Pin_2)", ("J3", "3"): "GND",
    ("J4", "1"): "+5V", ("J4", "2"): "Net-(J4-Pin_2)", ("J4", "3"): "GND",
    ("J5", "1"): "GND", ("J5", "2"): "+5V",
    ("J6", "1"): "+5V", ("J6", "2"): "/GPIO6", ("J6", "3"): "/GPIO4", ("J6", "4"): "/GPIO5", ("J6", "5"): "/GPIO7", ("J6", "6"): "GND",
    ("C1", "1"): "+5V", ("C1", "2"): "GND",
}

# ------------------------------------------------------------------ where everything goes
# Component leads: footprint pad -> hole.
PLACE = {
    "U1": {"1": "H09", "14": "H12"},                 # pins 1..7 down col 09, 14..8 down col 12
    "R1": {"1": "J08", "2": "J05"},                  # pin 3 output, left
    "R2": {"1": "M08", "2": "M05"},                  # pin 6 output, left
    "R3": {"1": "K13", "2": "K16"},                  # pin 11 output, right
    "R4": {"1": "N13", "2": "Q13"},                  # pin 8 output, right, standing down
    "C1": {"1": "V18", "2": "V16"},                  # + on the 5V rail, - two holes in
}
# Loose wires: each wire takes one hole. Pigtail pin 1 red, 2 green, 3 white.
WIRES = {
    "J1": ("Pigtail_3wire", {"1": "J18", "2": "J04", "3": "J01"}, "J04"),
    "J2": ("Pigtail_3wire", {"1": "M18", "2": "M04", "3": "M01"}, "M04"),
    "J3": ("Pigtail_3wire", {"1": "K18", "2": "L16", "3": "L01"}, "L16"),
    "J4": ("Pigtail_3wire", {"1": "O18", "2": "Q12", "3": "O01"}, "Q12"),
    "J5": ("USB_2wire", {"1": "V01", "2": "U18"}, "U09"),   # black landed in V01 on the real board
    "J6": ("ESP_6wire", {"1": "D18", "2": "J13", "3": "I08", "4": "L08", "5": "M13", "6": "D01"}, "D09"),
}
# Insulated jumpers on the front: net, then the holes the wire passes, first and last
# are the holes it goes through (vias), the middle ones are only bends.
JUMPERS = [
    ("GND", ["H08", "H01"]),                         # pin 1
    ("GND", ["K08", "K01"]),                         # pin 4
    ("GND", ["N08", "N01"]),                         # pin 7
    ("+5V", ["H13", "H18"]),                         # pin 14
    ("GND", ["I13", "I17", "T17", "T01"]),           # pin 13, down the right side and under
    ("GND", ["L13", "L14", "R14", "R01"]),           # pin 10, same idea, inside the first
    ("GND", ["V15", "U15", "U01"]),                  # C1 minus leg, bent to V15 first; V01 is taken by USB black
]
# Bare bends on the back: a lead's tail folded over to the hole beside it.
BENDS = [
    ("GND", "H09", "H08"), ("GND", "K09", "K08"), ("GND", "N09", "N08"),
    ("+5V", "H12", "H13"), ("GND", "I12", "I13"), ("GND", "L12", "L13"),
    ("/GPIO4", "I09", "I08"), ("/GPIO5", "L09", "L08"),
    ("/GPIO6", "J12", "J13"), ("/GPIO7", "M12", "M13"),
    ("Net-(R1-Pad1)", "J09", "J08"), ("Net-(J1-Pin_2)", "J05", "J04"),
    ("Net-(R2-Pad1)", "M09", "M08"), ("Net-(J2-Pin_2)", "M05", "M04"),
    ("Net-(R3-Pad1)", "K12", "K13"), ("Net-(J3-Pin_2)", "K16", "L16"),
    ("Net-(R4-Pad1)", "N12", "N13"), ("Net-(J4-Pin_2)", "Q13", "Q12"),
    ("GND", "V16", "V15"),
]
RAILS = [("GND", "01"), ("+5V", "18")]
RAIL_ROWS = "DV"

# ------------------------------------------------------------------ footprints
fps = {}
LIBS = {
    "U1": ("Package_DIP", "DIP-14_W7.62mm_Socket", "SN74AHCT125N"),
    "R1": ("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "220R"),
    "R2": ("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "220R"),
    "R3": ("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "220R"),
    "R4": ("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "220R"),
    "C1": ("Capacitor_THT", "CP_Radial_D10.0mm_P5.00mm", "1000uF"),
}
VALUES = {"J1": "OUT1 left front", "J2": "OUT2 right front", "J3": "OUT3 left drape",
          "J4": "OUT4 right drape", "J5": "USB 5V in", "J6": "ESP32-S3 leads"}


def assign_nets(fp, ref):
    for pad in fp.Pads():
        key = (ref, pad.GetNumber())
        if key in PAD_NETS:
            pad.SetNet(nets[PAD_NETS[key]])


def style_ref(fp, x, y, size=0.8):
    fp.Value().SetVisible(False)
    r = fp.Reference()
    r.SetPosition(P(x, y))
    r.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
    r.SetTextThickness(pcbnew.FromMM(size * 0.15))


def lib_footprint(ref):
    lib, name, value = LIBS[ref]
    fp = pcbnew.FootprintLoad(os.path.join(FP, lib + ".pretty"), name)
    fp.SetFPIDAsString("%s:%s" % (lib, name))
    fp.SetReference(ref)
    fp.SetValue(value)
    board.Add(fp)
    (n1, h1), (n2, h2) = list(PLACE[ref].items())
    t1, t2 = hole(h1), hole(h2)
    for rot in (0, 90, 180, 270):
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(P(0, 0))
        p1 = fp.FindPadByNumber(n1).GetPosition()
        fp.Move(P(*t1) - p1)
        p2 = mm(fp.FindPadByNumber(n2).GetPosition())
        if abs(p2[0] - t2[0]) < 0.1 and abs(p2[1] - t2[1]) < 0.1:
            break
    else:
        raise SystemExit("%s: no rotation puts pad %s on %s" % (ref, n2, h2))
    assign_nets(fp, ref)
    fps[ref] = fp
    return fp


# a round 1.7 mm pad with a 1.0 mm hole, same as a pin-header pad: that is a perfboard hole
_hdr = pcbnew.FootprintLoad(os.path.join(FP, "Connector_PinHeader_2.54mm.pretty"), "PinHeader_1x02_P2.54mm_Vertical")
_HOLE_PAD = _hdr.FindPadByNumber("2")


def wire_footprint(ref):
    name, pads, anchor = WIRES[ref]
    fp = pcbnew.FOOTPRINT(board)
    fp.SetFPIDAsString("SynthCape:" + name)
    fp.SetReference(ref)
    fp.SetValue(VALUES[ref])
    fp.SetPosition(P(*hole(anchor)))
    board.Add(fp)
    for num, h in pads.items():
        pad = pcbnew.PAD(_HOLE_PAD)
        pad.SetParent(fp)
        fp.Add(pad)
        pad.SetNumber(num)
        pad.SetPosition(P(*hole(h)))
    assign_nets(fp, ref)
    fps[ref] = fp
    return fp


for ref in LIBS:
    lib_footprint(ref)
for ref in WIRES:
    wire_footprint(ref)

# the DIP's remaining pins must have landed on the grid too
for n in range(1, 15):
    x, y = mm(fps["U1"].FindPadByNumber(str(n)).GetPosition())
    col = round((x - X0) / PITCH) + 1
    row = ROWS[round((y - Y0) / PITCH)]
    expect = ("%s%02d" % (ROWS[ROWS.index("H") + n - 1], 9)) if n <= 7 else ("%s%02d" % (ROWS[ROWS.index("H") + 14 - n], 12))
    assert "%s%02d" % (row, col) == expect, (n, row, col, expect)

# mounting holes, ELEGOO style corner holes
for i, (x, y) in enumerate(((2.2, 2.2), (BW - 2.2, 2.2), (2.2, BH - 2.2), (BW - 2.2, BH - 2.2)), 1):
    fp = pcbnew.FootprintLoad(os.path.join(FP, "MountingHole.pretty"), "MountingHole_2.2mm_M2")
    fp.SetFPIDAsString("MountingHole:MountingHole_2.2mm_M2")
    fp.SetReference("H%d" % i)
    fp.SetValue("M3")
    fp.SetPosition(P(x, y))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    fp.SetExcludedFromBOM(True)
    board.Add(fp)

# ------------------------------------------------------------------ outline
pts = [(0, 0), (BW, 0), (BW, BH), (0, BH)]
for a, b in zip(pts, pts[1:] + pts[:1]):
    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_SEGMENT)
    edge.SetStart(P(*a))
    edge.SetEnd(P(*b))
    edge.SetLayer(pcbnew.Edge_Cuts)
    edge.SetWidth(pcbnew.FromMM(0.1))
    board.Add(edge)

# ------------------------------------------------------------------ wiring
occupied = set()          # holes that have something in them
for fp in fps.values():
    for pad in fp.Pads():
        x, y = mm(pad.GetPosition())
        occupied.add("%s%02d" % (ROWS[round((y - Y0) / PITCH)], round((x - X0) / PITCH) + 1))


def via(net, h):
    assert h not in occupied, "jumper hole %s is already taken" % h
    occupied.add(h)
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(*hole(h)))
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetLayerPair(F, B)
    v.SetWidth(pcbnew.FromMM(1.7))
    v.SetDrill(pcbnew.FromMM(1.0))
    v.SetNet(nets[net])
    board.Add(v)


def track(net, layer, width, *holes):
    for a, b in zip(holes, holes[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(P(*hole(a)))
        t.SetEnd(P(*hole(b)))
        t.SetWidth(pcbnew.FromMM(width))
        t.SetLayer(layer)
        t.SetNet(nets[net])
        board.Add(t)


for net, path in JUMPERS:
    via(net, path[0])
    via(net, path[-1])
    track(net, F, 0.6, *path)
for net, a, b in BENDS:
    track(net, B, 0.6, a, b)
for net, col in RAILS:
    track(net, B, 0.8, RAIL_ROWS[0] + col, RAIL_ROWS[1] + col)

# ------------------------------------------------------------------ silkscreen


def text(s, x, y, size=0.8, rot=0, layer=pcbnew.F_SilkS, bold=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(P(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
    t.SetTextThickness(pcbnew.FromMM(size * 0.15))
    t.SetTextAngleDegrees(rot)
    t.SetBold(bold)
    board.Add(t)


def circle(x, y, r, layer=pcbnew.Dwgs_User):
    c = pcbnew.PCB_SHAPE(board)
    c.SetShape(pcbnew.SHAPE_T_CIRCLE)
    c.SetCenter(P(x, y))
    c.SetEnd(P(x + r, y))
    c.SetLayer(layer)
    c.SetWidth(pcbnew.FromMM(0.12))
    board.Add(c)


# rail labels sit in the empty columns 02 and 17
NO_CIRCLE = {"%s02" % r for r in "GHIJKLMNOPQ"} | {"%s17" % r for r in "ABCDEFG"}
for r in ROWS:
    for c in range(1, COLS + 1):
        h = "%s%02d" % (r, c)
        if h not in occupied and h not in NO_CIRCLE:
            circle(*hole(h), 0.75)
for c in range(1, COLS + 1):
    x = hole("A%02d" % c)[0]
    text("%02d" % c, x, Y0 - 1.9, 0.8)
    text("%02d" % c, x, Y0 + (NROWS - 1) * PITCH + 1.9, 0.8)
for r in ROWS:
    y = hole(r + "01")[1]
    text(r, 1.5, y, 0.8)
    text(r, BW - 1.5, y, 0.8)

text("GND RAIL: BARE WIRE ON BACK, COL 01, ROWS D-V", *hole("L02"), 0.8, rot=90)
text("5V RAIL: COL 18", *hole("D17"), 0.8, rot=90)
text("PERFBOARD 4", hole("A05")[0], hole("A04")[1] - 0.2, 1.0, bold=True)
text("notch", hole("G10")[0] + PITCH / 2, hole("G10")[1] + 0.3, 0.8)
for h, lab in (("I08", "G4"), ("L08", "G5"), ("J13", "G6"), ("M13", "G7")):
    x, y = hole(h)
    text(lab, x + (-2.1 if h.endswith("08") else 2.1), y, 0.8)

# references next to what they label
style_ref(fps["U1"], (hole("H09")[0] + hole("H12")[0]) / 2, hole("P09")[1] + 0.6)
for ref, (h, dx, dy) in {"R1": ("J06", 0.5, -1.9), "R2": ("M06", 0.5, -1.9), "R3": ("K14", 0.5, 1.9),
                         "R4": ("O13", 2.7, 1.27), "C1": ("W14", 0, 0)}.items():
    x, y = hole(h)
    style_ref(fps[ref], x + dx, y + dy)
for ref, (name, pads, anchor) in WIRES.items():
    x, y = hole(anchor)
    # J3's label goes below its hole: R3's pad sits in the hole above it
    style_ref(fps[ref], x, y if ref == "J6" else (y + 1.7 if ref == "J3" else y - 1.7))

pcbnew.SaveBoard(OUT, board)
print("wrote", OUT)
print("holes used:", len(occupied))
for ref in sorted(fps):
    print(" ", ref, {p.GetNumber(): "%s%02d" % (ROWS[round((mm(p.GetPosition())[1] - Y0) / PITCH)],
                                               round((mm(p.GetPosition())[0] - X0) / PITCH) + 1)
                     for p in fps[ref].Pads()})
