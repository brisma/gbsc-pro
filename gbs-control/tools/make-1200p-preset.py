#!/usr/bin/env python3
# Usage (from gbs-control/): python3 tools/make-1200p-preset.py ntsc_1280x1024.h ntsc_1920x1200.h
#                             python3 tools/make-1200p-preset.py pal_1280x1024.h pal_1920x1200.h pal
# Builds ntsc_1920x1200.h from ntsc_1280x1024.h (same 162 MHz display clock, scaled horizontal path):
# 1920x1200 CVT-RB timing scaled to 162 MHz, 4:3 pillarbox through the display blanking window like the 1080p preset.
import re, sys
src, dst = sys.argv[1], sys.argv[2]
pal = len(sys.argv) > 3 and sys.argv[3] == 'pal'
lines = open(src).read().split('\n')
idx = {}
for i, l in enumerate(lines):
    g = re.search(r'(0x[0-9A-Fa-f]+|\d+)\s*,\s*//\s*s(\d)_([0-9A-Fa-f]+)', l)
    if g: idx[(int(g.group(2)), int(g.group(3), 16))] = i
def getb(s, a):
    return int(re.search(r'(0x[0-9A-Fa-f]+|\d+)\s*,', lines[idx[(s, a)]]).group(1), 0)
def setb(s, a, v):
    i = idx[(s, a)]
    lines[i] = re.sub(r'(0x[0-9A-Fa-f]+|\d+)(\s*,)', '0x%X\\2' % v, lines[i], count=1)
def setf(s, a, bit, width, value):
    nbytes = (bit + width + 7) // 8
    v = sum(getb(s, a + k) << (8 * k) for k in range(nbytes))
    mask = ((1 << width) - 1) << bit
    v = (v & ~mask) | ((value << bit) & mask)
    for k in range(nbytes): setb(s, a + k, (v >> (8 * k)) & 0xFF)
F = {  # tv5725.h UReg<seg, addr, bit, width>
    'VDS_HSYNC_RST': (3, 0x01, 0, 12), 'VDS_VSYNC_RST': (3, 0x02, 4, 11),
    'VDS_HB_ST': (3, 0x04, 0, 12), 'VDS_HB_SP': (3, 0x05, 4, 12), 'VDS_VB_ST': (3, 0x07, 0, 11), 'VDS_VB_SP': (3, 0x08, 4, 11),
    'VDS_HS_ST': (3, 0x0A, 0, 12), 'VDS_HS_SP': (3, 0x0B, 4, 12), 'VDS_VS_ST': (3, 0x0D, 0, 11), 'VDS_VS_SP': (3, 0x0E, 4, 11),
    'VDS_DIS_HB_ST': (3, 0x10, 0, 12), 'VDS_DIS_HB_SP': (3, 0x11, 4, 12), 'VDS_DIS_VB_ST': (3, 0x13, 0, 11), 'VDS_DIS_VB_SP': (3, 0x14, 4, 11),
    'VDS_HSCALE': (3, 0x16, 0, 10), 'VDS_VSCALE': (3, 0x17, 4, 10), 'GBS_PRESET_ID': (1, 0x2B, 0, 7),
}
if not pal:
    # 162 MHz: htotal 2188 = 74.04 kHz (CVT-RB 1920x1200 line rate), vtotal 1235 = 59.95 Hz
    HT, VT = 2188, 1235
    HS_ST, HS_SP = 16, 50                 # 34 px = the 32 px CVT-RB pulse at 154 MHz; same sync convention as 1024p
    ACT_SP, ACT_ST = 134, 2154            # back porch 84, 2020 px active = 1920 at 154 MHz, front porch 50
    PIC = 1683                            # 4:3 at 1200 lines = 1600 at 154 MHz
    MEM_SP, MEM_ST = 132, 88              # memory read leads the display window as in ntsc 1024p
    VB_SP, VB_ST = 23, 21                 # memory vertical: 10/12 lines before the display window, as ntsc 1024p
    VSCALE = 410                          # 1024 * 480 / 1200 = 409.6 (240p x 4.99)
    HSCALE = 602                          # ntsc 1024p fills 1980 px with 512: 512 * 1980 / 1683
    PID = 0x07
else:
    # pal 1024p clock (108 MHz): htotal 1749 = 61.75 kHz (CVT-RB 1920x1200 at 50 Hz), vtotal 1235 = 50.0 Hz
    HT, VT = 1749, 1235
    HS_ST, HS_SP = 0, 27                  # CVT-RB 32/80/1920/48 of 2080 scaled to 1749; pal 1024p sync convention
    ACT_SP, ACT_ST = 94, 1709             # back porch 67, 1615 px active, front porch 40
    PIC = 1346                            # 4:3 = 1600 of 1920
    MEM_SP, MEM_ST = 138, 60              # as pal 1024p
    VB_SP, VB_ST = 30, 13                 # as pal 1024p: 3 / 20 lines before the display window
    VSCALE = 437                          # pal 1024p 512 for 1024 lines: 512 * 1024 / 1200
    HSCALE = 714                          # pal 1024p fills 1644 px with 585: 585 * 1644 / 1346
    PID = 0x17
DIS_SP = ACT_SP + (ACT_ST - ACT_SP - PIC) // 2
DIS_ST = DIS_SP + PIC
V = dict(VDS_HSYNC_RST=HT, VDS_VSYNC_RST=VT, VDS_HS_ST=HS_ST, VDS_HS_SP=HS_SP, VDS_VS_ST=1, VDS_VS_SP=7,
         VDS_DIS_HB_SP=DIS_SP, VDS_DIS_HB_ST=DIS_ST, VDS_DIS_VB_SP=33, VDS_DIS_VB_ST=1233,
         VDS_HB_SP=DIS_SP - MEM_SP, VDS_HB_ST=DIS_ST - MEM_ST, VDS_VB_SP=VB_SP, VDS_VB_ST=VB_ST,
         VDS_VSCALE=VSCALE, VDS_HSCALE=HSCALE, GBS_PRESET_ID=PID)
for k, v in V.items(): setf(*F[k], v)
out = '\n'.join(lines).replace('ntsc_1280x1024[]', 'ntsc_1920x1200[]').replace('pal_1280x1024[]', 'pal_1920x1200[]')
if not pal:
    hdr = ('// 1920x1200 at 60 Hz (issue #35): built from ntsc_1280x1024 (same 162 MHz clock). CVT-RB 1920x1200\n'
           '// line rate scaled to 162 MHz (htotal %d, 74.04 kHz, vtotal %d, 59.95 Hz), 4:3 picture pillarboxed by the\n'
           '// display blanking window. Experimental: the MS9288A mode table is not public, check on hardware.\n') % (HT, VT)
else:
    hdr = ('// 1920x1200 at 50 Hz (issue #35): built from pal_1280x1024 (same 108 MHz clock). CVT-RB 1920x1200\n'
           '// at 50 Hz line rate (htotal %d, 61.75 kHz, vtotal %d, 50.0 Hz), 4:3 picture pillarboxed by the display\n'
           '// blanking window. Experimental: no standard 1200p50 mode, check the MS9288A on hardware.\n') % (HT, VT)
open(dst, 'w').write(hdr + out)
print('display window', DIS_SP, DIS_ST, 'memory', DIS_SP - MEM_SP, DIS_ST - MEM_ST)
