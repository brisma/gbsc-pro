#!/usr/bin/env python3
"""GBSC PRO development commands, the same natively (Windows, macOS, Linux) and in the container (compose.yaml).

  python tools/dev.py doctor                    what each suite needs: found, missing (with the install hint), or a
                                                version other than the pinned one
  python tools/dev.py check [SUITE...] [--full] [--strict] [--shard I/N]
                                                suites gen esp adv web flasher tools (default: all, in this order);
                                                --full: Playwright on every project; --strict: a skipped suite fails;
                                                --shard: only part I of N of the Playwright tests (parallel CI jobs)
  python tools/dev.py build [--esp | --adv]     default both (a GBSC PRO has both controllers), results in out/
  python tools/dev.py flash [--esp | --adv] [--port P | --host H [--allow-downgrade]]
                                                out/ with gbsc-flasher: USB (default) or WiFi; on the computer only
In the container: docker compose run --rm dev <doctor | check | build> ...
"""
import argparse
import glob
import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
FW = os.path.join(REPO, "gbs-control")
WEB = os.path.join(FW, "public")
ADV = os.path.join(REPO, "adv-controller")
FLASHER = os.path.join(REPO, "gbsc-pro-flasher")
UI = os.path.join(FLASHER, "ui")
OUT = os.path.join(REPO, "out")
sys.path.insert(0, HERE)
import release  # noqa: E402  (BIN: the PlatformIO build output; make_image: the OTA image format)

SUITES = ("gen", "esp", "adv", "web", "flasher", "tools")
IS_WIN = os.name == "nt"
IS_MAC = sys.platform == "darwin"
IN_CONTAINER = os.environ.get("GBSC_DEV_CONTAINER") == "1"
NODE_MAJOR = 26
NODE_MIN = (22, 18)      # TypeScript type stripping (node sim/main.ts, the unit tests)
ARM_GCC = "15.3.1"        # Arm GNU Toolchain 15.3.rel2 (its --version still prints "15.3.Rel1")
PLATFORMIO = "6.2.0"
OS_KEY = "win" if IS_WIN else "mac" if IS_MAC else "linux"

HINTS = {
    "python": "Python 3.11+ (https://www.python.org)",
    "node": {"win": "winget install OpenJS.NodeJS (Node 26)", "mac": "Node 26 from https://nodejs.org or brew install node",
             "linux": "Node 26 from https://nodejs.org"},
    "npm": "comes with Node.js",
    "pio": "pip install platformio==6.2.0",
    "pyyaml": "pip install pyyaml",
    "fonttools": "pip install fonttools brotli",
    "c++": {"win": "MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc, and <msys64>\\ucrt64\\bin on PATH",
            "mac": "xcode-select --install", "linux": "sudo apt install build-essential"},
    "cc": {"win": "MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc, and <msys64>\\ucrt64\\bin on PATH",
           "mac": "xcode-select --install", "linux": "sudo apt install build-essential"},
    "make": {"win": "MSYS2 (https://www.msys2.org): pacman -S make; MSYS2_ROOT when it is not in C:\\msys64",
             "mac": "xcode-select --install", "linux": "sudo apt install build-essential"},
    "arm-gcc": {"win": "Arm GNU Toolchain 15.3.rel2 (https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads), bin on PATH",
                "mac": "brew install --cask gcc-arm-embedded",
                "linux": "Arm GNU Toolchain 15.3.rel2 (https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads), bin on PATH"},
    "cargo": {"win": "https://rustup.rs, plus Visual Studio Build Tools (Desktop development with C++)",
              "mac": "https://rustup.rs", "linux": "https://rustup.rs"},
    "webkit2gtk": "sudo apt install pkg-config libwebkit2gtk-4.1-dev libxdo-dev libssl-dev "
                  "libayatana-appindicator3-dev librsvg2-dev libudev-dev",
    "browsers": "npx --prefix gbs-control/public playwright install --with-deps",
    "msvc": "Visual Studio Build Tools (https://visualstudio.microsoft.com/visual-cpp-build-tools/), "
            "workload Desktop development with C++",
    "gbsc-flasher": "cargo build --release --manifest-path gbsc-pro-flasher/Cargo.toml -p gbsc-flasher-cli",
}

