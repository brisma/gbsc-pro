#include <stdio.h>
#include <string.h>
#include "osd-renderer.h"
#include "menu-model.h"
#include "osd-theme.h"
#include "menu-strings.h"
#include "../drivers/stv9426-map.h"
#include "../drivers/stv9426-udc.h"
#include "menu-charset.h"
#include "marquee.h"
#include "../util/str-copy.h"
using namespace stv;

static const uint8_t kDash = 0x3E;   // arrows are UDC chevrons: the ROM arrows 0x05/0x06/0x15/0x16 are boxed
static const uint8_t kLast = 28;            // last value column; column 29 holds the list scrollbar

OsdRenderer::OsdRenderer(Stv9426& d) : d_(d) {}

static const MarqueeTiming kScroll = {1200, 150};   // OSD: 1.2 s still at both ends, one cell every 150 ms

uint16_t OsdRenderer::scroll(MarqueeState& s, const char* text, uint16_t visible) {
    return marqueeOffset((uint16_t)strlen(text), visible, marqueeSince(s, text, now_), kScroll);   // new text: from its start
}

uint8_t OsdRenderer::titleCells(const MenuView& v) {
    uint8_t first = v.deep ? 3 : 1, last = kLast;
    if (v.posCount) {
        char pos[8];
        snprintf(pos, sizeof(pos), "%u/%u", v.posIndex, v.posCount);
        last = (uint8_t)(kLast - strlen(pos) - 1);    // one blank before the position
    }
    return (uint8_t)(last >= first ? last - first + 1 : 0);
}

// Cells the value of a plain row may use (after the label and one blank), as drawItem lays it out
static uint8_t valueCells(const MenuViewRow& r) {
    int avail = 28 - (1 + (int)strlen(r.label));
    return (uint8_t)(avail > 0 ? avail : 0);
}

uint8_t OsdRenderer::profileLabelCells(const MenuViewRow& r) {
    size_t vl = strlen(r.value);
    return (uint8_t)(vl ? kLast - vl - 1 : kLast);      // columns 1 .. (value - 2)
}

// Help row (list views only; confirm/revert/toast replace it and freeze nothing) and the selected profile row
uint16_t OsdRenderer::scrollSignature(const MenuView& v) {
    bool list = v.mode == VIEW_LIST || (v.mode == VIEW_OVERLAY && v.overMenu && v.overlay != OV_CONFIRM &&
                                        v.overlay != OV_REVERT && v.overlay != OV_TOAST && v.overlay != OV_SAME_SIGNAL);
    if (!list) return 0;
    uint16_t sig = scroll(helpScroll_, v.help, 28);
    if (v.mode == VIEW_LIST) sig = (uint16_t)(sig * 31 + scroll(titleScroll_, v.title, titleCells(v)));
    if (v.rowCount && v.cursorRow < v.rowCount) {
        const MenuViewRow& r = v.rows[v.cursorRow];
        if (r.kind == ROWK_PROFILE) sig = (uint16_t)(sig * 31 + scroll(rowScroll_, r.label, profileLabelCells(r)));
        else if (r.value[0] && r.kind == ROWK_SUB)       // a sub-menu with a value: one cell for the chevron
            sig = (uint16_t)(sig * 31 + scroll(rowScroll_, r.value, (uint16_t)(valueCells(r) - 1)));
        else if (r.value[0] && r.kind != ROWK_SLIDER && r.kind != ROWK_HEADER)
            sig = (uint16_t)(sig * 31 + scroll(rowScroll_, r.value, valueCells(r)));
    }
    return sig;
}

uint8_t OsdRenderer::a(uint8_t role) const { return osdAttr(theme_, role); }

static void addSpacing(Layout& l, uint16_t lines) {
    while (lines && l.count < kMaxContent) {
        uint8_t chunk = lines > 255 ? 255 : (uint8_t)lines;
        l.strips[l.count++] = {STRIP_SPACE, chunk, false};
        lines -= chunk;
    }
}

