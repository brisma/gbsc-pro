#include "oled-renderer.h"
#include "marquee.h"
#include <string.h>
#include <stdio.h>

static const int16_t kW = 128;
static const int16_t kTitleH = 14;        // inverted title band rows 0..12 (text rows 3..11), rows from 14
static const int16_t kBandH = 13;
static const int16_t kRowH = 10;
static const int16_t kTextDy = -2;        // ArialMT_Plain_10 glyphs start 3 rows into their cell (never at y < 0:
                                          // the library loses the top rows of glyphs drawn above the screen)
static const int16_t kLabelX = 4, kBackLabelX = 12, kRight = 127;   // 127 (not 126) keeps every row fitting
static const int16_t kGap = 2;                 // label to value: 2 fits "Menu position  Bottom right"
static const int16_t kBarX = 72, kBarW = 30;   // slider column: 1 px track, 5 px fill
static const int16_t kSliderLabelW = 66, kSliderValueW = 22;


static uint16_t textW(OLEDDisplay& d, const char* s) { return d.getStringWidth(s, (uint16_t)strlen(s)); }

// Copy of text cut with a trailing '.' so that it fits maxW pixels
static void clipText(OLEDDisplay& d, const char* text, int16_t maxW, char* out, size_t n) {
    snprintf(out, n, "%s", text);
    if (maxW <= 0) { out[0] = 0; return; }
    if (textW(d, out) <= maxW) return;
    size_t len = strlen(out);
    while (len > 0) {
        out[--len] = 0;
        char t[40];
        snprintf(t, sizeof(t), "%s.", out);
        if (textW(d, t) <= maxW) { snprintf(out, n, "%s", t); return; }
    }
}

// Home screen and info panel icons, one byte per row, bit 7 = leftmost pixel (the arrow is shared with the screensaver)
static const uint8_t kIcoArrow[7] = {0x08, 0x0C, 0xFE, 0xFF, 0xFE, 0x0C, 0x08};   // 8 x 7
static const uint8_t kIcoLock[7] = {0x38, 0x44, 0x44, 0xFE, 0xEE, 0xEE, 0xFE};    // 7 x 7
static const uint8_t kIcoSpeaker[7] = {0x10, 0x34, 0xF2, 0xF2, 0xF2, 0x34, 0x10}; // 7 x 7
static const uint8_t kIcoMuted[7] = {0x10, 0x30, 0xF5, 0xF2, 0xF5, 0x30, 0x10};   // 8 x 7

static const MarqueeTiming kRowScroll = {1500, 300};    // selected row label, loops
static const MarqueeTiming kPassScroll = {1200, 180};   // band description and home profile: one pass (< 15 s)
static const uint32_t kBandRestMs = 1500;               // cursor still this long before the band pass

uint8_t OledRenderer::skipToFit(const char* text, int16_t maxW) {
    uint8_t k = 0;
    size_t len = strlen(text);
    while (k < len && (int16_t)textW(d_, text + k) > maxW) k++;
    return k;
}

void OledRenderer::clipped(int16_t x, int16_t y, const char* text, int16_t maxW) {
    char t[40];
    clipText(d_, text, maxW, t, sizeof(t));
    d_.drawString(x, y, t);
}

void OledRenderer::centered(int16_t y, const char* text) {
    char t[40];
    clipText(d_, text, kW - 2, t, sizeof(t));
    d_.setTextAlignment(TEXT_ALIGN_CENTER);
    d_.drawString(kW / 2, y, t);
    d_.setTextAlignment(TEXT_ALIGN_LEFT);
}

// Value shown in the value column: "-" for disabled rows, ">" for sub-menus
static const char* shownValue(const MenuViewRow& r) {
    if (r.kind == ROWK_SUB) return ">";
    if (r.kind == ROWK_HEADER || (r.flags & ROWF_BACK)) return "";
    if ((r.flags & ROWF_DISABLED) && r.value[0]) return "-";
    return r.value;
}

bool OledRenderer::rowFits(OLEDDisplay& d, const char* label, const char* value, uint8_t kind) {
    d.setFont(ArialMT_Plain_10);
    if (kind == ROWK_SLIDER) return (int16_t)textW(d, value) <= kSliderValueW;   // the label is clipped to 66 px
    int16_t room = kRight - kLabelX;
    int16_t need = (int16_t)textW(d, label);
    if (kind == ROWK_SUB) need += kGap + (int16_t)textW(d, value[0] ? " >" : ">") + (int16_t)textW(d, value);
    else if (value[0]) need += kGap + (int16_t)textW(d, value);
    return need <= room;
}

