// stmbl test stand: a 3-axis machine that is printed whole, but for its three NEMA 23
// servos and three micro switches. It ships with a controller kit to show the kit working:
// it homes on its switches and draws on paper with a pen.
//
// The CN 720's layout (millweb type "bridge"): a table moving in Y on a solid base, a fixed
// bridge, an X carriage on its front, a Z slide on the carriage's front. Each axis is a
// printed dovetail slide with a gib, driven by a printed rack and pinion (module 1.5,
// 20 teeth: 94.25 mm a motor turn). The fasteners are printed too: P8x2 bolts, snap pins.
//
//   openscad -o out.stl -D 'part="base"' teststand.scad    (make: every part, the pictures)
//   part = "assembly", X, Y in mm (X ±55, Y ±65), Z 0 (up) .. -40 (the pen on the paper). Y is the pen
//   over the paper: the table moves the other way (-Y). The switches: X at -55, Y at +65, Z at 0
//
// Sized for a Bambu Lab H2C (330 x 320 x 320, 300 x 320 with both nozzles): every part within
// 295 x 315 x 310 mm, printed as print_part() turns it; xt, yt below shrink it for smaller beds. PETG or PLA,
// 0.2 mm layers, 4 walls, 25-40 % infill (the base: 15 %, it is solid on purpose: weight).

part = "assembly";
X = 0; Y = 0; Z = 0;
show_motors = true;
lite = false;               // threads as plain cylinders (fast; the collision check)

/* [the motors] */
nema_flange = 57;           // NEMA 23
nema_holes = 47.14;         // the holes' square
nema_hole_d = 5.2;
nema_pilot = 38.1;
nema_pilot_h = 1.6;
nema_flange_t = 7;          // the flange's thickness at its holes (the snap pins' grip)
motor_len = 110;            // the body (only drawn)
shaft_d = 8;                // 8 mm, or 6.35
shaft_flat = 0.5;           // the D flat's depth
shaft_len = 21;             // beyond the flange

/* [the gearing] */
m = 1.5; zp = 20; pa = 20;
backlash = 0.15;            // on each flank
gw = 10;                    // gear and rack width
hub_l = 6; hub_d = 22;
plate_t = 5;                // the motor plates
pr = m * zp / 2;            // 15
lead = PI * m * zp;         // 94.25 mm a motor turn
gear_c = 0.5 + hub_l + gw / 2;   // the gear's middle from the plate's face: 11.5

/* [fits] */
c = 0.25;                   // a sliding fit
gib = 3;                    // the gib strips
dt_h = 8;                   // the dovetails' height
fit = 0.3;                  // a printed bolt in its hole

/* [the switch: 20 x 10 x 6.4 micro switch with a lever (SS-5GL, KW12, V-156 ...)] */
sw_l = 20.4; sw_h = 10.6; sw_t = 6.7;

$fn = 48;
assert(0.5 + hub_l + 0.8 * gw <= shaft_len - plate_t, "the shaft must reach through the hub and most of the gear");

