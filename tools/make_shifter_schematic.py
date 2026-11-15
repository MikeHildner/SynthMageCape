"""Generate the KiCad 10 schematic for the Synth Mage shifter / distribution board.

Writes hardware/shifter-board/shifter-board.kicad_sch. Symbols are copied from the
KiCad standard libraries installed with KiCad 10, so the file opens with no
"missing library" warnings.

Usage:  python tools/make_shifter_schematic.py
Then:   kicad-cli sch erc  hardware/shifter-board/shifter-board.kicad_sch
        kicad-cli sch export pdf -o docs/diagrams/shifter-board-schematic.pdf hardware/shifter-board/shifter-board.kicad_sch

Circuit (matches docs/bench-session-1.md section 3):
  J6  six leads to the ESP32-S3: +5V, GND, GPIO 4/5/6/7
  U1  SN74AHCT125N, four buffers; enables tied to GND
  R1-R4  220 ohm in series with each output
  J1-J4  JST-SM 3-pin pigtails: pin 1 +5V (red), pin 2 data (green), pin 3 GND (white)
  C1  1000 uF across +5V and GND
  J5  USB-A cable, bare end: 5V and GND in
"""
import os
import sys
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB = r"C:\Program Files\KiCad\10.0\share\kicad\symbols"

# Two projects share this circuit. The default is the printed board; "--perfboard"
# writes the same schematic into hardware/perfboard-4/ with footprints that match
# the hand-wired perfboard plan made by tools/make_perfboard_plan.py.
PERFBOARD = "--perfboard" in sys.argv
if PERFBOARD:
    PROJECT = "perfboard-4"
    OUT = os.path.join(ROOT, "hardware", "perfboard-4", "perfboard-4.kicad_sch")
    SHEET_UUID = "7c1e2a90-4b7d-4f3e-9a21-5d8e0c6b3f41"
    TITLE = "Synth Mage shifter board, perfboard four plan"
else:
    PROJECT = "shifter-board"
    OUT = os.path.join(ROOT, "hardware", "shifter-board", "shifter-board.kicad_sch")
    SHEET_UUID = "2bc33eba-ebb5-42aa-9fa3-d6010a02a530"   # keep the uuid KiCad gave the sheet
    TITLE = "Synth Mage shifter and distribution board"

# ---------------------------------------------------------------- s-expression helpers

def block(text, start):
    """Return the balanced (...) block starting at index `start`."""
    depth = 0
    j = start
    while True:
        ch = text[j]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:j + 1]
        j += 1


def lib_symbol(lib, name):
    """Copy a symbol definition out of a KiCad library, renamed to lib:name.
    Symbols that `extend` another are flattened by copying the parent body."""
    src = open(os.path.join(LIB, lib + ".kicad_sym"), encoding="utf-8").read()
    blk = block(src, src.find('(symbol "%s"' % name))
    if "(extends " in blk:
        parent = blk.split('(extends "')[1].split('"')[0]
        pblk = block(src, src.find('(symbol "%s"' % parent))
        # KiCad flattens a derived symbol by taking the parent's body and replacing
        # the whole property list with the child's own properties.
        def props(b):
            out = []
            i = b.find('(property "')
            while i != -1:
                pb = block(b, i)
                out.append(pb)
                i = b.find('(property "', i + len(pb))
            return out
        pprops = props(pblk)
        cprops = props(blk)
        first = pblk.find(pprops[0])
        last = pblk.find(pprops[-1]) + len(pprops[-1])
        body = pblk[:first] + "\n\t\t".join(cprops) + pblk[last:]
        body = body.replace('(symbol "%s"' % parent, '(symbol "%s"' % name, 1)
        body = body.replace('(symbol "%s_' % parent, '(symbol "%s_' % name)
        blk = body
    return blk.replace('(symbol "%s"' % name, '(symbol "%s:%s"' % (lib, name), 1)


def U():
    return str(uuid.uuid4())


# ---------------------------------------------------------------- geometry
# KiCad places a library point (px, py) for a symbol at (X, Y) with rotation r as:
#   r = 0   -> (X + px, Y - py)
#   r = 90  -> (X - py, Y - px)
#   r = 180 -> (X - px, Y + py)
#   r = 270 -> (X + py, Y + px)
# (checked against kicad-cli netlist output on a scratch sheet)

def place(X, Y, r, px, py):
    return {0: (X + px, Y - py), 90: (X - py, Y - px),
            180: (X - px, Y + py), 270: (X + py, Y + px)}[r]


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


items = []


