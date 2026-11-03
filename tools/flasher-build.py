#!/usr/bin/env python3
"""Build the GBSC PRO Flasher for release
into one folder that tools/release.py --flasher zips next to the firmware files:

  <out>/GBSC PRO Flasher.app        macOS app (ad-hoc signed)          --mac
  <out>/gbsc-flasher-macos          macOS command line                 --mac
  <out>/GBSC PRO Flasher.exe        Windows app, portable exe          --windows (cargo-xwin) or --windows-native
  <out>/gbsc-flasher.exe            Windows command line               --windows
  <out>/gbsc-pro-flasher.AppImage   Linux app                          --linux-native
  <out>/gbsc-pro-flasher.deb        Linux package                      --linux-native
  <out>/gbsc-flasher-linux          Linux command line                 --linux-native
  <out>/esp8266-stub.json, LICENSES/ (always)

Portable binaries, not installers: the flasher preselects the firmware files of the folder it runs from. Windows
from the Mac needs: rustup target add x86_64-pc-windows-msvc, cargo install cargo-xwin, brew install llvm (clang-cl,
lld-link: brew install llvm lld). When that fails, build on Windows (a VM or EC2) with --windows-native.
Linux packages are built on a Linux x86-64 machine (--linux-native), in CI on ubuntu-22.04 (the AppImage's glibc
floor)."""
import argparse
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLASHER = os.path.join(REPO, "gbsc-pro-flasher")
UI = os.path.join(FLASHER, "ui")
APP = os.path.join(FLASHER, "app")
TARGET = os.path.join(FLASHER, "target")
WIN = "x86_64-pc-windows-msvc"


def run(cmd, cwd=None, env=None):
    print("+", " ".join(cmd), flush=True)
    exe = shutil.which(cmd[0], path=(env or os.environ).get("PATH")) or cmd[0]   # Windows: npm is npm.cmd
    subprocess.run([exe] + cmd[1:], cwd=cwd, env=env, check=True)


def build_ui():
    if not os.path.isdir(os.path.join(UI, "node_modules")):
        run(["npm", "ci"], cwd=UI)
    run(["npm", "run", "build"], cwd=UI)


def copy_common(out):
    shutil.copy2(os.path.join(FLASHER, "resources", "esp8266-stub.json"), out)
    shutil.copytree(os.path.join(FLASHER, "resources", "LICENSES"), os.path.join(out, "LICENSES"), dirs_exist_ok=True)


MAC_TARGETS = ("aarch64-apple-darwin", "x86_64-apple-darwin")


def build_mac(out):
    """Universal app and command line (Apple Silicon and Intel): a plain build only targets the host's CPU."""
    run(["rustup", "target", "add", *MAC_TARGETS])
    run(["cargo", "tauri", "build", "--target", "universal-apple-darwin", "--bundles", "app"], cwd=APP)
    app = os.path.join(TARGET, "universal-apple-darwin", "release", "bundle", "macos", "GBSC PRO Flasher.app")
    dst = os.path.join(out, "GBSC PRO Flasher.app")
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(app, dst, symlinks=True)
    run(["codesign", "--force", "--deep", "-s", "-", dst])     # ad hoc: Gatekeeper still asks once (README)
    for t in MAC_TARGETS:
        run(["cargo", "build", "--release", "--target", t, "-p", "gbsc-flasher-cli"], cwd=FLASHER)
    run(["lipo", "-create", "-output", os.path.join(out, "gbsc-flasher-macos"),
         *(os.path.join(TARGET, t, "release", "gbsc-flasher") for t in MAC_TARGETS)])


def xwin_env():
    """Homebrew keeps clang-cl in llvm and lld-link in lld (keg-only): put both on PATH for cargo-xwin."""
    extra = [p for p in ("/opt/homebrew/opt/llvm/bin", "/opt/homebrew/opt/lld/bin", "/usr/local/opt/llvm/bin", "/usr/local/opt/lld/bin") if os.path.isdir(p)]
    return dict(os.environ, PATH=os.pathsep.join(extra + [os.environ.get("PATH", "")]))