// ---------------------------------------------------------------- the layout (world mm; the pen at X Y 0)
// From the travels. Each rack rides on the moving part and its pinion stands still, so the table is
// longer than the Y travel and the carriage longer than the X travel (8 mm of rack past the pinion at
// each end); the bridge's columns stand outside the carriage. For the Bambu Lab H2C (300 x 320 mm,
// both nozzles) X ±55 and Y ±65 are the most: the base 280 x 300, the beam 280.
xt = 55; yt = 65;                                                             // X ±xt, Y ±yt
tab_l = 2 * (yt + 10);                                                        // the table's length (y): 150
car = [[-52, 76], [11, 38.5], [90, 190]];                                     // the X carriage at X 0: 128 long
x_rack_y = 28.5; x_pin_x = 12;                                                // the pinion: x_pin_x ± (car length / 2 - xt - 8) (here exactly)
assert(car[0][0] + xt + 8 <= x_pin_x && x_pin_x <= car[0][1] - xt - 8, "the X rack is too short for the travel");
cols = [[car[0][0] - xt - 21, car[0][0] - xt - 1], [car[0][1] + xt + 1, car[0][1] + xt + 21]]; col_y = [40, 65]; col_top = 90;  // the bridge's columns, part of the base
base_x = [cols[0][0], cols[1][1]]; base_y = [-tab_l / 2 - yt - 15, tab_l / 2 + yt + 5]; base_top = 60;
col_bolts = [[(cols[0][0] + cols[0][1]) / 2, 49], [(cols[1][0] + cols[1][1]) / 2, 49]];   // the beam's bolts into them
col_pins = [[col_bolts[0][0], 59], [col_bolts[1][0], 59]];                     // and pins on them that locate it
tab = [[-xt - 15, xt + 15], [-tab_l / 2, tab_l / 2], [61, 85]];              // the table at Y 0
y_rail_w = 24;
y_rack_x = tab[0][1] - 10; y_pin_z = tab[2][0] - 1.25 * m - pr;                           // 44.1
y_mo = [y_rack_x + gear_c, 0, y_pin_z];                                        // the Y motor plate's face (shaft -x)
beam = [[base_x[0], base_x[1]], [40, 65], [90, 150]];
x_rail_w = 36; x_rail_z = 127;
x_pin_z = car[2][1] + 1.25 * m + pr;          // 206.9
x_mo = [x_pin_x, x_rack_y + gear_c, x_pin_z];                                  // the X motor plate's face (shaft -y)
z_rail_w = 24;
sl = [[-16, 16], [-10, 10], [0, 130]];                                        // the Z slide, z from its bottom
sl_z0 = 140;                                                                  // its bottom at Z 0
z_tab_top = 33;                                                               // the tab that trips the Z switch, its top on the slide
pen_out = 15;                                                                 // the pen's tip below it: on the paper at Z -40
z_rack_y = -5; z_pin_x = sl[0][1] + 1.25 * m + pr; z_pin_z = 150;            // 32.9
z_mo = [z_pin_x, z_rack_y - gear_c, z_pin_z];                                  // the Z motor plate's face (shaft +y)
zbr_bolt_x = (z_pin_x + pr + m + 2 + car[0][1]) / 2;                           // the Z motor bracket's arm: its bolts

// ---------------------------------------------------------------- helpers
// place a 2D profile: its x along u, its y along v, extruded along w from o
module frame(o, u, v, w) multmatrix([[u[0], v[0], w[0], o[0]], [u[1], v[1], w[1], o[1]], [u[2], v[2], w[2], o[2]], [0, 0, 0, 1]]) children();
module prism(o, u, v, w, len) frame(o, u, v, w) linear_extrude(len, convexity = 4) children();

// a dovetail: root w on y 0, the flanks leaning out to y h (60°); neck: down to y -neck
module dovetail2d(w, h, neck = 0) {
  o = h / tan(60);
  polygon([[-w / 2, -neck], [w / 2, -neck], [w / 2, 0], [w / 2 + o, h], [-w / 2 - o, h], [-w / 2, 0]]);
}
// its groove: clearance, and the gib's room on the -x flank
module groove2d(w, h) {
  o = h / tan(60); e = (h + c + 1) / tan(60);
  polygon([[-w / 2 - c - gib, -1], [w / 2 + c, -1], [w / 2 + c + e, h + c], [-w / 2 - c - gib - e, h + c]]);
}
module gib2d(w, h) {
  o = h / tan(60);
  polygon([[-w / 2 - c - gib + 0.2, 0], [-w / 2 - c, 0], [-w / 2 - c - o, h], [-w / 2 - c - gib + 0.2 - o, h]]);
}

