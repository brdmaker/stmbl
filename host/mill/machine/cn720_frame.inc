# The Scripta CN 720's mechanics as an HSC conversion (cn720.mill). Masses
# and stiffnesses are estimates.
#
# Masses (set before frame and bridge, which use them)
table 40          # table, vacuum plate, work
carriage 12       # X carriage
zslide 10         # Z slide and spindle body
head 5 120 90 100 70 0.03   # the spindle tip: modal mass, x modes top/bottom, y modes top/bottom [Hz], damping
bridge 30 70 0.03 # bridge beam; the columns' y mode with all it carries [Hz]
frame 120 18 15 0.05   # base on its feet (a stand): x, y modes [Hz]
#
#
# Screws: motor side J [kg m^2], E A [N] (16 mm, 13 mm root), nut distance
# from the fixed (motor) bearing at 0 [m] and its direction, nut + bearing
# + coupling [N/m] (250, 300, 1000 Nm/rad), damping ratio
screw x 0.000066 2.8e7 0.10 1 1.1e8 0.03
screw y 0.000056 2.8e7 0.10 1 1.1e8 0.03
screw z 0.000041 2.8e7 0.05 -1 1.2e8 0.03
# Friction: motor side Coulomb [Nm], viscous [Nm s/rad]; guides Coulomb [N], viscous [N s/m]
fric x 0.05 0.0001 15 30
fric y 0.05 0.0001 20 40
fric z 0.05 0.0001 25 30
stroke x -5 705
stroke y -5 435
stroke z -205 5
servo 1
home 350 215 -20