// Vertical place of a panel h lines high: top 6 %, centre, bottom 94 % of the active lines (nine positions
// for the menu and the pop-ups), never above the menu's first line
static int panelTop(int active, int h, uint8_t vrow, int minTop) {
    int top = vrow == 0 ? active * 6 / 100 : vrow == 2 ? active * 94 / 100 - h : active / 2 - h / 2;
    return top < minTop ? minTop : top;
}

bool OsdRenderer::applyLayout(uint8_t lay, uint8_t rows, uint8_t vrow) {
    const StvTiming& t = d_.timing();
    uint16_t rowLines = (uint16_t)(t.chMenu * (t.zoomMenu ? 2 : 1));
    Layout l;
    l.count = 0;
    int active = (int)d_.activeLines();
    if (lay == LAY_LIST) {   // title, 7 rows, help with no transparent lines between them
        addSpacing(l, (uint16_t)panelTop(active, 9 * rowLines, vrow, t.topMenu));
        if (l.count + 9 > kMaxContent) return false;
        for (uint8_t r = 0; r <= 8; r++) l.strips[l.count++] = {STRIP_ROW, r, t.zoomMenu};
    } else {
        addSpacing(l, (uint16_t)panelTop(active, rows * rowLines, vrow, t.topMenu));
        for (uint8_t r = 0; r < rows && l.count < kMaxContent; r++) l.strips[l.count++] = {STRIP_ROW, r, t.zoomMenu};
    }
    bool changed = d_.setLayout(l, t.chMenu);
    for (uint8_t r = 0; r < kRows; r++) d_.shadow().setWidth(r, kRowWidth);
    return changed;
}

// Yes / No centred on a row; the chosen one on the cursor colour with one cell each side
void OsdRenderer::drawChoice(uint8_t row, uint8_t choice, uint8_t base) {
    uint8_t c = a(R_C), ya = choice ? c : base, na = choice ? base : c;
    char yes[6], no[6];
    menuStrCopy(S_YES, yes, sizeof(yes));
    menuStrCopy(S_NO, no, sizeof(no));
    fillRow(row, base);
    fillRow(row, ya, 9, 14);
    put(row, 10, yes, ya);
    fillRow(row, na, 16, 20);
    put(row, 17, no, na);
}

void OsdRenderer::fillRow(uint8_t row, uint8_t attr, uint8_t from, uint8_t to) {
    for (uint8_t c = from; c < to; c++) d_.shadow().set(row, c, OSD_BLANK, attr);
}

void OsdRenderer::blankRows(uint8_t role) {
    for (uint8_t r = 0; r < kRows; r++) fillRow(r, a(role));
}

void OsdRenderer::putCode(uint8_t row, uint8_t col, uint8_t code, uint8_t attr) { d_.shadow().set(row, col, code, attr); }

uint8_t OsdRenderer::put(uint8_t row, uint8_t col, const char* s, uint8_t attr, uint8_t lastCol) {
    for (; *s && col <= lastCol; s++, col++) {
        if (*s == '%') { putCode(row, col, d_.glyph(UDC_PERCENT), attr); continue; }   // not in the ROM
        if (*s == '?') { putCode(row, col, d_.glyph(UDC_QUESTION), attr); continue; }  // not in the ROM either
        if (*s == ',') { putCode(row, col, d_.glyph(UDC_COMMA), attr); continue; }     // nor this (keyboard only)
        d_.shadow().set(row, col, *s == ' ' ? OSD_BLANK : stvCharCode(*s), attr);
    }
    return col;
}

void OsdRenderer::putRight(uint8_t row, const char* s, uint8_t attr, uint8_t endCol) {
    uint8_t n = (uint8_t)strlen(s);
    put(row, (uint8_t)(endCol + 1 - n), s, attr, endCol);
}

void OsdRenderer::drawBar(uint8_t row, uint8_t col, uint8_t cells, int halves, uint8_t attr) {
    for (uint8_t i = 0; i < cells; i++) {
        int h = halves - 2 * i;
        uint8_t g = h >= 2 ? UDC_SLIDER_FULL : h == 1 ? UDC_SLIDER_HALF : UDC_SLIDER_TRACK;
        putCode(row, (uint8_t)(col + i), d_.glyph(g), attr);
    }
}

