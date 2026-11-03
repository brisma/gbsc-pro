#!/usr/bin/env python3
"""Release assets on GitHub: every Stable and Beta release carries the whole package (firmware,
ADV Controller, the flasher for macOS, Windows and Linux), but the flashers are built only when they changed.

  fingerprint                         hash of what goes into the flasher binaries (needs gbsc-pro-flasher/ui/dist)
  pick --fingerprint F --dir D TAG..  the newest release among TAG.. (D/<tag>/artifacts.json) with the same flashers
  pack --flasher DIR --out DIR        GBSC-PRO-Flasher-<system>.zip per system (file modes kept: an Actions
                                      artifact drops them, and the macOS app and the Linux binaries need them)
  unpack --assets DIR --flasher DIR   back into the folder tools/release.py --flasher reads
  manifest ...                        artifacts.json of a release (what it carries, where its flashers come from)
"""
import argparse
import hashlib
import json
import os
import stat
import subprocess
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI_DIST = "gbsc-pro-flasher/ui/dist"
SYSTEMS = ("macos", "windows", "linux")
ZIP_NAME = {"macos": "GBSC-PRO-Flasher-macOS.zip", "windows": "GBSC-PRO-Flasher-Windows.zip",
            "linux": "GBSC-PRO-Flasher-Linux.zip"}
COMMON = ("esp8266-stub.json", "LICENSES")
FILES = {"macos": ("GBSC PRO Flasher.app", "gbsc-flasher-macos"),
         "windows": ("GBSC PRO Flasher.exe", "gbsc-flasher.exe"),
         "linux": ("gbsc-pro-flasher.AppImage", "gbsc-pro-flasher.deb", "gbsc-flasher-linux")}
ZIP_DATE = (2026, 1, 1, 0, 0, 0)


def _skip(path):
    """Tracked files that never reach a binary: notes and tests."""
    parts = path.split("/")
    return path.endswith(".md") or "docs" in parts or "tests" in parts or "test" in parts


def flasher_fingerprint(repo, files):
    """sha256 over the tracked flasher sources (files: repo-relative paths, e.g. git ls-files) and the built UI bundle:
    the bundle carries whatever the UI imports from the web app, so no list of shared files to keep up to date."""
    dist = os.path.join(repo, UI_DIST)
    if not os.path.isdir(dist):
        sys.exit(f"{UI_DIST} is missing: build the flasher UI first (cd gbsc-pro-flasher/ui && npm ci && npm run build)")
    paths = sorted(set(p for p in files if not _skip(p)))
    for root, _dirs, names in os.walk(dist):
        paths += sorted(os.path.relpath(os.path.join(root, n), repo).replace(os.sep, "/") for n in names)
    h = hashlib.sha256()
    for p in paths:
        full = os.path.join(repo, p)
        if not os.path.isfile(full):
            continue
        h.update(p.encode() + b"\0" + hashlib.sha256(open(full, "rb").read()).digest())
    return h.hexdigest()


def tracked_flasher_files(repo):
    # release_assets.py too: it decides what each system's zip holds, so a new file there must not reuse older zips
    out = subprocess.run(["git", "ls-files", "gbsc-pro-flasher", "tools/flasher-build.py", "tools/release_assets.py"], cwd=repo,
                         capture_output=True, text=True, check=True).stdout
    return out.split()


def pick_reuse(releases, fingerprint):
    """releases: newest first, [{"tag", "flasher"}]. The first with the same flasher fingerprint, else None."""
    if not fingerprint:
        sys.exit("empty flasher fingerprint")
    return next((r["tag"] for r in releases if r.get("flasher") == fingerprint), None)


def pick_from_dir(d, tags, fingerprint):
    releases = []
    for tag in tags:
        p = os.path.join(d, tag, "artifacts.json")
        if os.path.isfile(p):
            try:
                releases.append({"tag": tag, "flasher": json.load(open(p))["flasher"]["fingerprint"]})
            except (ValueError, KeyError, TypeError):
                pass
    return pick_reuse(releases, fingerprint)


