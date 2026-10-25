# the vibration sensor in the simulator: vibsim (the IIS3DWB on a machine)
# feeds vib (docs/vibsensor.md); nrt runs as on the sensor (the FFTs)
load vibsim
load vib
vibsim0.rt_prio = 1
vib0.rt_prio = 2
vib0.rpm = vibsim0.rpm
vib0.teeth = vibsim0.teeth
vib0.temp = vibsim0.temp
autonrt 1
start
