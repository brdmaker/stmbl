# the sensor as it runs on its board (vibsensor/conf/vnode.txt), vibsim
# standing in for the IIS3DWB and the TMP117, on the simulated bus
load vibsim
vibsim0.rt_prio = 1
@include ../vibsensor/conf/vnode.txt
vib0.temp = vibsim0.temp
autonrt 1
start
