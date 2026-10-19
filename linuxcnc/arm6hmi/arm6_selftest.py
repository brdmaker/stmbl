"""ARM-6 screen self-test: with ARM6HMI_TEST=<tap file> in the environment
the handler loads this and it drives the running screen through its own
buttons, checking what LinuxCNC and the screen show, then saves a
screenshot per view next to the TAP file and closes LinuxCNC. With a tool
changer it also changes tools from the TOOLS view.
hwtest/arcmate-sim: make test-hmi (Xvfb).
"""

import os

import hal
import linuxcnc
from PyQt5 import QtCore

from qtvcp.core import Status

STATUS = Status()


class SelfTest:
    def __init__(self, handler, tap):
        self.h, self.w, self.tap = handler, handler.w, tap
        self.n = self.failed = 0
        self.lines = []
        self.steps = [
            ("the screen and the 3D view are up", self.s_view),
            ("E-STOP reset from the screen", self.s_estop_reset),
            ("POWER on from the screen", self.s_power),
            ("Home all", self.s_home),
            ("JOG: J1 +10 deg in joint mode", self.s_jog),
            ("MDI move in world mode", self.s_mdi),
            ("TEACH: record a point", self.s_record),
            ("JOG PAD: J2 +10 deg, then X -10 mm (the keys switch the frame)", self.s_pad),
        ]
        if handler.has_changer:
            self.steps += [
                ("TOOLS: mount T1 (the torch)", lambda: self.s_tool(1)),
                ("TOOLS: change to T2 (the gripper)", lambda: self.s_tool(2)),
                ("TOOLS: put T2 back", lambda: self.s_tool(0)),
            ]
        self.steps += [("E-STOP from the screen", self.s_estop)]
        self.timeouts = {"TOOLS": 120}
        self.i = 0
        self.t = 0.0
        self.state = {}
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(200)

    def ok(self, cond, what):
        self.n += 1
        self.failed += not cond
        self.lines.append("%s %d - %s" % ("ok" if cond else "not ok", self.n, what))

    def note(self, text):
        self.lines.append("# " + text)

    # each step: called every 200 ms with self.t (s in the step); returns
    # True when done (checks made), None to keep waiting; after 20 s: failed
    def tick(self):
        self.t += 0.2
        if self.i >= len(self.steps):
            return self.finish()
        name, fn = self.steps[self.i]
        try:
            done = fn()
        except Exception as e:  # noqa: BLE001
            self.ok(False, "%s: %r" % (name, e))
            done = True
        if done is None and self.t > self.timeouts.get(name.split(":")[0], 20):
            self.ok(False, name + ": timed out" + (" (%s)" % self.state["diag"] if "diag" in self.state else ""))
            done = True
        if done:
            self.i += 1
            self.t = 0.0
            self.state = {}

    def s_view(self):
        v = self.h.view
        if v is None or v.v is None:
            return None if self.t < 10 else self.ok(False, "3D view: %s" % (v.error if v else "none")) or True
        self.ok(True, "the 3D view loads %s and runs GL (%s)" % (os.path.basename(v._model_path), v.info))
        self.ok(self.w.table_drives.rowCount() >= 6, "the drive table has a row per drive (%d)" % self.w.table_drives.rowCount())
        try:
            hal.get_value("arm6.vis3d.joint-0")
            self.ok(True, "the view's HAL pins exist (arm6.vis3d.joint-0)")
        except Exception as e:  # noqa: BLE001
            self.ok(False, "the view's HAL pins: %s" % e)
        return True

    def s_estop_reset(self):
        if "clicked" not in self.state:
            self.w.btn_estop.click()
            self.state["clicked"] = 1
        # the pill follows on the screen's next 100 ms refresh: give it 3 s
        if STATUS.estop_is_clear() and (self.w.lbl_state.text() == "MACHINE OFF" or self.t > 3):
            self.ok(self.w.lbl_state.text() == "MACHINE OFF", "E-STOP reset: the state pill says %s" % self.w.lbl_state.text())
            return True

    def s_power(self):
        if "clicked" not in self.state:
            self.w.btn_power.click()
            self.state["clicked"] = 1
        if STATUS.machine_is_on() and (self.w.lbl_state.text() == "MACHINE ON" or self.t > 3):
            self.ok(self.w.lbl_state.text() == "MACHINE ON", "machine on: the state pill says %s" % self.w.lbl_state.text())
            return True

    def s_home(self):
        if "clicked" not in self.state:
            self.w.btn_home.click()
            self.state["clicked"] = 1
        if STATUS.is_all_homed():
            self.ok(True, "all joints homed")
            return True

    def s_jog(self):
        st = self.state
        if "start" not in st:
            self.w.btn_mode_jog.click()
            self.w.btn_frame_joint.click()
            self.w.btn_inc_2.click()  # 10
            st["start"] = STATUS.stat.joint[0]["input"]
            st["v0"] = self.h.view.v.frame(-1)[0] if self.h.view and self.h.view.v else None
            return None
        if "pressed" not in st and self.t > 1.0:
            minus, plus, label, pos = self.h.jog_buttons[0]
            plus.pressed.emit()
            plus.released.emit()
            st["pressed"] = 1
            return None
        moved = STATUS.stat.joint[0]["input"] - st["start"]
        if "pressed" in st and abs(moved - 10) < 0.05 and abs(STATUS.stat.joint[0]["velocity"]) < 1e-3:
            self.ok(True, "J1 jogged %.3f deg (10 asked)" % moved)
            row_text = self.h.joint_rows[0][1].text()
            self.ok(abs(float(row_text) - STATUS.stat.joint[0]["input"]) < 0.02, "the joint row shows it (%s)" % row_text)
            pin = hal.get_value("arm6.vis3d.joint-0")
            self.ok(abs(pin - STATUS.stat.joint[0]["input"]) < 0.02, "the 3D view's joint pin follows (%.2f)" % pin)
            v1 = self.h.view.v.frame(-1)[0]
            d = sum((a - b) ** 2 for a, b in zip(v1, st["v0"])) ** 0.5 if st["v0"] else 0
            self.ok(d > 0.05, "the 3D arm moved: tool point %.0f mm" % (d * 1000))
            return True

    def s_mdi(self):
        st = self.state
        if "sent" not in st:
            self.w.btn_frame_world.click()
            self.w.btn_mode_mdi.click()
            st["z0"] = STATUS.stat.actual_position[2]
            self.w.edit_mdi.setText("G91 G1 Z-20 F3000")
            self.w.btn_mdi_go.click()
            st["sent"] = 1
            return None
        dz = STATUS.stat.actual_position[2] - st["z0"]
        if abs(dz + 20) < 0.05 and STATUS.stat.interp_state == 1:  # INTERP_IDLE
            st.setdefault("idle", self.t)
        if "idle" in st and self.t - st["idle"] >= 0.6:  # settled, the pose refreshed
            dz = STATUS.stat.actual_position[2] - st["z0"]
            self.ok(abs(dz + 20) < 0.05, "MDI moved Z %.3f mm (-20 asked)" % dz)
            self.ok(abs(float(self.w.lbl_z.text()) - STATUS.stat.actual_position[2]) < 0.05, "the pose shows Z %s" % self.w.lbl_z.text())
            g = self.w.lbl_guard.text()
            self.ok(self.h.prevent_mm == 10 and g in ("GUARD: ARMED", "GUARD: NEAR"),
                    "the guard is armed with the cell's prevent %s mm (%s, %s)" % (self.h.prevent_mm, g, self.w.lbl_clear.text()))
            self.w.edit_mdi.setText("G90")
            self.w.btn_mdi_go.click()
            return True

    def s_record(self):
        if "clicked" not in self.state:
            self.w.btn_mode_teach.click()
            self.state["clicked"] = 1
            return None
        if self.t < 1.0:
            return None
        self.w.btn_record.click()
        ok = os.path.exists(self.h.teach_file) and open(self.h.teach_file).read().startswith("G1 X")
        self.ok(ok and self.w.lbl_points.text() == "1 points", "a point recorded to %s (%s)" % (os.path.basename(self.h.teach_file), self.w.lbl_points.text()))
        return True

    def s_pad(self):
        st = self.state
        rows = {(joint, i): (minus, plus) for joint, i, pos, minus, plus in self.h.pad_rows}
        if "start" not in st:
            self.w.btn_view_jog.click()
            self.w.btn_jinc_2.click()  # 10
            st["start"] = 1
            return None
        if "j" not in st and self.t > 1.0:
            st["j0"] = STATUS.stat.joint[1]["input"]
            rows[(True, 1)][1].pressed.emit()
            rows[(True, 1)][1].released.emit()
            st["j"] = self.t
            return None
        if "j" in st and "x" not in st:
            moved = STATUS.stat.joint[1]["input"] - st["j0"]
            if abs(moved - 10) < 0.05 and abs(STATUS.stat.joint[1]["velocity"]) < 1e-3:
                st.setdefault("jdone", self.t)
            if "jdone" in st and self.t - st["jdone"] >= 0.6:  # settled
                self.ok(STATUS.stat.motion_mode == linuxcnc.TRAJ_MODE_FREE, "the J2 key put LinuxCNC in joint mode")
                self.ok(True, "J2 jogged %.3f deg from the pad (10 asked)" % moved)
                # the world jog on the commanded position: the arm's feedback
                # still settles from the J2 move by a few 0.01 mm
                st["x0"] = STATUS.stat.position[0]
                rows[(False, 0)][0].pressed.emit()  # X -
                rows[(False, 0)][0].released.emit()
                st["x"] = self.t
            return None
        if "x" in st:
            dx = STATUS.stat.position[0] - st["x0"]
            st["diag"] = "X moved %.3f, J1 at %.3f, motion mode %d, events: %s" % (
                dx, STATUS.stat.joint[0]["input"], STATUS.stat.motion_mode, self.w.lbl_events.text())
            if abs(dx + 10) < 0.001 and STATUS.stat.current_vel < 1e-3:
                st.setdefault("xdone", self.t)
            if "xdone" in st and self.t - st["xdone"] >= 0.6:
                self.ok(STATUS.stat.motion_mode == linuxcnc.TRAJ_MODE_TELEOP, "the X key put LinuxCNC in world mode")
                self.ok(True, "X jogged %.3f mm from the pad (-10 asked)" % dx)
                pos = [p for joint, i, p, m, pl in self.h.pad_rows if not joint and i == 0][0].text()
                self.ok(abs(float(pos) - STATUS.stat.actual_position[0]) < 0.05, "the pad shows X %s" % pos)
                self.shot("jog")
                self.w.btn_view_main.click()
                return True

    def s_tool(self, tid):
        st = self.state
        if "clicked" not in st:
            self.w.btn_view_tools.click()
            st["from"] = hal.get_value("armtool.0.mounted")
            if tid:
                self.h.tool_rows[tid][1].click()
            else:
                self.w.btn_tool_putback.click()
            st["clicked"] = self.t
            return None
        if self.h.tool_change is not None or self.t < 1.0:
            if tid == 2 and "mid" not in st and self.h.tool_change and self.h.tool_change["stage"] == "change" \
                    and hal.get_value("stmbl-arm.0.changer.unlocked"):
                st["mid"] = 1  # T1 just released in its dock
                self.shot("tools_change")
            return None
        mounted = hal.get_value("armtool.0.mounted")
        name = self.h._tool_name(tid)
        self.ok(mounted == tid and STATUS.stat.tool_in_spindle == tid and hal.get_value("armkins.tool") == tid,
                "%s on the flange: armtool, LinuxCNC and the kinematics agree (%s)" % (name, self.w.lbl_chg_progress.text()))
        self.ok(self.w.lbl_tool_mounted.text() == name and self.w.lbl_chg_state.text() == "READY",
                "the TOOLS view shows %s, the changer READY" % self.w.lbl_tool_mounted.text())
        rows_ok = all(state.text() == ("ON FLANGE" if t == tid else "IN DOCK") for t, (state, b) in self.h.tool_rows.items())
        self.ok(rows_ok, "the tool rows: %s" % ", ".join("T%d %s" % (t, s.text()) for t, (s, b) in sorted(self.h.tool_rows.items())))
        v = self.h.view.v
        pin = hal.get_value("arm6.vis3d.tool")
        f6, R6 = v.frame(6)
        tp, _ = v.frame(-1)
        tcp = [t["tcp"] for t in self.h.tools if t["id"] == tid]
        want = sum(x * x for x in tcp[0]) ** 0.5 / 1000 if tcp else 0.0
        got = sum((a - b) ** 2 for a, b in zip(tp, f6)) ** 0.5
        self.ok(pin == tid and abs(got - want) < 1e-4, "the 3D view: tool pin %d, tool point %.0f mm off the flange" % (pin, got * 1000))
        if tid == 2:
            self.shot("tools")
        if tid == 0:
            self.w.btn_view_main.click()
        return True

    def shot(self, view):
        path = os.path.splitext(self.tap)[0] + ("_%s.png" % view)
        img = self.w.centralwidget.window().grab()
        self.ok(img.save(path), "screenshot %s (%dx%d)" % (os.path.basename(path), img.width(), img.height()))

    def s_estop(self):
        if "shot" not in self.state:  # the screenshot while running, guard and all
            path = os.path.splitext(self.tap)[0] + ".png"
            img = self.w.MainWindow.grab() if hasattr(self.w, "MainWindow") else self.h.w.centralwidget.window().grab()
            self.ok(img.save(path), "screenshot %s (%dx%d)" % (os.path.basename(path), img.width(), img.height()))
            self.note("guard: %s, %s" % (self.w.lbl_guard.text(), self.w.lbl_clear.text()))
            self.note("drives: " + ", ".join("%s %s A" % (self.w.table_drives.item(r, 0).text(), self.w.table_drives.item(r, 1).text())
                                             for r in range(self.w.table_drives.rowCount())))
            self.note("events: " + self.w.lbl_events.text())
            self.w.btn_estop.click()
            self.state["shot"] = 1
            return None
        if not STATUS.estop_is_clear():
            self.ok(self.w.lbl_state.text() == "E-STOP" and self.w.btn_estop.text() == "RESET", "E-STOP: the pill and the button say so")
            return True

    def finish(self):
        self.timer.stop()
        self.lines.append("1..%d" % self.n)
        self.lines.append("# %d/%d passed" % (self.n - self.failed, self.n))
        with open(self.tap, "w") as f:
            f.write("\n".join(self.lines) + "\n")
        with open(os.path.splitext(self.tap)[0] + ".result", "w") as f:
            f.write("%d\n" % (1 if self.failed else 0))
        win = self.w.centralwidget.window()
        QtCore.QTimer.singleShot(300, win.close)