void OledRenderer::drawRow(const MenuViewRow& r, int16_t y) {
    bool sel = r.flags & ROWF_SELECTED, edit = r.flags & ROWF_EDITING;
    d_.setColor(WHITE);
    if (sel && !edit) { d_.fillRect(0, y, kW, kRowH); d_.setColor(BLACK); }
    if (edit) d_.drawRect(0, y, kW, kRowH);
    OLEDDISPLAY_COLOR ink = (sel && !edit) ? BLACK : WHITE;
    int16_t ty = y + kTextDy;
    if ((r.flags & ROWF_CHANGED) && !edit) d_.fillRect(1, y + 4, 2, 2);   // it would touch the edit outline
    int16_t lx = kLabelX;
    if (r.flags & ROWF_BACK) { d_.drawString(kLabelX, ty, "<"); lx = kBackLabelX; }
    if (r.kind == ROWK_HEADER) {
        clipped(lx, ty, r.label, kRight - lx);
        int16_t end = lx + (int16_t)textW(d_, r.label) + 4;
        if (end < 125) d_.drawHorizontalLine(end, y + 5, 126 - end);
        return;
    }
    const char* val = shownValue(r);
    char v[40];
    snprintf(v, sizeof(v), "%s", val);
    int16_t labelW = (int16_t)textW(d_, r.label);
    bool slider = (r.flags & ROWF_SLIDER) && r.slider >= 0 && !(r.flags & ROWF_DISABLED);
    bool subValue = r.kind == ROWK_SUB && r.value[0];   // "<value> >" on the Profiles row
    int16_t subW = subValue ? (int16_t)textW(d_, " >") : 0;
    if (subValue) snprintf(v, sizeof(v), "%s", r.value);
    if (r.kind == ROWK_LIVE && v[0]) {                  // free text: the value is clipped, never the label
        int16_t room = kRight - (lx + labelW + kGap);
        clipText(d_, val, room, v, sizeof(v));
    } else if (v[0] && r.kind != ROWK_PROFILE && !slider && (int16_t)textW(d_, v) > kRight - (lx + labelW + kGap) - subW) {
        // a long value (the active profile on Save) never covers its label: selected it scrolls, else it is cut
        int16_t room = kRight - (lx + labelW + kGap) - subW;
        const char* base = subValue ? r.value : val;
        const char* from = base;
        if (sel && !edit) from += marqueeOffset((uint16_t)(skipToFit(base, room) + 1), 1, since(rowScroll_, base), kRowScroll);
        clipText(d_, from, room, v, sizeof(v));
    }
    if (subValue) { size_t l = strlen(v); snprintf(v + l, sizeof(v) - l, " >"); }
    int16_t vw = (int16_t)textW(d_, v);
    int16_t vx = kRight - vw;
    int16_t limit = v[0] ? vx - kGap : kRight;
    if (slider) {                                       // fixed column, 1 px track, 5 px fill
        d_.setColor(ink);
        d_.fillRect(kBarX, y + 4, kBarW, 1);
        int16_t fill = (int16_t)((r.slider * kBarW * 2 + 20) / 40);
        d_.fillRect(kBarX, y + 2, fill < 1 ? 1 : fill, 5);
        limit = lx + kSliderLabelW;
    }
    d_.setColor(ink);
    const char* label = r.label;
    if (sel && !edit && (int16_t)textW(d_, label) > limit - lx) {   // the selected long label scrolls
        uint8_t k = skipToFit(label, limit - lx);
        label += marqueeOffset((uint16_t)(k + 1), 1, since(rowScroll_, label), kRowScroll);
    }
    clipped(lx, ty, label, limit - lx);
    if (!v[0]) return;
    if (edit) {
        d_.setColor(WHITE);
        d_.fillRect(vx - 2, y + 1, vw + 4, kRowH - 2);
        d_.setColor(BLACK);
    }
    d_.drawString(vx, ty, v);
    d_.setColor(WHITE);
}

