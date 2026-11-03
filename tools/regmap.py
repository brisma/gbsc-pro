#!/usr/bin/env python3
"""Register map of the web app's Developer tools.

One JSON from the TV5725 fields of gbs-control/tv5725.h and the ADV register maps in shared/adv-regmap (configs,
the default preset for the values, the built-in macros; the ADV Manager YAML format), with the registers that are dangerous to write and the ones the ADV
Controller sets itself (found in its golden I2C traces). The firmware embeds it gzipped (gbs-control/regmap_gz.h,
served at GET /api/regmap); the web app's tooling writes the plain JSON to public/gen/regmap.json.

  python3 tools/regmap.py --out <json> [--header <h>]   # --empty: a header with regmap_gz_len = 0
Needs PyYAML (pip install pyyaml).
"""
import argparse
import glob
import gzip
import hashlib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# Writes that leave the picture wrong until the input changes or the device restarts (the ADV Controller does not
# re-apply its state after them): (I2C address, ADV7280 map, register) -> kind. "planner": the map select register
# the web planner writes around each group; the ADV Controller resets it every 50 ms, so it is never user-writable.
DANGER = {
    (0x42, 0x00, 0x0E): "planner",
    (0x42, 0x20, 0x0E): "planner",   # the same register seen from the other ADV7280 maps
    (0x42, 0x40, 0x0E): "planner",
    (0x42, 0x00, 0xFD): "warn",   # VPP I2C address
    (0x42, 0x00, 0x0F): "warn",   # bit 7 reset, bit 5 power down
    (0x84, 0x00, 0x41): "warn",   # DEINT_RESET
    (0x56, 0x00, 0x17): "warn",   # SW_RESET
    (0x56, 0x00, 0x00): "warn",   # sleep and DAC power
}
GOLDEN_SKIP = ("i2c_batch.txt",)   # replays a dev.adv_i2c batch: those writes are the user's, not the device's

UREG = re.compile(r"typedef\s+UReg<\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*>\s*(\w+)\s*;")


def inputs(repo):
    """Every file the map is built from (the build rebuilds the header when one is newer)."""
    adv = os.path.join(repo, "shared", "adv-regmap")
    files = sorted(glob.glob(os.path.join(adv, "configs", "*.yaml")))
    files.append(os.path.join(adv, "presets", "default.yaml"))
    files += sorted(glob.glob(os.path.join(adv, "macros", "*.yaml")))
    files.append(os.path.join(repo, "gbs-control", "tv5725.h"))
    files += sorted(glob.glob(os.path.join(repo, "adv-controller", "test", "golden", "*.txt")))
    files.append(os.path.abspath(__file__))
    return files


def stale(out, files):
    """True when out is missing or older than any of files."""
    if not os.path.exists(out):
        return True
    built = os.path.getmtime(out)
    return any(os.path.exists(f) and os.path.getmtime(f) > built for f in files)


def tv5725_fields(text):
    """[name, segment, register, bit, width, access] for every UReg typedef; access ro / confirm / rw."""
    rows = []
    for m in UREG.finditer(text):
        seg, reg, bit, width = int(m.group(1), 0), int(m.group(2), 0), int(m.group(3)), int(m.group(4))
        if seg == 0 and reg < 0x40:
            access = "ro"          # status, periods, chip id, test bus
        elif seg == 0 and reg in (0x46, 0x47):
            access = "confirm"     # soft resets
        else:
            access = "rw"
        rows.append([m.group(5), seg, reg, bit, width, access])
    return rows


def managed_registers(golden_dir):
    """(address, map, register) the ADV Controller writes in its golden traces; ADV7280 maps follow the 0x0E writes."""
    seen = set()
    for path in sorted(glob.glob(os.path.join(golden_dir, "*.txt"))):
        if os.path.basename(path) in GOLDEN_SKIP:
            continue
        current_map = 0
        with open(path) as f:
            for line in f:
                parts = line.split()
                if len(parts) != 4 or parts[0] != "w":
                    continue
                addr, reg, val = (int(x, 16) for x in parts[1:])
                if addr == 0x42 and reg == 0x0E:
                    current_map = val
                    seen.add((addr, 0x00, reg))
                    continue
                seen.add((addr, current_map if addr == 0x42 else 0x00, reg))
    return seen


def _num(v):
    return int(str(v), 0) if v is not None else None