NEEDS = {
    "gen": ["node", "npm", "pio", "c++", "pyyaml"],
    "esp": ["pio", "c++"],
    "adv": ["make", "cc"],
    "web": ["node", "npm", "fonttools", "browsers"],
    "flasher": ["node", "npm", "cargo", "fonttools"] + {"linux": ["webkit2gtk"], "win": ["msvc"]}.get(OS_KEY, []),
    "tools": ["pyyaml"],
    "build-esp": ["node", "npm", "pio", "c++", "pyyaml", "fonttools"],
    "build-adv": ["make", "arm-gcc"],
}


def hint(key):
    h = HINTS.get(key, "")
    return h.get(OS_KEY, "") if isinstance(h, dict) else h


def first_line(cmd, env=None):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60, env=env)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if r.returncode != 0:
        return None
    lines = (r.stdout or r.stderr or "").strip().splitlines()
    return lines[0].strip() if lines else ""


def msys_root():
    for d in (os.environ.get("MSYS2_ROOT"), r"C:\msys64"):
        if d and os.path.isfile(os.path.join(d, "usr", "bin", "bash.exe")):
            return d
    return None


def make_cmd(args):
    """make with its POSIX shell: MSYS2's bash on Windows (UCRT64 gcc, the Windows PATH kept), plain make elsewhere."""
    if not IS_WIN:
        return ["make", *args]
    bash = os.path.join(msys_root() or r"C:\msys64", "usr", "bin", "bash.exe")   # doctor names it when missing
    return [bash, "-lc", "make " + " ".join(shlex.quote(a) for a in args)]


def make_vars():
    """Windows: MSYS2's gcc, and this interpreter (a bare python in MSYS2's bash can be another one)."""
    return ["HOSTCC=gcc", "PYTHON=" + sys.executable.replace("\\", "/")] if IS_WIN else []


def base_env():
    env = dict(os.environ, PYTHON=sys.executable)
    if IS_WIN:
        env.update(MSYSTEM="UCRT64", CHERE_INVOKING="1", MSYS2_PATH_TYPE="inherit")
    return env


def find_flasher():
    exe = "gbsc-flasher.exe" if IS_WIN else "gbsc-flasher"
    for p in (shutil.which("gbsc-flasher"), os.path.join(FLASHER, "target", "release", exe)):
        if p and os.path.isfile(p):
            return p
    return None


def probe(key):
    """(found, text): text is the version, or why it is missing; a version off the pinned one starts with '!'"""
    if key == "python":
        return sys.version_info >= (3, 11), "%d.%d.%d" % sys.version_info[:3]
    if key in ("pyyaml", "fonttools"):
        mods = ("yaml",) if key == "pyyaml" else ("fontTools", "brotli")
        missing = [m for m in mods if importlib.util.find_spec(m) is None]
        return not missing, "missing " + ", ".join(missing) if missing else "ok (%s)" % sys.executable
    if key == "make":
        if IS_WIN:
            root = msys_root()
            ok = bool(root) and os.path.isfile(os.path.join(root, "usr", "bin", "make.exe"))
            return ok, "MSYS2 %s" % root if ok else "missing"
        v = first_line(["make", "--version"])
        return v is not None, v or "missing"
    if key in ("cc", "c++"):
        name = "g++" if key == "c++" else "gcc" if IS_WIN else "cc"
        exe = shutil.which(name)
        v = first_line([exe, "--version"]) if exe else None
        return v is not None, v or "missing"
    if key == "webkit2gtk":
        v = first_line(["pkg-config", "--modversion", "webkit2gtk-4.1"])
        return v is not None, v or "missing"
    if key == "gbsc-flasher":
        p = find_flasher()
        return p is not None, p or "missing (only dev.py flash needs it)"
    if key == "browsers":
        return probe_browsers()
    if key == "msvc":
        return probe_msvc()
    cmds = {"node": ["node", "--version"], "npm": ["npm", "--version"], "pio": ["pio", "--version"],
            "cargo": ["cargo", "--version"], "arm-gcc": ["arm-none-eabi-gcc", "-dumpversion"]}
    exe = shutil.which(cmds[key][0])
    v = first_line([exe, *cmds[key][1:]]) if exe else None
    if v is None:
        return False, "missing"
    if key == "node":
        m = re.match(r"v(\d+)\.(\d+)", v)
        if not m or (int(m.group(1)), int(m.group(2))) < NODE_MIN:
            return False, "%s too old (%d.%d+ is needed, %d recommended)" % (v, NODE_MIN[0], NODE_MIN[1], NODE_MAJOR)
        if int(m.group(1)) != NODE_MAJOR:
            return True, "!%s (Node %d is the tested one)" % (v, NODE_MAJOR)
    if key == "arm-gcc" and v != ARM_GCC:
        return True, "!%s (15.3.rel2 = %s is the pinned one: other versions build a different ADV image)" % (v, ARM_GCC)
    if key == "pio" and not v.endswith(PLATFORMIO):
        return True, "!%s (%s is the pinned one)" % (v, PLATFORMIO)
    return True, v


