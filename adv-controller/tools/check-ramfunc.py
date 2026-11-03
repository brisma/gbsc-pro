#!/usr/bin/env python3
"""Post-link check of the HC32 self-update RAM routine.

Usage: check-ramfunc.py <fw_copy.o> <firmware.elf>
1. fw_copy.o: the .rel.ramfunc section may only relocate data (the RAM sector buffer): no call/jump relocations.
2. firmware.elf: every branch inside hc32CopyAndReset targets its own address range.
Fails (exit 1) with the offending lines.
"""
import re
import subprocess
import sys

PREFIX = "arm-none-eabi-"


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def check_relocs(obj):
    out = run(PREFIX + "readelf", "-rW", obj)
    bad, section = [], None
    for line in out.splitlines():
        m = re.match(r"Relocation section '(.+?)'", line)
        if m:
            section = m.group(1)
            continue
        if section != ".rel.ramfunc" or not re.match(r"^[0-9a-f]{8}\s", line):
            continue
        if re.search(r"R_ARM_(THM_)?(CALL|JUMP\d+|PC\d+|THM_JUMP\d+)", line):
            bad.append(line.strip())
        elif "sSectorBuf" not in line and ".bss" not in line:
            bad.append(line.strip())
    return bad


COND = r"(?:eq|ne|cs|cc|hs|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?"
DIRECT = re.compile(r"\t(?:bl|blx|b" + COND + r"|cbn?z)(?:\.[nw])?\t(?:r\d+, )?([0-9a-f]+) <")
# The routine never returns: any register-indirect branch or a write to pc leaves it
INDIRECT = re.compile(r"\t(?:bx|blx)(?:\.[nw])?\t|\t(?:pop|ldm\w*)(?:\.[nw])?\t.*\bpc\b|\t(?:mov|ldr)(?:\.[nw])?\tpc\b")


def branch_violations(dis, start, end):
    """Branch lines of an objdump listing that leave [start, end) or cannot be checked (register targets)."""
    bad = []
    for line in dis.splitlines():
        m = DIRECT.search(line)
        if m:
            if not (start <= int(m.group(1), 16) < end):
                bad.append(line.strip())
        elif INDIRECT.search(line):
            bad.append(line.strip())
    return bad


def check_branches(elf):
    syms = run(PREFIX + "nm", "-S", elf)
    m = re.search(r"^([0-9a-f]+) ([0-9a-f]+) T hc32CopyAndReset$", syms, re.M)
    if not m:
        return ["hc32CopyAndReset not found"]
    start, size = int(m.group(1), 16), int(m.group(2), 16)
    end = start + size
    dis = run(PREFIX + "objdump", "-d", "--start-address=%#x" % start, "--stop-address=%#x" % end, elf)
    bad = branch_violations(dis, start, end)
    if start < 0x1FFF8000:
        bad.append("hc32CopyAndReset is not in RAM (%#x)" % start)
    return bad


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    bad = check_relocs(sys.argv[1]) + check_branches(sys.argv[2])
    if bad:
        print("check-ramfunc: FAILED")
        for b in bad:
            print("  " + b)
        sys.exit(1)
    print("check-ramfunc: ok")


if __name__ == "__main__":
    main()
