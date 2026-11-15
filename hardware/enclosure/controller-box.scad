// Synth Mage controller box: a tray and a screw-down lid, printed.
// Option 1 of the three concepts (square, screw lid), chosen 12 Nov 2026.
//
// Board positions were proved with fit plate two on 12 Nov 2026. The lid,
// its clamp posts and the cable slots are untested until the first print.
//
// To use: change a number, press F5 to preview, F6 to render, then
// File > Export > STL for PrusaSlicer.
//
// All sizes are millimetres. The origin is the inside corner of the tray
// floor; X runs along the length, Y across the width, Z up.
// The -X end holds the ESP32 antenna; the +X end is the cable end, where
// the perfboard's capacitor and USB cable sit and the slots are cut.

// ---------------------------------------------------------------- what to draw
// "all"      tray and lid as they go on the printer
// "tray"     the tray alone, for export
// "lid"      the lid alone, upside down as printed, for export
// "assembly" lid in place on the tray, to look at
// "clash"    anything where lid and tray overlap (should be empty)
// "fitplate" the thin test plate for board positions
part  = "all";
board = "perfboard";   // "perfboard" (board four) or "printed" (OSH Park board)

// ---------------------------------------------------------------- the boards
// Shifter board: length, width, mounting-hole spacing and hole diameter.
pb_l       = board == "printed" ? 56  : 69.8;  // perfboard: measured
pb_w       = board == "printed" ? 52  : 50.06; // perfboard: measured
pb_hole_dx = board == "printed" ? 49  : 66;    // perfboard: proved by fit plate two
pb_hole_dy = board == "printed" ? 45  : 46;    // perfboard: proved by fit plate two
pb_hole_d  = board == "printed" ? 3.2 : 2.0;   // perfboard: caliper 2.0 to 2.1
pb_t       = 1.6;                              // board thickness

// ESP32: the circuit board itself, and how far the antenna end of the
// module hangs past one end of it. No mounting holes. It lies plugs-up.
esp_l   = 57.65;       // measured 12 Nov 2026
esp_w   = 28.24;       // measured 12 Nov 2026
esp_ant = 6.5;         // antenna overhang, measured 12 Nov 2026

// Clear height inside, floor to underside of lid.
// Measured 12 Nov 2026: shifter board 21.5 high, ESP32 with plugs 20 high.
// ESP32: 4 (cradle) + 20 + about 6 for the leads to bend = 30.
inner_h = 30;

// ---------------------------------------------------------------- choices
wall       = 2;        // side walls
floor_t    = 2;        // tray floor
lid_t      = 2;        // lid plate
corner_r   = 3;        // rounding of the outside corners
gap        = 3;        // space between a board and a wall or another board
cable_room = 15;       // free space at the cable end for bends and loops

post_h   = 4;          // board sits this far above the floor (room for solder joints)
post_d   = 4.5;        // post diameter, tray and lid
peg_h    = 2.5;        // peg that goes up through the board's mounting hole
peg_fit  = 0.3;        // peg is this much thinner than the hole

esp_post_h = 4;        // ESP32 sits this far above the floor
esp_fit    = 0.4;      // slack around the ESP32 outline

// Posts under the lid that stop the boards lifting off their supports.
pb_clamp_gap  = 0.2;   // air between a lid post and the top of the shifter board
esp_clamp_gap = 1.0;   // same for the ESP32, which rests a little high on its solder joints

// Chip hold-down, perfboard only. The chip fell out of its socket once, so a
// blade under the lid stops it rising. Its place comes from the hole map:
// pins in columns 09 and 12, rows H to N, on a 2.54 mm grid of 18 x 24 holes
// centred on the board. Row A is at the antenna end; column 18 faces the ESP32.
chip_post = board == "perfboard";
chip_dx   = -1.5*2.54; // chip centre from board centre, along the box (toward the antenna end)
chip_dy   =  1.0*2.54; // chip centre from board centre, across the box (toward the ESP32)
chip_l    = 19.3;      // chip body, 14-pin DIP
chip_w    = 6.4;
chip_top  = 7;         // board surface to top of chip, measured 13 Nov 2026
chip_gap  = 1.0;       // air between blade and chip (the legs sit about 3 deep in the socket)
blade_l   = 17;        // blade runs along the chip
blade_t   = 3;
notch_l   = 9;         // gap in the middle of the blade for the GND lead that crosses the chip
notch_h   = 5;

boss_d        = 7;     // corner pillars the lid screws into
screw_pilot_d = 2.8;   // pilot hole for an M3 screw cutting its own thread (tested)
screw_clear_d = 3.4;   // hole in the lid that an M3 screw drops through