FOOTPRINTS = {
    "U": "Package_DIP:DIP-14_W7.62mm_Socket",
    "R": "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
    "C": "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm",
    "J5": "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MPT-0,5-2-2.54_1x02_P2.54mm_Horizontal",
    "J6": "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
    "J": "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MPT-0,5-3-2.54_1x03_P2.54mm_Horizontal",
    "H": "MountingHole:MountingHole_3.2mm_M3",
}
if PERFBOARD:
    # Hand-wired on the ELEGOO 5 x 7 cm board: 220 R on 3 holes, connectors are
    # loose wires that each take one hole, so they get footprints of their own.
    FOOTPRINTS.update({
        "R": "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
        "J5": "SynthCape:USB_2wire",
        "J6": "SynthCape:ESP_6wire",
        "J": "SynthCape:Pigtail_3wire",
        "H": "MountingHole:MountingHole_2.2mm_M2",
    })


def footprint_for(ref):
    if ref.startswith("#"):
        return ""
    return FOOTPRINTS.get(ref, FOOTPRINTS.get(ref[0], ""))


def symbol(lib, name, ref, value, X, Y, r, pins, unit=1, ref_at=None, val_at=None, hide_value=False):
    hide_ref = ref.startswith("#")   # power symbols and flags: KiCad hides their reference
    ref_at = ref_at or (X, Y - 5.08)
    val_at = val_at or (X, Y + 5.08)
    pin_uuids = "".join('\n\t\t(pin "%s"\n\t\t\t(uuid "%s")\n\t\t)' % (p, U()) for p in pins)
    items.append('''\t(symbol
\t\t(lib_id "%s:%s")
\t\t(at %s %s %d)
\t\t(unit %d)
\t\t(body_style 1)
\t\t(exclude_from_sim no)
\t\t(in_bom %s)
\t\t(on_board yes)
\t\t(in_pos_files yes)
\t\t(dnp no)
\t\t(uuid "%s")
\t\t(property "Reference" "%s"
\t\t\t(at %s %s 0)
\t\t\t%s(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "%s"
\t\t\t(at %s %s 0)
\t\t\t%s(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Footprint" "%s"
\t\t\t(at %s %s 0)
\t\t\t(hide yes)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at %s %s 0)
\t\t\t(hide yes)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)%s
\t\t(instances
\t\t\t(project "%s"
\t\t\t\t(path "/%s"
\t\t\t\t\t(reference "%s")
\t\t\t\t\t(unit %d)
\t\t\t\t)
\t\t\t)
\t\t)
\t)''' % (lib, name, f(X), f(Y), r, unit, "no" if ref.startswith("H") else "yes", U(), ref, f(ref_at[0]), f(ref_at[1]),
           "(hide yes)\n\t\t\t" if hide_ref else "", value,
           f(val_at[0]), f(val_at[1]), "(hide yes)\n\t\t\t" if hide_value else "",
           footprint_for(ref), f(X), f(Y), f(X), f(Y), pin_uuids, PROJECT, SHEET_UUID, ref, unit))


def wire(*pts):
    for a, b in zip(pts, pts[1:]):
        items.append('\t(wire\n\t\t(pts\n\t\t\t(xy %s %s) (xy %s %s)\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n\t\t(uuid "%s")\n\t)'
                     % (f(a[0]), f(a[1]), f(b[0]), f(b[1]), U()))


def junction(x, y):
    items.append('\t(junction\n\t\t(at %s %s)\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)' % (f(x), f(y), U()))


def label(text, x, y, r=0):
    just = {0: "left bottom", 180: "right bottom"}[r]
    items.append('\t(label "%s"\n\t\t(at %s %s %d)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n\t\t\t(justify %s)\n\t\t)\n\t\t(uuid "%s")\n\t)'
                 % (text, f(x), f(y), r, just, U()))


def note(text, x, y, size=1.27):
    items.append('\t(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %s %s 0)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size %s %s)\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n\t\t(uuid "%s")\n\t)'
                 % (text.replace("\n", "\\n"), f(x), f(y), f(size), f(size), U()))


# power symbols: pin at the origin. +5V graphic goes up, GND goes down, PWR_FLAG goes up.
_pwr_n = [0]


def power(name, x, y, r=0):
    _pwr_n[0] += 1
    symbol("power", name, "#PWR%02d" % _pwr_n[0], name, x, y, r, ["1"],
           ref_at=(x, y), val_at=(x, y - 3.81 if name != "GND" else y + 3.81))


_flg_n = [0]


def pwr_flag(x, y):
    _flg_n[0] += 1
    symbol("power", "PWR_FLAG", "#FLG%02d" % _flg_n[0], "PWR_FLAG", x, y, 0, ["1"],
           ref_at=(x, y), val_at=(x, y - 5.08))