// Inverted title band: rows 0..12, black title at x 2, black position right-aligned to x 126
void OledRenderer::band(const char* title, const char* right) {
    d_.setColor(WHITE);
    d_.fillRect(0, 0, kW, kBandH);
    d_.setColor(BLACK);
    int16_t pw = 0;
    if (right && right[0]) {
        pw = (int16_t)textW(d_, right);
        d_.setTextAlignment(TEXT_ALIGN_RIGHT);
        d_.drawString(kRight, 0, right);
        d_.setTextAlignment(TEXT_ALIGN_LEFT);
    }
    clipped(2, 0, title, kRight - (pw ? pw + 5 : 0) - 2);
    d_.setColor(WHITE);
}

void OledRenderer::plainRow(int16_t y, const char* label, const char* value, MarqueeState* scroll) {
    int16_t lw = (int16_t)textW(d_, label);
    d_.drawString(kLabelX, y + kTextDy, label);
    char v[40];
    int16_t room = kRight - (kLabelX + lw + kGap);
    if (scroll && (int16_t)textW(d_, value) > room) {   // drawn from the left edge of the room while it moves,
        room -= 2;                                      // 2 px further from the label (it read "WiFiVodafone")
        value += marqueeOffset((uint16_t)(skipToFit(value, room) + 1), 1, since(*scroll, value), kRowScroll);
        clipText(d_, value, room, v, sizeof(v));
        d_.drawString(kRight - room, y + kTextDy, v);
        return;
    }
    clipText(d_, value, room, v, sizeof(v));
    d_.setTextAlignment(TEXT_ALIGN_RIGHT);
    d_.drawString(kRight, y + kTextDy, v);
    d_.setTextAlignment(TEXT_ALIGN_LEFT);
}

void OledRenderer::bigValue(const char* text) {
    d_.setFont(ArialMT_Plain_16);
    centered(18, text);
    d_.setFont(ArialMT_Plain_10);
}

void OledRenderer::frameBar(int16_t fill108) {
    d_.drawRect(8, 44, 112, 10);
    if (fill108 > 108) fill108 = 108;
    if (fill108 > 0) d_.fillRect(10, 46, fill108, 6);
}

void OledRenderer::feet(const char* a, const char* b) {
    if (!b) { centered(52, a); return; }
    centered(40, a);
    centered(51, b);
}

// Rows of the info panel from the engine lines: Input, Output (+ lock icon), Profile, WiFi, IP
uint8_t oledInfoRows(const MenuView& v, OledInfoRow out[5]) {
    OledInfoRow in = {}, outr = {}, prof = {}, net = {}, ip = {};
    bool hIn = false, hOut = false, hProf = false, hNet = false, hIp = false;
    for (uint8_t i = 0; i < v.lineCount && i < MENU_VIEW_ROWS; i++) {
        const char* l = v.lines[i];
        if (strlen(l) < 5) continue;
        char val[29];
        const char* p = l + 5;
        const char* gap = strstr(p, "  ");
        size_t n = gap ? (size_t)(gap - p) : strlen(p);
        if (n >= sizeof(val)) n = sizeof(val) - 1;
        memcpy(val, p, n);
        val[n] = 0;
        if (!strncmp(l, "IN ", 3)) { snprintf(in.label, sizeof(in.label), "Input"); snprintf(in.value, sizeof(in.value), "%s", val); hIn = true; }
        else if (!strncmp(l, "OUT", 3)) {                 // resolution + rate ("60.0Hz" -> "60Hz")
            char hz[12] = "";
            if (gap) {
                const char* r = gap;
                while (*r == ' ') r++;
                size_t k = 0;
                while (r[k] && r[k] != ' ' && k < sizeof(hz) - 1) { hz[k] = r[k]; k++; }
                hz[k] = 0;
                char* dot = strstr(hz, ".0Hz");
                if (dot) memmove(dot, dot + 2, strlen(dot + 2) + 1);
            }
            snprintf(outr.label, sizeof(outr.label), "Output");
            snprintf(outr.value, sizeof(outr.value), hz[0] ? "%s %s" : "%s", val, hz);
            outr.lock = strstr(l, " LOCK") != nullptr;
            hOut = true;
        } else if (!strncmp(l, "PROF", 4)) {             // the whole name (user names may hold double spaces)
            snprintf(prof.label, sizeof(prof.label), "Profile"); snprintf(prof.value, sizeof(prof.value), "%s", p); hProf = true;
        } else if (!strncmp(l, "NET", 3)) {              // the whole network name, as the profile
            snprintf(net.label, sizeof(net.label), "WiFi"); snprintf(net.value, sizeof(net.value), "%s", p); hNet = true;
        } else if (!strncmp(l, "IP ", 3)) {
            snprintf(ip.label, sizeof(ip.label), "IP"); snprintf(ip.value, sizeof(ip.value), "%s", val); hIp = true;
        }
    }
    uint8_t k = 0;
    if (hIn) out[k++] = in;
    if (hOut) out[k++] = outr;
    if (hProf) out[k++] = prof;
    if (hNet) out[k++] = net;
    if (hIp && k < 5) out[k++] = ip;
    return k;
}

