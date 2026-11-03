#include <stdio.h>
#include <string.h>
#include "stv-fake.h"
#include "stv9426-udc.h"

// Datasheet Table 1 (ROM character generator), C(6:4) = column; icons drawn as single-width placeholders
static const char* const kRom[128] = {
    " ", " ", " ", "⇦", "[", "◀", "▲", "⊏", "⌜", "⌈", "◖", "☼", "Σ", "◉", "∠", " ",
    " ", "▎", "▍", "⇨", "]", "▶", "▼", "⊐", "⌟", "⌋", "◗", "☀", "ʒ", "○", "⌝", " ",
    "à", "è", "ù", "ê", "ô", "ë", "ß", "'", "â", "î", "ç", "ñ", "▌", "▋", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", "±", "+", "=", "-", "█",
    "ö", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "á", "ó", "í", "ú", "█",
    "é", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "ã", "õ", "å", "ü", "↘",
};
static const char* const kUdc[UDC_COUNT] = {"⓪", "①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧", "⑨", "⑩",
                                            "⑪", "⑫", "⑬", "⑭", "⑮", "⑯", "?"};

FakeStv::FakeStv() {
    memset(ram, 0, sizeof(ram));
    memset(reg, 0, sizeof(reg));
    // Power-on register values (datasheet §I.4)
    reg[0] = 0x3F; reg[1] = 0x08; reg[2] = 0x12; reg[3] = 0x00;
    reg[4] = 0x22; reg[5] = 0x33; reg[6] = 0x28; reg[7] = 0x0A;
}

bool FakeStv::write(uint16_t addr, const uint8_t* data, uint8_t len) {
    if (failWrites > 0) { failWrites--; return false; }
    char line[24];
    if (addr >= 0x3FF0) {
        for (uint8_t i = 0; i < len && addr + i <= 0x3FFF; i++) {
            reg[(addr + i) & 0x0F] = data[i];
            snprintf(line, sizeof(line), "R %02X=%02X", (unsigned)((addr + i) & 0xFF), data[i]);
            log.push_back(line);
        }
        return true;
    }
    snprintf(line, sizeof(line), "W %03X+%u", (unsigned)addr, (unsigned)len);
    log.push_back(line);
    for (uint8_t i = 0; i < len; i++)
        if (addr + i < 0x400) ram[addr + i] = data[i];
    return true;
}

void FakeStv::read(uint16_t addr, uint8_t* out, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(addr + i);
        if (a < 0x400) out[i] = ram[a];
        else if (a >= 0x3FF0) out[i] = reg[a & 0x0F];
        else if (a >= 0x2000 && a <= 0x32FF) out[i] = (uint8_t)(a & 0xFF);
        else out[i] = 0;
    }
}

// A UDC cell is shown as the glyph its slot holds (⓪..⑯ = StvUdc), so goldens do not depend on slot order
static const char* udcText(const uint8_t* ram, uint8_t n) {
    uint8_t b[38];
    for (uint8_t g = 0; g < UDC_COUNT; g++) {
        stvUdcBytes(g, b);
        if (memcmp(b, ram + 38 * n, 38) == 0) return kUdc[g];
    }
    return "?";
}

static const char* cellText(const uint8_t* ram, uint8_t attr, uint8_t code) {
    bool bk3 = attr & 0x80;
    if (!bk3 && (attr & 0x07) == ((attr >> 4) & 0x07)) return " ";   // foreground = background
    if (code & 0x80) {
        uint8_t n = code & 0x7F;
        if (n <= 25) return udcText(ram, n);
        return " ";                                                    // 26..63: space
    }
    return kRom[code];
}

std::string FakeStv::decode(unsigned frameLines) const {
    std::string out;
    char buf[96];
    unsigned page = (reg[3] & 0x07) * 64;
    unsigned ch = reg[2] & 0x3F;
    snprintf(buf, sizeof(buf), "OSD %s page=0x%03X LD=%u DD=%u CH=%u PP=%u FM=%u\n", (reg[3] & 0x80) ? "on" : "off",
             page, reg[0] & 0x3F, reg[1], ch, reg[6], reg[7] & 0x0F);
    out += buf;
    std::vector<uint8_t> attrs;
    unsigned lines = 0, runValue = 0, runCount = 0;
    auto endRun = [&]() {
        if (!runCount) return;
        if (runCount == 1) snprintf(buf, sizeof(buf), "S %u\n", runValue);
        else snprintf(buf, sizeof(buf), "S %ux%u\n", runValue, runCount);
        out += buf;
        runCount = 0;
    };
    for (unsigned i = 0; i < 64 && lines < frameLines; i++) {
        unsigned a = page + 2 * i;
        if (a + 1 >= 0x400) { endRun(); out += "DESC RAM-END\n"; break; }
        uint8_t lsb = ram[a], msb = ram[a + 1];
        if (!(msb & 0x80)) {                                  // spacing strip
            if (runCount && runValue != lsb) endRun();
            runValue = lsb; runCount++;
            lines += lsb;
            continue;
        }
        endRun();
        unsigned addr = ((unsigned)(msb & 0x03) << 8) | (lsb & 0xFE);
        bool zoom = msb & 0x10;
        lines += ch * (zoom ? 2 : 1);
        std::string text, letters, tail;
        unsigned c = 0;
        for (;; c++) {
            unsigned p = addr + 2 * c;
            if (c == 64) { tail = " NO-EOL"; break; }
            if (p + 1 >= 0x400) { tail = " RAM-END"; break; }
            uint8_t at = ram[p], code = ram[p + 1];
            if ((code & 0x80) && (code & 0x7F) > 63) break;  // end of line
            text += cellText(ram, at, code);
            size_t k = 0;
            while (k < attrs.size() && attrs[k] != at) k++;
            if (k == attrs.size()) attrs.push_back(at);
            letters += (char)(k < 26 ? 'A' + k : 'a' + (k - 26) % 26);
        }
        snprintf(buf, sizeof(buf), "R %03X %c%s |", addr, zoom ? 'z' : '-', (msb & 0x40) ? "" : " DE=0");
        out += buf; out += text; out += "|"; out += tail; out += "\n";
        out += "        |"; out += letters; out += "|\n";
    }
    endRun();
    if (!attrs.empty()) {
        out += "attrs";
        for (size_t k = 0; k < attrs.size(); k++) {
            snprintf(buf, sizeof(buf), " %c=%02X", (char)(k < 26 ? 'A' + k : 'a' + (k - 26) % 26), attrs[k]);
            out += buf;
        }
        out += "\n";
    }
    return out;
}

std::string FakeStv::udcArt(uint8_t n) const {
    std::string out;
    for (unsigned s = 0; s < 18; s++) {
        unsigned base = 38 * n + 2 * s;
        unsigned v = ((unsigned)(ram[base + 1] & 0x0F) << 8) | ram[base];
        for (int x = 11; x >= 0; x--) out += (v >> x) & 1 ? '#' : '.';
        out += '\n';
    }
    return out;
}