// an involute spur gear
function inv(rb, t) = [rb * (cos(t) + t * PI / 180 * sin(t)), rb * (sin(t) - t * PI / 180 * cos(t))];
function rot2(p, a) = [p[0] * cos(a) - p[1] * sin(a), p[0] * sin(a) + p[1] * cos(a)];
module gear2d(m, z, pa = 20, bl = 0) {
  r = m * z / 2; rb = r * cos(pa); ra = r + m; rf = r - 1.25 * m;
  tmax = sqrt(ra * ra / (rb * rb) - 1) * 180 / PI;
  tp = sqrt(r * r / (rb * rb) - 1) * 180 / PI;
  ap = atan2(inv(rb, tp)[1], inv(rb, tp)[0]);
  half = 90 / z - bl / r * 180 / PI + ap;
  n = 8;
  fl = [for (i = [0:n]) inv(rb, tmax * i / n)];
  union() {
    circle(r = rf, $fn = z * 4);
    for (k = [0:z - 1]) rotate(k * 360 / z)
      polygon(concat([[0, 0]], [for (p = fl) rot2(p, -half)], [for (i = [n:-1:0]) rot2([fl[i][0], -fl[i][1]], half)]));
  }
}
// a rack along +x from 0 to len, its teeth toward +z, their root on z 0, gw wide about y 0
module rack3d(len) {
  p = PI * m; t = tan(pa); w = p / 2 - 2 * backlash;
  intersection() {
    translate([0, gw / 2, 1.25 * m]) rotate([90, 0, 0]) linear_extrude(gw)
      for (k = [-1:ceil(len / p) + 1]) translate([k * p, 0])
        polygon([[-w / 2 - 1.25 * m * t, -1.25 * m - 0.01], [w / 2 + 1.25 * m * t, -1.25 * m - 0.01], [w / 2 - m * t, m], [-w / 2 + m * t, m]]);
    translate([0, -gw / 2 - 1, -1]) cube([len, gw + 2, 10]);
  }
}

// printed threads, P8x2: a circle off centre, twisted (round, single start)
module thread(l, d = 8, p = 2, grow = 0) {
  e = 0.3 * p;
  if (lite) cylinder(d = d + 2 * grow, h = l, $fn = 16); else
  linear_extrude(height = l, twist = -360 * l / p, slices = max(2, ceil(l / p * 12)), convexity = 4)
    translate([e / 2, 0]) circle(d = d - e + 2 * grow, $fn = 24);
}
module thread_hole(l) translate([0, 0, -0.01]) thread(l + 0.02, grow = fit / 2 + 0.1);
// the gib screws: P6x1.5 (the dovetails leave no room for 8 mm), a hex head 10
module gib_hole(l) translate([0, 0, -0.01]) thread(l + 0.02, d = 6, p = 1.5, grow = fit / 2 + 0.1);
module gib_screw(l) union() { cylinder(d = 10, h = 4, $fn = 6); translate([0, 0, 3.99]) thread(l, d = 6, p = 1.5); }
module bolt(l) union() { cylinder(d = 13, h = 5, $fn = 6); translate([0, 0, 4.99]) thread(l); }
module clear_hole(l) translate([0, 0, -0.01]) cylinder(d = 8 + 2 * fit + 0.6, h = l + 0.02);

// ---------------------------------------------------------------- motors, plates, pinions
// motor frame: the origin on the plate's face toward the pinion, +z along the shaft
module at_motor(o, ax) translate(o) rotate(ax == [-1, 0, 0] ? [0, -90, 0] : ax == [0, -1, 0] ? [90, 0, 0] : ax == [0, 1, 0] ? [-90, 0, 0] : [0, 0, 0]) children();
module nema_body() {  // the flange face on z 0, the body toward -z, the shaft +z
  color("dimgray") translate([-nema_flange / 2, -nema_flange / 2, -motor_len]) cube([nema_flange, nema_flange, motor_len]);
  color("silver") { cylinder(d = nema_pilot, h = nema_pilot_h); cylinder(d = shaft_d, h = shaft_len); }
}
// the plate's cuts (in the motor frame: the plate from z -plate_t to 0)
module nema_cuts() {
  translate([0, 0, -plate_t - 1]) cylinder(d = nema_pilot + 0.6, h = nema_pilot_h + 1.3);
  cylinder(d = 24, h = 3 * plate_t, center = true);
  for (sx = [-1, 1], sy = [-1, 1]) translate([sx * nema_holes / 2, sy * nema_holes / 2, 0]) cylinder(d = nema_hole_d + 0.2, h = 3 * plate_t, center = true);
}
// the pinion: its hub on z 0, the gear beyond; a D bore, a set screw onto the flat
module pinion() difference() {
  union() { cylinder(d = hub_d, h = hub_l); translate([0, 0, hub_l]) linear_extrude(gw) gear2d(m, zp, pa, backlash); }
  translate([0, 0, -1]) linear_extrude(gw + hub_l + 2) difference() { circle(d = shaft_d + 0.15); translate([shaft_d / 2 - shaft_flat, -shaft_d]) square(2 * shaft_d); }
  translate([0, 0, hub_l / 2]) rotate([0, 90, 0]) thread_hole(hub_d);
}
// a snap pin through the flange and the plate; its split barbs catch behind the plate
module snap_pin() {
  g = nema_flange_t + plate_t;
  difference() {
    union() {
      cylinder(d = 9, h = 2.5);
      cylinder(d = nema_hole_d - 0.3, h = 2.5 + g + 3);
      translate([0, 0, 2.5 + g + 0.2]) cylinder(d1 = nema_hole_d + 0.9, d2 = nema_hole_d - 0.6, h = 2.8);
    }
    translate([-0.8, -5, 2.5 + g - 4]) cube([1.6, 10, 20]);
  }
}