bool OsdRenderer::rowFits(const char* label, const char* value, uint8_t kind) {
    int ll = (int)strlen(label), vl = (int)strlen(value);
    switch (kind) {
        case ROWK_SLIDER: return 23 - ll >= 3;
        case ROWK_SUB: return 1 + ll + 1 < kLast;
        case ROWK_HEADER: return 1 + ll + 1 <= kLast;
        case ROWK_ACTION: if (!vl) vl = 2; return 1 + ll + 1 + vl <= kLast + 1;
        default: return 1 + ll + 1 + vl <= kLast + 1;
    }
}

void OsdRenderer::drawItem(uint8_t row, const MenuViewRow& r) {
    bool sel = r.flags & ROWF_SELECTED, dis = r.flags & ROWF_DISABLED;
    if (r.kind == ROWK_HEADER) {
        uint8_t f = a(R_F);
        fillRow(row, f);
        char up[24];
        size_t n = 0;
        for (; r.label[n] && n < sizeof(up) - 1; n++) up[n] = (char)(r.label[n] >= 'a' && r.label[n] <= 'z' ? r.label[n] - 32 : r.label[n]);
        up[n] = '\0';
        uint8_t c = put(row, 1, up, f);
        for (c++; c <= kLast; c++) putCode(row, c, kDash, f);
        return;
    }
    uint8_t role = dis ? (sel ? R_DC : R_D) : sel ? R_C : R_N;
    uint8_t ra = a(role);
    fillRow(row, ra);
    if (r.kind == ROWK_PROFILE) {                      // the region always shows; the name is cut or scrolls
        uint8_t cells = profileLabelCells(r);
        uint16_t off = sel ? scroll(rowScroll_, r.label, cells) : 0;
        put(row, 1, r.label + (off < strlen(r.label) ? off : 0), ra, cells);
        if (r.value[0]) putRight(row, r.value, ra);
        return;
    }
    uint8_t labelEnd = put(row, 1, r.label, ra);   // first column after the label
    uint8_t va = ra;   // yellow = cursor only: a changed value in yellow looks like a stuck cursor
    if (r.kind == ROWK_SUB) {
        putCode(row, kLast, d_.glyph(UDC_CHEV_R), ra);
        int vl = (int)strlen(r.value), avail = kLast - 1 - labelEnd;   // Profiles shows the active one
        if (vl && avail >= 2) {
            if (vl <= avail) putRight(row, r.value, va, kLast - 1);
            else {                                       // the selected row scrolls it, else it is cut
                uint16_t off = sel ? scroll(rowScroll_, r.value, (uint16_t)avail) : 0;
                put(row, (uint8_t)(kLast - avail), r.value + (off < (uint16_t)vl ? off : 0), va, kLast - 1);
            }
        }
        return;
    }
    if (r.kind == ROWK_SLIDER && !dis && r.slider >= 0) {
        int cells = 23 - (int)strlen(r.label);
        if (cells > 10) cells = 10;
        if (cells < 3) cells = 3;
        drawBar(row, (uint8_t)(25 - cells), (uint8_t)cells, (r.slider * cells + 5) / 10, sel ? ra : a(R_BF));
        putRight(row, r.value, va);
        return;
    }
    const char* value = r.value;
    if (r.kind == ROWK_ACTION && !value[0]) { static char ok[4]; menuStrCopy(S_OK, ok, sizeof(ok)); value = ok; }
    int vl = (int)strlen(value);
    if (!vl) return;
    int avail = kLast - labelEnd;                   // cells after one blank following the label
    if (r.kind == ROWK_CYCLE && sel && !dis && vl + 2 <= avail) {
        putCode(row, kLast, d_.glyph(UDC_CHEV_R), ra);
        putRight(row, value, va, kLast - 1);
        putCode(row, (uint8_t)(kLast - 1 - vl), d_.glyph(UDC_CHEV_L), ra);
        return;
    }
    if (vl <= avail) { putRight(row, value, va); return; }
    if (avail < 2) return;
    if (sel) {                                      // the selected row scrolls a value that does not fit
        uint16_t off = scroll(rowScroll_, value, (uint16_t)avail);
        put(row, (uint8_t)(kLast - avail + 1), value + (off < (uint16_t)vl ? off : 0), va, kLast);
        return;
    }
    char clip[22];                                  // runtime overflow: keep the label, clip the value
    memcpy(clip, value, (size_t)avail - 1);
    clip[avail - 1] = '.';
    clip[avail] = '\0';
    putRight(row, clip, va);
}

