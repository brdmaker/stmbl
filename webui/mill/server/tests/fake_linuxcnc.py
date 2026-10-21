"""A stand-in for LinuxCNC's `linuxcnc` module: enough of stat, command and
error_channel for millweb's tests. One shared machine; moves complete at once."""
import threading

STATE_ESTOP, STATE_ESTOP_RESET, STATE_OFF, STATE_ON = 1, 2, 3, 4
MODE_MANUAL, MODE_AUTO, MODE_MDI = 1, 2, 3
INTERP_IDLE, INTERP_READING, INTERP_PAUSED, INTERP_WAITING = 1, 2, 3, 4
TRAJ_MODE_FREE, TRAJ_MODE_COORD, TRAJ_MODE_TELEOP = 1, 2, 3
JOG_STOP, JOG_CONTINUOUS, JOG_INCREMENT = 0, 1, 2
AUTO_RUN, AUTO_PAUSE, AUTO_RESUME, AUTO_STEP = 0, 1, 2, 3
SPINDLE_FORWARD, SPINDLE_REVERSE, SPINDLE_OFF = 1, -1, 0
FLOOD_ON, FLOOD_OFF, MIST_ON, MIST_OFF = 1, 0, 1, 0
NML_ERROR, NML_TEXT, NML_DISPLAY, OPERATOR_ERROR, OPERATOR_TEXT, OPERATOR_DISPLAY = 1, 2, 3, 11, 12, 13


class _M:
    def __init__(self):
        self.lock = threading.Lock()
        self.reset()

    def reset(self):
        self.task_state = STATE_ESTOP
        self.task_mode = MODE_MANUAL
        self.interp_state = INTERP_IDLE
        self.motion_mode = TRAJ_MODE_FREE
        self.homed = [0, 0, 0, 0]
        self.pos = [0.0] * 9
        self.g5x = [0.0] * 9
        self.g5x_index = 1
        self.g92 = [0.0] * 9
        self.file = ""
        self.paused = False
        self.feedrate = 1.0
        self.rapidrate = 1.0
        self.max_velocity = 100.0
        self.spindle = {"speed": 0.0, "direction": 0, "enabled": 0, "override": 1.0}
        self.flood = self.mist = 0
        self.tool = 0
        self.jogging = {}
        self.mdi_log = []
        self.errors = []
        self.probe_tripped = 0
        self.probed = [0.0] * 9
        self.optional_stop = 0
        self.block_delete = 0


M = _M()


class stat:
    def poll(self):
        m = M
        self.task_state = m.task_state
        self.task_mode = m.task_mode
        self.interp_state = m.interp_state
        self.motion_mode = m.motion_mode
        self.homed = tuple(m.homed) + (0,) * 5
        self.joints = 4
        self.joint = [{"ferror_current": 0.0}] * 4
        self.actual_position = tuple(m.pos)
        self.position = tuple(m.pos)
        self.g5x_offset = tuple(m.g5x)
        self.g5x_index = m.g5x_index
        self.g92_offset = tuple(m.g92)
        self.tool_offset = (0.0,) * 9
        self.rotation_xy = 0.0
        self.dtg = (0.0,) * 9
        self.current_line = 0
        self.motion_line = 0
        self.file = m.file
        self.paused = m.paused
        self.queue = 0
        self.current_vel = 0.0
        self.settings = (0, 0.0, 0.0)
        self.spindle = (dict(m.spindle),)
        self.feedrate = m.feedrate
        self.rapidrate = m.rapidrate
        self.max_velocity = m.max_velocity
        self.flood, self.mist = m.flood, m.mist
        self.optional_stop, self.block_delete = m.optional_stop, m.block_delete
        self.gcodes = (0, 10, 170, 210, 900, 940, 540, -1)
        self.mcodes = (0, 5, 9, -1)
        self.probe_tripped = m.probe_tripped
        self.probed_position = tuple(m.probed)
        self.tool_in_spindle = m.tool


class command:
    def state(self, s):
        if s == STATE_ESTOP:
            M.task_state = STATE_ESTOP
        elif s == STATE_ESTOP_RESET:
            M.task_state = STATE_OFF
        elif s in (STATE_ON, STATE_OFF):
            if M.task_state != STATE_ESTOP:
                M.task_state = s

    def mode(self, m):
        M.task_mode = m

    def wait_complete(self, t=5.0):
        return 1

    def teleop_enable(self, on):
        M.motion_mode = TRAJ_MODE_TELEOP if on else TRAJ_MODE_FREE

    def home(self, j):
        if j < 0:
            M.homed = [1, 1, 1, 1]
        else:
            M.homed[j] = 1

    def unhome(self, j):
        M.homed[j] = 0

    def jog(self, kind, joint_flag, axis, vel=0.0, inc=0.0):
        if kind == JOG_INCREMENT:
            M.pos[axis] += inc * (1 if vel > 0 else -1)
        elif kind == JOG_CONTINUOUS:
            M.jogging[axis] = vel
        else:
            M.jogging.pop(axis, None)

    def mdi(self, text):
        M.mdi_log.append(text)
        import re
        t = text.upper()
        if re.match(r"^G[01]\b", t):
            for a, v in re.findall(r"([XYZ])\s*(-?\d*\.?\d+)", t):
                i = "XYZ".index(a)
                M.pos[i] = float(v) + M.g5x[i]
        m = re.match(r"^G10 L20 P0 ([XYZ])(-?[\d.]+)", t)
        if m:
            i = "XYZ".index(m.group(1))
            M.g5x[i] = M.pos[i] - float(m.group(2))
        m = re.match(r"^G5([4-9])$", t)
        if m:
            M.g5x_index = int(m.group(1)) - 3
        if "G38.2" in t:
            M.probe_tripped = 1
            M.probed = list(M.pos)
            M.probed[2] = -12.5
        if t.startswith("T") and "M6" in t:
            M.tool = int(re.match(r"T(\d+)", t).group(1))

    def auto(self, kind, line=0):
        if kind == AUTO_RUN:
            M.interp_state = INTERP_READING
        elif kind == AUTO_PAUSE:
            M.paused = True
        elif kind == AUTO_RESUME:
            M.paused = False

    def abort(self):
        M.interp_state = INTERP_IDLE
        M.paused = False

    def program_open(self, path):
        M.file = path

    def feedrate(self, v): M.feedrate = v
    def rapidrate(self, v): M.rapidrate = v
    def maxvel(self, v): M.max_velocity = v
    def spindleoverride(self, v, n=0): M.spindle["override"] = v

    def spindle(self, d, speed=0.0, n=0):
        M.spindle.update(direction=d, speed=speed if d else 0.0, enabled=int(d != 0))

    def flood(self, on): M.flood = on
    def mist(self, on): M.mist = on
    def set_optional_stop(self, on): M.optional_stop = int(on)
    def set_block_delete(self, on): M.block_delete = int(on)
    def load_tool_table(self): pass


class error_channel:
    def poll(self):
        return M.errors.pop(0) if M.errors else None