// ---------------------------------------------------------------- the switch holder
// lever face on z 0, the body down to -14, back face on y 12 (it bolts through x 15.5, z -7)
module switch_holder() difference() {
  translate([-21, 0, -14]) cube([42, 12, 14]);
  translate([-sw_l / 2, 6 - sw_t / 2, -sw_h]) cube([sw_l, sw_t, sw_h + 1]);
  translate([-sw_l / 2 - 5, 6 - 2.5, -sw_h - 2]) cube([6, 5, sw_h + 3]);        // the wires
  translate([15.5, -1, -7]) rotate([-90, 0, 0]) clear_hole(14);
  translate([-20, 6 - sw_t / 2 - 1.2, -sw_h / 2 - 1]) cube([2, 1.2, 2]);       // a crush rib's relief
}
// where each sits: the matrix from its frame to the world
module y_switch_at() frame([-40, base_y[0] + 14, base_top + 12], [1, 0, 0], [0, 0, -1], [0, 1, 0]) children();     // the lever toward +y
module x_switch_at() frame([car[0][0] - xt - 1, 44, beam[2][1] + 12], [0, 1, 0], [0, 0, -1], [1, 0, 0]) children();               // the lever toward +x
module z_switch_at() frame([-38, -1, sl_z0 + z_tab_top + 0.5], [1, 0, 0], [0, 1, 0], [0, 0, -1]) children();                          // the lever down

// ---------------------------------------------------------------- BASE: solid (weight), the Y rail, the columns, the Y motor's plate
module base() {
  difference() {
    union() {
      translate([base_x[0], base_y[0], 0]) cube([base_x[1] - base_x[0], base_y[1] - base_y[0], base_top]);
      prism([0, base_y[0], base_top], [1, 0, 0], [0, 0, 1], [0, 1, 0], base_y[1] - base_y[0]) dovetail2d(y_rail_w, dt_h);
      for (cx = cols) translate([cx[0], col_y[0], 0]) cube([cx[1] - cx[0], col_y[1] - col_y[0], col_top]);
      for (pn = col_pins) translate([pn[0], pn[1], col_top - 0.01]) cylinder(d1 = 10, d2 = 9, h = 5);
      at_motor(y_mo, [-1, 0, 0]) translate([-(y_pin_z - 2), -34, -plate_t]) cube([y_pin_z - 2 + 32, 68, plate_t]);   // up to 76, down into the base
    }
    // the rack's way, the pinion's pocket, the room behind the plate for the pins' barbs (all open at the top)
    translate([y_rack_x - gw / 2 - 1, -tab_l / 2 - yt - 2, base_top - 4.5]) cube([gw + 2, tab_l + 2 * yt + 4, 10]);
    translate([y_rack_x - gw / 2 - 1.5, -18, y_pin_z - pr - 3]) cube([gw + 3, 36, 60]);
    translate([y_mo[0] - 11, -32, y_pin_z - 32]) cube([11, 64, 80]);
    // the motor's trench: through the right side, open at the top
    translate([y_mo[0] + plate_t, -nema_flange / 2 - 1, y_pin_z - nema_flange / 2 - 1]) cube([200, nema_flange + 2, 100]);
    at_motor(y_mo, [-1, 0, 0]) nema_cuts();
    // the beam's bolts; the switch holder's
    for (b = col_bolts) translate([b[0], b[1], col_top - 25]) thread_hole(26);
    y_switch_at() translate([15.5, 12, -7]) rotate([-90, 0, 0]) thread_hole(20);
    // a hand hold / cable way under it, both sides
    for (sx = [-1, 1]) translate([sx > 0 ? base_x[1] - 30 : base_x[0] - 1, -60, -1]) cube([31, 120, 12]);
  }
}

