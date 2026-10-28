// the collision check: intersect a group of parts with another at a position (check.py runs it)
include <teststand.scad>
part = "none"; lite = true;
A = "F"; B = "Y";
module group(g) {
  if (g == "F") { base(); beam(); x_motor_bracket(); y_switch_at() switch_holder(); x_switch_at() switch_holder();
    at_motor(y_mo, [-1, 0, 0]) translate([0, 0, -plate_t]) nema_body(); at_motor(x_mo, [0, -1, 0]) translate([0, 0, -plate_t]) nema_body(); }
  if (g == "Y") translate([0, -Y, 0]) { table(); table_gib(); }
  if (g == "X") translate([X, 0, 0]) { x_carriage(); x_gib(); z_gib(); z_motor_bracket(); z_switch_at() switch_holder();
    at_motor(z_mo, [0, 1, 0]) translate([0, 0, -plate_t]) nema_body(); }
  if (g == "S") translate([X, 0, sl_z0 + Z]) z_slide();
  // single parts, for the checks inside a group
  if (g == "base") base(); if (g == "beam") beam(); if (g == "xbr") x_motor_bracket();
  if (g == "xmot") at_motor(x_mo, [0, -1, 0]) translate([0, 0, -plate_t]) nema_body();
  if (g == "ymot") at_motor(y_mo, [-1, 0, 0]) translate([0, 0, -plate_t]) nema_body();
  if (g == "car") x_carriage(); if (g == "gibx") x_gib(); if (g == "gibz") z_gib(); if (g == "zbr") z_motor_bracket();
  if (g == "zmot") at_motor(z_mo, [0, 1, 0]) translate([0, 0, -plate_t]) nema_body();
  if (g == "zsw") z_switch_at() switch_holder(); if (g == "xsw") x_switch_at() switch_holder(); if (g == "ysw") y_switch_at() switch_holder();
  if (g == "table") table(); if (g == "giby") table_gib();
}
intersection() { group(A); group(B); }