// Cable slots in the +X end wall, listed from the perfboard side across.
// Each cable drops in from above; a tongue on the lid closes the slot over it.
slot_labels = ["USB", "1", "2", "3", "4"];
slot_gaps   = [4.5, 2.5, 2.5, 2.5, 2.5];   // height left for each cable under its tongue
slot_w      = 6;
slot_depth  = 10;      // measured down from the top edge of the wall
tongue_fit  = 0.6;     // tongue is this much narrower than the slot
label_size  = 5;
label_depth = 0.6;     // how deep the labels are cut

$fn = 32;              // smoothness of round parts

// ---------------------------------------------------------------- layout
left = boss_d + 1;                                   // boards start past the pillars
span = max(pb_l, esp_ant + esp_l);                   // longest thing along X
in_l = left + span + cable_room;                     // inside length
in_w = gap + pb_w + gap + esp_w + gap;               // inside width
pb_x  = left;            pb_y  = gap;                // shifter board corner
esp_x = left + esp_ant;  esp_y = gap + pb_w + gap;   // ESP32 board corner

chip_cx = pb_x + pb_l/2 + chip_dx;                   // chip centre
chip_cy = pb_y + pb_w/2 + chip_dy;

slot_n = len(slot_labels);
function slot_y(k) = boss_d + 2 + (k + 0.5)*(in_w - 2*(boss_d + 2))/slot_n;

// Centres of the shifter board's four mounting holes.
function hole_xy() = [for (i = [0, 1], j = [0, 1])
    [pb_x + (pb_l - pb_hole_dx)/2 + i*pb_hole_dx,
     pb_y + (pb_w - pb_hole_dy)/2 + j*pb_hole_dy]];

// Centres of the four lid screws.
function screw_xy() = [for (i = [0, 1], j = [0, 1])
    [i ? in_l - boss_d/2 : boss_d/2, j ? in_w - boss_d/2 : boss_d/2]];

echo(str("Inside: ", in_l, " x ", in_w, " x ", inner_h, " mm"));
echo(str("Outside: ", in_l + 2*wall, " x ", in_w + 2*wall, " x ",
         inner_h + floor_t + lid_t, " mm"));

// ---------------------------------------------------------------- shapes
// A box with rounded vertical corners, sitting on Z = 0.
module rbox(l, w, h, r) {
    hull() for (x = [r, l - r], y = [r, w - r])
        translate([x, y, 0]) cylinder(r = r, h = h);
}

// Posts for the shifter board and a corner cradle for the ESP32.
module internals() {
    for (p = hole_xy()) translate([p[0], p[1], 0]) {
        cylinder(d = post_d, h = post_h);
        cylinder(d = pb_hole_d - peg_fit, h = post_h + peg_h);
    }
    difference() {
        for (i = [0, 1], j = [0, 1])
            translate([esp_x + i*esp_l - 2, esp_y + j*esp_w - 2, 0])
                cube([4, 4, esp_post_h + 2]);
        translate([esp_x - esp_fit/2, esp_y - esp_fit/2, esp_post_h])
            cube([esp_l + esp_fit, esp_w + esp_fit, 10]);
    }
}

// One lid pillar in the corner at the origin, without its screw hole.
module boss_solid() {
    translate([boss_d/2, boss_d/2, 0]) cylinder(d = boss_d, h = inner_h);
    translate([-0.5, -0.5, 0]) cube([boss_d/2 + 0.5, boss_d/2 + 0.5, inner_h]);
}

module bosses() {
    difference() {
        for (i = [0, 1], j = [0, 1])
            translate([i ? in_l : 0, j ? in_w : 0, 0])
                mirror([i, 0, 0]) mirror([0, j, 0]) boss_solid();
        for (p = screw_xy())
            translate([p[0], p[1], inner_h - 12])
                cylinder(d = screw_pilot_d, h = 13);
    }
}

module cable_slots() {
    for (k = [0 : slot_n - 1])
        translate([in_l - 1, slot_y(k) - slot_w/2, inner_h - slot_depth])
            cube([wall + 2, slot_w, slot_depth + 1]);
}

// Labels cut into the outside of the end wall, under each slot.
module wall_labels() {
    for (k = [0 : slot_n - 1])
        translate([in_l + wall - label_depth, slot_y(k), inner_h - slot_depth - 6])
            rotate([90, 0, 90]) linear_extrude(label_depth + 1)
                text(slot_labels[k], size = label_size, halign = "center",
                     valign = "center", font = "Liberation Sans:style=Bold");
}

