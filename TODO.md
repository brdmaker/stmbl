# TODO

(The GUI items below, webui/..., are in brdmaker/brdmot since the repo split, 2026-11-06.)

Open work, grouped by area. Tick an item off (or delete it) when it lands, and
record any debug findings in `DEBUGNOTES.md` as usual.

## Drives (stmbl, the SEW bench)

- [x] **Motor temperature sensor on the LV board** (docs/motor_temp_lv.md):
      on drive 208535A65631 the KTY on FB0 pins 7 / 8 (JP2 opened), the
      resolver clean through a 30 min soak; calibrated to 25 C
      (`rpull0.rpull = 9064`: a ~97 k leak beside the cut jumper).
- [ ] **USB console under motor current** (DEBUGNOTES 2026-10-29): the PC
      stops polling the drive after a STALL on the bulk IN; the wizard
      reconnects, smart-serial avoids it. Try a USB isolator / another port.
      Find why the tuning over USB came out lower.
- [ ] **Resolver on FB1:** needs an excitation on FB1's Z driver (TIM1 CH3,
      PE13) synced to FB0's, polarity demodulation for sin1/cos1 in `adc.c`, a
      `res_fb1` template; FB1 gets 1 of 10 ADC samples, so it is noisier.
- [ ] **SEW CMP40M:** commissioned (wizard, all steps, `conf/sew_cmp40m.txt`),
      the KTY now on FB0. Run at 314 V for the full 3000 rpm.
- [ ] **Wizard simulator:** `syn_torque_14p` fails at the torque constant check
      (identified 1.31 vs nameplate 10.4 Nm/A), already before 2026-10-29.
- [ ] **FANUC βiA128 (1-pair) feedback driver:** `fanuc_tw` (docs/fanuc_tw.md) runs both
      βiS 4/4000 in closed loop (setup wizard, `conf/fanuc_bis4-4000.txt`, `-2.txt`, 2026-11-05).
      Still to do: the motor temperature (battery alarm found: frame bit 6) and
      the other status bits (w4, bits 71-73), whether the position jumps at the reference after a long outage,
      the βiS 12/2000, the wizard's idm0.inertia set (DEBUGNOTES). The frame is decoded
      (DEBUGNOTES 2026-11-03, brdmot probe-log.md). Next: decode it in
      `shared/fanuc1p.c` (24-bit position, turns, CRC-5 0x0B, commutation
      from the position) with a host test on the recorded frames, `fanuci`
      outputs for `fb_switch0`, a template; then the battery-out test and a
      βiS motor. Drive 208535A65631 has the probe config now (SEW config
      backed up as config-pre-fanuci.txt).
- [ ] Boot-time fault 11 (HV volt error at 57 ms, before the bus is measured)
      is logged on every boot.

## Arm (millweb arm page, `webui/mill/js/arm.js`)

- [x] Configure page: machine type "arm" (`generate_arm`: armkins, armguard,
      armcollide, armgrip, armtool + M6, powerquota, the E-STOP chain;
      `machines/arcmate_sim`, run by `make test-gui-arm-machine`).
- [x] Simulation: a program's and MDI's A B C by IK; MDI moves, M6, gripper.
- [x] Teach pendant: Go home, the points list (Go, reorder, delete, clear,
      Load as program); the teach file is a program (M2).
- [x] Hardware enabling switch (`arm.enable`: jog-inhibit / jog-stop,
      optional feed-inhibit; the console shows it and stops its own jogs).
- [x] TOOL-frame jog (simulated; live as one G53 MDI move).
- [x] Hide the mill tool 3D-model card on the arm Tools page.
- [x] Safety note in the README (the browser jog is not safety-rated).
- [ ] Real drives on an arm: the generated config (gear scaling, enabling
      switch, changer pins, estop input) is only checked by unit tests; run it
      on hardware.
- [ ] TOOL-frame turns follow LinuxCNC's linear A B C between the ends, not
      an exact turn about the tool axis (fine for small steps; a held turn
      goes 45° in one move).
- [ ] A phone-sized pendant layout (`docs/hmi/pendant.dc.html`, 390 px).
- [ ] Real dock-stand parts in the cell model (docks are placeholders).
- [ ] Check the 3D frame rate on a Raspberry Pi and a tablet.

## Mikron HSM 400U retrofit (brdmot: `sim/mill/HSM400U.md`, `hwtest/hsm400u-sim`)

- [x] Rough simulation: five axes, the Siemens servos on stmbl's bus,
      glass scales; LinuxCNC runs five-axis programs on it.