static const int16_t kLockW = 9;                       // 7 px icon + 2 px gap

void oledInfoShownValue(OLEDDisplay& d, const OledInfoRow& r, char* out, size_t n) {
    d.setFont(ArialMT_Plain_10);
    int16_t room = kRight - (kLabelX + (int16_t)textW(d, r.label) + kGap) - (r.lock ? kLockW : 0);
    snprintf(out, n, "%s", r.value);
    const char* x = strchr(r.value, 'x');
    if ((int16_t)textW(d, out) > room && x && x > r.value) {   // "<w>x<h> <rate>" -> "<h>p <rate>"
        const char* sp = strchr(x, ' ');
        size_t hl = sp ? (size_t)(sp - x - 1) : strlen(x + 1);
        snprintf(out, n, "%.*sp%s", (int)hl, x + 1, sp ? sp : "");
    }
    char t[40];
    clipText(d, out, room, t, sizeof(t));
    snprintf(out, n, "%s", t);
}

void OledRenderer::drawList(const MenuView& v) {
    char pos[8] = "";
    if (v.posCount && v.posIndex) snprintf(pos, sizeof(pos), "%u/%u", v.posIndex, v.posCount);
    // The selected row's description, once, after the cursor rests 1.5 s (not while editing a value)
    const MenuViewRow* cur = v.rowCount && v.cursorRow < v.rowCount ? &v.rows[v.cursorRow] : nullptr;
    bool shown = false;
    if (cur && v.help[0] && !(cur->flags & ROWF_EDITING)) {
        char key[100];
        snprintf(key, sizeof(key), "%s|%s", cur->label, v.help);   // a new row restarts it, even with the same text
        uint32_t e = since(bandScroll_, key);
        if (e >= kBandRestMs) {
            e -= kBandRestMs;
            uint8_t k = skipToFit(v.help, kW - 4);
            uint32_t cycle = k ? marqueeCycleMs((uint16_t)(k + 1), 1, kPassScroll) : 3000;
            if (e < cycle) { band(v.help + (k ? marqueeOffset((uint16_t)(k + 1), 1, e, kPassScroll) : 0)); shown = true; }
        }
    }
    if (!shown) {                                       // a long title (a profile name) scrolls, looping
        const char* title = v.title;
        int16_t room = kRight - (pos[0] ? (int16_t)textW(d_, pos) + 5 : 0) - 2;
        if ((int16_t)textW(d_, title) > room) {
            uint8_t k = skipToFit(title, room);
            title += marqueeOffset((uint16_t)(k + 1), 1, since(titleScroll_, title), kRowScroll);
        }
        band(title, pos);
    }
    uint8_t start = 0;
    if (v.rowCount > kRows) {
        int s = (int)v.cursorRow - 2;
        if (s < 0) s = 0;
        if (s > v.rowCount - kRows) s = v.rowCount - kRows;
        start = (uint8_t)s;
    }
    // no "more below" marker: it covered the last row's value, and the title's n/m tells the position
    for (uint8_t i = 0; i < kRows && start + i < v.rowCount; i++)
        drawRow(v.rows[start + i], (int16_t)(kTitleH + kRowH * i));
}