// TABLE: the groove underneath with its gib, the Y rack on its underside, slots for the paper clips
module table() difference() {
  union() {
    translate([tab[0][0], tab[1][0], tab[2][0]]) cube([tab[0][1] - tab[0][0], tab[1][1] - tab[1][0], tab[2][1] - tab[2][0]]);
    frame([y_rack_x, tab[1][0], tab[2][0]], [0, 1, 0], [1, 0, 0], [0, 0, -1]) rack3d(tab[1][1] - tab[1][0]);
  }
  prism([0, tab[1][0] - 1, base_top], [1, 0, 0], [0, 0, 1], [0, 1, 0], tab[1][1] - tab[1][0] + 2) groove2d(y_rail_w, dt_h);
  for (yy = [-tab_l / 2 + 20, 0, tab_l / 2 - 20]) translate([tab[0][0] - 1, yy, base_top + dt_h / 2 + 0.5 + 0.5]) rotate([0, 90, 0]) {
    gib_hole(-tab[0][0] - 17.4 + 1);   // the whole way: a long gib screw
  }
  for (sx = [-1, 1], sy = [-1, 1]) translate([sx * (tab[0][1] - 10) - 4, sy * (tab[1][1] - 8) - 2.2, tab[2][1] - 6]) cube([8, 4.4, 7]);
}
module table_gib() prism([0, tab[1][0], base_top], [1, 0, 0], [0, 0, 1], [0, 1, 0], tab[1][1] - tab[1][0]) gib2d(y_rail_w, dt_h);

// BEAM: bolted on the columns; the X rail on its front; the X motor's bracket and the X switch on top
module beam() difference() {
  union() {
    translate([beam[0][0], beam[1][0], beam[2][0]]) cube([beam[0][1] - beam[0][0], beam[1][1] - beam[1][0], beam[2][1] - beam[2][0]]);
    prism([beam[0][0], beam[1][0], x_rail_z], [0, 0, 1], [0, -1, 0], [1, 0, 0], beam[0][1] - beam[0][0]) dovetail2d(x_rail_w, dt_h);
  }
  for (b = col_bolts) translate([b[0], b[1], beam[2][0]]) { clear_hole(60); translate([0, 0, 26]) cylinder(d = 16, h = 40); }
  for (pn = col_pins) translate([pn[0], pn[1], beam[2][0] - 0.01]) cylinder(d = 10.6, h = 6);
  for (xx = [x_pin_x - 18, x_pin_x + 18]) translate([xx, 57, beam[2][1] - 20]) thread_hole(21);
  x_switch_at() translate([15.5, 12, -7]) rotate([-90, 0, 0]) thread_hole(20);
}
// the X motor's bracket: a foot on the beam, the plate rising in front of the motor
module x_motor_bracket() difference() {
  union() {
    translate([x_pin_x - 32, beam[1][0], beam[2][1]]) cube([64, beam[1][1] - beam[1][0], 8]);
    translate([x_pin_x - 32, x_mo[1], beam[2][1]]) cube([64, plate_t, x_pin_z + 32 - beam[2][1]]);
    for (sx = [-1, 1]) translate([x_pin_x + sx * 32 - (sx > 0 ? 5 : 0), x_mo[1], beam[2][1]]) cube([5, beam[1][1] - x_mo[1], 24]);
  }
  for (xx = [x_pin_x - 18, x_pin_x + 18]) translate([xx, 57, beam[2][1] - 1]) { clear_hole(10); translate([0, 0, 9]) cylinder(d = 16, h = 30); }
  at_motor(x_mo, [0, -1, 0]) nema_cuts();
  translate([x_pin_x - 26.9, x_mo[1] + plate_t, beam[2][1] + 8.01]) cube([53.8, 30, 120]);   // behind the plate: the motor
}

