"""dynwiz: identifying a machine's dynamics from recordings.

Pure Python (no numpy), so it runs in LinuxCNC's own python on a headless
controller. Every function takes plain lists sampled at a fixed dt [s].

  ringdown(y, dt, fmin, fmax)     a decaying oscillation: frequency, damping
  beam_fit(points, x_center, rails)  the head's y mode along the bridge
  mass_fit(rec, cmd, cur)         moving mass and friction from back-and-forth moves
  cogging_fit(pos, cur, ...)      force ripple against position
  circle_lag(cx, cy, fx, fy, ...) the servo's steady lag per acceleration (tau2)
  settle(...)                     residual vibration after a move
"""
import cmath
import math


def mean(v):
    return sum(v) / len(v) if v else 0.0


def solve(A, b):
    """Solve A x = b (small, dense) by Gaussian elimination with pivoting."""
    n = len(b)
    M = [list(A[i]) + [b[i]] for i in range(n)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(M[r][c]))
        if abs(M[p][c]) < 1e-300:
            raise ValueError("singular")
        M[c], M[p] = M[p], M[c]
        for r in range(n):
            if r != c:
                f = M[r][c] / M[c][c]
                for k in range(c, n + 1):
                    M[r][k] -= f * M[c][k]
    return [M[i][n] / M[i][i] for i in range(n)]


def lstsq(rows, y):
    """Least squares: the x that minimises |rows x - y|, by the normal equations."""
    n = len(rows[0])
    A = [[sum(r[i] * r[j] for r in rows) for j in range(n)] for i in range(n)]
    b = [sum(r[i] * v for r, v in zip(rows, y)) for i in range(n)]
    return solve(A, b)


def detrend(y, order=2):
    """y without its best polynomial of the given order (in the sample index)."""
    n = len(y)
    u = [2.0 * k / max(n - 1, 1) - 1.0 for k in range(n)]
    rows = [[uk ** j for j in range(order + 1)] for uk in u]
    c = lstsq(rows, y)
    return [yk - sum(cj * r for cj, r in zip(c, row)) for yk, row in zip(y, rows)]


def dft(y, dt, f):
    """The complex amplitude of y (already windowed) at f [Hz]."""
    w = -2j * math.pi * f * dt
    return sum(v * cmath.exp(w * k) for k, v in enumerate(y))


