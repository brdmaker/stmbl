"""The machine folder as one .zip, and back: the console's Backup tab (Configure page).

make()      the folder as a zip: machine.json, custom.hal, tool table, linuxcnc.var, comp/, bitstreams/,
            models/, history/ and (programs=True) nc_files/; not run/, logs/ (written all the time) or
            backups/ (the restores' safety copies). millweb-backup.json in it says what it is.
upload()    a zip from the browser, in pieces (a WebSocket message is at most 8 MB), into run/restore.part;
            the last piece answers what restoring it would change (inspect()).
apply()     the restore itself, by the supervisor while LinuxCNC is down (LinuxCNC writes linuxcnc.var
            when it ends): the folder as it is now goes into backups/ first (the last KEEP), then every
            file of the zip is written and the files it doesn't have are removed (nc_files/ only when the
            zip has programs: one made without them leaves the machine's programs alone).
"""
import io
import json
import os
import re
import shutil
import time
import zipfile
import zlib

MANIFEST = "millweb-backup.json"
SKIP = ("run", "logs", "backups")   # never in a backup, never touched by a restore
PROGRAMS = "nc_files"
MAX = 512 * 1024 * 1024
KEEP = 5


class Error(Exception):
    pass


def files(mdir, programs=True):
    """the folder's files (relative paths, "/"), the ones a backup holds"""
    out = []
    for root, dirs, names in os.walk(mdir):
        rel = os.path.relpath(root, mdir).replace(os.sep, "/")
        if rel == ".":
            dirs[:] = [d for d in dirs if d not in SKIP and not d.startswith(".") and (programs or d != PROGRAMS)]
        dirs.sort()
        for n in sorted(names):
            if n.startswith(".") or n.endswith(".tmp") or (rel == "." and n == MANIFEST):
                continue
            out.append(n if rel == "." else "%s/%s" % (rel, n))
    return out


def system_version():
    try:
        with open("/etc/ebaz-release.json") as f:
            return json.load(f).get("version")
    except (OSError, ValueError):
        return None


def machine_name(text):
    try:
        return str(json.loads(text).get("name") or "machine")
    except (ValueError, AttributeError):
        return "machine"


def make(mdir, programs=True):
    """(file name, zip bytes)"""
    try:
        with open(os.path.join(mdir, "machine.json"), encoding="utf-8") as f:
            name = machine_name(f.read())
    except OSError:
        name = "machine"
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(MANIFEST, json.dumps({"format": 1, "machine": name, "made": time.strftime("%Y-%m-%d %H:%M:%S"),
                                         "system": system_version(), "programs": bool(programs)}, indent=1) + "\n")
        for rel in files(mdir, programs):
            z.write(os.path.join(mdir, rel), rel)
    slug = re.sub(r"[^A-Za-z0-9]+", "-", name).strip("-").lower() or "machine"
    return "%s-%s.zip" % (slug, time.strftime("%Y%m%d-%H%M")), buf.getvalue()


def _members(z):
    """{relative path in the machine folder: ZipInfo}, the zip's manifest, and whether it has programs.
    A zip of the folder itself (machine/machine.json, as Windows' "Send to: compressed folder" makes it) works too."""
    names = [i for i in z.infolist() if not i.is_dir()]
    roots = [i.filename for i in names if i.filename.replace("\\", "/").split("/")[-1] == "machine.json"]
    prefix = None
    for r in sorted(roots, key=lambda x: x.count("/")):
        if r.count("/") <= 1:
            prefix = r[:-len("machine.json")]
            break
    if prefix is None:
        raise Error("no machine.json in it: not a backup of a machine folder")
    out, manifest = {}, {}
    for i in names:
        n = i.filename.replace("\\", "/")
        if not n.startswith(prefix):
            continue
        rel = n[len(prefix):]
        if rel == MANIFEST:
            try:
                manifest = json.loads(z.read(i).decode())
            except ValueError:
                pass
            continue
        parts = rel.split("/")
        if rel.startswith("/") or ".." in parts or ":" in rel or any(p == "" for p in parts):
            raise Error("a file name the restore won't write: %s" % n)
        if parts[0] in SKIP or parts[-1].startswith(".") or (i.external_attr >> 16) & 0o170000 == 0o120000:
            continue  # run/, logs/, backups/, hidden files, links: not part of the machine
        out[rel] = i
    programs = any(r.split("/")[0] == PROGRAMS for r in out) or bool(manifest.get("programs"))
    return out, manifest, programs