# ---------------------------------------------------------------- the circuit
CHIP_PINS = [str(n) for n in range(1, 15)]

# J6: six leads to the ESP32-S3. Rotation 180 puts the pins on the right, pin 1 at the bottom.
JX, JY = 40.64, 101.6
symbol("Connector_Generic", "Conn_01x06", "J6", "ESP32-S3 leads", JX, JY, 180, ["1", "2", "3", "4", "5", "6"],
       ref_at=(JX - 2.54, JY - 11.43), val_at=(JX - 2.54, JY + 8.89))
# rotation 180 puts pin 1 at the bottom; the pin order is chosen so the PCB routes without crossings
pin6 = place(JX, JY, 180, -5.08, -7.62)   # GND   (top of symbol)
pin5 = place(JX, JY, 180, -5.08, -5.08)   # GPIO 7
pin4 = place(JX, JY, 180, -5.08, -2.54)   # GPIO 5
pin3 = place(JX, JY, 180, -5.08, 0)       # GPIO 4
pin2 = place(JX, JY, 180, -5.08, 2.54)    # GPIO 6
pin1 = place(JX, JY, 180, -5.08, 5.08)    # +5V   (bottom of symbol)
wire(pin1, (50.8, pin1[1]), (50.8, 111.76)); power("+5V", 50.8, 111.76)
wire(pin6, (50.8, pin6[1]), (50.8, 88.9)); power("GND", 50.8, 88.9)
for p, name in ((pin5, "GPIO7"), (pin4, "GPIO5"), (pin3, "GPIO4"), (pin2, "GPIO6")):
    wire(p, (55.88, p[1])); label(name, 55.88, p[1])

# U1A..U1D: the four buffers, stacked. Input pin on the left, output on the right, enable below.
UX = 116.84
buffers = [  # unit, Y, input pin, output pin, enable pin, gpio, resistor, connector, output name
    (1, 50.8, "2", "3", "1", "GPIO4", "R1", "J1", "OUT1 left front"),
    (2, 76.2, "5", "6", "4", "GPIO5", "R2", "J2", "OUT2 right front"),
    (4, 101.6, "12", "11", "13", "GPIO6", "R3", "J3", "OUT3 left drape"),
    (3, 127.0, "9", "8", "10", "GPIO7", "R4", "J4", "OUT4 right drape"),
]
for unit, Y, pin_in, pin_out, pin_en, gpio, rref, jref, jname in buffers:
    symbol("74xx", "74AHCT125", "U1", "SN74AHCT125N", UX, Y, 0, CHIP_PINS, unit=unit,
           ref_at=(UX, Y - 7.62), val_at=(UX, Y - 5.08), hide_value=(unit != 1))
    p_in = place(UX, Y, 0, -7.62, 0)
    p_out = place(UX, Y, 0, 7.62, 0)
    p_en = place(UX, Y, 0, 0, -6.35)
    # input from a net label
    wire((99.06, Y), p_in); label(gpio, 99.06, Y)
    # enable to ground
    wire(p_en, (UX, Y + 10.16)); power("GND", UX, Y + 10.16)
    # output through 220R to the connector's data pin
    RX = 133.35
    symbol("Device", "R", rref, "220R", RX, Y, 90, ["1", "2"], ref_at=(RX, Y - 3.81), val_at=(RX, Y + 3.81))
    r1 = place(RX, Y, 90, 0, 3.81)
    r2 = place(RX, Y, 90, 0, -3.81)
    wire(p_out, r1)
    CX = 152.4
    symbol("Connector_Generic", "Conn_01x03", jref, jname, CX, Y, 0, ["1", "2", "3"],
           ref_at=(CX + 2.54, Y - 5.08), val_at=(CX + 2.54, Y + 6.35))
    c5v = place(CX, Y, 0, -5.08, 2.54)
    cdat = place(CX, Y, 0, -5.08, 0)
    cgnd = place(CX, Y, 0, -5.08, -2.54)
    wire(r2, cdat)
    wire(c5v, (142.24, c5v[1]), (142.24, Y - 7.62)); power("+5V", 142.24, Y - 7.62)
    wire(cgnd, (142.24, cgnd[1]), (142.24, Y + 7.62)); power("GND", 142.24, Y + 7.62)

# U1E: chip power pins
EX, EY = 203.2, 96.52
symbol("74xx", "74AHCT125", "U1", "SN74AHCT125N", EX, EY, 0, CHIP_PINS, unit=5,
       ref_at=(EX + 5.08, EY - 2.54), val_at=(EX + 5.08, EY), hide_value=True)
