"""ARM-6 operator screen (QtVCP): the handler for arm6.ui.

The layout is docs/hmi/operator.dc.html made real: the joints and the stmbl
drives on the left, the arm in 3D (linuxcnc/vis3d's Vis3dView) with the
Cartesian pose and the program in the middle, jog / gripper / power quota
/ safety on the right, events at the bottom. Repeated rows (joints, jog)
are built here, the rest is arm6.ui (Designer) and arm6.qss.

INI [ARM6HMI] (all optional):
    VIS3D       linuxcnc/vis3d (libvis3d.so, vis3d.py, qtvcp/vis3d_widget.py)
    MODEL       the vis3d model (default: VIS3D/models/arm6_lcnc.vis)
    DRIVE_PINS  the drives' pin prefix, {j} = joint (default stmbl-arm.0.{j})
    GRIP_OPEN   MDI that opens the gripper (default M65 P0)
    GRIP_CLOSE  MDI that closes it (default M64 P0)
    QUOTA_MAX   full scale of the power bar [W] (default 2000)
    TEACH_FILE  where Record point appends (default teach.ngc in the config dir)
    PREVENT_MM  armguard's clearance limit [mm] (default: `prevent` in
                [ARMGUARD] CELL and its includes)
HAL: the 3D view's pins (<screen>.vis3d.joint-N, .guard-state,
.clearance) are netted in arm6_postgui.hal ([HAL] POSTGUI_HALFILE).
"""

import os
import sys
import time

import hal
import linuxcnc
from PyQt5 import QtCore, QtWidgets

from qtvcp import logger
from qtvcp.core import Action, Info, Status

STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

AXES = "XYZABC"
GUARD_REASON = {1: "collision", 2: "protective stop"}  # armguard.0.reason (shared/robot/guard.h)


# armgrip.0.state (shared/robot/gripper.h)
GRIP_STATES = {0: "OPEN", 1: "CLOSING", 2: "GRIPPED", 3: "WRONG PART", 4: "EMPTY", 5: "OPENING", 6: "LOST"}


def cell_prevent_mm(path, depth=0):
    """the `prevent MM` limit of an armguard cell file (following includes); 0 if none"""
    try:
        lines = open(path).read().splitlines()
    except OSError:
        return 0.0
    found = 0.0
    for line in lines:
        t = line.split("#")[0].split()
        if len(t) >= 2 and t[0] == "prevent":
            found = float(t[1])
        elif len(t) >= 2 and t[0] == "include" and depth < 8:
            inc = cell_prevent_mm(os.path.join(os.path.dirname(path), t[1]), depth + 1)
            found = inc or found
    return found


def hal_value(name, default=None):
    """a pin or signal's value; default when it doesn't exist"""
    try:
        return hal.get_value(name)
    except (RuntimeError, NameError):
        return default


def restyle(w, **props):
    for k, v in props.items():
        w.setProperty(k, v)
    w.style().unpolish(w)
    w.style().polish(w)