void OledRenderer::drawOverlay(const MenuView& v) {
    char t[40];
    d_.setColor(WHITE);
    switch (v.overlay) {
        case OV_VOLUME:
            band(v.lines[0]);
            snprintf(t, sizeof(t), "%d%%", v.value);
            bigValue(t);
            frameBar((int16_t)((v.value * 108 + 50) / 100));   // percent
            break;
        case OV_MUTE:
            band("Sound");
            bigValue(v.lines[0]);
            break;
        case OV_INFO: {
            band("Signal info");
            OledInfoRow r[5];
            uint8_t n = oledInfoRows(v, r);
            for (uint8_t i = 0; i < n; i++) {               // long profile and network names scroll
                int16_t y = (int16_t)(kTitleH + kRowH * i);
                if (!strcmp(r[i].label, "Output")) {
                    char val[32];
                    oledInfoShownValue(d_, r[i], val, sizeof(val));
                    d_.drawString(kLabelX, y + kTextDy, r[i].label);
                    d_.setTextAlignment(TEXT_ALIGN_RIGHT);
                    d_.drawString(kRight - (r[i].lock ? kLockW : 0), y + kTextDy, val);
                    d_.setTextAlignment(TEXT_ALIGN_LEFT);
                    if (r[i].lock) icon(kRight - 7, y + 2, kIcoLock, 7);
                    continue;
                }
                MarqueeState* sc = !strcmp(r[i].label, "Profile") ? &infoScroll_ : !strcmp(r[i].label, "WiFi") ? &wifiScroll_ : nullptr;
                plainRow(y, r[i].label, r[i].value, sc);
            }
            break;
        }
        case OV_CONFIRM: {
            band("Confirm");
            const char* label = v.lines[0];             // the question; a long one wraps on two lines, 3 px higher
            d_.setTextAlignment(TEXT_ALIGN_CENTER);
            d_.drawStringMaxWidth(kW / 2, textW(d_, label) > 124 ? 15 : 18, 124, label);
            d_.setTextAlignment(TEXT_ALIGN_LEFT);
            const char* lab[2] = {"Yes", "No"};
            for (uint8_t b = 0; b < 2; b++) {
                int16_t x = b == 0 ? 14 : 70;
                bool on = (b == 0) == (v.choice == 1);
                d_.setColor(WHITE);
                if (on) d_.fillRect(x, 44, 44, 16); else d_.drawRect(x, 44, 44, 16);
                d_.setColor(on ? BLACK : WHITE);
                d_.setTextAlignment(TEXT_ALIGN_CENTER);
                d_.drawString(x + 22, 45, lab[b]);
                d_.setTextAlignment(TEXT_ALIGN_LEFT);
            }
            d_.setColor(WHITE);
            break;
        }
        case OV_STATUS: {                               // status pop-up: source, output, profile, third line
            band("Status");
            char out[29];
            const char* hz = strstr(v.lines[1], "Hz ");
            size_t k = hz ? (size_t)(hz - v.lines[1]) + 2 : strlen(v.lines[1]);
            if (k >= sizeof(out)) k = sizeof(out) - 1;
            memcpy(out, v.lines[1], k);
            out[k] = '\0';
            const char* rows[4] = {v.lines[0], out, hz ? hz + 3 : "", v.lineCount > 2 ? v.lines[2] : ""};
            for (uint8_t i = 0; i < 4; i++) clipped(2, (int16_t)(kTitleH + 2 + 12 * i), rows[i], 124);
            break;
        }
        case OV_SAME_SIGNAL: {                          // the matching profile, one choice at a time
            band(v.lines[0]);
            d_.setTextAlignment(TEXT_ALIGN_CENTER);
            d_.drawStringMaxWidth(kW / 2, 18, 124, v.lines[1]);
            d_.setColor(WHITE);
            d_.fillRect(14, 42, 100, 16);
            d_.setColor(BLACK);
            d_.drawString(kW / 2, 43, v.lines[2 + (v.choice < 3 ? v.choice : 0)]);
            d_.drawString(20, 43, "<");
            d_.drawString(108, 43, ">");
            d_.setColor(WHITE);
            d_.setTextAlignment(TEXT_ALIGN_LEFT);
            break;
        }
        case OV_REVERT: {
            char label[29], value[29];
            const char* sep = strstr(v.lines[0], ": ");
            size_t n = sep ? (size_t)(sep - v.lines[0]) : strlen(v.lines[0]);
            if (n >= sizeof(label)) n = sizeof(label) - 1;
            memcpy(label, v.lines[0], n);
            label[n] = 0;
            snprintf(value, sizeof(value), "%s", sep ? sep + 2 : "");
            band(label);
            bigValue(value);
            snprintf(t, sizeof(t), "Keep? %ds", v.value);
            feet(t, "Click keep  Hold undo");
            break;
        }
        case OV_TOAST:
            band("Notice");
            d_.setTextAlignment(TEXT_ALIGN_CENTER);
            d_.drawStringMaxWidth(kW / 2, textW(d_, v.lines[0]) > 120 ? 20 : 30, 120, v.lines[0]);
            d_.setTextAlignment(TEXT_ALIGN_LEFT);
            break;
        case OV_UPDATE: {
            band(v.lines[0]);
            if (v.choice != UPV_PROGRESS) {                  // the outcome (or Verifying) replaces the bar
                d_.setTextAlignment(TEXT_ALIGN_CENTER);
                d_.drawStringMaxWidth(kW / 2, 30, 120, v.lines[1]);
                d_.setTextAlignment(TEXT_ALIGN_LEFT);
                break;
            }
            int pct = v.value < 0 ? 0 : v.value;
            snprintf(t, sizeof(t), "%d%%", pct);
            bigValue(t);
            frameBar((int16_t)((pct * 108 + 50) / 100));
            break;
        }
        case OV_SLIDER:
            band(v.lines[0]);
            bigValue(v.lines[1]);
            frameBar((int16_t)((v.choice * 108 + 10) / 20));
            break;
        case OV_KEYBOARD: drawKeyboard(v); break;
        case OV_TVOSD:
            band(v.lines[0]);
            snprintf(t, sizeof(t), "%ds", v.value);
            bigValue(t);
            feet("Turn = change", "Click next  Hold exit");
            break;
        case OV_ADJUST:
            band(v.lines[0]);
            bigValue(v.lines[1]);
            feet(v.choice ? "Turn = Vertical" : "Turn = Horizontal", "Click H/V  Hold done");
            break;
        default:
            break;
    }
}