// X CARRIAGE: the X groove at its back (the gib on the lower flank, screws from below), the X rack on
// top, the Z groove at its front (the gib on the -x flank, screws from the left)
module x_carriage() difference() {
  union() {
    translate([car[0][0], car[1][0], car[2][0]]) cube([car[0][1] - car[0][0], car[1][1] - car[1][0], car[2][1] - car[2][0]]);
    translate([car[0][0], x_rack_y, car[2][1]]) rack3d(car[0][1] - car[0][0]);
  }
  prism([car[0][0] - 1, beam[1][0], x_rail_z], [0, 0, 1], [0, -1, 0], [1, 0, 0], car[0][1] - car[0][0] + 2) groove2d(x_rail_w, dt_h);
  for (xx = [car[0][0] + 18, x_pin_x, car[0][1] - 32]) translate([xx, beam[1][0] - dt_h / 2 - 1.5, car[2][0]]) {   // their heads sunk in the underside
    translate([0, 0, -0.01]) cylinder(d = 11.5, h = 5); translate([0, 0, 5]) gib_hole(x_rail_z - x_rail_w / 2 - c - gib - car[2][0] - 2.3 + 1 - 5); }
  prism([0, car[1][0], car[2][0] - 1], [1, 0, 0], [0, 1, 0], [0, 0, 1], car[2][1] - car[2][0] + 2) groove2d(z_rail_w, dt_h);
  for (zz = [100, 135, 165]) translate([car[0][0], car[1][0] + dt_h / 2, zz]) rotate([0, 90, 0]) gib_hole(-car[0][0] - z_rail_w / 2 - c - gib - 2.3 + 1);
  for (zz = [113, 185]) translate([zbr_bolt_x, car[1][0] - 0.01, zz]) rotate([-90, 0, 0]) thread_hole(16);   // the Z motor's bracket
  z_switch_at() translate([15.5, 12, -7]) rotate([-90, 0, 0]) thread_hole(16);
}
module x_gib() prism([car[0][0], beam[1][0], x_rail_z], [0, 0, 1], [0, -1, 0], [1, 0, 0], car[0][1] - car[0][0]) gib2d(x_rail_w, dt_h);
module z_gib() prism([0, car[1][0], car[2][0]], [1, 0, 0], [0, 1, 0], [0, 0, 1], car[2][1] - car[2][0]) gib2d(z_rail_w, dt_h);
// the Z motor's bracket: its plate in front, an arm back to the carriage beside the pinion
module z_motor_bracket() difference() {
  ax = z_pin_x + pr + m + 2;   // the arm clear of the pinion
  union() {
    translate([z_pin_x - 30, z_mo[1] - plate_t, 104]) cube([car[0][1] - z_pin_x + 30, plate_t, 92]);
    translate([ax, z_mo[1] - 0.01, 104]) cube([car[0][1] - ax, car[1][0] - z_mo[1], 92]);
  }
  at_motor(z_mo, [0, 1, 0]) { nema_cuts(); for (sz = [-1, 1]) translate([nema_holes / 2, sz * nema_holes / 2, -0.01]) cylinder(d = 9, h = 7); }
  for (zz = [113, 185]) translate([zbr_bolt_x, z_mo[1] - plate_t - 1, zz]) rotate([-90, 0, 0]) { clear_hole(40); cylinder(d = 16, h = 1 + 4); }
}

// Z SLIDE: the rail on its back, the Z rack on its +x side, the pen's bore and thumb screw, the switch's tab
module z_slide() difference() {
  union() {
    translate([sl[0][0], sl[1][0], sl[2][0]]) cube([sl[0][1] - sl[0][0], sl[1][1] - sl[1][0], sl[2][1] - sl[2][0]]);
    prism([0, car[1][0], 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], sl[2][1]) dovetail2d(z_rail_w, dt_h, car[1][0] - sl[1][1] + 0.01);
    frame([sl[0][1], z_rack_y, 0], [0, 0, 1], [0, 1, 0], [1, 0, 0]) rack3d(sl[2][1]);
    // the tab that trips the switch at Z 0: under the lever, its underside chamfered (no support)
    hull() { translate([-34, 1, z_tab_top - 3]) cube([18, 7, 3]); translate([-16.01, 1, z_tab_top - 21]) cube([0.01, 7, 21]); }
  }
  translate([0, 0, -1]) cylinder(d = 12.4, h = sl[2][1] + 2);
  translate([sl[0][0] - 0.01, 0, 10]) rotate([0, 90, 0]) thread_hole(11);
}