class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths
        self.view = None
        self.joint_rows = []
        self.jog_buttons = []
        self.events = []
        self.cycle_start = None
        self.cycle_time = 0.0
        self.teach_points = 0
        self.last_guard = None
        self.ini = INFO.INI

    def _ini(self, key, default=None):
        return self.ini.find("ARM6HMI", key) or default

    # ---------------------------------------------------------------- setup
    def initialized__(self):
        self.njoints = int(self.ini.find("KINS", "JOINTS") or 6)
        self.config_dir = os.path.dirname(INFO.INIPATH) if hasattr(INFO, "INIPATH") else os.getcwd()
        self.drive_pins = self._ini("DRIVE_PINS", "stmbl-arm.0.{j}")
        cell = self.ini.find("ARMGUARD", "CELL") or ""
        if cell and not os.path.isabs(cell):
            cell = os.path.join(self.config_dir, cell)
        self.prevent_mm = float(self._ini("PREVENT_MM", 0) or 0) or cell_prevent_mm(cell)
        self._build_view()
        self._build_joint_rows()
        self._build_jog_rows()
        self._build_drive_table()
        self._connect()
        self._setup_panels()
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self._update)
        self.timer.start(100)
        selftest = os.environ.get("ARM6HMI_TEST")
        if selftest:  # tests/: drive the screen and record what it shows
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import arm6_selftest
            self.selftest = arm6_selftest.SelfTest(self, selftest)

    def _build_view(self):
        vdir = self._ini("VIS3D", os.path.join(self.config_dir, "vis3d"))
        model = self._ini("MODEL", os.path.join(vdir, "models", "arm6_lcnc.vis"))
        sys.path.insert(0, os.path.join(vdir, "qtvcp"))
        try:
            from vis3d_widget import Vis3dView
        except ImportError as e:
            self._event("3D view unavailable: %s" % e)
            return
        self.view = Vis3dView(model=model, fps=30)
        self.view.guard_limit_mm = self.prevent_mm
        self.view.setObjectName("vis3d")
        self.w.view_holder.layout().addWidget(self.view)
        # the 3D view takes the spare height, the program panel only what it needs
        grow, keep = QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Maximum
        self.w.view_holder.setSizePolicy(grow, grow)
        self.w.panel_view.layout().setStretchFactor(self.w.view_holder, 1)
        self.w.panel_program.setSizePolicy(QtWidgets.QSizePolicy.Preferred, keep)
        try:
            self.view.hal_init(HAL_NAME="vis3d")  # its pins: <screen>.vis3d.*
        except Exception as e:  # noqa: BLE001  outside QtVCP's HAL
            self._event("3D view without HAL pins: %s" % e)

    def _build_joint_rows(self):
        grid = QtWidgets.QGridLayout()
        grid.setSpacing(8)
        grid.setContentsMargins(0, 0, 0, 0)
        for j in range(self.njoints):
            lo = float(self.ini.find("JOINT_%d" % j, "MIN_LIMIT") or -180)
            hi = float(self.ini.find("JOINT_%d" % j, "MAX_LIMIT") or 180)
            name = QtWidgets.QLabel("J%d" % (j + 1))
            bar = QtWidgets.QProgressBar()
            bar.setRange(int(lo * 10), int(hi * 10))
            bar.setTextVisible(False)
            pos, vel, ferr = QtWidgets.QLabel("0.00"), QtWidgets.QLabel("0.0"), QtWidgets.QLabel("0.000")
            for wd, role in ((name, "jname"), (bar, "jbar"), (pos, "jpos"), (vel, "jsmall"), (ferr, "jsmall")):
                wd.setProperty("role", role)
            for wd in (pos, vel, ferr):
                wd.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            pos.setMinimumWidth(86)
            for c, wd in enumerate((name, bar, pos, vel, ferr)):
                grid.addWidget(wd, j, c)
            grid.setColumnStretch(1, 1)
            self.joint_rows.append((bar, pos, vel, ferr))
        self.w.joint_rows.layout().addLayout(grid)

    def _build_jog_rows(self):
        grid = QtWidgets.QGridLayout()
        grid.setSpacing(6)
        grid.setContentsMargins(0, 0, 0, 0)
        for j in range(self.njoints):
            minus, plus = QtWidgets.QPushButton("−"), QtWidgets.QPushButton("+")
            label, pos = QtWidgets.QLabel(), QtWidgets.QLabel("0.00")
            for b in (minus, plus):
                b.setProperty("role", "jog")
                b.setAutoRepeat(False)
            label.setProperty("role", "jname")
            pos.setProperty("role", "jsmall")
            label.setAlignment(QtCore.Qt.AlignCenter)
            pos.setAlignment(QtCore.Qt.AlignCenter)
            minus.pressed.connect(lambda j=j: self._jog(j, -1))
            plus.pressed.connect(lambda j=j: self._jog(j, 1))
            minus.released.connect(lambda j=j: self._jog_release(j))
            plus.released.connect(lambda j=j: self._jog_release(j))
            grid.addWidget(minus, j, 0)
            grid.addWidget(label, j, 1)
            grid.addWidget(pos, j, 2)
            grid.addWidget(plus, j, 3)
            grid.setColumnStretch(1, 1)
            grid.setColumnStretch(2, 1)
            self.jog_buttons.append((minus, plus, label, pos))
        self.w.jog_rows.layout().addLayout(grid)

    def _build_drive_table(self):
        t = self.w.table_drives
        cols = ["Drive", "I [A]", "P [W]", "State"]
        t.setColumnCount(len(cols))
        t.setHorizontalHeaderLabels(cols)
        t.verticalHeader().setVisible(False)
        t.horizontalHeader().setStretchLastSection(True)
        t.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        t.setSelectionMode(QtWidgets.QAbstractItemView.NoSelection)
        t.setFocusPolicy(QtCore.Qt.NoFocus)
        rows = ["J%d" % (j + 1) for j in range(self.njoints)]
        if hal_value("stmbl-arm.0.grip.current") is not None:
            rows.append("GRIP")
        t.setRowCount(len(rows))
        for r, name in enumerate(rows):
            for c in range(len(cols)):
                it = QtWidgets.QTableWidgetItem(name if c == 0 else "-")
                it.setTextAlignment(QtCore.Qt.AlignVCenter | (QtCore.Qt.AlignLeft if c == 0 else QtCore.Qt.AlignRight))
                t.setItem(r, c, it)
        self.drive_rows = rows

    def _connect(self):
        w = self.w
        w.btn_estop.clicked.connect(self._toggle_estop)
        w.btn_power.clicked.connect(lambda on: ACTION.SET_MACHINE_STATE(bool(on)))
        w.btn_mode_auto.clicked.connect(lambda: ACTION.SET_AUTO_MODE())
        w.btn_mode_mdi.clicked.connect(lambda: ACTION.SET_MDI_MODE())
        w.btn_mode_jog.clicked.connect(lambda: self._set_jog_mode(teach=False))
        w.btn_mode_teach.clicked.connect(lambda: self._set_jog_mode(teach=True))
        w.btn_frame_joint.clicked.connect(lambda: ACTION.SET_MOTION_TELEOP(0))
        w.btn_frame_world.clicked.connect(lambda: ACTION.SET_MOTION_TELEOP(1))
        w.btn_frame_tool.setEnabled(False)
        w.btn_frame_tool.setToolTip("Tool-frame jogging needs a kinematics that offers it (armkins)")
        for i, inc in enumerate((0.1, 1.0, 10.0, 45.0)):
            getattr(w, "btn_inc_%d" % i).clicked.connect(lambda _=False, inc=inc, i=i: self._set_increment(inc, i))
        w.btn_inc_cont.clicked.connect(lambda: self._set_increment(0.0, -1))
        w.slider_jog.valueChanged.connect(self._set_jog_rate)
        w.btn_home.clicked.connect(self._home_all)
        w.btn_record.clicked.connect(self._record_point)
        w.btn_open.clicked.connect(self._open_program)
        w.btn_run.clicked.connect(lambda: ACTION.RUN(0))
        w.btn_step.clicked.connect(lambda: ACTION.STEP())
        w.btn_pause.clicked.connect(lambda: ACTION.PAUSE())
        w.btn_stop.clicked.connect(lambda: ACTION.ABORT())
        w.btn_mdi_go.clicked.connect(self._send_mdi)
        w.edit_mdi.returnPressed.connect(self._send_mdi)
        w.btn_grip_open.clicked.connect(lambda: self._mdi(self._ini("GRIP_OPEN", "M65 P0")))
        w.btn_grip_close.clicked.connect(lambda: self._mdi(self._ini("GRIP_CLOSE", "M64 P0")))

        STATUS.connect("state-estop", lambda w: self._machine_state())
        STATUS.connect("state-estop-reset", lambda w: self._machine_state())
        STATUS.connect("state-on", lambda w: self._machine_state())
        STATUS.connect("state-off", lambda w: self._machine_state())
        STATUS.connect("mode-manual", lambda w: self._mode_buttons())
        STATUS.connect("mode-auto", lambda w: self._mode_buttons())
        STATUS.connect("mode-mdi", lambda w: self._mode_buttons())
        STATUS.connect("motion-mode-changed", lambda w, mode: self._frame_buttons())
        STATUS.connect("file-loaded", lambda w, f: self._file_loaded(f))
        STATUS.connect("line-changed", lambda w, line: self._line_changed(line))
        STATUS.connect("interp-run", lambda w: self._cycle(True))
        STATUS.connect("interp-idle", lambda w: self._cycle(False))
        STATUS.connect("error", lambda w, kind, text: self._event(text.strip()))

    def _setup_panels(self):
        self.has_grip = hal_value("armgrip.0.state") is not None
        self.w.panel_grip.setVisible(self.has_grip)
        self.has_quota = hal_value("powerquota.0.total-power") is not None
        self.w.panel_quota.setVisible(self.has_quota)
        self.quota_max = float(self._ini("QUOTA_MAX", 2000))
        self.w.bar_quota.setMaximum(int(self.quota_max))
        self._set_increment(1.0, 1)
        self._set_jog_rate(self.w.slider_jog.value())
        self.w.btn_frame_world.setChecked(True)
        self.teach_file = self._ini("TEACH_FILE", os.path.join(self.config_dir, "teach.ngc"))
        self.w.lbl_subtitle.setText("LinuxCNC · %s · %d joints" % (self.ini.find("KINS", "KINEMATICS") or "", self.njoints))
        self._machine_state()
        self._mode_buttons()

    # -------------------------------------------------------------- actions
    def _toggle_estop(self):
        ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())  # clear -> estop, estop -> reset

    def _set_jog_mode(self, teach):
        ACTION.SET_MANUAL_MODE()
        self.teach = teach
        self.w.btn_record.setEnabled(teach)
        self._mode_buttons()

    def _set_increment(self, inc, idx):
        for i in range(4):
            getattr(self.w, "btn_inc_%d" % i).setChecked(i == idx)
        self.w.btn_inc_cont.setChecked(idx < 0)
        text = "%g" % inc if inc else "Continuous"
        ACTION.SET_JOG_INCR(inc, text)
        ACTION.SET_JOG_INCR_ANGULAR(inc, text)

    def _set_jog_rate(self, pct):
        lin = float(self.ini.find("DISPLAY", "MAX_LINEAR_VELOCITY") or 250) * 60
        ang = float(self.ini.find("DISPLAY", "MAX_ANGULAR_VELOCITY") or 90) * 60
        ACTION.SET_JOG_RATE(lin * pct / 100.0)
        ACTION.SET_JOG_RATE_ANGULAR(ang * pct / 100.0)
        self.w.lbl_jog_speed.setText("%d%%" % pct)

    def _jog(self, j, direction):
        if not STATUS.machine_is_on():
            return
        if direction and not STATUS.is_man_mode():
            ACTION.SET_MANUAL_MODE()
        if STATUS.is_joint_mode():
            ACTION.DO_JOG(j, direction)
        else:
            ACTION.DO_JOG(AXES[j], direction)

    def _jog_release(self, j):
        # a continuous jog runs while the button is held; an incremental one
        # finishes its step (a stop here would cut it short)
        if STATUS.get_jog_increment() == 0:
            self._jog(j, 0)

    def _home_all(self):
        ACTION.SET_MACHINE_HOMING(-1)

    def _record_point(self):
        p = STATUS.stat.actual_position
        line = "G1 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f F%d\n" % (p[0], p[1], p[2], p[3], p[4], p[5],
                                                               int(STATUS.get_jograte() or 1000))
        try:
            with open(self.teach_file, "a") as f:
                f.write(line)
            self.teach_points += 1
            self.w.lbl_points.setText("%d points" % self.teach_points)
            self._event("recorded %s" % line.strip())
        except OSError as e:
            self._event("cannot write %s: %s" % (self.teach_file, e))

    def _open_program(self):
        fn, _ = QtWidgets.QFileDialog.getOpenFileName(None, "Open program", self.config_dir, "G-code (*.ngc *.nc *.tap);;All (*)")
        if fn:
            ACTION.OPEN_PROGRAM(fn)

    def _send_mdi(self):
        text = self.w.edit_mdi.text().strip()
        if text:
            self._mdi(text)
            self.w.edit_mdi.clear()

    def _mdi(self, code):
        if not STATUS.machine_is_on():
            self._event("machine is off: %s not sent" % code)
            return
        ACTION.CALL_MDI(code)
        self._event("MDI %s" % code)

    # --------------------------------------------------------------- status
    def _machine_state(self):
        if not STATUS.estop_is_clear():
            state, text = "estop", "E-STOP"
        elif STATUS.machine_is_on():
            state, text = "on", "MACHINE ON"
        else:
            state, text = "off", "MACHINE OFF"
        restyle(self.w.lbl_state, state=state)
        self.w.lbl_state.setText(text)
        restyle(self.w.btn_estop, state=state)
        self.w.btn_estop.setText("RESET" if state == "estop" else "E-STOP")
        self.w.btn_power.setEnabled(state != "estop")
        self.w.btn_power.setChecked(state == "on")

    def _mode_buttons(self):
        man = STATUS.is_man_mode()
        teach = getattr(self, "teach", False)
        self.w.btn_mode_auto.setChecked(STATUS.is_auto_mode())
        self.w.btn_mode_mdi.setChecked(STATUS.is_mdi_mode())
        self.w.btn_mode_jog.setChecked(man and not teach)
        self.w.btn_mode_teach.setChecked(man and teach)
        self.w.btn_record.setEnabled(man and teach)

    def _frame_buttons(self):
        joint = STATUS.is_joint_mode()
        self.w.btn_frame_joint.setChecked(joint)
        self.w.btn_frame_world.setChecked(not joint)
        self.w.lbl_frame.setText("frame %s" % ("JOINT" if joint else "WORLD"))

    def _file_loaded(self, fn):
        self.w.lbl_program.setText(os.path.basename(fn))
        self.w.list_gcode.clear()
        try:
            with open(fn) as f:
                self.w.list_gcode.addItems([l.rstrip("\n") for l in f])
        except OSError:
            pass

    def _line_changed(self, line):
        n = self.w.list_gcode.count()
        if 0 < line <= n:
            self.w.list_gcode.setCurrentRow(line - 1)
            self.w.lbl_program.setText("%s · line %d / %d" % (os.path.basename(STATUS.stat.file or ""), line, n))

    def _cycle(self, running):
        if running and self.cycle_start is None:
            self.cycle_start = time.monotonic()
        elif not running and self.cycle_start is not None:
            self.cycle_time = time.monotonic() - self.cycle_start
            self.cycle_start = None

    def _event(self, text):
        if not text:
            return
        self.events = ([time.strftime("%H:%M:%S ") + text] + self.events)[:3]
        self.w.lbl_events.setText("   ".join(self.events))

    def _update(self):
        try:
            self._update_all()
        except Exception as e:  # noqa: BLE001  one bad pin must not freeze the screen
            if not getattr(self, "_update_failed", False):
                import traceback
                LOG.error("update: %s" % traceback.format_exc())
                self._event("screen update: %r" % e)
                self._update_failed = True

    def _update_all(self):
        s = STATUS.stat
        self._machine_state()
        # joints
        for j, (bar, pos, vel, ferr) in enumerate(self.joint_rows):
            js = s.joint[j]
            p = js["input"]
            bar.setValue(int(p * 10))
            pos.setText("%.2f" % p)
            vel.setText("%.1f" % js["velocity"])
            fe = abs(js["ferror_current"])
            ferr.setText("%.3f" % fe)
            warn = fe > 0.5 * max(js["max_ferror"], 1e-9)
            if ferr.property("warn") != warn:
                restyle(ferr, warn=warn)
            minus, plus, label, jpos = self.jog_buttons[j]
            if STATUS.is_joint_mode():
                label.setText("J%d" % (j + 1))
                jpos.setText("%.2f°" % p)
            else:
                label.setText(AXES[j])
                jpos.setText("%.1f" % s.actual_position[j])
        # pose
        for i, a in enumerate(AXES):
            getattr(self.w, "lbl_" + a.lower()).setText("%.3f" % s.actual_position[i] if i < 3 else "%.2f" % s.actual_position[i])
        self.w.lbl_tcp_speed.setText("TCP %.0f mm/s" % (s.current_vel))
        self.w.lbl_tool.setText("tool T%d" % s.tool_in_spindle)
        # cycle
        t = time.monotonic() - self.cycle_start if self.cycle_start is not None else self.cycle_time
        self.w.lbl_cycle.setText(time.strftime("%H:%M:%S", time.gmtime(t)))
        # drives
        faults = 0
        for r, name in enumerate(self.drive_rows):
            pre = "stmbl-arm.0.grip" if name == "GRIP" else self.drive_pins.format(j=r)
            cur, pw, flt = hal_value(pre + ".current"), hal_value(pre + ".power"), hal_value(pre + ".fault", False)
            en = hal_value(pre + ".enable")
            self._cell(r, 1, "-" if cur is None else "%.2f" % abs(cur))
            self._cell(r, 2, "-" if pw is None else "%.0f" % pw)
            self._cell(r, 3, "FAULT" if flt else "ENABLED" if en else "OFF")
            faults += bool(flt)
        self.w.lbl_safe_drives.setText("%d" % faults if faults else "none")
        # the guard (armguard): acting (state != 0, with its reason), near
        # (below 2x the cell's prevent limit) or armed; clearances in mm
        gstate, clear = hal_value("armguard.0.state"), hal_value("armguard.0.clearance")
        if gstate is not None and clear is not None:
            reason = GUARD_REASON.get(hal_value("armguard.0.reason", 0), "stop")
            if gstate:
                key, text = "stop", "GUARD: " + reason.upper()
            elif self.prevent_mm > 0 and clear < 2 * self.prevent_mm:
                key, text = "slow", "GUARD: NEAR"
            else:
                key, text = "ok", "GUARD: ARMED" if self.prevent_mm > 0 else "GUARD: ON"
            self.w.lbl_guard.setText(text)
            if self.w.lbl_guard.property("state") != key:
                restyle(self.w.lbl_guard, state=key)
            cs = "> 1 m" if clear > 1000 else "%.0f mm" % clear
            self.w.lbl_clear.setText("clearance " + cs)
            self.w.lbl_safe_guard.setText(text.split(": ")[1].lower())
            self.w.lbl_safe_stop.setText("%.0f mm" % self.prevent_mm if self.prevent_mm else "off")
            if self.last_guard is not None and key != self.last_guard and key != "ok":
                self._event("guard: %s, clearance %s" % (text.split(": ")[1].lower(), cs))
            self.last_guard = key
        coll = hal_value("armcollide.0.collision")
        self.w.lbl_safe_collide.setText("-" if coll is None else "COLLISION" if coll else "armed")
        # gripper
        if self.has_grip:
            gs = hal_value("armgrip.0.state")
            self.w.lbl_grip_state.setText("-" if gs is None else GRIP_STATES.get(gs, str(gs)))
            gap = hal_value("armgrip.0.gap-fb")  # mm
            self.w.lbl_grip_width.setText("-" if gap is None else "%.1f mm" % gap)
            cur = hal_value("stmbl-arm.0.grip.current")
            self.w.lbl_grip_current.setText("-" if cur is None else "%.2f A" % abs(cur))
            m = hal_value("armgrip.0.payload-mass")
            self.w.lbl_grip_payload.setText("-" if m is None else "%.2f kg" % m)
            closed = bool(hal_value("armgrip.0.close", False))
            self.w.btn_grip_close.setChecked(closed)
            self.w.btn_grip_open.setChecked(not closed)
        # power quota
        if self.has_quota:
            total = hal_value("powerquota.0.total-power", 0.0)
            self.w.bar_quota.setValue(int(min(max(total, 0), self.quota_max)))
            self.w.lbl_quota_power.setText("%.0f W of %.0f W" % (total, self.quota_max))
            self.w.lbl_quota_feed.setText("feed %.0f %%" % (100 * hal_value("powerquota.0.feed-scale", 1.0)))
            limiting = bool(hal_value("powerquota.0.limiting", False))
            if self.w.bar_quota.property("state") != ("limiting" if limiting else ""):
                restyle(self.w.bar_quota, state="limiting" if limiting else "")
        # footer
        info = self.view.info if self.view is not None and getattr(self.view, "info", None) else (self.view.error if self.view else "no 3D view")
        self.w.lbl_rt.setText("%d joints · %s" % (self.njoints, info))

    def _cell(self, r, c, text):
        it = self.w.table_drives.item(r, c)
        if it.text() != text:
            it.setText(text)

    # ------------------------------------------------------ QtVCP plumbing
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


def get_handlers(halcomp, widgets, paths):
    return [HandlerClass(halcomp, widgets, paths)]