def adv_db(repo, yaml):
    adv = os.path.join(repo, "shared", "adv-regmap")
    with open(os.path.join(adv, "presets", "default.yaml"), encoding="utf-8") as f:
        preset = yaml.safe_load(f)
    managed = managed_registers(os.path.join(repo, "adv-controller", "test", "golden"))
    chips = []
    for name in ("adv7280.yaml", "adv7391.yaml"):
        with open(os.path.join(adv, "configs", name), encoding="utf-8") as f:
            devices = yaml.safe_load(f)["devices"]
        for cname, c in devices.items():
            chip_addr = _num(c.get("address"))
            preset_maps = ((preset.get("devices") or {}).get(cname) or {}).get("maps") or {}
            maps = []
            for mname, m in c["register_maps"].items():
                i2c = _num(m.get("i2c_address")) or chip_addr or 0
                map_value = _num(m.get("map_value")) if i2c == 0x42 else 0
                pregs = (preset_maps.get(mname) or {}).get("registers") or {}
                pregs = {str(k).lower(): v for k, v in pregs.items()}
                regs = []
                for addr, r in (m.get("registers") or {}).items():
                    fields, default = [], 0
                    for fd in r.get("fields", []):
                        bits = sorted(fd.get("bits", []))
                        kind = fd.get("type", "int")
                        values = {str(k): str(v) for k, v in (fd.get("values") or {}).items()}
                        if "default" in fd and bits:
                            try:
                                default |= (int(fd["default"]) & ((1 << len(bits)) - 1)) << bits[0]
                            except (TypeError, ValueError):
                                pass
                        fields.append([fd.get("name", ""), bits[0] if bits else 0, bits[-1] if bits else 0, kind[0],
                                       fd.get("description", ""), values, fd.get("range")])
                    a = str(addr).lower()
                    reg = int(a, 16)
                    value = pregs.get(a)
                    value = int(str(value), 16) if value is not None else default
                    key = (i2c, map_value or 0, reg)
                    regs.append([a, r.get("name", ""), r.get("description", ""), value, 1 if r.get("readonly") else 0,
                                 fields, DANGER.get(key, ""), 1 if key in managed else 0])
                maps.append({"id": mname, "name": m.get("description", "").split(" - ")[0] or mname,
                             "mv": m.get("map_value"), "i2c": m.get("i2c_address"), "regs": regs})
            chips.append({"id": cname, "addr": c.get("address"), "desc": c.get("description", ""), "maps": maps})
    macros = []
    for path in sorted(glob.glob(os.path.join(adv, "macros", "*.yaml"))):
        with open(path, encoding="utf-8") as f:
            m = yaml.safe_load(f)
        macros.append([m.get("name"), m.get("commands") or []])
    danger = [[a, mp, r, k] for (a, mp, r), k in sorted(DANGER.items())]
    return {"chips": chips, "macros": macros, "danger": danger}


def build(repo):
    try:
        import yaml
    except ImportError:
        sys.exit("regmap: PyYAML is needed (pip install pyyaml)")
    with open(os.path.join(repo, "gbs-control", "tv5725.h")) as f:
        tv = tv5725_fields(f.read())
    return {"version": 1, "tv5725": tv, "adv": adv_db(repo, yaml)}


def encode(db):
    return json.dumps(db, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def header_text(gz):
    lines = ["// Generated by tools/regmap.py: the register map of the web app (GET /api/regmap). Not in git.",
             "#pragma once", "#include <Arduino.h>", "const uint8_t regmap_gz[] PROGMEM = {"]
    for i in range(0, len(gz), 16):
        lines.append("  " + ", ".join("0x%02x" % b for b in gz[i:i + 16]) + ",")
    if not gz:
        lines.append("  0x00")
    lines.append("};")
    lines.append("const unsigned int regmap_gz_len = %d;" % len(gz))
    # the ETag follows the content: an incremental build that only regenerates this header still gets a new one
    lines.append('const char regmap_etag[] = "%s";' % (hashlib.sha256(gz).hexdigest()[:16] if gz else "none"))
    return "\n".join(lines) + "\n"


def write_if_changed(path, data):
    old = None
    if os.path.exists(path):
        with open(path, "rb") as f:
            old = f.read()
    if old != data:
        with open(path, "wb") as f:
            f.write(data)
    else:
        os.utime(path, None)   # up to date with its inputs


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=os.path.dirname(HERE))
    ap.add_argument("--out", help="plain JSON (public/gen/regmap.json)")
    ap.add_argument("--header", help="C header with the gzipped JSON (gbs-control/regmap_gz.h)")
    ap.add_argument("--empty", action="store_true", help="header with regmap_gz_len = 0 (GBSC_NO_REGMAP=1)")
    a = ap.parse_args(argv)
    if a.empty:
        if a.header:
            write_if_changed(a.header, header_text(b"").encode())
        return 0
    js = encode(build(a.repo))
    if a.out:
        write_if_changed(a.out, js)
    if a.header:
        write_if_changed(a.header, header_text(gzip.compress(js, 9, mtime=0)).encode())
    print("regmap: %d B json, %d B gzip" % (len(js), len(gzip.compress(js, 9, mtime=0))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