vcc = place(EX, EY, 0, 0, 12.7)
gnd = place(EX, EY, 0, 0, -12.7)
wire(vcc, (EX, EY - 17.78)); power("+5V", EX, EY - 17.78)
wire(gnd, (EX, EY + 17.78)); power("GND", EX, EY + 17.78)

# C1: 1000 uF bulk capacitor, + at the top
CX1, CY1 = 223.52, 96.52
symbol("Device", "C_Polarized", "C1", "1000uF", CX1, CY1, 0, ["1", "2"], ref_at=(CX1 + 3.81, CY1 - 2.54), val_at=(CX1 + 3.81, CY1 + 2.54))
cp = place(CX1, CY1, 0, 0, 3.81)
cn = place(CX1, CY1, 0, 0, -3.81)
wire(cp, (CX1, CY1 - 17.78)); power("+5V", CX1, CY1 - 17.78)
wire(cn, (CX1, CY1 + 17.78)); power("GND", CX1, CY1 + 17.78)

# J5: USB-A cable bare end. Pin 2 (upper) = 5V red, pin 1 (lower) = GND black.
JX5, JY5 = 248.92, 96.52
symbol("Connector_Generic", "Conn_01x02", "J5", "USB 5V in", JX5, JY5, 0, ["1", "2"],
       ref_at=(JX5 + 2.54, JY5 - 5.08), val_at=(JX5 + 2.54, JY5 + 5.08))
u_gnd = place(JX5, JY5, 0, -5.08, 0)
u_5v = place(JX5, JY5, 0, -5.08, -2.54)
wire(u_5v, (238.76, u_5v[1]), (238.76, JY5 - 17.78)); power("+5V", 238.76, JY5 - 17.78)
pwr_flag(238.76, JY5 - 10.16); junction(238.76, JY5 - 10.16)
wire(u_gnd, (238.76, u_gnd[1]), (238.76, JY5 + 17.78)); power("GND", 238.76, JY5 + 17.78)
pwr_flag(238.76, JY5 + 10.16); junction(238.76, JY5 + 10.16)

# mounting holes, so the PCB and the schematic list the same parts
for i in range(4):
    symbol("Mechanical", "MountingHole", "H%d" % (i + 1), "M3", 30.48 + i * 15.24, 160.02, 0, [],
           ref_at=(30.48 + i * 15.24, 155.0), val_at=(30.48 + i * 15.24, 163.0))

# notes
note("Synth Mage: level shifter and distribution board", 30.48, 30.48, 2.54)
note("ESP32-S3 GPIO 3.3 V in, 5 V out through SN74AHCT125N. 220R in series with every data line.\n"
     "Outputs J1-J4 are 3-way screw terminals: pin 1 +5V (pigtail red), pin 2 data (green), pin 3 GND (white). J5 USB in and J1-J4 are Phoenix MPT 0,5 2.54 mm blocks.\n"
     "Today only OUT1 (GPIO 4) and OUT2 (GPIO 5) are built; OUT3 (GPIO 6) and OUT4 (GPIO 7) are wired the same way later.",
     30.48, 40.64)
note("Power: USB-A cable from the bank, red to +5V, black to GND. C1 1000 uF, long leg to +5V.\n"
     "The ESP32-S3 is powered from this rail through its 5V and GND header pins.", 180.34, 134.62)

# ---------------------------------------------------------------- write the file
libs = "\n".join(lib_symbol(*ln) for ln in (
    ("74xx", "74AHCT125"), ("Device", "R"), ("Device", "C_Polarized"),
    ("Connector_Generic", "Conn_01x02"), ("Connector_Generic", "Conn_01x03"),
    ("Connector_Generic", "Conn_01x06"), ("power", "+5V"), ("power", "GND"), ("power", "PWR_FLAG"),
    ("Mechanical", "MountingHole")))
# indent library symbols to sit inside (lib_symbols ...)
libs = "\n".join("\t" + ln if ln.strip() else ln for ln in libs.splitlines())

sch = '''(kicad_sch
\t(version 20260306)
\t(generator "eeschema")
\t(generator_version "10.0")
\t(uuid "%s")
\t(paper "A4")
\t(title_block
\t\t(title "%s")
\t\t(date "2026-11-07")
\t\t(rev "A")
\t\t(comment 1 "Generated by tools/make_shifter_schematic.py")
\t)
\t(lib_symbols
%s
\t)
%s
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
\t(embedded_fonts no)
)
''' % (SHEET_UUID, TITLE, libs, "\n".join(items))
open(OUT, "w", encoding="utf-8", newline="\n").write(sch)
print("wrote", OUT, len(sch), "bytes,", len(items), "items")