// Keyboard: title, field (its end) with a caret bar, four key rows of 9 px keys, selected key inverted
void OledRenderer::drawKeyboard(const MenuView& v) {
    band(v.lines[0]);                                   // a flashed keyboard message replaces the purpose
    // the field window (around the cursor) cut further from the left until it fits, never past the cursor
    const char* f = v.lines[1];
    uint8_t caret = v.caret;
    while (caret && textW(d_, f) > 118) { f++; caret--; }
    d_.drawString(2, 11, f);
    char before[40];
    snprintf(before, sizeof(before), "%.*s", (int)caret, f);
    int16_t cx = (int16_t)(3 + textW(d_, before));
    d_.fillRect(cx, 14, 1, 9);
    uint8_t selRow = (uint8_t)(v.value / 16), selCol = (uint8_t)(v.value % 16);
    for (uint8_t r = 0; r < 4; r++) {
        int16_t top = (int16_t)(24 + 10 * r);
        if (r < 3) {
            const char* keys = v.lines[2 + r];
            for (uint8_t k = 0; keys[k]; k++) {
                int16_t x = (int16_t)(1 + 9 * k);
                bool sel = r == selRow && k == selCol;
                d_.setColor(WHITE);
                if (sel) { d_.fillRect(x, top, 9, 10); d_.setColor(BLACK); }
                char ch[2] = {keys[k], 0};
                d_.setTextAlignment(TEXT_ALIGN_CENTER);
                d_.drawString(x + 5, top + kTextDy, ch);
                d_.setTextAlignment(TEXT_ALIGN_LEFT);
            }
        } else {
            static const int16_t kBox[7] = {1, 19, 33, 47, 79, 101, 127};
            static const char* const kSpec[6] = {"aA", "<", ">", "Space", "Del", "Done"};
            for (uint8_t k = 0; k < 6; k++) {
                bool sel = selRow == 3 && selCol == k;
                d_.setColor(WHITE);
                if (sel) { d_.fillRect(kBox[k], top, kBox[k + 1] - kBox[k] - 1, 10); d_.setColor(BLACK); }
                d_.setTextAlignment(TEXT_ALIGN_CENTER);
                d_.drawString((kBox[k] + kBox[k + 1]) / 2, top + kTextDy, kSpec[k]);
                d_.setTextAlignment(TEXT_ALIGN_LEFT);
            }
        }
        d_.setColor(WHITE);
    }
}


void OledRenderer::icon(int16_t x, int16_t y, const uint8_t* rows, uint8_t h) {
    for (uint8_t j = 0; j < h; j++)
        for (uint8_t i = 0; i < 8; i++)
            if (rows[j] & (0x80 >> i)) d_.setPixel(x + i, y + j);
}

// Wi-Fi strength as 4 bars (heights 2/4/6/7, 3 px apart); missing bars leave their bottom pixel
static void wifiBars(OLEDDisplay& d, int16_t x, int16_t y, int8_t rssi) {
    if (rssi == 0) return;                             // unknown (access-point mode)
    uint8_t bars = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    static const uint8_t kH[4] = {2, 4, 6, 7};
    for (uint8_t b = 0; b < 4; b++) {
        if (b < bars) d.fillRect(x + b * 3, y + 7 - kH[b], 2, kH[b]);
        else d.setPixel(x + b * 3, y + 6);
    }
}