def probe_browsers():
    """The Playwright browsers of the locked version: the quick run needs Chromium (and WebKit off macOS), --full
    also Firefox."""
    pw = os.path.join(WEB, "node_modules", ".bin", "playwright.cmd" if IS_WIN else "playwright")
    if not os.path.isfile(pw):
        return True, "!not checked yet: dev.py check web installs the npm packages first"
    try:
        r = subprocess.run([pw, "install", "--dry-run"], capture_output=True, text=True, timeout=60, cwd=WEB)
    except (OSError, subprocess.TimeoutExpired):
        return False, "playwright did not run"
    found = re.findall(r"\(playwright ([\w-]+) v\d+\)\s*\n\s*Install location:\s*(.+)", r.stdout)
    missing = [name for name, where in found if not os.path.isdir(where.strip())]
    need = ["chromium-headless-shell"] + ([] if IS_MAC else ["webkit"])
    if not found or any(n in missing for n in need):
        return False, "missing " + ", ".join(n for n in missing if n in need) if found else "not listed by playwright"
    if "firefox" in missing:
        return True, "!no Firefox (only --full needs it)"
    return True, "ok"


def probe_msvc():
    """Windows: Rust links with the MSVC tools of Visual Studio (Build Tools)."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return False, "missing"
    v = first_line([vswhere, "-latest", "-products", "*", "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"])
    return bool(v), v or "missing (Visual Studio without the C++ tools)"


def missing_for(need):
    return [k for k in NEEDS[need] if not probe(k)[0]]


def playwright_mismatch():
    """In the container the browsers are baked for one Playwright version: it must be the locked one."""
    want = os.environ.get("PLAYWRIGHT_VERSION")
    lock = os.path.join(WEB, "package-lock.json")
    if not want or not os.path.isfile(lock):
        return ""
    m = re.search(r'"node_modules/@playwright/test":\s*\{\s*"version":\s*"([^"]+)"', open(lock, encoding="utf-8").read())
    if m and m.group(1) != want:
        return ("package-lock has %s, the image's browsers are for %s: bump PLAYWRIGHT_VERSION in docker/Dockerfile"
                % (m.group(1), want))
    return ""


def cmd_doctor(_a):
    keys = ["python"] + sorted({k for v in NEEDS.values() for k in v}) + ([] if IN_CONTAINER else ["gbsc-flasher"])
    bad = 0
    for k in keys:
        ok, text = probe(k)
        mark = "ok" if ok and not text.startswith("!") else "!!" if ok else "--"
        bad += not ok and k != "gbsc-flasher"          # only dev.py flash needs it
        line = "  %-2s  %-13s %s" % (mark, k, text.lstrip("!"))
        if not ok:
            line += "  ->  " + hint(k)
        print(line)
    print("needed by: " + "; ".join("%s: %s" % (s, " ".join(NEEDS[s])) for s in NEEDS))
    pw = playwright_mismatch()
    if pw:
        bad += 1
        print("  !!  playwright    " + pw)
    if IS_MAC and not IN_CONTAINER:
        print("note: the WebKit Playwright projects do not run on macOS: docker compose run --rm dev check web --full")
    return 1 if bad else 0


def run(cmd, cwd, env=None):
    print("dev: %s   (in %s)" % (" ".join(cmd), os.path.relpath(cwd, REPO)), flush=True)
    return subprocess.run(cmd, cwd=cwd, env=env or base_env()).returncode == 0


def tool(name):
    return shutil.which(name) or name


def npm_ci(d):
    """npm ci when node_modules is missing or older than package-lock.json (a pull or a branch switch changed it)."""
    nm = os.path.join(d, "node_modules")
    stamp, lock = os.path.join(nm, ".package-lock.json"), os.path.join(d, "package-lock.json")
    if os.path.isfile(stamp) and os.path.getmtime(stamp) >= os.path.getmtime(lock):
        return True
    if os.path.isdir(nm) and not os.access(nm, os.W_OK):
        print("dev: %s exists but is not writable (a container mount point?): remove it and run again" % nm)
        return False
    return run([tool("npm"), "ci"], d)


def suite_gen(a):
    if not npm_ci(WEB):
        return False
    return run([tool("npm"), "run", "-s", "gen:check"], WEB) or run([tool("npm"), "run", "gen"], WEB)


def suite_esp(a):
    return run([tool("pio"), "test", "-e", "native"], FW)


def suite_adv(a):
    return run(make_cmd(["test", *make_vars()]), ADV)


def suite_web(a):
    if IS_MAC and not IN_CONTAINER:
        print("dev: web: the WebKit projects (iPhone 15, iPad, Safari) are left out on macOS; "
              "docker compose run --rm dev check web --full runs them")
    env = base_env()
    if a.full:
        env["GBSC_E2E"] = "full"
    shard = ["--", "--shard=" + a.shard] if a.shard else []
    return (npm_ci(WEB) and run([tool("npm"), "run", "-s", "check:static"], WEB)
            and run([tool("npm"), "run", "-s", "test:e2e", *shard], WEB, env))


def suite_flasher(a):
    return (npm_ci(WEB) and npm_ci(UI) and run([tool("npm"), "run", "build"], UI) and run([tool("npm"), "test"], UI)
            and run([tool("cargo"), "test", "--workspace"], FLASHER))


def suite_tools(a):
    files = sorted(glob.glob(os.path.join(REPO, "tools", "test_*.py")) + glob.glob(os.path.join(REPO, "tools", "ota", "test_*.py")))
    ok = all([run([sys.executable, f], REPO) for f in files])
    return run([sys.executable, os.path.join(FW, "tools", "adv-controller-image.py"), "--self-test"], REPO) and ok


def cmd_build(a):
    adv, esp = not a.esp, not a.adv
    for need, on in (("build-adv", adv), ("build-esp", esp)):
        missing = missing_for(need) if on else []
        if missing:
            print("dev: build: %s missing, see python tools/dev.py doctor" % ", ".join(missing))
            return 1
    os.makedirs(OUT, exist_ok=True)
    env = base_env()
    if esp and not npm_ci(WEB):
        return 1
    if adv:
        if not run(make_cmd(["BUILD=release", *make_vars()]), ADV):
            return 1
        image = os.path.join(OUT, "adv-controller.bin")
        shutil.copyfile(os.path.join(ADV, "build", "release", "adv-controller.bin"), image)
        env["GBSC_ADV_IMAGE"] = image
    if esp:
        if not run([tool("pio"), "run", "-e", "gbsc-pro"], FW, env):
            return 1
        if not os.path.isfile(release.BIN):
            print("dev: build: %s not found (PLATFORMIO_BUILD_DIR moves it)" % release.BIN)
            return 1
        raw = open(release.BIN, "rb").read()
        with open(os.path.join(OUT, "gbsc-pro.bin"), "wb") as f:
            f.write(raw)
        with open(os.path.join(OUT, "gbsc-pro.bin.gz"), "wb") as f:
            f.write(release.make_image(raw))
    for name in ("gbsc-pro.bin", "gbsc-pro.bin.gz", "adv-controller.bin"):
        p = os.path.join(OUT, name)
        if os.path.isfile(p) and ((name.startswith("gbsc") and esp) or (name.startswith("adv") and adv)):
            print("out/%-18s %7d bytes" % (name, os.path.getsize(p)))
    return 0


def cmd_flash(a):
    if IN_CONTAINER:
        print("dev: flash runs on the computer (USB, local network), not in the container")
        return 2
    fl = find_flasher()
    if not fl:
        print("dev: gbsc-flasher not found: " + hint("gbsc-flasher"))
        return 2
    esp_bin, esp_gz, adv_bin = (os.path.join(OUT, n) for n in ("gbsc-pro.bin", "gbsc-pro.bin.gz", "adv-controller.bin"))
    do_esp, do_adv = not a.adv, not a.esp
    if a.host:                                   # WiFi: the ESP image, then the ADV image the new ESP firmware holds
        if do_esp and not os.path.isfile(esp_gz):
            print("dev: out/gbsc-pro.bin.gz missing: python tools/dev.py build first")
            return 1
        downgrade = ["--allow-downgrade"] if a.allow_downgrade else []
        if do_esp and not run([fl, "update", esp_gz, "--host", a.host, *downgrade], REPO):
            return 1
        return 0 if not do_adv or run([fl, "update-adv", "--host", a.host], REPO) else 1
    port = ["--port", a.port] if a.port else []
    for path, what in ((esp_bin, do_esp), (adv_bin, do_adv)):
        if not what:
            continue
        if not os.path.isfile(path):
            print("dev: %s missing: python tools/dev.py build first" % os.path.relpath(path, REPO))
            return 1
        if path == adv_bin and sys.stdin.isatty():
            input("ADV Controller: hold its button while plugging the USB-C cable (bootloader), then press Enter ")
        if not run([fl, "flash", path, *port], REPO):
            return 1
    return 0


RUNNERS = {"gen": suite_gen, "esp": suite_esp, "adv": suite_adv, "web": suite_web, "flasher": suite_flasher,
           "tools": suite_tools}


def cmd_check(a):
    bad = [s for s in a.suites if s not in SUITES]
    if bad:
        print("dev: unknown suite(s): %s (suites: %s)" % (" ".join(bad), " ".join(SUITES)))
        return 2
    m = re.fullmatch(r"(\d+)/(\d+)", a.shard or "1/1")
    if not m or not 1 <= int(m.group(1)) <= int(m.group(2)):
        print("dev: --shard takes I/N with 1 <= I <= N, e.g. 2/4")
        return 2
    named = set(a.suites)                  # a named suite that cannot run is a failure, not a skip
    wanted = set(named or SUITES)
    if wanted & {"web", "flasher"}:
        wanted.add("gen")                  # the web tests and the flasher's simulator read gen/
    results = []
    for s in SUITES:
        if s not in wanted:
            continue
        missing = missing_for(s)
        gen_state = next((st for name, st, _ in results if name == "gen"), "ok")
        if s in ("web", "flasher") and gen_state != "ok":
            results.append((s, "skipped" if gen_state == "skipped" and s not in named else "failed", "needs gen"))
            continue
        if missing:
            why = "%s missing, see python tools/dev.py doctor" % ", ".join(missing)
            results.append((s, "failed" if s in named else "skipped", why))
            continue
        results.append((s, "ok" if RUNNERS[s](a) else "failed", ""))
    print("\ncheck:")
    for s, state, why in results:
        print("  %-8s %s%s" % (s, state, " (%s)" % why if why else ""))
    failed = any(st == "failed" for _, st, _ in results)
    skipped = any(st == "skipped" for _, st, _ in results)
    return 1 if failed or (a.strict and skipped) else 0


def main(argv):
    ap = argparse.ArgumentParser(prog="dev.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("doctor")
    c = sub.add_parser("check")
    c.add_argument("suites", nargs="*", metavar="SUITE", help="gen esp adv web flasher tools (default: all)")
    c.add_argument("--full", action="store_true", help="Playwright on every project")
    c.add_argument("--strict", action="store_true", help="a skipped suite is a failure (CI)")
    c.add_argument("--shard", metavar="I/N", help="only part I of N of the Playwright tests (parallel CI jobs)")
    b = sub.add_parser("build")
    g = b.add_mutually_exclusive_group()
    g.add_argument("--esp", action="store_true", help="only GBS-Control (ESP8266) and the web app")
    g.add_argument("--adv", action="store_true", help="only the ADV Controller")
    f = sub.add_parser("flash")
    g = f.add_mutually_exclusive_group()
    g.add_argument("--esp", action="store_true", help="only GBS-Control (ESP8266)")
    g.add_argument("--adv", action="store_true", help="only the ADV Controller")
    w = f.add_mutually_exclusive_group()
    w.add_argument("--port", help="USB serial port (default: the one found)")
    w.add_argument("--host", help="WiFi: the unit's address (host or host:port)")
    f.add_argument("--allow-downgrade", action="store_true",
                   help="WiFi: install over a newer version (a local build X.Y.Z-dev0 sorts below the release X.Y.Z)")
    a = ap.parse_args(argv)
    return {"doctor": cmd_doctor, "check": cmd_check, "build": cmd_build, "flash": cmd_flash}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
