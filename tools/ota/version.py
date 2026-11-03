"""GBSC PRO version order, frozen from 3.0.0 (the same rule as gbs-control/pro/update/update-manifest.cpp
versionCompare and the web app's compareVersions): X.Y.Z numeric, then "-devN" < "-betaN" < no suffix with N
numeric; any other suffix sorts below "-dev" (a local build)."""
import re

_SUFFIX = re.compile(r"^-(beta|dev)([0-9]+)$")
_VALID = re.compile(r"^[0-9A-Za-z.\-]{1,15}$")


def _parts(v):
    m = re.match(r"^([0-9]*)\.?([0-9]*)\.?([0-9]*)", v)
    nums = [int(x) if x else 0 for x in m.groups()]
    rest = v[m.end():]
    if not rest.startswith("-"):
        return nums, 3, 0, rest
    s = _SUFFIX.match(rest)
    if not s:
        return nums, 0, 0, rest
    return nums, 2 if s.group(1) == "beta" else 1, int(s.group(2)), rest


def compare(a, b):
    """-1, 0 or 1."""
    na, ra, ia, sa = _parts(a)
    nb, rb, ib, sb = _parts(b)
    for x, y in ((na, nb), (ra, rb)):
        if x != y:
            return -1 if x < y else 1
    if ra in (1, 2):
        return 0 if ia == ib else (-1 if ia < ib else 1)
    return 0 if sa == sb else (-1 if sa < sb else 1)


def channel_of(v):
    """"stable" for X.Y.Z, "beta" for X.Y.Z-betaN, None for anything else (X.Y.Z-devN is a local build: no
    channel publishes it)."""
    if re.match(r"^[0-9]+\.[0-9]+\.[0-9]+$", v):
        return "stable"
    m = re.match(r"^[0-9]+\.[0-9]+\.[0-9]+-(beta)[0-9]+$", v)
    return m.group(1) if m else None


def valid(v):
    """What the device's manifest parser accepts: 1..15 characters of [0-9A-Za-z.-]."""
    return bool(_VALID.match(v))
