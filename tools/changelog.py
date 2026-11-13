#!/usr/bin/env python3
"""The CHANGELOG.md sections a release reads and writes.

  changelog.py section X.Y.Z[-betaN]  print the body of a version's section (the GitHub release notes)
  changelog.py beta X.Y.Z-betaN [--date YYYY-MM-DD]
                                      "## X.Y.Z-betaN" becomes "## X.Y.Z-betaN - <date>" (default: today)
  changelog.py stable X.Y.Z [--date YYYY-MM-DD]
                                      the sections of its betas become one "## X.Y.Z - <date>", oldest first
  changelog.py check vX.Y.Z-betaN | vX.Y.Z
                                      exits 1 with the reason when the tag has no matching section

Sections, newest on top: "## X.Y.Z-betaN" without a date is the next beta, where every change for users adds a line
(what changed since the release before it); "## X.Y.Z-betaN - YYYY-MM-DD" is a published beta; "## X.Y.Z -
YYYY-MM-DD" a Stable, which holds what its betas held.
"""
import argparse
import datetime
import os
import re
import sys

PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "CHANGELOG.md")
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
TAG = re.compile(r"^v(\d+\.\d+\.\d+)(-beta\d+)?$")
BETA = re.compile(r"^(\d+\.\d+\.\d+)-beta(\d+)$")
HEADING = re.compile(r"^## (\S+)(?: - (\d{4}-\d{2}-\d{2}))?$")


class ChangelogError(Exception):
    pass


def _parse(doc):
    """(preamble, [[heading, body], ...]) split at the "## " headings."""
    parts = re.split(r"^(## .*)$", doc, flags=re.M)
    return parts[0], [[parts[i].strip(), parts[i + 1].strip()] for i in range(1, len(parts), 2)]


def _join(preamble, sections):
    out = preamble.rstrip() + "\n\n"
    for heading, body in sections:
        out += heading + "\n\n" + (body + "\n\n" if body else "")
    return out.rstrip() + "\n"


def _find(sections, version):
    """(index, date or None) of the section of a version, else (-1, None)."""
    for i, (heading, _) in enumerate(sections):
        m = HEADING.match(heading)
        if m and m.group(1) == version:
            return i, m.group(2)
    return -1, None


def _date(date):
    if not DATE.match(date):
        raise ChangelogError(f"date {date!r}: YYYY-MM-DD")


def section(doc, version):
    _, sections = _parse(doc)
    i, _ = _find(sections, version)
    return sections[i][1] if i >= 0 else None


def beta(doc, version, date):
    _date(date)
    if not BETA.match(version):
        raise ChangelogError(f"{version}: not a beta version (X.Y.Z-betaN)")
    preamble, sections = _parse(doc)
    i, dated = _find(sections, version)
    if i < 0 or not sections[i][1]:
        raise ChangelogError(f'no "## {version}" section with entries: nothing to release')
    if dated:
        raise ChangelogError(f"{version} is already released ({dated})")
    sections[i][0] = f"## {version} - {date}"
    return _join(preamble, sections)


def stable(doc, version, date):
    _date(date)
    preamble, sections = _parse(doc)
    betas = []                                          # (N, index), the sections of this version's betas
    for i, (heading, _) in enumerate(sections):
        m = HEADING.match(heading)
        b = BETA.match(m.group(1)) if m else None
        if b and b.group(1) == version:
            if not m.group(2):
                raise ChangelogError(f"{m.group(1)} is still in the works: release it, or move its entries")
            betas.append((int(b.group(2)), i))
    if not betas:
        raise ChangelogError(f"no section of a {version} beta: a Stable is the promotion of a beta")
    body = "\n\n".join(sections[i][1] for _, i in sorted(betas) if sections[i][1])
    first = min(i for _, i in betas)
    sections[first] = [f"## {version} - {date}", body]
    drop = {i for _, i in betas} - {first}
    return _join(preamble, [s for i, s in enumerate(sections) if i not in drop])


def check(doc, tag):
    """"" when CHANGELOG.md is ready for this release tag, else the reason."""
    m = TAG.match(tag)
    if not m:
        return f"{tag}: not a release tag (vX.Y.Z-betaN or vX.Y.Z)"
    version = tag[1:]
    _, sections = _parse(doc)
    i, dated = _find(sections, version)
    return "" if i >= 0 and dated and sections[i][1] else f'CHANGELOG.md has no "## {version} - <date>" section with entries'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("section", "beta", "stable"):
        sub.add_parser(name).add_argument("version")
    for name in ("beta", "stable"):
        sub.choices[name].add_argument("--date", default=datetime.date.today().isoformat())
    sub.add_parser("check").add_argument("tag")
    ap.add_argument("--file", default=PATH)
    a = ap.parse_args(argv)
    doc = open(a.file, encoding="utf-8").read()
    try:
        if a.cmd == "section":
            body = section(doc, a.version)
            if body is None:
                raise ChangelogError(f"no section for {a.version}")
            print(body)
        elif a.cmd == "check":
            why = check(doc, a.tag)
            if why:
                raise ChangelogError(why)
            print(f"CHANGELOG.md is ready for {a.tag}")
        else:
            out = (beta if a.cmd == "beta" else stable)(doc, a.version, a.date)
            with open(a.file, "w", encoding="utf-8") as f:
                f.write(out)
            print(f"CHANGELOG.md: {a.version} {a.cmd}")
    except ChangelogError as e:
        print(f"changelog: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
