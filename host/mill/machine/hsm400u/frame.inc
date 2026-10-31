# Mikron HSM 400U as the simulation lays it out (host/mill/mplant.h): the
# table with the trunnion on Y, the X carriage on a bridge, Z vertical,
# the A/C trunnion on the table. Masses, stiffnesses and modes are
# GUESSES for a compact polymer-concrete 5-axis HSC machine: replace them
# with the machine's (weigh, tap-test).
table 350          # Y: table, trunnion, work
carriage 120       # X carriage
zslide 80          # Z slide and motor spindle body
head 15 180 140 160 120 0.04   # spindle tip: modal mass, x top/bottom, y top/bottom [Hz], damping
bridge 400 70 0.04 # bridge / column: its y mode with all it carries
frame 2500 30 25 0.06  # base on its feet
#
# Screws: 32 x 16 mm (E A 1.2e8 N), motor side J (rotor 7.7, coupling 1,
# screw 6.5 kg cm^2), nut distance from the fixed bearing, nut + bearing +
# coupling 4e8 N/m
screw x 0.00152 1.2e8 0.15 1 4e8 0.04
screw y 0.00152 1.2e8 0.15 1 4e8 0.04
screw z 0.00162 1.2e8 0.10 -1 4e8 0.04
fric x 0.1 0.0005 60 200
fric y 0.1 0.0005 90 300
fric z 0.1 0.0005 70 200
stroke x -2 402
stroke y -2 452
stroke z -362 2
#
# The trunnion: motor side J (rotor + gear), the gear train's stiffness at
# the axis [Nm/rad], what turns [kg m^2], damping
rotary a 0.00092 1.5e6 4.0 0.05
rotary c 0.00087 8.0e5 0.4 0.05
fric a 0.1 0.0005 30 5
fric c 0.1 0.0005 15 2
stroke a -100 100
unbalance 2.0 0   # 2 kg m below the A axis at A 0
#
# xyzac-trt-kins: the A axis at y 225, z -250, the C axis at x 200 on it,
# its table 80 mm above the A axis; a 100 mm tool
trt 200 225 -250 0 80 100
servo 1
home 200 225 -100 0 0
