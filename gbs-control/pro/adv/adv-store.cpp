#include <stdio.h>
#include <string.h>
#include "adv-store.h"
#include "../api/api-json.h"

void advYamlBegin(AdvYamlCheck& c, size_t max) {
    memset(&c, 0, sizeof(c));
    c.max = max;
}

static void fail(AdvYamlCheck& c, const char* why) {
    if (!c.bad) { c.bad = true; c.why = why; }
}

// Copies a YAML scalar (quotes removed, trimmed) into out, cut at a UTF-8 character boundary
static void scalar(const char* s, char* out, size_t cap) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (n >= 2 && (s[0] == '\'' || s[0] == '"') && s[n - 1] == s[0]) { s++; n -= 2; }
    if (n > cap - 1) {
        n = cap - 1;
        while (n && ((uint8_t)s[n] & 0xC0) == 0x80) n--;   // s[n] continues a character: cut before it
    }
    memcpy(out, s, n);
    out[n] = '\0';
}

static void endLine(AdvYamlCheck& c) {
    c.line[c.lineLen] = '\0';
    c.lineLen = 0;
    if (c.bad) return;
    const char* l = c.line;
    if (l[0] == '[') { fail(c, "section"); return; }
    if (!strncmp(l, "name:", 5)) { scalar(l + 5, c.name, sizeof(c.name)); return; }
    if (!strncmp(l, "description:", 12)) { scalar(l + 12, c.desc, sizeof(c.desc)); return; }
    while (*l == ' ' || *l == '\t') l++;
    if (((l[0] == '\'' || l[0] == '"') && l[1] == '0' && (l[2] == 'x' || l[2] == 'X') && strchr(l, ':')) ||
        (l[0] == '-' && l[1] == ' '))
        c.count++;
}

bool advYamlFeed(AdvYamlCheck& c, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n && !c.bad; i++) {
        uint8_t b = p[i];
        if (++c.size > c.max) { fail(c, "size"); break; }
        if (b == '\n') { endLine(c); continue; }
        if (b == '\r') continue;   // CRLF files (Windows): the line length counts the text only
        if (b < 0x20 && b != '\t') { fail(c, "control"); break; }
        if (b == 0x7F) { fail(c, "control"); break; }
        if (c.lineLen >= ADV_LINE_MAX) { fail(c, "line"); break; }
        c.line[c.lineLen++] = (char)b;
    }
    return !c.bad;
}

bool advYamlFinish(AdvYamlCheck& c) {
    if (c.lineLen) endLine(c);
    if (!c.bad && !c.name[0]) fail(c, "name");
    return !c.bad;
}

bool advSpaceOk(size_t freeBytes, size_t blockSize, size_t fileSize, size_t profileReserve) {
    if (!blockSize) return false;
    size_t blocks = fileSize ? (fileSize + blockSize - 1) / blockSize : 1;
    size_t need = blocks * blockSize + profileReserve;
    return freeBytes >= need;
}

int advParseId(const char* id, AdvKind k) {
    if (!id || id[0] != (k == ADV_PRESET ? 'p' : 'm')) return -1;
    if (id[1] < '0' || id[1] > '9' || id[2] < '0' || id[2] > '9' || id[3]) return -1;
    int i = (id[1] - '0') * 10 + (id[2] - '0');
    return i < advMax(k) ? i : -1;
}

void advId(AdvKind k, uint8_t i, char out[4]) {
    snprintf(out, 4, "%c%02u", k == ADV_PRESET ? 'p' : 'm', (unsigned)(i % 100));
}

void advPath(AdvKind k, uint8_t i, char* out, size_t n) {
    char id[4];
    advId(k, i, id);
    snprintf(out, n, "/adv/%s/%s.yaml", k == ADV_PRESET ? "presets" : "macros", id);
}

void advTempPath(AdvKind k, char* out, size_t n) {
    snprintf(out, n, "/adv/%s/upload.tmp", k == ADV_PRESET ? "presets" : "macros");
}

size_t advListEntryJson(const char* id, const AdvYamlCheck& c, uint32_t bytes, char* out, size_t n) {
    char name[ADV_NAME_MAX * 6 + 1], desc[ADV_DESC_MAX * 6 + 1];
    jsonEscape(c.name, name, sizeof(name));
    jsonEscape(c.desc, desc, sizeof(desc));
    int w = snprintf(out, n, "{\"id\":\"%s\",\"name\":\"%s\",\"description\":\"%s\",\"count\":%u,\"bytes\":%lu}", id,
                     name, desc, (unsigned)c.count, (unsigned long)bytes);
    if (w < 0 || (size_t)w >= n) { if (n) out[0] = '\0'; return 0; }
    return (size_t)w;
}

bool advGateBegin(AdvUploadGate& g, uintptr_t req) {
    if (g.owner && g.owner != req) return false;
    g.owner = req;
    return true;
}

void advGateEnd(AdvUploadGate& g, uintptr_t req) {
    if (g.owner == req) g.owner = 0;
}