void OsdRenderer::drawHelp(const MenuView& v) {
    char t[32];
    if (v.mode == VIEW_OVERLAY && v.overlay == OV_REVERT) {
        uint8_t aa = a(R_MSG);
        fillRow(8, aa);
        snprintf(t, sizeof(t), "%s  %ds", v.lines[1], v.value);
        put(8, 1, t, aa);
        return;
    }
    if (v.mode == VIEW_OVERLAY && v.overlay == OV_SAME_SIGNAL) {   // two rows over the list
        uint8_t h = a(R_H), c = a(R_C);
        fillRow(7, h);
        uint8_t col = put(7, 1, v.lines[0], h);
        put(7, (uint8_t)(col + 1), v.lines[1], h, kLast);
        fillRow(8, h);
        static const uint8_t kCol[3] = {1, 11, 22};
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t at = v.choice == i ? c : h;
            uint8_t end = put(8, kCol[i], v.lines[2 + i], at);
            if (v.choice == i) { fillRow(8, c, (uint8_t)(kCol[i] - 1), kCol[i]); fillRow(8, c, end, (uint8_t)(end + 1)); }
        }
        return;
    }
    if (v.mode == VIEW_OVERLAY && v.overlay == OV_CONFIRM) {   // the whole question on the last list row, the choice below
        uint8_t h = a(R_H);
        fillRow(7, h);
        put(7, 1, v.lines[0], h, kLast);
        drawChoice(8, v.choice, h);
        return;
    }
    if (v.mode == VIEW_OVERLAY && v.overlay == OV_TOAST) {
        uint8_t aa = a(R_MSG);
        fillRow(8, aa);
        put(8, 1, v.lines[0], aa);
        return;
    }
    uint8_t h = a(R_H);
    fillRow(8, h);
    uint16_t off = scroll(helpScroll_, v.help, 28);   // descriptions longer than the row scroll
    put(8, 1, v.help + (off < strlen(v.help) ? off : 0), h);
}

void OsdRenderer::drawList(const MenuView& v) {
    blankRows(R_N);
    uint8_t t = a(R_T);
    fillRow(0, t);
    uint8_t c = 1;
    if (v.deep) { putCode(0, 1, d_.glyph(UDC_CHEV_L), t); c = 3; }
    uint8_t tc = titleCells(v);                         // a long title (a profile name) scrolls
    uint16_t toff = scroll(titleScroll_, v.title, tc);
    put(0, c, v.title + (toff < strlen(v.title) ? toff : 0), t, (uint8_t)(c + tc - 1));
    if (v.posCount) {
        char pos[8];
        snprintf(pos, sizeof(pos), "%u/%u", v.posIndex, v.posCount);
        putRight(0, pos, t);
    }
    for (uint8_t i = 0; i < 7; i++) {
        if (i < v.rowCount) drawItem((uint8_t)(1 + i), v.rows[i]);
        else fillRow((uint8_t)(1 + i), a(R_N));
    }
    drawScrollbar(v);
    drawHelp(v);
}

// Column 29 of the list rows: a track with a thumb as long as the window's share of the level; it touches the
// top only at the top of the list and the bottom only at its end
void OsdRenderer::drawScrollbar(const MenuView& v) {
    uint8_t w = v.listWindow < 7 ? v.listWindow : 7;
    if (!w || v.listTotal <= w) return;
    uint8_t len = (uint8_t)((2 * w * w + v.listTotal) / (2 * v.listTotal));      // round(w * w / total)
    if (!len) len = 1;
    uint8_t start = (uint8_t)((2 * v.listTop * w + v.listTotal) / (2 * v.listTotal));
    if (start + len > w) start = (uint8_t)(w - len);
    uint8_t lo = v.listTop > 0 && len < w ? 1 : 0;                                  // first allowed thumb row
    if (start < lo) start = lo;
    if (v.listTop + w >= v.listTotal) start = (uint8_t)(w - len);                   // the end of the list
    else if (start + len >= w && w - 1 - len >= lo) start = (uint8_t)(w - 1 - len);
    uint8_t fgN = a(R_N) & 0x07, fgC = a(R_C) & 0x07;
    for (uint8_t i = 0; i < w; i++) {
        uint8_t row = (uint8_t)(1 + i);
        bool sel = i < v.rowCount && (v.rows[i].flags & ROWF_SELECTED);
        uint8_t bg = d_.shadow().attr(row, 1) & 0xF8;                             // the row's background
        bool thumb = i >= start && i < start + len;
        putCode(row, 29, d_.glyph(thumb ? UDC_BAR_THUMB : UDC_BAR_TRACK), (uint8_t)(bg | (sel ? fgC : fgN)));
    }
}

