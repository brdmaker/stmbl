"""stmbl drives' firmware versions and flash configs, for the console
(docs/firmware_versions.md): what the console expects of a drive, and jobs
that read, back up, patch and restore a drive's config over its console.

A drive's versions come from its `version` command (drives.py). The console
expects the firmware of its own checkout (shared/version_number.h,
STMBL_FW_VERSION; [MILLWEB] DRIVE_FW to say otherwise): another one, or an
HV board running another firmware than the drive carries, is a problem that
keeps the machine off ([MILLWEB] DRIVE_FW_BLOCK = no: only said).

Writing a config goes as servoterm-cli --load does: deleteconf, appendconf
a line at a time (each answered "adding …"), flashsaveconf, then showconf to
compare. Every write keeps the config that was there in drive-backups/<drive>/
first; Restore writes a backup back. The drive runs a written config from its
next start (Reset, or power).
"""
import asyncio
import datetime
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))  # the repo: webui/mill/server -> .
sys.path.insert(0, os.path.join(ROOT, "tools"))
try:
    import stmblconf  # noqa: E402  tools/stmblconf.py: the patches
except ImportError:  # a console installed without the firmware tree
    stmblconf = None

LINE_MAX = 63 - len("appendconf ")  # the drive's console takes 63 characters a line


def expected_fw(cfg):
    """The firmware version this console expects of its drives."""
    v = cfg.get("MILLWEB", "DRIVE_FW")
    if v:
        return int(v)
    return stmblconf.fw_version(ROOT) if stmblconf else None


def patches_between(a, b):
    """The patches with something to change from config version a up to b."""
    if not stmblconf:
        return []
    ps = stmblconf.load_patches()
    return [ps[v] for v in range(a + 1, b + 1) if v in ps and ps[v].get("ops")]


def problems(ver, want):
    """A drive's version problems: [{level, text, block, action}]."""
    if not ver or want is None:
        return []
    out = []
    fw, hv, img, conf = ver.get("fw"), ver.get("hv"), ver.get("hv_image"), ver.get("conf")
    if fw == 0:
        out.append({"level": "bad", "block": True, "action": "flash",
                    "text": "firmware from before versioning; this console expects v%d: flash the drive (make btburn at this checkout)" % want})
    elif fw is not None and fw < want:
        out.append({"level": "bad", "block": True, "action": "flash",
                    "text": "firmware v%d; this console expects v%d: flash the drive (make btburn at this checkout)" % (fw, want)})
    elif fw is not None and fw > want:
        out.append({"level": "bad", "block": True, "action": "flash",
                    "text": "firmware v%d is newer than this console's v%d: update the console, or flash v%d" % (fw, want, want)})
    if fw and hv is not None and hv >= 0 and img is not None and img >= 0 and hv != img:
        out.append({"level": "bad", "block": True, "action": "hv_update",
                    "text": "the HV board runs v%d, the drive carries v%d: hv_update (until then it won't enable)" % (hv, img)})
    if fw and conf is not None:
        if conf > fw:
            out.append({"level": "bad", "block": True, "action": "patch",
                        "text": "config for v%d on firmware v%d: it may use what this firmware lacks; patch it down, or restore a backup" % (conf, fw)})
        elif conf < fw:
            need = patches_between(conf, fw)
            out.append({"level": "bad" if need else "warn", "block": bool(need), "action": "patch",
                        "text": ("config for v%d, firmware v%d: %d patch%s to apply (Patch config)" % (conf, fw, len(need), "" if len(need) == 1 else "es")) if need
                        else ("config %s, firmware v%d: Patch config gives it its version line (nothing else changes)" % ("from before versioning" if conf == 0 else "for v%d" % conf, fw))})
    return out


# ------------------------------------------------------------------ talking to the drive
async def collect(link, cmd, quiet=1.2, timeout=30.0, until=None):
    """Send cmd; the lines the drive answers: until a line matches `until`, or it
    has said something and then nothing for `quiet` s, or timeout."""
    link.capture = []
    err = link.user(cmd)
    if err:
        link.capture = None
        raise RuntimeError(err)
    loop = asyncio.get_event_loop()
    t0 = loop.time()
    try:
        while loop.time() - t0 < timeout:
            await asyncio.sleep(0.05)
            got = link.capture
            if until and any(re.search(until, l) for l in got):
                return list(got)
            if got and (asyncio.get_event_loop().time() - t0) > 0.2 and _quiet(link) > quiet:
                return list(got)
        if until:
            raise RuntimeError("%s: no answer like %r in %d s" % (cmd, until, timeout))
        return list(link.capture)
    finally:
        link.capture = None


def _quiet(link):
    import time
    return time.time() - link.last_rx


def clean(text):
    """A config as the drive keeps it: its lines, trailing spaces and empty lines gone."""
    return "\n".join(l.rstrip() for l in text.replace("\r", "").splitlines() if l.strip()) + "\n"


# what the drive says on its own, which can come between a config's lines (it is never a config line)
CHATTER = re.compile(r"^(> |(<[^>]*>)*(INFO|WARNING|ERROR):|version fw=|hv_update:|trigger fault|not found: )")


async def read_conf(link):
    lines = await collect(link, "showconf", quiet=1.5, timeout=60)
    return clean("\n".join(l for l in lines if not CHATTER.match(l)))


async def write_conf(link, text, say=lambda s: None):
    """Write text as the drive's flash config and check it; raises RuntimeError."""
    lines = clean(text).splitlines()
    long = [l for l in lines if len(l) > LINE_MAX]
    if long:
        raise RuntimeError("lines longer than %d characters (the drive's console drops them): %s" % (LINE_MAX, long[0][:40]))
    await collect(link, "deleteconf", quiet=0.3, timeout=5)
    for i, l in enumerate(lines):
        await collect(link, "appendconf " + l, until=r"^adding ", timeout=5)
        if i % 10 == 9:
            say("%d of %d lines" % (i + 1, len(lines)))
    got = await collect(link, "flashsaveconf", until=r"bytes written|error", timeout=20)
    bad = next((l for l in got if "error" in l), None)
    if bad:
        raise RuntimeError("flashsaveconf: %s" % bad)
    back = await read_conf(link)
    if back != clean(text):
        raise RuntimeError("the flash doesn't read back as written (%d lines written, %d read)" % (len(lines), len(back.splitlines())))


# ------------------------------------------------------------------ backups
def backup_dir(base, name):
    return os.path.join(base, "drive-backups", re.sub(r"[^A-Za-z0-9_.-]", "_", name))


def save_backup(base, name, text, ver, why):
    d = backup_dir(base, name)
    os.makedirs(d, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    fn = "%s-fw%s-conf%s-%s.conf" % (stamp, (ver or {}).get("fw", "x"), stmblconf.version_of(text) if stmblconf else "x", re.sub(r"[^a-z]", "", why)[:12])
    with open(os.path.join(d, fn), "w") as f:
        f.write(text)
    return fn


def list_backups(base, name):
    d = backup_dir(base, name)
    try:
        names = sorted((n for n in os.listdir(d) if n.endswith(".conf")), reverse=True)
    except FileNotFoundError:
        return []
    return [{"name": n, "size": os.path.getsize(os.path.join(d, n))} for n in names]


def read_backup(base, name, fn):
    if not re.match(r"^[0-9]{8}-[0-9]{6}-[A-Za-z0-9_-]+\.conf$", str(fn)):
        raise ValueError("no such backup")
    with open(os.path.join(backup_dir(base, name), fn)) as f:
        return f.read()