// ---------------------------------------------------------------- small parts
module thumb_screw() union() { cylinder(d = 18, h = 6, $fn = 12); translate([0, 0, 5.99]) thread(14); }
// a grub screw with a slot, flush in the pinion's hub
module set_screw(l = 8) difference() { thread(l); translate([-5, -0.9, l - 2.5]) cube([10, 1.8, 3]); }
// a paper clip: its plug in a slot of the table, its arm over the paper's corner
module paper_clip() {
  translate([0, 0, -6.5]) cube([7.4, 4, 6.5]);
  translate([-24, 0, 0]) cube([31.4, 4, 1.6]);
  translate([-24, 0, -0.6]) cube([3, 4, 0.7]);
}

// ---------------------------------------------------------------- the machine
module motor_and_pinion(angle) {
  translate([0, 0, 0.5]) color("orange") rotate([0, 0, angle]) pinion();
  if (show_motors) translate([0, 0, -plate_t]) nema_body();
}
module assembly() {
  color("lightsteelblue") base();
  color("steelblue") beam();
  color("cadetblue") x_motor_bracket();
  translate([0, -Y, 0]) { color("khaki") table(); color("peru") table_gib(); color("white") translate([tab[0][0] + 5, tab[1][0] + 5, tab[2][1]]) cube([tab[0][1] - tab[0][0] - 10, tab[1][1] - tab[1][0] - 10, 0.2]); }
  translate([X, 0, 0]) {
    color("lightgreen") x_carriage(); color("peru") { x_gib(); z_gib(); }
    color("mediumseagreen") z_motor_bracket();
    translate([0, 0, sl_z0 + Z]) color("plum") z_slide();
    translate([0, 0, sl_z0 + Z - pen_out]) color("royalblue") { cylinder(d1 = 1, d2 = 11, h = 12); translate([0, 0, 12]) cylinder(d = 11, h = 130); }
    at_motor(z_mo, [0, 1, 0]) motor_and_pinion(360 * Z / lead);
    color("gold") z_switch_at() switch_holder();
  }
  at_motor(y_mo, [-1, 0, 0]) motor_and_pinion(360 * Y / lead);
  at_motor(x_mo, [0, -1, 0]) motor_and_pinion(-360 * X / lead);
  color("gold") { y_switch_at() switch_holder(); x_switch_at() switch_holder(); }
}

// ---------------------------------------------------------------- the parts as printed
module print_part(p) {
  if (p == "base") base();
  else if (p == "beam") translate([0, 0, beam[1][1]]) rotate([-90, 0, 0]) beam();                       // on its back, the rail up
  else if (p == "table") translate([0, 0, tab[2][1]]) rotate([180, 0, 0]) table();                     // its top down
  else if (p == "x_carriage") translate([0, 0, -car[1][0]]) rotate([90, 0, 0]) x_carriage();          // its front down
  else if (p == "z_slide") z_slide();                                                                   // standing
  else if (p == "x_motor_bracket") translate([0, 0, -x_mo[1]]) rotate([90, 0, 0]) x_motor_bracket();  // the plate down
  else if (p == "z_motor_bracket") translate([0, 0, plate_t - z_mo[1]]) rotate([90, 0, 0]) z_motor_bracket();  // the plate down
  else if (p == "gib_y") rotate([90, 0, 0]) linear_extrude(tab[1][1] - tab[1][0]) gib2d(y_rail_w, dt_h);       // lying, the profile up
  else if (p == "gib_x") rotate([90, 0, 0]) linear_extrude(car[0][1] - car[0][0]) gib2d(x_rail_w, dt_h);
  else if (p == "gib_z") rotate([90, 0, 0]) linear_extrude(car[2][1] - car[2][0]) gib2d(z_rail_w, dt_h);
  else if (p == "pinion") pinion();
  else if (p == "snap_pin") translate([0, 0, 4.5]) rotate([90, 0, 0]) snap_pin();                          // lying, the split upright
  else if (p == "switch_holder") translate([0, 0, 12]) rotate([-90, 0, 0]) switch_holder();
  else if (p == "bolt_25") bolt(25);
  else if (p == "bolt_40") bolt(40);
  else if (p == "bolt_45") bolt(45);
  else if (p == "set_screw") set_screw();
  else if (p == "gib_screw_14") gib_screw(14);
  else if (p == "gib_screw_25") gib_screw(25);
  else if (p == "gib_screw_50") gib_screw(50);
  else if (p == "thumb_screw") thumb_screw();
  else if (p == "paper_clip") rotate([90, 0, 0]) paper_clip();
}

if (part == "assembly") assembly(); else print_part(part);