static uint8_t upperCopy(char* out, size_t n, const char* s) {
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) out[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    out[i] = '\0';
    return (uint8_t)i;
}

// Status pop-up: monitor icon and source on the title row, output and profile, then the third line
void OsdRenderer::drawStatus(const MenuView& v) {
    uint8_t t = a(R_T), n = a(R_N), ac = a(R_A);
    for (uint8_t r = 0; r < v.lineCount && r < 3; r++) fillRow(r, r ? n : t);
    putCode(0, 1, d_.glyph(UDC_MONITOR), t);
    put(0, 3, v.lines[0], t);
    put(1, 1, v.lines[1], n);
    if (v.lineCount > 2) put(2, 1, v.lines[2], ac);
}

void OsdRenderer::drawInfo(const MenuView& v) {
    blankRows(R_N);
    uint8_t t = a(R_T), n = a(R_N), lb = a(R_LB), ac = a(R_A), ok = a(R_GOOD);
    fillRow(0, t);
    putCode(0, 1, d_.glyph(UDC_SYNC), t);
    char title[24];
    menuStrCopy(S_SIGNAL_INFO, title, sizeof(title));
    put(0, 3, title, t);
    // Engine lines are "<4-letter code> <main>  <details>[ LOCK]"; drawn as the menu rows: the whole label on the
    // left, the value on the right (main in the accent colour, details plain), a value that does not fit ends with a
    // dot, the lock is its icon in the last cell
    static const struct { char code[5]; uint16_t label; } kLabels[] = {
        {"IN", S_INPUT}, {"OUT", S_OUTPUT}, {"OK", S_OK}, {"PROF", S_PROFILE}, {"NET", S_WIFI}, {"IP", S_IP}};
    for (uint8_t i = 0; i < v.lineCount && i < 6; i++) {
        uint8_t row = (uint8_t)(1 + i);
        const char* line = v.lines[i];
        fillRow(row, n);
        char code[6] = {0}, label[12] = {0};
        memcpy(code, line, strlen(line) < 5 ? strlen(line) : 5);
        for (char* c = code + strlen(code); c > code && c[-1] == ' ';) *--c = '\0';
        copyStr(label, sizeof(label), code);
        for (const auto& l : kLabels)
            if (!strcmp(code, l.code)) menuStrCopy(l.label, label, sizeof(label));
        uint8_t col = put(row, 1, label, lb);
        const char* val = strlen(line) > 5 ? line + 5 : "";
        const char* lock = !strcmp(code, "OUT") ? strstr(val, " LOCK") : nullptr;   // a profile name may say LOCK
        char text[29];
        size_t tl = lock ? (size_t)(lock - val) : strlen(val);
        if (tl >= sizeof(text)) tl = sizeof(text) - 1;
        memcpy(text, val, tl);
        text[tl] = '\0';
        const char* gap = strstr(text, "  ");
        size_t mainLen = gap ? (size_t)(gap - text) : strlen(text);
        if (gap) {                                       // "main  details" -> "main details"
            const char* d = gap;
            while (*d == ' ') d++;
            memmove(text + mainLen + 1, d, strlen(d) + 1);
        }
        bool net = !strcmp(code, "NET");
        int room = (int)kLast - (int)col - (lock ? 2 : 0) - (net ? 2 : 0);   // one blank cell after the label
        if (room < 1) continue;
        if ((int)strlen(text) > room) { text[room - 1] = '.'; text[room] = '\0'; }
        if (mainLen > strlen(text)) mainLen = strlen(text);
        uint8_t start = (uint8_t)(kLast + 1 - strlen(text) - (lock ? 2 : 0) - (net ? 2 : 0));
        if (net) { putCode(row, start, d_.glyph(UDC_WIFI), ac); start = (uint8_t)(start + 2); }
        char mainv[29];
        memcpy(mainv, text, mainLen);
        mainv[mainLen] = '\0';
        start = put(row, start, mainv, ac);
        put(row, start, text + mainLen, n);
        if (lock) putCode(row, kLast, d_.glyph(UDC_LOCK), ok);
    }
    uint8_t foot = (uint8_t)(1 + (v.lineCount < 6 ? v.lineCount : 6));
    char close[29];
    menuStrCopy(S_T_INFO_CLOSE, close, sizeof(close));
    fillRow(foot, a(R_H));
    put(foot, 1, close, a(R_H));
}