// Text centred on cx but kept inside [lo, hi]; returns its left edge
int16_t OledRenderer::boxed(int16_t cx, int16_t y, const char* text, int16_t lo, int16_t hi) {
    int16_t w = (int16_t)textW(d_, text), x = (int16_t)(cx - w / 2);
    if (x > hi - w) x = (int16_t)(hi - w);
    if (x < lo) x = lo;
    d_.drawString(x, y, text);
    return x;
}

// Home screen: band with the Wi-Fi bars; source -> output resolution in Arial 16 with the rates under them; a rule;
// profile + region; network + volume
void OledRenderer::drawIdle(const MenuIdle& idle) {
    band(idle.title);
    d_.setColor(BLACK);
    wifiBars(d_, 116, 3, idle.rssi);
    d_.setColor(WHITE);
    if (!idle.noSignal) {
        d_.setFont(ArialMT_Plain_16);
        boxed(32, 13, idle.inRes, 1, 57);
        boxed(96, 13, idle.outRes, 70, 126);
        d_.setFont(ArialMT_Plain_10);
        icon(60, 19, kIcoArrow, 7);
        boxed(32, 29, idle.inLine, 1, 63);
        int16_t lockW = idle.locked ? 9 : 0;
        int16_t x = boxed(96, 29, idle.outHz, 66, (int16_t)(126 - lockW));
        if (idle.locked) icon((int16_t)(x + textW(d_, idle.outHz) + 2), 32, kIcoLock, 7);
    } else {
        d_.setFont(ArialMT_Plain_16);
        d_.setTextAlignment(TEXT_ALIGN_CENTER);
        d_.drawString(kW / 2, 13, idle.searching ? "Searching" : "No signal");
        d_.setFont(ArialMT_Plain_10);
        d_.drawString(kW / 2, 29, idle.inLine);
        d_.setTextAlignment(TEXT_ALIGN_LEFT);
    }
    d_.drawHorizontalLine(0, 42, kW);
    // no signal: the hint replaces the profile name and keeps the whole line (no region beside it)
    int16_t rw = idle.region[0] && !idle.noSignal ? (int16_t)textW(d_, idle.region) : 0;
    if (rw) {
        d_.setTextAlignment(TEXT_ALIGN_RIGHT);
        d_.drawString(126, 42, idle.region);
        d_.setTextAlignment(TEXT_ALIGN_LEFT);
    }
    int16_t slot = (int16_t)(120 - (rw ? rw + 6 : 0));
    const char* prof = idle.noSignal ? "OK: choose the input" : idle.profile[0] ? idle.profile : "No profile";
    if ((int16_t)textW(d_, prof) > slot) {              // a long name scrolls once after it changes, then rests
        uint8_t k = skipToFit(prof, slot);
        uint32_t e = since(homeScroll_, prof);
        if (e < marqueeCycleMs((uint16_t)(k + 1), 1, kPassScroll))
            prof += marqueeOffset((uint16_t)(k + 1), 1, e, kPassScroll);
    }
    clipped(3, 42, prof, slot);
    char vol[8];
    if (idle.muted) snprintf(vol, sizeof(vol), "Mute");
    else snprintf(vol, sizeof(vol), "%u%%", idle.volume);
    int16_t vw = (int16_t)textW(d_, vol);
    d_.setTextAlignment(TEXT_ALIGN_RIGHT);
    d_.drawString(126, 52, vol);
    d_.setTextAlignment(TEXT_ALIGN_LEFT);
    icon((int16_t)(126 - vw - 10), 55, idle.muted ? kIcoMuted : kIcoSpeaker, 7);
    clipped(3, 52, idle.net, (int16_t)(126 - vw - 14 - 3));
}

