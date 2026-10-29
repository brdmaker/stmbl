"""The machine's own HAL components: comp/*.comp in the machine folder, compiled on the controller.

Before every LinuxCNC start the supervisor calls build(): each comp/NAME.comp is compiled with halcompile
(an RT component into NAME.so, one with `option userspace` into the program NAME), unless the cache has it
(run/comp-cache: keyed by the source and the LinuxCNC it was built for, so an update of the controller
compiles them again, once). Then they are installed: an RT module into LinuxCNC's modules directory
(`loadrt NAME` in custom.hal), a userspace program into run/comp-bin (on LinuxCNC's PATH: `loadusr NAME`).

LinuxCNC's modules directory must be writable for it (the EBAZ4205 image: a tmpfs of links to LinuxCNC's
own modules, bound over /usr/lib/linuxcnc/modules; MILLWEB_RTLIB_STOCK names the originals, so a comp
removed from the folder brings back a LinuxCNC module of the same name). A compile error stops the start
like a bad machine.json: the first error line in the status, the whole output in the log.
"""
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

RTLIB = "/usr/lib/linuxcnc/modules"


class Error(Exception):
    pass


def rtlib():
    return os.environ.get("MILLWEB_RTLIB_DIR", RTLIB)


def halcompile():
    return os.environ.get("MILLWEB_HALCOMPILE", "halcompile")


def sources(mdir):
    return sorted(glob.glob(os.path.join(mdir, "comp", "*.comp")))


def userspace(text):
    return re.search(r"^\s*option\s+userspace\s+(yes\s*)?;", text, re.M) is not None


def toolchain():
    """what a compiled comp depends on: LinuxCNC's build files (their size and time change with an update)"""
    out = []
    for p in (shutil.which(halcompile()), "/usr/share/linuxcnc/Makefile.modinc", "/usr/include/linuxcnc/hal.h",
              os.path.join(os.environ.get("MILLWEB_RTLIB_STOCK", rtlib()), "hal_lib.so")):
        try:
            st = os.stat(p)
            out.append("%s %d %d" % (p, st.st_size, int(st.st_mtime)))
        except (OSError, TypeError):
            out.append("%s -" % p)
    return "\n".join(out)


def compile_one(src, cache, log):
    """NAME.so or the program NAME for src, from the cache or compiled now: (path, kind)"""
    with open(src, encoding="utf-8", errors="replace") as f:
        text = f.read()
    name = os.path.splitext(os.path.basename(src))[0]
    usr = userspace(text)
    key = hashlib.sha256((text + "\0" + toolchain()).encode()).hexdigest()[:20]
    out = os.path.join(cache, "%s-%s" % (name, key), name if usr else name + ".so")
    if os.path.exists(out):
        return out, "userspace" if usr else "rt", False
    tmp = tempfile.mkdtemp(prefix="comp-")
    try:
        shutil.copy(src, tmp)  # halcompile writes next to the source: not into the machine folder
        log("== comp/%s.comp: compiling (%s)\n" % (name, "userspace" if usr else "realtime"))
        p = subprocess.run([halcompile(), "--compile", os.path.basename(src)], cwd=tmp, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, timeout=600)
        log(p.stdout)
        built = os.path.join(tmp, name if usr else name + ".so")
        if p.returncode != 0 or not os.path.exists(built):
            lines = [l.strip() for l in p.stdout.splitlines() if l.strip()]
            err = [l for l in lines if re.search(r"error|Error|unexpected|expected|undefined", l)]
            raise Error("comp/%s.comp: %s" % (name, (err or lines or ["halcompile: exit %d" % p.returncode])[0][:200]))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copy2(built, out + ".tmp")
        os.replace(out + ".tmp", out)
        return out, "userspace" if usr else "rt", True
    except FileNotFoundError:
        raise Error("comp/%s.comp: no halcompile on this controller (LinuxCNC's development files)" % name)
    except subprocess.TimeoutExpired:
        raise Error("comp/%s.comp: halcompile took over 10 min" % name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def build(mdir, log):
    """compile and install the machine's comps; returns a note for the event log, or None. Raises Error."""
    run = os.path.join(mdir, "run")
    cache, bindir, state = os.path.join(run, "comp-cache"), os.path.join(run, "comp-bin"), os.path.join(run, "comps.json")
    srcs = sources(mdir)
    try:
        with open(state) as f:
            before = json.load(f)
    except (OSError, ValueError):
        before = {}
    if not srcs and not before:
        return None
    lib, stock = rtlib(), os.environ.get("MILLWEB_RTLIB_STOCK")
    now, built, replaced = {}, [], []
    for src in srcs:
        path, kind, fresh = compile_one(src, cache, log)
        name = os.path.splitext(os.path.basename(src))[0]
        if fresh:
            built.append(name)
        if kind == "rt":
            dst = os.path.join(lib, name + ".so")
            if name not in before and (os.path.islink(dst) or os.path.exists(dst)):
                replaced.append(name)
            try:
                if os.path.islink(dst) or os.path.exists(dst):
                    os.remove(dst)
                shutil.copy2(path, dst)
            except OSError as e:
                raise Error("comp/%s.comp: can't install it into %s (%s)" % (name, lib, e.strerror))
        else:
            os.makedirs(bindir, exist_ok=True)
            shutil.copy2(path, os.path.join(bindir, name))
            os.chmod(os.path.join(bindir, name), 0o755)
        now[name] = kind
    # comps taken out of the folder: out of LinuxCNC too (and LinuxCNC's own module of that name back)
    for name, kind in before.items():
        if name in now:
            continue
        try:
            if kind == "rt":
                dst = os.path.join(lib, name + ".so")
                if os.path.exists(dst) and not os.path.islink(dst):
                    os.remove(dst)
                if stock and os.path.exists(os.path.join(stock, name + ".so")):
                    os.symlink(os.path.join(stock, name + ".so"), dst)
            else:
                os.remove(os.path.join(bindir, name))
        except OSError:
            pass
        log("== comp/%s.comp is gone: removed\n" % name)
    os.makedirs(run, exist_ok=True)
    with open(state + ".tmp", "w") as f:
        json.dump(now, f)
    os.replace(state + ".tmp", state)
    log("== comps: %s\n" % (", ".join("%s (%s)" % (n, k) for n, k in sorted(now.items())) or "none"))
    note = []
    if built:
        note.append("Compiled comp%s: %s" % ("s" if len(built) > 1 else "", ", ".join(built)))
    if replaced:
        note.append("replacing LinuxCNC's own %s" % ", ".join(replaced))
    return "; ".join(note) or None


def bin_dir(mdir):
    return os.path.join(mdir, "run", "comp-bin")
