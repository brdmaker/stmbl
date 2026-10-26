// vibsensor enclosure, concept "Shard" (docs/vibsensor.md, hardware/vibsensor).
//
// A milled aluminium base with a solid floor, bolted flat to the machine by
// two swept-back lugs (M4), or screwed onto an M6 stud; the board on two M2
// bosses of the floor, the accelerometer over the stud axis; the cavity
// potted; a faceted cover with a light-pipe slit along its ridge; the M8
// socket in the tail, tilted up 15 degrees so the cable leaves clear of the
// mounting face. The pointed nose is the sensor's +X axis.
//
// Coordinates: x along the length (nose +x), y across, z up; the mounting
// face at z = 0, the board's centre over the origin.
//
//   openscad -D 'part="assembly"' shard.scad      (assembly, exploded, base, cover, pcb)
//   make -C hardware/vibsensor/enclosure          STLs and renders
//
// Concept: dimensions follow the board (gen/board.py) and are to be checked
// against the real parts (M8 socket, potting depth, wall thickness for
// milling) before drawings.

part = "assembly";
explode = 0;          // mm, the cover and board lifted apart

// ---- the board (hardware/vibsensor/gen/board.py)
pcb_l = 32; pcb_w = 18; pcb_nose = 4; pcb_tail = 2.5; pcb_t = 1.6;
hole_x = 9;           // the board's M2 holes at +-9 on the long axis

// ---- the base
gap = 0.6;            // board to wall
wall = 2.4;
floor_t = 5;          // solid floor: the accelerometer's path to the machine
standoff = 2.2;       // floor to board: the bottom-side parts (SOIC 1.75)
cavity_top = floor_t + standoff + pcb_t + 2.6;   // board top + its tallest part + potting cover
nose_ext = 7;         // the pointed nose beyond the cavity
flare = 1.6;          // the walls' outward draft at the foot
lug_t = 5;            // the lugs' thickness
m4 = 4.5; m4_cb = 8.2; m4_cb_d = 2.2;
m6_tap = 5.0; m6_depth = 4.0;    // M6 thread core, blind from below
m8_tilt = 15; m8_d = 8; m8_boss = 13; m8_len = 7;

// ---- the cover
ridge_h = 5.5;        // above the cavity's top
ridge_w = 3.2;        // the ridge's width
slit_w = 1.2; slit_l = 14;

$fn = 48;

// the board's outline (mirrors board.py's polygon)
function pcb_pts() = [[-pcb_l/2 + pcb_tail, -pcb_w/2], [pcb_l/2 - pcb_nose, -pcb_w/2], [pcb_l/2, -pcb_w/2 + pcb_nose],
                      [pcb_l/2, pcb_w/2 - pcb_nose], [pcb_l/2 - pcb_nose, pcb_w/2], [-pcb_l/2 + pcb_tail, pcb_w/2],
                      [-pcb_l/2, pcb_w/2 - pcb_tail], [-pcb_l/2, -pcb_w/2 + pcb_tail]];

module pcb_outline() { polygon(pcb_pts()); }
module pocket() { offset(delta = gap) pcb_outline(); }

// the body's footprint: the pocket grown by the wall, the nose drawn out to a point
module footprint() {
    hull() {
        offset(delta = gap + wall, chamfer = true) pcb_outline();
        translate([pcb_l/2 + gap + wall + nose_ext, 0]) circle(d = 2.4, $fn = 6);
    }
}

// swept-back lugs at the tail's sides, with the M4 holes
lug_pts = [[-6, 0], [-21, 0], [-25, 11], [-14, 11]];
function lug_hole(s) = [-18.5, s * (pcb_w/2 + gap + wall + 4.6)];
module lugs2d() {
    for (s = [-1, 1]) hull() {
        translate([-6, s * (pcb_w/2 + gap)]) square([0.1, 0.1], center = true);
        translate([-22, s * (pcb_w/2 + gap)]) square([0.1, 0.1], center = true);
        translate(lug_hole(s) + [-1.5, 0]) circle(d = 9.5, $fn = 6);
        translate([-9, s * (pcb_w/2 + gap + wall + 1.5)]) circle(d = 2, $fn = 6);
    }
}

module m8_axis() {
    translate([-pcb_l/2 - gap - wall + 1.5, 0, floor_t + standoff + pcb_t + 1.2]) rotate([0, -90 - m8_tilt, 0]) children();
}