def spectrum_peak(y, dt, fmin, fmax):
    """The strongest frequency in [fmin, fmax]: (f, |A|, peak / median)."""
    n = len(y)
    win = [0.5 - 0.5 * math.cos(2 * math.pi * k / (n - 1)) for k in range(n)]
    yw = [a * b for a, b in zip(y, win)]
    df = 0.25 / (n * dt)  # a quarter of the resolution
    fs, mags = [], []
    f = fmin
    while f <= fmax:
        fs.append(f)
        mags.append(abs(dft(yw, dt, f)))
        f += df
    i = max(range(len(mags)), key=mags.__getitem__)
    # refine: golden section around the coarse peak
    a, b = fs[max(i - 1, 0)], fs[min(i + 1, len(fs) - 1)]
    g = (math.sqrt(5) - 1) / 2
    for _ in range(30):
        c, d = b - g * (b - a), a + g * (b - a)
        if abs(dft(yw, dt, c)) > abs(dft(yw, dt, d)):
            b = d
        else:
            a = c
    fp = 0.5 * (a + b)
    med = sorted(mags)[len(mags) // 2]
    peak = abs(dft(yw, dt, fp))
    # |A| of a sine: the Hann window's coherent gain is 0.5
    return fp, peak * 2.0 / (0.5 * n), peak / med if med > 0 else float("inf")


def spectrum_peaks(y, dt, fmin, fmax, n=3):
    """The strongest local maxima of y's (Hann-windowed) spectrum in
    [fmin, fmax], strongest first: [(f, |A|, peak / median)]. Peaks closer
    than 10 % to a stronger one are left out."""
    m = len(y)
    win = [0.5 - 0.5 * math.cos(2 * math.pi * k / (m - 1)) for k in range(m)]
    yw = [a * b for a, b in zip(y, win)]
    df = 0.25 / (m * dt)
    fs, mags = [], []
    f = fmin
    while f <= fmax:
        fs.append(f)
        mags.append(abs(dft(yw, dt, f)))
        f += df
    med = sorted(mags)[len(mags) // 2] or 1e-30
    loc = [i for i in range(1, len(mags) - 1) if mags[i] >= mags[i - 1] and mags[i] >= mags[i + 1]]
    loc.sort(key=lambda i: -mags[i])
    out = []
    for i in loc:
        if any(abs(fs[i] - g) < 0.1 * g for g, _, _ in out):
            continue
        a, b = fs[i - 1], fs[i + 1]
        g = (math.sqrt(5) - 1) / 2
        for _ in range(25):
            c, d = b - g * (b - a), a + g * (b - a)
            if abs(dft(yw, dt, c)) > abs(dft(yw, dt, d)):
                b = d
            else:
                a = c
        fp = 0.5 * (a + b)
        pk = abs(dft(yw, dt, fp))
        out.append((fp, pk * 2.0 / (0.5 * m), pk / med))
        if len(out) == n:
            break
    return out


def envelope(y, dt, f):
    """The amplitude of y's component at f over time: demodulated and
    averaged over one period, every half period: [(t, |A|)]."""
    per = max(int(round(1.0 / (f * dt))), 2)
    z = [v * cmath.exp(-2j * math.pi * f * dt * k) for k, v in enumerate(y)]
    out = []
    for s in range(0, len(z) - per, max(per // 2, 1)):
        out.append(((s + per / 2.0) * dt, 2.0 * abs(sum(z[s:s + per])) / per))
    return out


def ringdown(y, dt, fmin=5.0, fmax=200.0, f=None):
    """A decaying oscillation in y (taken after a move has ended): its
    frequency [Hz], damping ratio, initial amplitude (y's units) and how far
    the peak stands out of the spectrum (snr; below ~5 the result is noise).
    The slow servo settling is removed first (a quadratic). The strongest
    peak, or the one at f [Hz]; "peaks": the three strongest."""
    r = detrend(y, 2)
    peaks = spectrum_peaks(r, dt, fmin, fmax, 3)
    if f is None:
        f, amp, snr = peaks[0] if peaks else spectrum_peak(r, dt, fmin, fmax)
    else:  # a given peak: refined there; its snr against the whole band
        g = min(peaks, key=lambda p: abs(p[0] - f)) if peaks else None
        f, amp, snr = spectrum_peak(r, dt, f * 0.95, f * 1.05)
        snr = g[2] if g and abs(g[0] - f) < 0.05 * f else snr
    env = envelope(r, dt, f)
    # the decay: log|A| against t, while well above the noise (the
    # spectrum's median as an amplitude: amp / snr)
    a0 = max(a for _, a in env) if env else 0.0
    floor = amp / snr if snr > 0 else 0.0
    pts = []
    for t, a in env:
        if a < max(3.0 * floor, 0.1 * a0):
            break
        pts.append((t, math.log(a)))
    zeta = float("nan")
    if len(pts) >= 3:
        slope, icpt = lstsq([[t, 1.0] for t, _ in pts], [v for _, v in pts])
        sigma = -slope
        zeta = sigma / math.sqrt(sigma * sigma + (2 * math.pi * f) ** 2)
        a0 = math.exp(icpt)
    return {"f": f, "zeta": zeta, "amp": a0, "snr": snr, "n_decay": len(pts), "peaks": peaks}


def beam_fit(points, x_center, rails):
    """The head's y mode along the bridge: points [(X [mm], f [Hz])] fitted
    with the beam model pcomp uses,
        1/f^2 = 1/f_end^2 + (1/f_mid^2 - 1/f_end^2) (1 - u^2),  u = 2 (X - x_center) / rails,
    (rails in m, X in mm). (f_end, f_mid, rms error [Hz])."""
    rows, ys = [], []
    for X, f in points:
        u = 2.0 * (X - x_center) / (rails * 1000.0)
        rows.append([1.0, 1.0 - u * u])
        ys.append(1.0 / (f * f))
    if len(points) < 2:
        raise ValueError("beam_fit needs two positions or more")
    c0, c1 = lstsq(rows, ys)
    f_end = 1.0 / math.sqrt(c0) if c0 > 0 else float("nan")
    f_mid = 1.0 / math.sqrt(c0 + c1) if c0 + c1 > 0 else float("nan")
    err = math.sqrt(mean([(1.0 / math.sqrt(max(r[0] * c0 + r[1] * c1, 1e-12)) - f) ** 2 for r, (_, f) in zip(rows, points)]))
    return f_end, f_mid, err


def derivatives(p, dt):
    """Central differences: velocity and acceleration of p (same length)."""
    n = len(p)
    v = [0.0] * n
    a = [0.0] * n
    for k in range(1, n - 1):
        v[k] = (p[k + 1] - p[k - 1]) / (2 * dt)
        a[k] = (p[k + 1] - 2 * p[k] + p[k - 1]) / (dt * dt)
    return v, a


def mass_fit(cmd, cur, dt, vmin=5.0):
    """Current against the commanded motion: back-and-forth moves of one
    axis (cmd [mm], cur [A] of one or more motors summed), each from rest
    to rest.
        i = k_a a + k_c sign(v) + k_v v + i0
    k_a [A per m/s^2] (mass / Kf), k_c [A] (Coulomb friction / Kf),
    k_v [A s/m], i0 [A] (a constant push, gravity or a cable).
    Fitted as impulse: both sides summed over the moving samples (|v| >
    vmin [mm/s]; standing, friction holds whatever it must). A head ringing
    on its springs adds a force that swings both ways; summed, what is left
    of it is its momentum, which stays small, where sample by sample it
    would bias the mass (by its phase during the short accelerations)."""
    v, a = derivatives(cmd, dt)
    n = len(cmd)
    rows, ys = [], []
    acc = [0.0, 0.0, 0.0, 0.0]
    I = 0.0
    moving = 0
    for k in range(1, n - 1):
        if abs(v[k]) < vmin:
            continue
        moving += 1
        acc[0] += a[k] / 1000.0 * dt
        acc[1] += (1.0 if v[k] > 0 else -1.0) * dt
        acc[2] += v[k] / 1000.0 * dt
        acc[3] += dt
        I += cur[k] * dt
        rows.append(list(acc))
        ys.append(I)
    if moving < 50:
        raise ValueError("mass_fit: too few moving samples (%d)" % moving)
    x = lstsq(rows, ys)
    # how well the per-sample model then fits (the residual's rms) [A]
    res = []
    for k in range(1, n - 1):
        if abs(v[k]) >= vmin:
            res.append(cur[k] - (x[0] * a[k] / 1000 + x[1] * (1 if v[k] > 0 else -1) + x[2] * v[k] / 1000 + x[3]))
    rms = math.sqrt(mean([r * r for r in res]))
    return {"k_a": x[0], "k_c": x[1], "k_v": x[2], "i0": x[3], "rms": rms, "n": moving}


def cogging_fit(pos, cur, vel_sign, pmin=4.0, pmax=120.0):
    """The force ripple in the current of a slow constant-speed pass:
    pos [mm] (the scale), cur [A], vel_sign (+1/-1 per sample; a pass each
    way removes friction). The strongest period in [pmin, pmax] mm:
    (period [mm], amplitude [A], phase [deg]) with
        cur ~ amplitude sin(2 pi pos / period + phase)
    (the motor pushes against cogging -A sin(...): this is +A sin)."""
    # remove friction (sign), the mean and a slope (a cable chain's pull)
    rows = [[1.0, s, p] for p, s in zip(pos, vel_sign)]
    c = lstsq(rows, cur)
    r = [i - (c[0] + c[1] * s + c[2] * p) for p, s, i in zip(pos, vel_sign, cur)]

    def amp_at(per):  # a least-squares sine at this period (positions uneven)
        w = 2 * math.pi / per
        sc = lstsq([[math.sin(w * p), math.cos(w * p)] for p in pos], r)
        return sc

    best, bestp = -1.0, pmin
    per = pmin
    span = max(pos) - min(pos)
    while per <= min(pmax, span / 2):
        sc = amp_at(per)
        a = math.hypot(*sc)
        if a > best:
            best, bestp = a, per
        per *= 1.005
    # refine
    lo, hi = bestp / 1.01, bestp * 1.01
    for _ in range(40):
        m1, m2 = lo + (hi - lo) / 3, hi - (hi - lo) / 3
        if math.hypot(*amp_at(m1)) > math.hypot(*amp_at(m2)):
            hi = m2
        else:
            lo = m1
    per = 0.5 * (lo + hi)
    s, co = amp_at(per)
    amp = math.hypot(s, co)
    # s sin + c cos = A sin(x + ph): A cos ph = s, A sin ph = c
    ph = math.degrees(math.atan2(co, s))
    noise = math.sqrt(mean([v * v for v in r]))
    return {"period": per, "amp": amp, "phase": ph, "rms": noise}


def circle_lag(cx, cy, fx, fy, dt, center):
    """The servo's steady lag on a circle: the commanded and the scales'
    mean radius about center (X, Y [mm]), and tau2 = (r_cmd - r_fb) /
    a_centripetal [s^2] (what pcomp's servo term feeds forward)."""
    rc = [math.hypot(x - center[0], y - center[1]) for x, y in zip(cx, cy)]
    rf = [math.hypot(x - center[0], y - center[1]) for x, y in zip(fx, fy)]
    v, _ = derivatives(cx, dt)
    w, _ = derivatives(cy, dt)
    sp = [math.hypot(a, b) for a, b in zip(v, w)][1:-1]
    r_cmd, r_fb, speed = mean(rc), mean(rf), mean(sp)
    acc = speed * speed / r_cmd
    return {"r_cmd": r_cmd, "r_fb": r_fb, "speed": speed, "acc": acc, "tau2": (r_cmd - r_fb) / acc}


def moves(cmd, dt, eps=1e-6):
    """The moves in a command: [(start, end)] sample indices of the runs
    where it changes."""
    out, start = [], None
    for k in range(1, len(cmd)):
        moving = abs(cmd[k] - cmd[k - 1]) > eps
        if moving and start is None:
            start = k - 1
        if not moving and start is not None:
            out.append((start, k))
            start = None
    if start is not None:
        out.append((start, len(cmd) - 1))
    return out


def merge_moves(ms, gap):
    """Moves closer than `gap` samples joined (a move's own pauses, a
    circle's quadrants)."""
    out = []
    for s, e in ms:
        if out and s - out[-1][1] <= gap:
            out[-1] = (out[-1][0], e)
        else:
            out.append((s, e))
    return out


def settle(err, dt, end, tol, window=0.4):
    """After a move ends (sample `end`): the largest |err| in the window,
    and the time until it stays within tol (err in mm, tol in mm)."""
    seg = err[end:end + int(window / dt)]
    if not seg:
        return {"peak": 0.0, "t_settle": 0.0}
    t = 0.0
    for k in range(len(seg) - 1, -1, -1):
        if abs(seg[k]) > tol:
            t = (k + 1) * dt
            break
    return {"peak": max(abs(v) for v in seg), "t_settle": t}
