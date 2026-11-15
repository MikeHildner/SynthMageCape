// Proxy concepts at true scale, for comparison only.
$fn = 28;
wall = 2; H = 30; gap = 3; cable = 15; boss = 7;
PL = 69.8; PW = 50.06; EL = 57.65; EW = 28.24; ANT = 6.5;

module perf() {                       // capacitor at the +X end
    color("green") cube([PL, PW, 1.6]);
    color("dimgray") translate([59, 44, 1.6]) cylinder(d = 10, h = 20);
    color("black") translate([23, 23, 1.6]) cube([17, 9, 8]);
    color("silver") for (y = [3.4, 46.6]) translate([13, y - 0.4, -0.8]) cube([46, 0.8, 0.8]);
}
module esp() {                        // antenna at the -X end, plugs up
    color([0.12, 0.12, 0.12]) cube([EL, EW, 1.6]);
    color([0.12, 0.12, 0.12]) translate([-ANT, 5.1, -1]) cube([ANT + 12, 18, 1]);
    color("gold") for (y = [0.2, EW - 2.7]) translate([1, y, 1.6]) cube([55.6, 2.5, 2.5]);
    color([0.2, 0.2, 0.2]) translate([8, 0.2, 4.1]) cube([10.2, 2.5, 14]);
    color([0.2, 0.2, 0.2]) translate([49, 0.2, 4.1]) cube([5.1, 2.5, 14]);
}
module rbox(l, w, h, r = 3) {
    hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}
module slots(L, W, n = 5) {
    y0 = boss + 2; span = W - 2*y0;
    for (k = [0 : n - 1])
        translate([L - 1, y0 + (k + 0.5)*span/n - 3, H - 10]) cube([wall + 2, 6, 30]);
}
module bosses(L, W) {
    for (x = [boss/2, L - boss/2], y = [boss/2, W - boss/2])
        translate([x, y, 0]) difference() { cylinder(d = boss, h = H); translate([0, 0, H - 10]) cylinder(d = 2.8, h = 11); }
}
module screw_box(L, W) {
    color("SteelBlue") {
        difference() {
            translate([-wall, -wall, -wall]) rbox(L + 2*wall, W + 2*wall, H + wall);
            cube([L, W, H + 1]);
            slots(L, W);
        }
        bosses(L, W);
    }
}
module screw_lid(L, W) {
    color("LightSteelBlue") translate([0, W + 12, -wall]) difference() {
        translate([-wall, -wall, 0]) rbox(L + 2*wall, W + 2*wall, wall);
        for (x = [boss/2, L - boss/2], y = [boss/2, W - boss/2]) translate([x, y, -1]) cylinder(d = 3.4, h = 5);
    }
}
module label(a, b, c) color("black") {
    translate([0, -12, -2]) linear_extrude(0.4) text(a, size = 7, font = "Liberation Sans:style=Bold");
    translate([0, -22, -2]) linear_extrude(0.4) text(b, size = 5.5);
    translate([0, -31, -2]) linear_extrude(0.4) text(c, size = 5.5);
}

// 1: side by side, screw lid
module c1() {
    left = boss + 1; L = left + PL + cable; W = gap + PW + gap + EW + gap;
    screw_box(L, W); screw_lid(L, W);
    translate([left, gap, 4]) perf();
    translate([left + ANT, gap + PW + gap, 4]) esp();
    label("1  Square, screw lid", "97 x 91 x 34 mm", "4 x M3 screws");
}
// 2: end to end, screw lid
module c2() {
    left = boss + 1; L = left + ANT + EL + gap + PL + cable; W = gap + PW + gap;
    screw_box(L, W); screw_lid(L, W);
    translate([left + ANT, gap + (PW - EW)/2, 4]) esp();
    translate([left + ANT + EL + gap, gap, 4]) perf();
    label("2  Long, screw lid", "164 x 60 x 34 mm", "4 x M3 screws");
}
// 3: side by side, slide lid
module c3() {
    L = gap + PL + cable; W = gap + PW + gap + EW + gap; t = 2.4;
    color("SteelBlue") difference() {
        translate([-t, -t, -wall]) rbox(L + 2*t, W + 2*t, H + wall + 4);
        cube([L, W, H + 10]);
        translate([0, -1.2, H]) cube([L + t + 10, W + 2.4, 2.2]);
        translate([L - 1, -1, H]) cube([t + 5, W + 2, 10]);
        slots(L, W);
    }
    color("LightSteelBlue") translate([48, -1, H + 0.2]) cube([L + t, W + 2, 1.8]);
    translate([gap, gap, 4]) perf();
    translate([gap + ANT, gap + PW + gap, 4]) esp();
    label("3  Square, slide lid", "93 x 92 x 36 mm", "no screws");
}

c1();
translate([150, 0, 0]) c3();
translate([20, 250, 0]) c2();
