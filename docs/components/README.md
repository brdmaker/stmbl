# Component reference

Every HAL component of the firmware, generated from its source by `tools/comp_docs.py` (`make -C host check` fails while a page is out of date). Describe a component in a comment before its pins and each pin in a comment after it; then run `tools/comp_docs.py`.

112 components, 539 of 2041 pins described.

## F4 and F3 (shared)

| component | pins described | summary |
|---|---|---|
| [ac](ac.md) | 0/3 |  |
| [acim_ttc](acim_ttc.md) | 1/26 |  |
| [acvf](acvf.md) | 36/40 | Sensorless V/f control for induction motors (conveyors, fans, pumps), with what an industrial inverter adds to plain V/f: - torque from ... |
| [and](and.md) | 0/5 |  |
| [auto_ac](auto_ac.md) | 0/5 |  |
| [avg](avg.md) | 0/5 |  |
| [biquad](biquad.md) | 6/6 | ## Brief `biquad` is a second order filter for a command signal, typically a notch on the torque command at a mechanical resonance. ... |
| [can](can.md) | 1/30 | The drive's end of stmbl over CAN (shared/canproto.h, docs/can_support.md): in place of the smart-serial link, with the same pins, so ... |
| [curpid](curpid.md) | 4/24 | enable |
| [dbrake](dbrake.md) | 16/35 | Dynamic brake: the motor's own back-EMF stops it, with no position feedback (docs/emergency_stop.md). fault0.dbrake asks for it (a ... |
| [dc](dc.md) | 0/4 | dc voltage input |
| [dc_limits](dc_limits.md) | 0/18 | motor values |
| [dc_ttc](dc_ttc.md) | 0/3 | motor values |
| [debounce](debounce.md) | 0/4 |  |
| [dob](dob.md) | 9/9 | ## Brief `dob` is a disturbance (load torque) observer on the torque command. It estimates the torque the rigid body model cannot ... |
| [dq](dq.md) | 1/11 |  |
| [en](en.md) | 0/6 |  |
| [encmon](encmon.md) | 24/32 | Encoder monitor: checks the motor feedback against the motor's own voltage equation. The current loop works in the frame of com ... |
| [fanuc](fanuc.md) | 0/5 | fanuc encoder |
| [fault](fault.md) | 17/59 |  |
| [fb_switch](fb_switch.md) | 11/50 |  |
| [field](field.md) | 0/19 | in |
| [fmove](fmove.md) | 0/29 |  |
| [frf](frf.md) | 33/33 | ## Brief `frf` measures the frequency response of the mechanics with the position loop running (stepped sine) and fits a two-mass model ... |
| [fwup](fwup.md) | 0/4 | In-band firmware update over the console (the smart-serial console while LinuxCNC runs, or USB): servoterm-cli --update sends the image ... |
| [gain](gain.md) | 0/3 |  |
| [gantry](gantry.md) | 7/16 | gantry: one of two drives moving a gantry's bridge (Y1, Y2), each with its own scale. Each drive is tuned for its share of the moving ... |
| [hal_test](hal_test.md) | 0/2 |  |
| [home](home.md) | 0/14 |  |
| [idacim](idacim.md) | 14/41 | Induction motor identification, all at standstill (the rotor does not turn: dc on one axis makes no torque), so a conveyor can stay ... |
| [iddc](iddc.md) | 0/29 |  |
| [idm](idm.md) | 0/49 |  |
| [idpmsm](idpmsm.md) | 1/36 |  |
| [idq](idq.md) | 0/10 |  |
| [ids](ids.md) | 5/42 |  |
| [idx_home](idx_home.md) | 1/7 |  |
| [iit](iit.md) | 0/8 | conf |
| [iit_old](iit_old.md) | 0/16 |  |
| [jog](jog.md) | 0/1 |  |
| [linrev](linrev.md) | 3/14 | Calculate motor angle from position in machine units |
| [mad](mad.md) | 0/4 |  |
| [motsim](motsim.md) | 21/44 | sim inputs |
| [move](move.md) | 0/4 |  |
| [mpid](mpid.md) | 0/23 |  |
| [mtemp](mtemp.md) | 11/12 | The motor's temperature sensor on the HV board (hv0.mot_temp, ohms) and the thermal model (iit0.temp): temp, what fault0 checks, is the ... |
| [mturn](mturn.md) | 1/18 | mturn: the multi-turn position of an absolute encoder, for linrev. |
| [mux](mux.md) | 0/3 |  |
| [not](not.md) | 0/2 |  |
| [or](or.md) | 0/5 |  |
| [pe](pe.md) | 9/22 | Power estimation. The motor's electrical input from the dq voltages and currents the hv reports (amplitude-invariant dq: 3/2 (ud id + uq ... |
| [pid](pid.md) | 28/40 |  |
| [pmsm](pmsm.md) | 0/15 | in |
| [pmsm_limits](pmsm_limits.md) | 0/19 | motor values |
| [pmsm_ttc](pmsm_ttc.md) | 9/17 | motor values |
| [pos_filter](pos_filter.md) | 0/7 |  |
| [psi](psi.md) | 0/8 |  |
| [qstop](qstop.md) | 4/23 | Quick stop: a controlled stop in the command path, before pid (docs/emergency_stop.md). fault0.qstop asks for it (link lost, enable ... |
| [ramp](ramp.md) | 0/11 | input |
| [reslimit](reslimit.md) | 0/3 |  |
| [rev](rev.md) | 0/7 |  |
| [rl](rl.md) | 0/26 |  |
| [rlpsij](rlpsij.md) | 21/61 | Motor identification. State sequence:   1  r: voltage mode, d axis locked, current regulated to test_cur;      r from a least-squares ... |
| [rpull](rpull.md) | 5/6 | A resistive sensor on a feedback port's pin 7: its resistance from the pin's voltage (shared/rpull.h, docs/motor_temp_lv.md), for ... |
| [scale](scale.md) | 0/5 |  |
| [scope](scope.md) | 0/13 | Capture scope in the drive: records up to 8 pins into its own memory at the full loop rate, triggered, for download over a slow console ... |
| [sensorless](sensorless.md) | 0/24 | in |
| [sim](sim.md) | 2/12 |  |
| [spid](spid.md) | 12/18 | input |
| [stp](stp.md) | 0/15 |  |
| [stp_ttc](stp_ttc.md) | 0/10 | Torque to current for a two-phase hybrid stepper (brdstepper), in place of pmsm_ttc: torque = polecount * psi * iq (two phases: no 3/2), ... |
| [svm](svm.md) | 0/12 | U V W inputs |
| [term](term.md) | 0/5 | the console over smart-serial, where it is linked (the F4 app, the host; not the F3 power stage): weak references, NULL elsewhere |
| [th](th.md) | 0/7 |  |
| [uf](uf.md) | 0/8 |  |
| [uf2](uf2.md) | 0/12 |  |
| [uvw](uvw.md) | 1/20 | u,v,w inputs |
| [vel](vel.md) | 2/16 |  |
| [vel_int](vel_int.md) | 5/17 |  |
| [velbuf](velbuf.md) | 0/6 |  |
| [veltime](veltime.md) | 0/6 |  |
| [veltopos](veltopos.md) | 0/6 |  |
| [vib](vib.md) | 10/56 | The vibration sensor's processing (docs/vibsensor.md): the accelerometer's samples (vibbuf, 26.7 kHz x y z) become, per reporting ... |
| [vnode](vnode.md) | 1/43 | The vibration sensor on its measurement bus (docs/vibsensor.md §3): what vib0 finds goes out as frames, the PC's machine state comes in, ... |
| [wobl](wobl.md) | 0/9 |  |
| [ypid](ypid.md) | 12/20 | #include "commands.h" |

## F4 (logic board)

| component | pins described | summary |
|---|---|---|
| [adc](adc.md) | 11/20 | V_DIFF of a sum of OVER samples as sum * V_K(OVER) - V_K0 (shared/adc_sum.h: adc_lin) |
| [conf](conf.md) | 57/57 |  |
| [dmm](dmm.md) | 1/5 |  |
| [enc_cmd](enc_cmd.md) | 2/8 |  |
| [enc_fb](enc_fb.md) | 2/22 |  |
| [encf](encf.md) | 5/18 |  |
| [encm](encm.md) | 0/12 |  |
| [encs](encs.md) | 0/3 | Sanyo denki absolute encoder, tested with PA035C |
| [endat](endat.md) | 0/21 |  |
| [fanuc_tw](fanuc_tw.md) | 39/56 | fanuc_tw: feedback driver for FANUC 2-wire (1-pair, "αi interface") serial pulsecoders, the βiA128 (A860-2020-T301) of the βiS motors, ... |
| [fanuci](fanuci.md) | 34/42 | fanuci: probe for FANUC 1-pair ("αi interface", 2-wire, half-duplex) serial pulsecoders such as the βiA128 (A860-2020-T301), on FB0's Z ... |
| [fbtx](fbtx.md) | 1/8 | fbtx: drive the fb1 RS-485 transceivers, for testing the port (hwtest/fbtest). Each pair (a: pins 1-2, b: 3-6, z: 5-4) has a SN65176B ... |
| [hv](hv.md) | 10/49 | process data from LS |
| [hvf1](hvf1.md) | 0/17 |  |
| [hx711](hx711.md) | 0/9 |  |
| [o_fb](o_fb.md) | 0/2 |  |
| [res](res.md) | 3/14 |  |
| [smartabs](smartabs.md) | 0/3 |  |
| [sserial](sserial.md) | 6/30 | pins |
| [usart](usart.md) | 1/7 |  |
| [yaskawa](yaskawa.md) | 7/25 |  |

## F4 (logic board, board io)

| component | pins described | summary |
|---|---|---|
| [io3](f4-io3.md) | 0/4 |  |
| [io4](f4-io4.md) | 9/51 |  |

## F3 (HV board)

| component | pins described | summary |
|---|---|---|
| [enc](f3-enc.md) | 0/3 |  |
| [hv](f3-hv.md) | 2/14 |  |
| [io](f3-io.md) | 0/23 |  |
| [ls](f3-ls.md) | 5/45 | process data from LS |