def _entries(flasher, system):
    """(path in the zip, file on disk) for one system's binaries plus the stub and the licences."""
    out = []
    for name in COMMON + FILES[system]:
        src = os.path.join(flasher, name)
        if os.path.isdir(src):
            for root, _dirs, names in os.walk(src):
                for n in names:
                    f = os.path.join(root, n)
                    out.append((os.path.relpath(f, flasher).replace(os.sep, "/"), f))
        elif os.path.isfile(src):
            out.append((name, src))
        else:
            sys.exit(f"{name} is missing in {flasher}: the {system} flasher is not complete")
    return sorted(out)


def pack(flasher, out_dir, system):
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, ZIP_NAME[system])
    with zipfile.ZipFile(path, "w") as z:
        for arc, src in _entries(flasher, system):
            info = zipfile.ZipInfo(arc, date_time=ZIP_DATE)
            info.external_attr = (stat.S_IFREG | stat.S_IMODE(os.stat(src).st_mode)) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, open(src, "rb").read())
    return path


def unpack(assets, flasher):
    """Every GBSC-PRO-Flasher-*.zip in assets into flasher, with the modes stored in the zip."""
    found = 0
    for system in SYSTEMS:
        p = os.path.join(assets, ZIP_NAME[system])
        if not os.path.isfile(p):
            continue
        found += 1
        with zipfile.ZipFile(p) as z:
            for info in z.infolist():
                dst = os.path.join(flasher, *info.filename.split("/"))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, "wb") as f:
                    f.write(z.read(info))
                mode = (info.external_attr >> 16) & 0o777
                if mode:
                    os.chmod(dst, mode)
    if found != len(SYSTEMS):
        sys.exit(f"{assets}: {found} of {len(SYSTEMS)} flasher zips")


def artifacts_manifest(version, channel, commit, fingerprint, source, adv_version, adv_crc):
    return {"version": version, "channel": channel, "commit": commit,
            "flasher": {"fingerprint": fingerprint, "from": source},
            "adv_controller": {"version": adv_version, "build": f"{adv_crc:08X}"}}


def adv_controller_info():
    import importlib.util
    spec = importlib.util.spec_from_file_location("adv_controller_image",
                                                  os.path.join(REPO, "gbs-control", "tools", "adv-controller-image.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.parse_image(open(os.path.join(REPO, "adv-controller", "release", "adv-controller.bin"), "rb").read())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("fingerprint")
    p = sub.add_parser("pick")
    p.add_argument("--fingerprint", required=True)
    p.add_argument("--dir", required=True)
    p.add_argument("tags", nargs="*")
    p = sub.add_parser("pack")
    p.add_argument("--flasher", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--system", choices=SYSTEMS, action="append", help="default: all three")
    p = sub.add_parser("unpack")
    p.add_argument("--assets", required=True)
    p.add_argument("--flasher", required=True)
    p = sub.add_parser("manifest")
    for a in ("--version", "--channel", "--commit", "--fingerprint", "--source", "--out"):
        p.add_argument(a, required=True)
    a = ap.parse_args()
    if a.cmd == "fingerprint":
        print(flasher_fingerprint(REPO, tracked_flasher_files(REPO)))
    elif a.cmd == "pick":
        print(pick_from_dir(a.dir, a.tags, a.fingerprint) or "")
    elif a.cmd == "pack":
        for s in a.system or SYSTEMS:
            print(pack(a.flasher, a.out, s))
    elif a.cmd == "unpack":
        unpack(a.assets, a.flasher)
    elif a.cmd == "manifest":
        version, crc = adv_controller_info()
        m = artifacts_manifest(a.version, a.channel, a.commit, a.fingerprint, a.source, version, crc)
        with open(a.out, "w") as f:
            json.dump(m, f, indent=1)
            f.write("\n")
        print(a.out)


if __name__ == "__main__":
    main()
