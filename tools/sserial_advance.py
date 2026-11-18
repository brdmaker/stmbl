#!/usr/bin/env python3
"""sserial0.pos_advance from LinuxCNC's own following error.

The last commissioning step of a smart-serial axis (after id_pid and the
loop settings, and again after a firmware or loop change). From the
stmbl_freak fork (tools/sserial_advance.py); the fit is split out here so
host/tests/test_sserial_advance.py checks it on synthetic data.

The drive answers each frame with pos_fb + vel_fb * (pos_advance + its own
snapshot age). Whatever else delays the position LinuxCNC compares against
its command (the servo thread's read/write timing, the encoder transport,
the loop following its observer) shows up as a following error in
proportion to the speed. This fits

    ferror = k * vel + b * acc + c

over a run and prints sserial0.pos_advance = advance + k.

On the machine, LinuxCNC on and homed, the joint free to move:
    sserial_advance.py --joint 1 --advance 0.0006 --program /abs/move.ngc
or without --program, log for --time seconds while you jog or run a program
that moves the joint back and forth at a steady feed (cruise segments of a
few hundred ms at 50-100 % of the joint's speed fit best). --advance is the
drive's present sserial0.pos_advance (conf/template/sserial.txt: 0.0006).
Repeat with the new value: k should then come out near 0. Our open question
(DEBUGNOTES 2026-10-26): is 0.0006 right on a real drive?
"""
import argparse
import sys
import time


def fit(t, fe, ve, advance, min_vel=0.0):
    """Least squares ferror = k*vel + b*acc + c over the moving samples.
    Returns a dict (k, b, c, new, lines) or raises ValueError."""
    import numpy as np

    t, fe, ve = (np.asarray(x, float) for x in (t, fe, ve))
    if len(t) < 100:
        raise ValueError(f"only {len(t)} samples: did the joint move?")
    acc = np.gradient(ve, t)
    vmin = min_vel or 0.02 * np.abs(ve).max()
    m = np.abs(ve) > vmin
    if m.sum() < 50:
        raise ValueError("too few moving samples")
    X = np.c_[ve[m], acc[m], np.ones(m.sum())]
    co, *_ = np.linalg.lstsq(X, fe[m], rcond=None)
    res = fe[m] - X @ co
    k, b, c = co
    new = advance + k
    lines = [
        f"{len(t)} samples ({len(t) / (t[-1] - t[0]):.0f}/s), {m.sum()} moving, peak vel {np.abs(ve).max():.2f}",
        f"fit: ferror = {k * 1e6:+.1f} us * vel {b * 1e6:+.2f} us^2 * acc {c:+.6f}, residual rms {res.std():.6f}",
    ]
    cr = m & (np.abs(acc) < 0.05 * np.abs(acc).max())
    for sgn in (1, -1):
        s = cr & (np.sign(ve) == sgn)
        if s.sum():
            lines.append(f"cruise {'+' if sgn > 0 else '-'}: vel {ve[s].mean():+.2f}, "
                         f"ferror mean {fe[s].mean():+.6f} sd {fe[s].std():.6f}")
    lines.append(f"ferror moving: rms {np.sqrt((fe[m] ** 2).mean()):.6f}, max {np.abs(fe).max():.6f}")
    lines.append(f"sserial0.pos_advance = {new:.5f}  # was {advance:.5f}")
    if abs(k) < 20e-6:
        lines.append("change under 20 us: inside the run to run scatter, keep the present value")
    if new < 0 or new > 0.002:
        lines.append("warning: outside 0..2 ms, check the joint's scale and the feedback before using it")
    return {"k": k, "b": b, "c": c, "new": new, "lines": lines}


def log(args):
    import linuxcnc

    st = linuxcnc.stat()
    cmd = linuxcnc.command()
    st.poll()
    if st.task_state != linuxcnc.STATE_ON:
        sys.exit("machine is not on")
    rows, last = [], None

    def sample():
        nonlocal last
        st.poll()
        j = st.joint[args.joint]
        v = (j["ferror_current"], j["velocity"])
        if v != last:  # stat updates at the task rate: only new values
            rows.append((time.time(),) + v)
            last = v

    t0 = time.time()
    if args.program:
        if not all(st.homed[: st.joints]):
            sys.exit("not homed")
        cmd.mode(linuxcnc.MODE_AUTO)
        cmd.wait_complete()
        cmd.program_open(args.program)
        cmd.wait_complete()
        cmd.auto(linuxcnc.AUTO_RUN, 0)
        while time.time() - t0 < 0.5 or st.interp_state != linuxcnc.INTERP_IDLE:
            sample()
            time.sleep(0.0003)
            if time.time() - t0 > 600:
                sys.exit("program did not finish in 600 s")
    else:
        while time.time() - t0 < args.time:
            sample()
            time.sleep(0.0003)
    return rows


def main():
    ap = argparse.ArgumentParser(description="fit sserial0.pos_advance from LinuxCNC's following error")
    ap.add_argument("--joint", type=int, default=0)
    ap.add_argument("--advance", type=float, required=True, help="the drive's present sserial0.pos_advance [s]")
    ap.add_argument("--time", type=float, default=60.0, help="logging time [s] without --program")
    ap.add_argument("--program", help="absolute path of a program to run in auto mode while logging")
    ap.add_argument("--min-vel", type=float, default=0.0,
                    help="fit only above this speed [units/s] (default 2 %% of the peak)")
    args = ap.parse_args()
    rows = log(args)
    try:
        r = fit([x[0] for x in rows], [x[1] for x in rows], [x[2] for x in rows], args.advance, args.min_vel)
    except ValueError as e:
        sys.exit(str(e))
    print(f"joint {args.joint}: " + "\n".join(r["lines"]))


if __name__ == "__main__":
    main()