void OsdRenderer::drawPill(const MenuView& v) {
    blankRows(R_N);
    fillRow(0, OSD_ATTR_TRANSPARENT);
    if (v.overlay == OV_MUTE) {
        char word[8];
        uint8_t role = v.value ? R_BAD : R_T;
        menuStrCopy(v.value ? S_MUTE : S_ON, word, sizeof(word));
        char up[8];
        uint8_t len = upperCopy(up, sizeof(up), word);
        // padding, icon, space, word, padding: as wide as the word, centred or on its side
        uint8_t w = (uint8_t)(len + 4);
        uint8_t x = col_ == 0 ? 1 : col_ == 2 ? (uint8_t)(29 - w) : (uint8_t)(15 - w / 2);
        fillRow(0, a(role), x, (uint8_t)(x + w));
        putCode(0, (uint8_t)(x + 1), d_.glyph(v.value ? UDC_MUTED : UDC_SPEAKER), a(role));
        put(0, (uint8_t)(x + 3), up, a(role), (uint8_t)(x + 2 + len));
        return;
    }
    uint8_t t = a(R_T);
    int dx = col_ == 0 ? -7 : col_ == 2 ? 7 : 0;                  // cells 8..21; left: from 1, right: to 28
    fillRow(0, t, (uint8_t)(8 + dx), (uint8_t)(22 + dx));        // icon, bar, value + one padding cell each side
    putCode(0, (uint8_t)(9 + dx), d_.glyph(UDC_SPEAKER), t);
    drawBar(0, (uint8_t)(11 + dx), 5, (v.value * 10 + 50) / 100, t);   // percent, 5 cells
    char num[6];
    snprintf(num, sizeof(num), "%d%%", v.value);
    putRight(0, num, t, (uint8_t)(20 + dx));
}