// Screensaver: the resolution path in Arial 16 with the profile and region under it (no signal:
// "No signal" and the input), placed at a random position that changes every kSaverStepMs (no pixel stays lit)
void OledRenderer::drawSaver(const MenuIdle& idle) {
    char line2[40];
    if (idle.noSignal) snprintf(line2, sizeof(line2), "%s", idle.inLine);
    else if (idle.profile[0]) snprintf(line2, sizeof(line2), idle.region[0] ? "%s  %s" : "%s", idle.profile, idle.region);
    else snprintf(line2, sizeof(line2), idle.region[0] ? "%s  %s" : "%s%s", idle.region, idle.outHz);
    d_.setFont(ArialMT_Plain_16);
    int16_t wIn = (int16_t)textW(d_, idle.inRes), wOut = (int16_t)textW(d_, idle.outRes);
    int16_t w1 = idle.noSignal ? (int16_t)textW(d_, "No signal") : (int16_t)(wIn + 14 + wOut);
    d_.setFont(ArialMT_Plain_10);
    int16_t w2 = (int16_t)textW(d_, line2);
    if (w2 > kW) w2 = kW;
    int16_t w = w1 > w2 ? w1 : w2;
    const int16_t h = 30;
    int16_t x = (int16_t)((lcg_ >> 16) % (uint32_t)(kW - w + 1));
    int16_t y = (int16_t)((lcg_ >> 8) % (uint32_t)(64 - h + 1));
    int16_t x1 = (int16_t)(x + (w - w1) / 2);
    d_.setColor(WHITE);
    d_.setFont(ArialMT_Plain_16);
    if (idle.noSignal) {
        d_.drawString(x1, y, "No signal");
    } else {
        d_.drawString(x1, y, idle.inRes);
        d_.setFont(ArialMT_Plain_10);
        icon((int16_t)(x1 + wIn + 3), (int16_t)(y + 6), kIcoArrow, 7);
        d_.setFont(ArialMT_Plain_16);
        d_.drawString((int16_t)(x1 + wIn + 14), y, idle.outRes);
    }
    d_.setFont(ArialMT_Plain_10);
    clipped((int16_t)(x + (w - w2) / 2), (int16_t)(y + 17), line2, kW);
}

void OledRenderer::draw(const MenuView& v, const MenuIdle& idle, bool saver, uint32_t now) {
    now_ = now;
    d_.clear();
    d_.setFont(ArialMT_Plain_10);
    d_.setTextAlignment(TEXT_ALIGN_LEFT);
    d_.setColor(WHITE);
    if (v.mode == VIEW_LIST) drawList(v);
    else if (v.mode == VIEW_OVERLAY) drawOverlay(v);
    else if (saver) drawSaver(idle);
    else drawIdle(idle);
}

void OledRenderer::advanceSaver(uint32_t step) {
    if (step < step_) { lcg_ = 0x2545F491u; step_ = 0; }
    while (step_ < step) { lcg_ = lcg_ * 1103515245u + 12345u; step_++; }
}

bool OledRenderer::scrollPossible(const MenuView& v, const MenuIdle& idle, bool saver) {
    if (v.mode == VIEW_LIST) return true;               // band description, selected long label
    if (v.mode == VIEW_OVERLAY && v.overlay == OV_INFO) return true;   // a long profile name
    if (v.mode != VIEW_CLOSED || saver || !idle.profile[0]) return false;
    d_.setFont(ArialMT_Plain_10);
    return (int16_t)textW(d_, idle.profile) > 70;      // the home screen's profile space may be too narrow for it
}

static bool sameIdle(const MenuIdle& a, const MenuIdle& b) { return memcmp(&a, &b, sizeof(a)) == 0; }   // idle() zeroes it

bool OledRenderer::render(const MenuView& v, const MenuIdle& idle, uint32_t now) {
    bool closed = v.mode == VIEW_CLOSED;
    bool saver = closed && now - lastActivity_ >= kSaverMs;
    uint32_t step = saver ? (now - lastActivity_ - kSaverMs) / kSaverStepMs : 0;
    bool changed = dirty_ || v.rev != rev_ || v.mode != mode_ || saver != saver_ || (saver && step != step_)
                || (closed && !sameIdle(idle, idle_));   // the saver shows the same information
    // Scrolling text: redraw the buffer every 100 ms; display() sends only what changed (double buffer)
    if (!changed && now - lastScroll_ >= 100 && scrollPossible(v, idle, saver)) { changed = true; lastScroll_ = now; }
    if (!changed) return false;
    if (!dirty_ && now - lastFrame_ < kMinFrameMs) return false;
    if (saver) advanceSaver(step);
    draw(v, idle, saver, now);
    d_.display();
    dirty_ = false;
    rev_ = v.rev;
    mode_ = v.mode;
    saver_ = saver;
    idle_ = idle;
    lastFrame_ = now;
    return true;
}
