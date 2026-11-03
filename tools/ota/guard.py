#!/usr/bin/env python3
"""May an OTA workflow run publish this version?
Prints "publish" (newer than the live channel, and a Beta newer than the live Stable) or "resume" (a re-run of
the commit that published this very version: run_number is kept on re-runs, so finish that run); exits non-zero
otherwise.
  guard.py --version V --channel C [--live L] [--stable S] [--live-commit SHA] --commit SHA
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import version  # noqa: E402


def decide(v, live, stable, channel, live_commit, commit):
    if live and version.compare(v, live) == 0:
        if live_commit and live_commit == commit:
            return "resume"
        sys.exit(f"{v} is already live on {channel} from another commit")
    if live and version.compare(v, live) < 0:
        sys.exit(f"{v} is not newer than the live {channel} {live}")
    if channel != "stable" and stable and version.compare(v, stable) <= 0:
        sys.exit(f"{v} is not newer than the live stable {stable}: move GBS_FW_VERSION on")
    return "publish"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--version", required=True)
    ap.add_argument("--channel", required=True)
    ap.add_argument("--live", default="")
    ap.add_argument("--stable", default="")
    ap.add_argument("--live-commit", default="")
    ap.add_argument("--commit", required=True)
    a = ap.parse_args()
    print(decide(a.version, a.live, a.stable, a.channel, a.live_commit, a.commit))


if __name__ == "__main__":
    main()
