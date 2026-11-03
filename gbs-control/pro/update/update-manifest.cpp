#include <stdlib.h>
#include <string.h>
#include "update-manifest.h"


static bool parseVersionPart(const char*& p, long& out) {
    if (*p < '0' || *p > '9') { out = 0; return false; }
    out = strtol(p, (char**)&p, 10);
    return true;
}

// Suffix rank of a channel build: none (Stable) 3 > "betaN" 2 > "devN" 1 > anything else 0 (a local build)
static int suffixRank(const char* s, long& n) {
    n = 0;
    if (*s != '-') return 3;
    s++;
    const char* rest = nullptr;
    int rank = 0;
    if (!strncmp(s, "beta", 4)) { rank = 2; rest = s + 4; }
    else if (!strncmp(s, "dev", 3)) { rank = 1; rest = s + 3; }
    if (!rest || *rest < '0' || *rest > '9') return 0;
    char* end;
    n = strtol(rest, &end, 10);
    return *end ? 0 : rank;
}

int versionCompare(const char* a, const char* b) {
    for (int i = 0; i < 3; i++) {
        long x = 0, y = 0;
        parseVersionPart(a, x);
        parseVersionPart(b, y);
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    long na, nb;
    int ra = suffixRank(a, na), rb = suffixRank(b, nb);
    if (ra != rb) return ra < rb ? -1 : 1;
    if (ra == 1 || ra == 2) return na == nb ? 0 : (na < nb ? -1 : 1);
    int c = strcmp(a, b);                               // two other suffixes (or none): a total order anyway
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

static bool hexByte(const char* p, uint8_t& out) {
    int v = 0;
    for (int k = 0; k < 2; k++) {
        char c = p[k];
        int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        v = v * 16 + d;
    }
    out = (uint8_t)v;
    return true;
}

static bool keyChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }
static bool versionChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-';
}
static bool fileChar(char c) { return versionChar(c) || c == '_' || c == '/'; }

static bool keyIs(const char* k, size_t kn, const char* name) { return strlen(name) == kn && !strncmp(k, name, kn); }

bool manifestParse(const char* text, size_t len, Manifest& m) {
    enum : uint8_t { K_VERSION = 1, K_SIZE = 2, K_SHA = 4, K_FILE = 8, K_NOTES = 16 };
    if (len > MANIFEST_MAX_BYTES) return false;
    memset(&m, 0, sizeof(m));
    uint8_t seen = 0;
    char unknown[4][16];                                  // repeated unknown keys are errors too (first 4 kept)
    uint8_t nUnknown = 0;
    size_t pos = 0;
    while (pos < len) {
        const char* line = text + pos;
        const char* nl = (const char*)memchr(line, '\n', len - pos);
        size_t lineLen = nl ? (size_t)(nl - line) : len - pos;
        pos += lineLen + (nl ? 1 : 0);
        if (!lineLen) continue;                           // blank line
        if (memchr(line, '\r', lineLen)) return false;
        const char* eq = (const char*)memchr(line, '=', lineLen);
        if (!eq || eq == line) return false;
        size_t kn = (size_t)(eq - line);
        for (size_t i = 0; i < kn; i++) if (!keyChar(line[i])) return false;
        const char* v = eq + 1;
        size_t vLen = lineLen - kn - 1;
        uint8_t bit = keyIs(line, kn, "version") ? K_VERSION : keyIs(line, kn, "size") ? K_SIZE
                    : keyIs(line, kn, "sha256") ? K_SHA : keyIs(line, kn, "file") ? K_FILE
                    : keyIs(line, kn, "notes") ? K_NOTES : 0;
        if (!bit) {                                       // unknown key: ignored, but not twice
            char k[16];
            size_t c = kn < sizeof(k) - 1 ? kn : sizeof(k) - 1;
            memcpy(k, line, c); k[c] = '\0';
            for (uint8_t i = 0; i < nUnknown; i++) if (!strcmp(unknown[i], k)) return false;
            if (nUnknown < 4) memcpy(unknown[nUnknown++], k, c + 1);
            continue;
        }
        if (seen & bit) return false;
        seen |= bit;
        switch (bit) {
            case K_VERSION:
                if (!vLen || vLen >= sizeof(m.version)) return false;
                for (size_t i = 0; i < vLen; i++) if (!versionChar(v[i])) return false;
                memcpy(m.version, v, vLen);
                break;
            case K_SIZE: {
                char buf[12];
                if (!vLen || vLen >= sizeof(buf)) return false;
                memcpy(buf, v, vLen); buf[vLen] = '\0';
                for (size_t i = 0; i < vLen; i++) if (buf[i] < '0' || buf[i] > '9') return false;
                unsigned long n = strtoul(buf, nullptr, 10);
                if (!n) return false;
                m.size = (uint32_t)n;
                break;
            }
            case K_SHA:
                if (vLen != 64) return false;
                for (int i = 0; i < 32; i++) if (!hexByte(v + 2 * i, m.sha256[i])) return false;
                break;
            case K_FILE:
                if (!vLen || vLen >= sizeof(m.file) || v[0] == '/') return false;
                for (size_t i = 0; i < vLen; i++) {
                    if (!fileChar(v[i])) return false;
                    if (v[i] == '.' && i + 1 < vLen && v[i + 1] == '.') return false;   // no ".."
                }
                memcpy(m.file, v, vLen);
                break;
            case K_NOTES:
                if (vLen >= sizeof(m.notes)) return false;
                memcpy(m.notes, v, vLen);
                break;
        }
    }
    return (seen & (K_VERSION | K_SIZE | K_SHA | K_FILE)) == (K_VERSION | K_SIZE | K_SHA | K_FILE);
}
