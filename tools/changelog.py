#!/usr/bin/env python3
"""The CHANGELOG.md sections a release reads and writes.

  changelog.py section X.Y.Z          print the body of a version's section (the GitHub release notes)
  changelog.py beta X.Y.Z             "## Unreleased" becomes "## X.Y.Z - beta" (a later beta adds the new entries)
                                      and goes away until the next change puts it back
  changelog.py stable X.Y.Z [--date YYYY-MM-DD]
                                      "## X.Y.Z - beta" becomes "## X.Y.Z - <date>" (default: today)
  changelog.py check vX.Y.Z-betaN | vX.Y.Z
                                      exits 1 with the reason when the tag has no matching section

Sections: "## Unreleased" on top while there are changes not yet released (every change for users adds a line there,
and the heading when it is not there), "## X.Y.Z - beta" while that version is in beta, "## X.Y.Z - YYYY-MM-DD" once
it is Stable.
"""
import argparse
import datetime
import os
import re
import sys

PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "CHANGELOG.md")
UNRELEASED = "## Unreleased"
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
TAG = re.compile(r"^v(\d+\.\d+\.\d+)(-beta\d+)?$")


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


def _find(sections, version, suffix=None):
    """Index of "## <version> - <suffix>" (any suffix when None), else -1."""
    for i, (heading, _) in enumerate(sections):
        m = re.match(r"^## (\S+) - (.+)$", heading)
        if m and m.group(1) == version and (suffix is None or re.fullmatch(suffix, m.group(2))):
            return i
    return -1


def section(doc, version):
    _, sections = _parse(doc)
    i = _find(sections, version)
    return sections[i][1] if i >= 0 else None


def beta(doc, version):
    preamble, sections = _parse(doc)
    if not sections or sections[0][0] != UNRELEASED or not sections[0][1]:
        raise ChangelogError(f'no "{UNRELEASED}" section with entries on top: nothing to release')
    entries = sections[0][1]
    if _find(sections, version, r"\d{4}-\d{2}-\d{2}") >= 0:
        raise ChangelogError(f"{version} is already released")
    i = _find(sections, version, "beta")
    if i >= 0:
        sections[i][1] = sections[i][1] + "\n\n" + entries
    else:
        sections.insert(1, [f"## {version} - beta", entries])
    del sections[0]   # no empty heading left on top: it reads as "this version is not out"
    return _join(preamble, sections)


def stable(doc, version, date):
    if not DATE.match(date):
        raise ChangelogError(f"date {date!r}: YYYY-MM-DD")
    preamble, sections = _parse(doc)
    i = _find(sections, version, "beta")
    if i < 0:
        raise ChangelogError(f'no "## {version} - beta" section: a Stable is the promotion of a beta')
    sections[i][0] = f"## {version} - {date}"
    return _join(preamble, sections)


def check(doc, tag):
    """"" when CHANGELOG.md is ready for this release tag, else the reason."""
    m = TAG.match(tag)
    if not m:
        return f"{tag}: not a release tag (vX.Y.Z-betaN or vX.Y.Z)"
    version, is_beta = m.group(1), bool(m.group(2))
    _, sections = _parse(doc)
    if is_beta:
        i = _find(sections, version, "beta")
        return "" if i >= 0 and sections[i][1] else f'CHANGELOG.md has no "## {version} - beta" section with entries'
    i = _find(sections, version, r"\d{4}-\d{2}-\d{2}")
    return "" if i >= 0 and sections[i][1] else f'CHANGELOG.md has no "## {version} - <date>" section with entries'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("section", "beta", "stable"):
        sub.add_parser(name).add_argument("version")
    sub.choices["stable"].add_argument("--date", default=datetime.date.today().isoformat())
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
            out = beta(doc, a.version) if a.cmd == "beta" else stable(doc, a.version, a.date)
            with open(a.file, "w", encoding="utf-8") as f:
                f.write(out)
            print(f"CHANGELOG.md: {a.version} {a.cmd}")
    except ChangelogError as e:
        print(f"changelog: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
