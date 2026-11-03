#!/usr/bin/env python3
"""Host tests for the branch scan of tools/check-ramfunc.py (run by `make test`)."""
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("check_ramfunc", os.path.join(HERE, "check-ramfunc.py"))
cr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cr)

START, END = 0x1FFF8134, 0x1FFF8300
FAILS = 0


def expect(name, lines, n_bad):
    global FAILS
    bad = cr.branch_violations("\n".join(lines), START, END)
    if len(bad) != n_bad:
        FAILS += 1
        print("FAIL %s: expected %d violations, got %d: %s" % (name, n_bad, len(bad), bad))


expect("inside conditional", ["1fff815a:\td5fc      \tbpl.n\t1fff8156 <hc32CopyAndReset+0x22>"], 0)
expect("bic is not a branch", ["1fff814e:\tf421 3180 \tbic.w\tr1, r1, #65536\t@ 0x10000"], 0)
expect("b.n to flash (5 digits)", ["1fff8200:\te7fe      \tb.n\t16db8 <main+0x10>"], 1)
expect("bl to flash", ["1fff8200:\tf000 f800 \tbl\t12345 <EFM_SectorErase>"], 1)
expect("plain b outside", ["1fff8200:\te7fe      \tb\t1fff8400 <other>"], 1)
expect("cbz outside", ["1fff8200:\tb110      \tcbz\tr0, 1fff8400 <other>"], 1)
expect("bx register", ["1fff8200:\t4718      \tbx\tr3"], 1)
expect("blx register", ["1fff8200:\t4790      \tblx\tr2"], 1)
expect("pop pc", ["1fff8200:\tbd10      \tpop\t{r4, pc}"], 1)
if FAILS:
    sys.exit(1)
print("check-ramfunc tests: ok")