void OsdRenderer::drawLow(const MenuView& v) {
    blankRows(R_N);
    uint8_t t = a(R_T), n = a(R_N), h = a(R_H);
    char buf[32];
    if (v.overlay == OV_ADJUST) {
        fillRow(0, t);
        upperCopy(buf, sizeof(buf), v.lines[0]);
        put(0, 1, buf, t);
        putRight(0, v.lines[1], t);
        fillRow(1, n);
        char hz[16], vt[16];
        menuStrCopy(S_T_HORIZONTAL, hz, sizeof(hz));
        menuStrCopy(S_T_VERTICAL, vt, sizeof(vt));
        putCode(1, 1, d_.glyph(UDC_CHEV_L), n);
        putCode(1, 2, d_.glyph(UDC_CHEV_R), n);
        uint8_t c = put(1, 4, hz, n);
        putCode(1, (uint8_t)(c + 2), d_.glyph(UDC_CHEV_U), n);
        putCode(1, (uint8_t)(c + 3), d_.glyph(UDC_CHEV_D), n);
        put(1, (uint8_t)(c + 5), vt, n);
        fillRow(2, h);
        put(2, 1, v.lines[2], h);
        return;
    }
    if (v.overlay == OV_SLIDER) {                      // quick slider: label, 10-cell bar, value (bottom row)
        fillRow(0, t);
        uint8_t vl = (uint8_t)strlen(v.lines[1]);
        uint8_t barCol = (uint8_t)(kLast + 1 - vl - 11);
        put(0, 1, v.lines[0], t, (uint8_t)(barCol - 2));
        drawBar(0, barCol, 10, v.choice, t);
        putRight(0, v.lines[1], t);
        return;
    }
    if (v.overlay == OV_CONFIRM) {                     // without a menu (ADV Controller boot prompt): question, then the choice
        fillRow(0, t);
        put(0, 1, v.lines[0], t, kLast);
        drawChoice(1, v.choice, t);
        return;
    }
    if (v.overlay == OV_TOAST) {                       // as wide as the text + one cell each side, like the mute box
        fillRow(0, OSD_ATTR_TRANSPARENT);
        uint8_t len = (uint8_t)strnlen(v.lines[0], 28);
        uint8_t w = (uint8_t)(len + 2);
        uint8_t x = col_ == 0 ? 1 : col_ == 2 ? (uint8_t)(29 - w) : (uint8_t)(15 - w / 2);
        if (x + w > 30) x = (uint8_t)(30 - w);
        fillRow(0, t, x, (uint8_t)(x + w));
        put(0, (uint8_t)(x + 1), v.lines[0], t, (uint8_t)(x + len));
        return;
    }
    fillRow(0, t);                                     // a countdown without a menu
    if (v.overlay == OV_REVERT) snprintf(buf, sizeof(buf), "%s  %ds", v.lines[0], v.value);
    else snprintf(buf, sizeof(buf), "%s", v.lines[0]);
    put(0, 1, buf, t);
}

// Keyboard: title, field with the caret cell, four key rows (one cell apart), help row
void OsdRenderer::drawKeyboard(const MenuView& v) {
    blankRows(R_N);
    uint8_t t = a(R_T), n = a(R_N), c = a(R_C), h = a(R_H);
    fillRow(0, t);
    put(0, 1, v.lines[0], t);
    fillRow(1, n);
    char field[29];
    snprintf(field, sizeof(field), "%s", v.lines[1]);
    for (char* p = field; *p; p++) if (*p != ',' && !menuCharOk(*p)) *p = '.';   // ',' is a UDC (put)
    put(1, 1, field, n);
    if (v.caret < 27) {                                // the cursor: the character under it, inverted
        char under[2] = {field[v.caret], 0};
        if (under[0]) put(1, (uint8_t)(1 + v.caret), under, c);
        else putCode(1, (uint8_t)(1 + v.caret), OSD_BLANK, c);
    }
    fillRow(2, n);
    uint8_t selRow = (uint8_t)(v.value / 16), selCol = (uint8_t)(v.value % 16);
    for (uint8_t r = 0; r < 3; r++) {
        uint8_t row = (uint8_t)(3 + r);
        fillRow(row, n);
        const char* keys = v.lines[2 + r];
        uint8_t first = strlen(keys) >= 14 ? 1 : 2;   // 13 keys from column 2, 14 (digits + comma) from column 1
        for (uint8_t k = 0; keys[k]; k++) {
            char ch[2] = {keys[k], 0};
            uint8_t at = (r == selRow && k == selCol) ? c : n;
            put(row, (uint8_t)(first + 2 * k), ch, at);
        }
    }
    // aA, cursor left/right (ROM arrows: no '<' '>' in the ROM), Space, Del, Done
    static const uint8_t kSpecCol[6] = {2, 6, 9, 12, 19, 24};
    static const char* const kSpec[6] = {"aA", "", "", "Space", "Del", "Done"};
    fillRow(6, n);
    for (uint8_t k = 0; k < 6; k++) {
        bool sel = selRow == 3 && selCol == k;
        uint8_t w = k == 1 || k == 2 ? 1 : (uint8_t)strlen(kSpec[k]);
        if (sel) fillRow(6, c, (uint8_t)(kSpecCol[k] - 1), (uint8_t)(kSpecCol[k] + w + 1));
        if (k == 1) putCode(6, kSpecCol[k], d_.glyph(UDC_CHEV_L), sel ? c : n);
        else if (k == 2) putCode(6, kSpecCol[k], d_.glyph(UDC_CHEV_R), sel ? c : n);
        else put(6, kSpecCol[k], kSpec[k], sel ? c : n);
    }
    fillRow(7, h);
    put(7, 1, v.help, h);
}