module base() {
    difference() {
        union() {
            // body: the walls flare out toward the mounting face (draft), a chamfered foot
            hull() {
                translate([0, 0, cavity_top - 0.01]) linear_extrude(0.01) footprint();
                translate([0, 0, 2.2]) linear_extrude(0.01) offset(delta = flare) footprint();
                linear_extrude(1.2) offset(delta = flare - 1.0) footprint();
            }
            // lugs: chamfered top edge
            hull() {
                linear_extrude(lug_t - 1) lugs2d();
                linear_extrude(lug_t) offset(delta = -1) lugs2d();
            }
            // the M8 socket's boss
            m8_axis() cylinder(d = m8_boss, h = m8_len, $fn = 6);
        }
        // the cavity, potted
        translate([0, 0, floor_t]) linear_extrude(cavity_top) pocket();
        // M4 counterbored through holes
        for (s = [-1, 1]) translate(concat(lug_hole(s), 0)) {
            translate([0, 0, -1]) cylinder(d = m4, h = lug_t + 2);
            translate([0, 0, lug_t - m4_cb_d]) cylinder(d = m4_cb, h = m4_cb_d + 1);
        }
        // M6 stud thread, blind from below, under the accelerometer
        translate([0, 0, -1]) cylinder(d = m6_tap, h = m6_depth + 1);
        // M8 socket bore into the cavity
        m8_axis() translate([0, 0, -6]) cylinder(d = m8_d, h = m8_len + 10);
        // the parting line: a fine groove under the cover
        translate([0, 0, cavity_top - 1.2]) linear_extrude(0.4) difference() { offset(delta = 1) footprint(); offset(delta = -0.35) footprint(); }
    }
    // the board's bosses (M2)
    for (s = [-1, 1]) translate([s * hole_x, 0, floor_t - 0.01]) difference() {
        cylinder(d = 4.6, h = standoff, $fn = 6);
        cylinder(d = 1.6, h = standoff + 1);
    }
}

module cover() {
    difference() {
        hull() {
            translate([0, 0, cavity_top]) linear_extrude(0.01) footprint();
            translate([0, 0, cavity_top + 1.2]) linear_extrude(0.01) offset(delta = -0.6) footprint();
            translate([0, 0, cavity_top + ridge_h]) linear_extrude(0.01)
                scale([0.82, ridge_w / (pcb_w + 2 * (gap + wall))]) footprint();
        }
        // the light pipe's slit along the ridge (an LED under it, a clear insert in it)
        translate([2, 0, cavity_top + ridge_h - 2]) linear_extrude(3) offset(r = slit_w / 2) square([slit_l, 0.01], center = true);
        // the +X arrow, engraved in the ridge ahead of the slit
        translate([13.2, 0, cavity_top + ridge_h - 0.5]) linear_extrude(1) polygon([[0, -1.3], [3.2, 0], [0, 1.3], [0.9, 0]]);
    }
}

module light_pipe() {
    translate([2, 0, cavity_top + ridge_h - 1.6]) linear_extrude(1.5) offset(r = slit_w / 2 - 0.05) square([slit_l, 0.01], center = true);
}

module pcb() {
    translate([0, 0, floor_t + standoff]) {
        color([0.05, 0.35, 0.2]) linear_extrude(pcb_t) difference() {
            pcb_outline();
            for (s = [-1, 1]) translate([s * hole_x, 0]) circle(d = 2.2);
        }
        // the main parts, as blocks: MCU, transceiver, buck inductor; the accelerometer below
        color([0.12, 0.12, 0.13]) {
            translate([-3.5, -3.5, pcb_t]) cube([7, 7, 0.9]);
            translate([-12, 3.5, pcb_t]) cube([3, 3, 1.5]);
            translate([-1.25, -1.5, -0.83]) cube([2.5, 3, 0.83]);
            translate([-13, -6.5, -1.75]) cube([5, 4, 1.75]);
        }
    }
}

module m8_socket() {
    color([0.75, 0.76, 0.78]) m8_axis() {
        translate([0, 0, m8_len]) cylinder(d = m8_d, h = 6);
        translate([0, 0, m8_len]) cylinder(d = 10.5, h = 1.2, $fn = 6);
    }
}

module assembly(e = 0) {
    color([0.17, 0.18, 0.2]) base();
    translate([0, 0, e]) pcb();
    translate([0, 0, 2.2 * e]) {
        color([0.88, 0.9, 0.92]) cover();
        color([1.0, 0.45, 0.1, 0.9]) light_pipe();
    }
    m8_socket();
}

if (part == "assembly") assembly(0);
else if (part == "exploded") assembly(explode > 0 ? explode : 9);
else if (part == "base") base();
else if (part == "cover") cover();
else if (part == "pcb") pcb();