def _crc(path):
    c = 0
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            c = zlib.crc32(b, c)
    return c


def inspect(mdir, path, check=None):
    """what restoring the zip at path would do. check(text, custom) -> problems, if given"""
    try:
        z = zipfile.ZipFile(path)
    except (zipfile.BadZipFile, OSError) as e:
        raise Error("not a zip file (%s)" % e)
    with z:
        mem, manifest, programs = _members(z)
        if sum(i.file_size for i in mem.values()) > MAX:
            raise Error("over %d MB unpacked" % (MAX >> 20))
        new, changed, same = [], [], 0
        for rel, i in sorted(mem.items()):
            p = os.path.join(mdir, rel)
            if not os.path.isfile(p):
                new.append(rel)
            elif os.path.getsize(p) != i.file_size or _crc(p) != i.CRC:
                changed.append(rel)
            else:
                same += 1
        removed = [r for r in files(mdir, programs) if r not in mem]
        text = z.read(mem["machine.json"]).decode("utf-8", "replace")
        custom = z.read(mem["custom.hal"]).decode("utf-8", "replace") if "custom.hal" in mem else ""
    problems = check(text, custom) if check else []
    return {"machine": manifest.get("machine") or machine_name(text), "made": manifest.get("made"),
            "system": manifest.get("system"), "programs": programs, "files": len(mem),
            "size": sum(i.file_size for i in mem.values()), "new": new, "changed": changed, "removed": removed,
            "same": same, "problems": problems}


def part_path(mdir):
    return os.path.join(mdir, "run", "restore.part")


def pending_path(mdir):
    return os.path.join(mdir, "run", "restore.zip")


def upload(mdir, offset, data, total):
    """one piece of a zip; None until the last, then the zip's path"""
    p = part_path(mdir)
    if not 0 < total <= MAX:
        raise Error("a backup is 1 byte to %d MB" % (MAX >> 20))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    have = os.path.getsize(p) if offset and os.path.exists(p) else 0
    if offset != have:
        raise Error("the upload got out of step (at %d, not %d): try again" % (have, offset))
    with open(p, "wb" if offset == 0 else "ab") as f:
        f.write(data)
    size = offset + len(data)
    if size > total:
        raise Error("more than the %d bytes announced" % total)
    return p if size == total else None


def safety_copy(mdir):
    """the folder as it is, into backups/ (the last KEEP of them): a restore can be undone with it"""
    d = os.path.join(mdir, "backups")
    os.makedirs(d, exist_ok=True)
    _, data = make(mdir, programs=True)
    p = os.path.join(d, "before-restore-%s.zip" % time.strftime("%Y%m%d-%H%M%S"))
    with open(p + ".tmp", "wb") as f:
        f.write(data)
    os.replace(p + ".tmp", p)
    old = sorted(n for n in os.listdir(d) if n.startswith("before-restore-") and n.endswith(".zip"))
    for n in old[:-KEEP]:
        os.remove(os.path.join(d, n))
    return p


def apply(mdir, path):
    """restore the zip at path into the folder (LinuxCNC down); a line for the event log"""
    with zipfile.ZipFile(path) as z:
        mem, manifest, programs = _members(z)
        copy = safety_copy(mdir)
        removed = [r for r in files(mdir, programs) if r not in mem]
        for rel, i in sorted(mem.items()):
            dst = os.path.join(mdir, *rel.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with z.open(i) as src, open(dst + ".tmp", "wb") as out:
                shutil.copyfileobj(src, out)
            os.replace(dst + ".tmp", dst)
            t = time.mktime(i.date_time + (0, 0, -1))
            os.utime(dst, (t, t))
    for rel in removed:
        os.remove(os.path.join(mdir, *rel.split("/")))
    # directories the restore emptied
    for root, dirs, names in os.walk(mdir, topdown=False):
        rel = os.path.relpath(root, mdir)
        if rel != "." and rel.split(os.sep)[0] not in SKIP and not os.listdir(root):
            os.rmdir(root)
    return "Restored %s (%s%s): %d files, %d removed; the folder before it: backups/%s" % (
        manifest.get("machine") or "the backup", manifest.get("made") or "undated",
        "" if programs else ", programs kept", len(mem), len(removed), os.path.basename(copy))