void OsdRenderer::drawUpdate(const MenuView& v) {
    blankRows(R_N);
    uint8_t t = a(R_T);
    fillRow(0, t);
    put(0, 1, v.lines[0], t);
    fillRow(1, a(R_N));
    if (v.choice != UPV_PROGRESS) {                      // the outcome (or Verifying) replaces the bar, centred
        uint8_t attr = v.choice == UPV_DONE ? a(R_GOOD) : v.choice == UPV_FAILED ? a(R_BAD) : a(R_N);
        put(1, (uint8_t)((kLast + 1 - strlen(v.lines[1])) / 2), v.lines[1], attr);
        return;
    }
    char pct[6];
    snprintf(pct, sizeof(pct), "%d%%", v.value);   // the OLED shows "N%" too
    putRight(0, pct, t);
    drawBar(1, 2, 26, v.value * 52 / 100, a(R_BF));
}

void OsdRenderer::render(const MenuView& v, uint8_t theme, uint32_t nowMs) {
    if (d_.exclusive()) { rev_ = 0xFFFF; return; }   // another screen owns the chip; redraw when it is released
    if (v.mode == VIEW_CLOSED) {
        if (open_) {
            for (uint8_t r = 0; r < kRows; r++) fillRow(r, OSD_ATTR_TRANSPARENT);   // nothing left in always-on mode
            d_.flush();
            d_.powerOff();
            open_ = false;
        }
        rev_ = 0xFFFF;
        return;
    }
    open_ = true;
    d_.requestOn();
    bool overList = v.mode == VIEW_LIST ||
                    (v.overMenu && (v.overlay == OV_CONFIRM || v.overlay == OV_REVERT || v.overlay == OV_TOAST ||
                        v.overlay == OV_SAME_SIGNAL));
    uint8_t lay = LAY_LIST, rows = 9;
    if (!overList) {
        switch (v.overlay) {
            case OV_INFO: lay = LAY_INFO; rows = (uint8_t)(v.lineCount + 2); break;
            case OV_MUTE: case OV_VOLUME: lay = LAY_PILL; rows = 1; break;
            case OV_STATUS: lay = LAY_STATUS; rows = v.lineCount; break;
            case OV_UPDATE: lay = LAY_UPDATE; rows = 2; break;
            case OV_ADJUST: lay = LAY_LOW; rows = 3; break;
            case OV_KEYBOARD: lay = LAY_INFO; rows = 8; break;
            case OV_CONFIRM: lay = LAY_CENTER; rows = 2; break;
            default: lay = LAY_CENTER; rows = 1; break;
        }
    }
    now_ = nowMs;
    uint16_t sig = scrollSignature(v);
    bool popup = lay == LAY_PILL || lay == LAY_STATUS || (lay == LAY_CENTER && v.overlay == OV_TOAST);   // a toast alone too
    uint8_t place = popup ? popup_ : lay == LAY_LOW ? (uint8_t)(6 + pos_ % 3) : pos_;
    col_ = (uint8_t)(place % 3);
    d_.setHAlign(col_);
    bool relayout = applyLayout(lay, rows, (uint8_t)(place / 3));
    if (!relayout && v.rev == rev_ && theme == theme_ && place == place_ && sig == scrollSig_ &&
        d_.output() == out_ && d_.generation() == gen_)
        return;
    scrollSig_ = sig;
    rev_ = v.rev;
    place_ = place;
    theme_ = theme;
    out_ = d_.output();
    d_.glyphFrame();                                   // glyphs of this screen are loaded on demand
    switch (lay) {
        case LAY_LIST: drawList(v); break;
        case LAY_INFO: if (v.overlay == OV_KEYBOARD) drawKeyboard(v); else drawInfo(v); break;
        case LAY_PILL: drawPill(v); break;
        case LAY_STATUS: drawStatus(v); break;
        case LAY_UPDATE: drawUpdate(v); break;
        default: drawLow(v); break;
    }
    d_.flush();
    gen_ = d_.generation();
}