// The same labels cut into the top of the lid, readable from the cable end.
module lid_labels() {
    for (k = [0 : slot_n - 1])
        translate([in_l - 6, slot_y(k), inner_h + lid_t - label_depth])
            rotate([0, 0, 90]) linear_extrude(label_depth + 1)
                text(slot_labels[k], size = label_size, halign = "center",
                     valign = "center", font = "Liberation Sans:style=Bold");
}

module tray() {
    difference() {
        translate([-wall, -wall, -floor_t])
            rbox(in_l + 2*wall, in_w + 2*wall, inner_h + floor_t, corner_r);
        cube([in_l, in_w, inner_h + 1]);
        cable_slots();
        wall_labels();
    }
    bosses();
    internals();
}

// The lid, drawn in place on top of the tray.
module lid() {
    difference() {
        translate([-wall, -wall, inner_h])
            rbox(in_l + 2*wall, in_w + 2*wall, lid_t, corner_r);
        for (p = screw_xy())
            translate([p[0], p[1], inner_h - 1])
                cylinder(d = screw_clear_d, h = lid_t + 2);
        lid_labels();
    }

    bite = 0.5;   // posts and tongues run this far up into the lid plate, so they print as one piece

    // posts that sit over the pegs and hold the shifter board down
    pb_top = post_h + pb_t + pb_clamp_gap;
    for (p = hole_xy()) translate([p[0], p[1], pb_top]) difference() {
        cylinder(d = post_d, h = inner_h - pb_top + bite);
        translate([0, 0, -1]) cylinder(d = pb_hole_d + 0.4, h = peg_h + 1);
    }

    // two posts that hold the ESP32 down along its centre line
    esp_top = esp_post_h + pb_t + esp_clamp_gap;
    for (f = [0.3, 0.7])
        translate([esp_x + f*esp_l, esp_y + esp_w/2, esp_top])
            cylinder(d = post_d, h = inner_h - esp_top + bite);

    // blade that stops the chip rising out of its socket; it touches the two
    // ends of the chip and leaves the middle open for the wire that crosses it
    if (chip_post) {
        cz = post_h + pb_t + chip_top + chip_gap;
        translate([chip_cx - blade_l/2, chip_cy - blade_t/2, cz]) difference() {
            cube([blade_l, blade_t, inner_h - cz + bite]);
            translate([(blade_l - notch_l)/2, -1, -1])
                cube([notch_l, blade_t + 2, notch_h + 1]);
        }
    }

    // tongues that close each cable slot down to the cable
    for (k = [0 : slot_n - 1])
        translate([in_l, slot_y(k) - (slot_w - tongue_fit)/2,
                   inner_h - slot_depth + slot_gaps[k]])
            cube([wall, slot_w - tongue_fit, slot_depth - slot_gaps[k] + bite]);
}

// The lid turned over, top face on the bed, as it is printed.
module lid_printed() {
    translate([0, in_w, inner_h + lid_t]) rotate([180, 0, 0]) lid();
}

// A thin plate carrying only the posts and cradle: a quick print to check
// that the boards sit where the model says before printing the full tray.
module fitplate() {
    translate([left - 4, 0, -1.2]) cube([span + 20, in_w, 1.2]);
    internals();
    // a short lid pillar, to try an M3 screw in the pilot hole
    translate([left + span + 10, 8, 0]) difference() {
        cylinder(d = boss_d, h = 10);
        translate([0, 0, 1]) cylinder(d = screw_pilot_d, h = 10);
    }
}

// Ghost outlines of the two boards. Shown in preview only, never exported.
module ghosts() {
    %translate([pb_x, pb_y, post_h]) cube([pb_l, pb_w, pb_t]);
    if (chip_post)
        %translate([chip_cx - chip_l/2, chip_cy - chip_w/2, post_h + pb_t])
            cube([chip_l, chip_w, chip_top]);
    %translate([esp_x, esp_y, esp_post_h]) cube([esp_l, esp_w, pb_t]);
    %translate([esp_x - esp_ant, esp_y + esp_w/2 - 9, esp_post_h - 1])
        cube([esp_ant + 19, 18, 1]);
}

// ---------------------------------------------------------------- output
if (part == "tray")     { tray(); ghosts(); }
if (part == "lid")      lid_printed();
if (part == "fitplate") { fitplate(); ghosts(); }
if (part == "assembly") { tray(); ghosts(); lid(); }
if (part == "clash")    intersection() { tray(); lid(); }
if (part == "all") {
    tray(); ghosts();
    translate([0, in_w + 2*wall + 8, -floor_t]) lid_printed();
}