def build_windows(out):
    env = xwin_env()
    run(["cargo", "xwin", "build", "--release", "--target", WIN, "-p", "gbsc-flasher-cli"], cwd=FLASHER, env=env)
    shutil.copy2(os.path.join(TARGET, WIN, "release", "gbsc-flasher.exe"), out)
    # the app through the Tauri CLI (custom-protocol feature: a plain cargo build would load the dev URL)
    run(["cargo", "tauri", "build", "--runner", "cargo-xwin", "--target", WIN, "--no-bundle"], cwd=APP, env=env)
    shutil.copy2(os.path.join(TARGET, WIN, "release", "gbsc-pro-flasher.exe"), os.path.join(out, "GBSC PRO Flasher.exe"))


def build_windows_native(out):
    run(["cargo", "build", "--release", "-p", "gbsc-flasher-cli"], cwd=FLASHER)
    shutil.copy2(os.path.join(TARGET, "release", "gbsc-flasher.exe"), out)
    run(["cargo", "tauri", "build", "--no-bundle"], cwd=APP)
    shutil.copy2(os.path.join(TARGET, "release", "gbsc-pro-flasher.exe"), os.path.join(out, "GBSC PRO Flasher.exe"))


def bundle_file(bundle, d, ext):
    """The file Tauri just wrote: a reused target/ keeps the bundles of older versions next to it."""
    files = [os.path.join(bundle, d, f) for f in os.listdir(os.path.join(bundle, d)) if f.endswith(ext)]
    if not files:
        sys.exit(f"no {ext} in {os.path.join(bundle, d)}")
    return max(files, key=os.path.getmtime)


def build_linux_native(out):
    """On a Linux host with the WebKitGTK and udev development packages (CI)."""
    env = dict(os.environ, APPIMAGE_EXTRACT_AND_RUN="1")
    run(["cargo", "tauri", "build", "--bundles", "appimage,deb"], cwd=APP, env=env)
    run(["cargo", "build", "--release", "-p", "gbsc-flasher-cli"], cwd=FLASHER)
    bundle = os.path.join(TARGET, "release", "bundle")
    shutil.copy2(bundle_file(bundle, "appimage", ".AppImage"), os.path.join(out, "gbsc-pro-flasher.AppImage"))
    shutil.copy2(bundle_file(bundle, "deb", ".deb"), os.path.join(out, "gbsc-pro-flasher.deb"))
    shutil.copy2(os.path.join(TARGET, "release", "gbsc-flasher"), os.path.join(out, "gbsc-flasher-linux"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mac", action="store_true")
    ap.add_argument("--windows", action="store_true", help="cross-build from the Mac with cargo-xwin")
    ap.add_argument("--windows-native", action="store_true", help="on a Windows machine")
    ap.add_argument("--linux-native", action="store_true", help="on a Linux host (CI)")
    a = ap.parse_args()
    if not (a.mac or a.windows or a.windows_native or a.linux_native):
        sys.exit("choose at least one of --mac --windows --windows-native --linux-native")
    os.makedirs(a.out, exist_ok=True)
    build_ui()
    copy_common(a.out)
    if a.mac:
        build_mac(a.out)
    if a.windows:
        build_windows(a.out)
    if a.windows_native:
        build_windows_native(a.out)
    if a.linux_native:
        build_linux_native(a.out)
    for f in sorted(os.listdir(a.out)):
        p = os.path.join(a.out, f)
        size = sum(os.path.getsize(os.path.join(r, x)) for r, _, fs in os.walk(p) for x in fs) if os.path.isdir(p) else os.path.getsize(p)
        print(f"{size / 1e6:8.1f} MB  {f}")


if __name__ == "__main__":
    main()