- [x] B/C kinematics (`xyzbc-trt-kins`, the sim's `trtbc` line) with the
      manual's travels about the table centre and B +/-110.
- [ ] Real data: motor nameplates (kE decides the rapids), the iTNC 520's
      machine parameters (back up before removing anything), scale and EnDat
      model numbers, masses, tap tests. The pivots: MP950 (C) and the B
      pivot replace the guessed B axis 80 mm below the pallet face. The
      belt ratios, and whether B and C are direct drives (150 / 250 rpm).
- [ ] EnDat 2.1 / 2.2 on the stmbl drive: check `src/comps/endat.c` against
      the encoders' order code.
- [ ] Glass scales into LinuxCNC / the drive: 1 Vpp (interpolator) or
      absolute EnDat (LC).
- [ ] Feed and rapids capped at 8000 mm/min (the owner): in the sim's
      `hsm.ini`; carry it into the real config. Spindle: the 42k HVC 140,
      warm-up as the manual says.
- [ ] The spindle: is its drive a stand-alone VFD or a module of the
      Heidenhain inverter rack (then it needs its own)?
- [ ] Console: a trunnion machine type (Configure: xyzbc/xyzac-trt-kins and
      its pivots; the 3D view's tilting table; B/C in the DRO).
- [ ] Material removal for five axes (TODO below: the height map is 3-axis).
- [ ] Five-axis functions: tool-offset from the tool length (G43), TCP on/off
      (switchkins), a tilted work plane (G68.2-style remap), B/C clamps
      (M10/M11, M15/M16 with the pressure switches), C wrapping, a pivot
      calibration probe macro (cycle 307's job), a CAM post.

## Lathe (`linuxcnc/lathe`, Emco Compact 5)

- [ ] Material removal: compare against a finished-part model (pick the
      model, align its axis with Z).
- [ ] C axis and live tools.
- [x] CTX TWIN: manuals as text (`docs/ctxtwin/`: mechanical, operating
      German + English, Sauter turret).
- [ ] CTX TWIN: twin-lathe synchronisation. The original transfers with the
      spindles angle-synchronous (M803/M804/M800, expert programs %XTWI1U2);
      the handshake, the partition wall (M46/M47) and a two-lathe simulation
      (README "Two lathes in one machine").
- [ ] CTX TWIN: turret as an stmbl servo axis (Sauter: servo indexing, one
      lock/engage valve, S1/S4/S5); turret manual part b and the electrical
      documentation are missing.
- [ ] CTX TWIN: read the feed-motor nameplates (MAC 93 vs MDD 093) and the
      ISM spindle drives.
- [ ] EBAZ4205 template (`machines/emco_compact5/machine-stmbl.json`): fill the
      placeholders — board name, stepgen/GPIO pins, X/Z screw leads.
- [ ] Run the stmbl spindle and the EBAZ4205 config in LinuxCNC (needs the
      hm2 board; untested so far).
- [ ] Measure turret offset, overtravel and lock back-off on the machine.

## CAM scene import (`webui/mill/js/scene.js`, `server/scene.py`)

- [x] Import a CAM setup's scene zip: the stock as the workpiece, the placed
      copies as finished parts (sim and machine; tests: test_scene, scene_test.js,
      sim/live GUI cases).
- [ ] Set the work offset from the scene: probe or touch off at its zero (e.g.
      top centre: probe the plate's edges and top) from the scene panel.
- [ ] Check a program against the scene's box before it runs (outside the
      stock, into a part's keep-out); the material removal already compares.
- [ ] Keep the scene's facts (material, work zero, units) in the setup, not
      only in the stock object's name and scenes/<name>/scene.json.

## Mill material removal (`webui/mill/js/mstock.js`)

- [ ] Undercuts and more than 3 axes (height map is 3-axis only).
- [ ] Spindle on/off state in the live preview.
- [ ] Model the spindle nose and the setup's rotation.
- [ ] Preview paths past 100k moves are merged; keep full detail.

## Servo rate (`docs/servo-timing.md`)

- [ ] Bench: isolate the RT core (isolcpus, irqaffinity), trim hostmot2
      modules, log servo-thread tmax for an hour under load.
- [ ] Bench: 2 kHz (SERVO_PERIOD 500000, vel_int0.cmd_freq 2000); drive rt
      max and sserial errors.
- [ ] Drive firmware: `sserial0.reply_mode` and `adc0.fast` benched on the
      SEW drive (DEBUGNOTES 2026-11-01 entries): rt max 172.8 -> 140.8 us.
      Discovery bug in reply_mode 1 fixed (24/24). 2 kHz on the Pi 4 + SPI:
      no ferror gain (pos_advance swept) and hm2's DoIt overruns: stay at
      1 kHz. Next: the hwtest suite with the switches on, then make them
      the default.
- [ ] Zynq: `docs/zynq-measurement-plan.md` (steps 1-8, servo_rate.py and
      servo_load.py), pick the rate.
- [ ] Measure the front ends with `hwtest/timing/servo_rate.py`: Pi 4 + 7C81
      (SPI), Pi 5 / CM4 + PCIe Mesa card (hm2_pci), Zynq EBAZ4205
      (hm2_soc_ol). Pick by the largest servo time; 4 kHz needs < ~90 us.
- [ ] `servo_rate.py` is only checked without a board (no hm2 hardware in
      the simulator); first run on each board may need fixes.
- [ ] mill_sim: 5 kHz servo period faults before sync (DEBUGNOTES).

## Other

- [ ] `host/arm` and `host/lcnc` keep duplicated source lists in their
      Makefiles; share one list.
- [ ] Velocity observer using the system inertia (task #57).
- [ ] Two known `@todo` host test failures:
  - `host/tests/motor_sew_cmp40m.test` — light rotor, slow L/R: reaches
    max_vel before the current settles; J 24 % high, friction half.
  - `host/tests/stepper_ipropi_worst_detent.test` — detent compensation
    stick-slips with worst-case IPROPI.
