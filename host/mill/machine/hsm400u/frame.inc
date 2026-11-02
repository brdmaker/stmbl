# Mikron HSM 400U as the simulation lays it out (host/mill/mplant.h): the
# table with the trunnion on Y, the X carriage on a bridge, Z vertical,
# the B/C trunnion on the table (B tilts about Y, C turns the pallet).
# From the manual (docs/hsm400u/manual/08_p163-177.md, 9.2.1 and 9.2.10):
# X 400 (+/-200 about the table centre), Y 240 (-160 / +80 about it),
# Z 350, B +/-110 deg; max. 350 mm work height, 230 mm turning diameter. Masses, stiffnesses and modes are
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
# travels: X 0..400, Y 0..240, Z -350..0 (the end stops 2 mm beyond)
stroke x -2 402
stroke y -2 242
stroke z -352 2
#
# The trunnion: motor side J (rotor + gear), the gear train's stiffness at
# the axis [Nm/rad], what turns [kg m^2], damping
rotary b 0.00092 1.5e6 4.0 0.05
rotary c 0.00087 8.0e5 0.4 0.05
fric b 0.1 0.0005 30 5
fric c 0.1 0.0005 15 2
stroke b -112 112
unbalance 2.0 0   # 2 kg m below the B axis at B 0
#
# xyzbc-trt-kins. The C axis (the table centre) at X 200, Y 160 (the
# manual's travels about it). Z is the spindle's gauge line; the pallet
# face is at Z -350 in world coordinates (the lowest Z, with the manual's
# 100 mm least spindle-to-table distance [uncertain, HSM 400 drawing] and
# a 100 mm tool: the tip touches it). GUESSES: the B axis 80 mm below the
# pallet face, crossing the C axis (x-offset 0). Replace with MP950 and
# the B pivot from the iTNC.
#   trtbc XR YR ZR DX DZ TOOL: z-rot-point -450 (the pallet face, tip
#   frame), z-offset -80 (the B axis below it), tool-offset 100
trtbc 200 160 -450 0 -80 100
servo 1
home 200 160 -100 0 0
