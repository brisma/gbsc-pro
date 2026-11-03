#!/usr/bin/env python3
"""Which OTA images to delete from the bucket after a publish: per channel the newest `keep` versions stay, and so
does every image a live channel manifest names (e.g. after a rollback). Manifests and unknown keys are never touched. Prints one key per line; the workflow deletes them with `aws s3 rm`.

  prune.py --listing listing.json [--manifest stable=path] [--manifest beta=path] [--keep 3]
"""
import argparse
import functools
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import version  # noqa: E402

_IMAGE = re.compile(r"^v1/fw/gbsc-pro-([0-9A-Za-z.\-]+)\.bin\.gz$")


def keys_from_listing(listing):
    """Keys of an `aws s3api list-objects-v2` JSON answer."""
    return [o["Key"] for o in listing.get("Contents", [])]


def _named(manifest_text):
    m = re.search(r"^file=(.+)$", manifest_text or "", re.M)
    return "v1/" + m.group(1).strip() if m else None


def to_delete(keys, live_manifests, keep=3):
    named = {_named(t) for t in live_manifests.values()} - {None}
    by_channel = {}
    for k in keys:
        m = _IMAGE.match(k)
        ch = version.channel_of(m.group(1)) if m else None
        if ch:
            by_channel.setdefault(ch, []).append((m.group(1), k))
    out = []
    for items in by_channel.values():
        items.sort(key=functools.cmp_to_key(lambda a, b: version.compare(b[0], a[0])))   # newest first
        out += [k for _, k in items[keep:] if k not in named]
    return sorted(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--listing", required=True)
    ap.add_argument("--manifest", action="append", default=[], help="channel=path of the live manifest (missing: skip)")
    ap.add_argument("--keep", type=int, default=3)
    a = ap.parse_args()
    live = {}
    for item in a.manifest:
        ch, path = item.split("=", 1)
        if os.path.exists(path):
            live[ch] = open(path).read()
    for k in to_delete(keys_from_listing(json.load(open(a.listing))), live, a.keep):
        print(k)


if __name__ == "__main__":
    main()
