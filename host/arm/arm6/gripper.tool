# electric parallel gripper on the flange: its own stmbl drive (gripper.conf:
# 2 A = 0.24 Nm, ~190 N per finger), a 4 mm/turn screw per finger, 0..80 mm.
# Gripping holds the fingers at the current limit for as long as the part
# is held, so gripper.conf has conf0.max_sat = 0 (no saturation fault; the
# default 0.2 s would trip). stmbl configs take no comments: notes go here.
gripper drive gripper.conf sim.txt
gripper mech 4 80 0.3 0.00001 0.01
# jaws: centre 150 mm out on the flange z, fingers 60 mm long, 30 mm wide
gripper jaw 0.15 0.06 0.03
# the gripper body (housing) for the collision model
body 6 capsule 0 0 0.02   0 0 0.09     0.045
