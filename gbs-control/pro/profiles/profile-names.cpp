#include <stdio.h>
#include <string.h>
#include "profile-names.h"
#include "../input/input-types.h"

static bool allowed(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == ' ' || c == '-' || c == '.' || c == '/' || c == ':' || c == '+' || c == '=' || c == '\'';
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool profileNameSanitize(const char* in, char* out, size_t n) {
    if (!n) return false;
    while (*in == ' ' || (*in && !allowed(*in))) in++;   // leading spaces (and junk that would become spaces)
    size_t o = 0;
    for (; *in && o < PROFILE_NAME_MAX && o + 1 < n; in++) out[o++] = allowed(*in) ? *in : ' ';
    while (o && out[o - 1] == ' ') o--;
    out[o] = '\0';
    return o > 0;
}

int profileNameCompare(const char* a, const char* b) {
    for (size_t i = 0;; i++) {
        char x = lower(a[i]), y = lower(b[i]);
        if (x != y) return x < y ? -1 : 1;
        if (!x) break;
    }
    int c = strcmp(a, b);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

bool profileNameSame(const char* a, const char* b) {
    for (size_t i = 0;; i++) {
        if (lower(a[i]) != lower(b[i])) return false;
        if (!a[i]) return true;
    }
}

void profileNameUnique(const char* base, ProfileNameTaken taken, void* ctx, char* out, size_t n) {
    snprintf(out, n, "%s", base);
    if (!taken(out, ctx)) return;
    for (int k = 2; k < 100; k++) {
        char suffix[5];
        int sl = snprintf(suffix, sizeof(suffix), " %d", k);
        size_t keep = strlen(base);
        if (keep + (size_t)sl > PROFILE_NAME_MAX) keep = PROFILE_NAME_MAX - (size_t)sl;
        while (keep && base[keep - 1] == ' ') keep--;
        snprintf(out, n, "%.*s%s", (int)keep, base, suffix);
        if (!taken(out, ctx)) return;
    }
}

void profileExportFileName(const char* name, char* out, size_t n) {
    size_t o = 0;
    for (; *name && o + 1 < n; name++) {
        char c = *name;
        out[o++] = (c == '/' || c == ':' || c == '\'' || c == ' ') ? '_' : c;
    }
    out[o] = '\0';
    snprintf(out + o, n - o, ".gbscprofile.txt");
}

void profileGeneratedName(uint8_t input, uint8_t sourceMode, bool interlaced, bool hasSignal, char* out, size_t n) {
    if (!hasSignal || !inputTypeValid(input)) { snprintf(out, n, "Profile"); return; }
    const char* res = nullptr;
    switch (sourceMode) {
        case 1: res = interlaced ? "480i" : "240p"; break;
        case 2: res = interlaced ? "576i" : "288p"; break;
        case 3: res = "480p"; break;
        case 4: res = "576p"; break;
        case 8: res = "medium"; break;
        default: break;
    }
    if (res) snprintf(out, n, "%s %s", inputLongName(input), res);
    else snprintf(out, n, "%s", inputLongName(input));
}

static const char* const kRegions[PROFILE_REGION_COUNT] = {"NTSC-U", "NTSC-J", "PAL", "PAL60", "None"};

const char* profileRegionName(uint8_t r) { return kRegions[r < PROFILE_REGION_COUNT ? r : PROFILE_REGION_NONE]; }

uint8_t profileRegionIndex(const char* s) {
    if (!s) return PROFILE_REGION_NONE;
    for (uint8_t r = 0; r < PROFILE_REGION_COUNT; r++) if (!strcmp(s, kRegions[r])) return r;
    if (!strcmp(s, "NTSC")) return PROFILE_REGION_NTSC_U;     // old names (NTSC, JP)
    if (!strcmp(s, "JP")) return PROFILE_REGION_NTSC_J;
    return PROFILE_REGION_NONE;
}

static bool stripWord(char* s, const char* w) {
    size_t len = strlen(s), wl = strlen(w);
    if (len <= wl + 1 || s[len - wl - 1] != ' ' || strcmp(s + len - wl, w)) return false;
    len -= wl + 1;
    while (len && s[len - 1] == ' ') len--;
    s[len] = '\0';
    return true;
}

uint8_t profileDefaultRegion(bool pal50Hz, int8_t advStandard) {
    if (advStandard == 3) return PROFILE_REGION_PAL60;
    return pal50Hz ? PROFILE_REGION_PAL : PROFILE_REGION_NTSC_U;
}

uint8_t profileReplaceRegion(bool named, uint8_t region) {
    if (!named) return PROFILE_REGION_COUNT;
    return region < PROFILE_REGION_NONE ? region : PROFILE_REGION_NONE;
}

void profileNameWithoutRegion(const char* name, uint8_t region, char* out, size_t n) {
    if (!n) return;
    snprintf(out, n, "%s", name ? name : "");
    if (region >= PROFILE_REGION_NONE) return;
    if (stripWord(out, kRegions[region])) return;
    if (region == PROFILE_REGION_NTSC_U) stripWord(out, "NTSC");      // old names (NTSC, JP)
    else if (region == PROFILE_REGION_NTSC_J) stripWord(out, "JP");
}

void profileLabel(const char* name, uint8_t region, char* out, size_t n) {
    char base[PROFILE_NAME_BUF];
    profileNameWithoutRegion(name, region, base, sizeof(base));
    profileNameWithRegion(base, region, out, n);
}

void profileNameWithRegion(const char* base, uint8_t region, char* out, size_t n) {
    if (!n) return;
    if (region >= PROFILE_REGION_NONE) { snprintf(out, n, "%s", base); return; }
    const char* suffix = kRegions[region];
    size_t cap = n - 1 < PROFILE_LABEL_MAX ? n - 1 : PROFILE_LABEL_MAX, sl = strlen(suffix);
    if (cap < sl + 2) { snprintf(out, n, "%s", base); return; }   // no room for "x REGION": what fits of the name
    size_t room = cap - 1 - sl;                                   // base characters that fit before " REGION"
    char b[PROFILE_NAME_MAX + 1];
    snprintf(b, sizeof(b), "%s", base);
    if (strlen(b) > room) b[room] = '\0';
    size_t len = strlen(b);
    while (len && b[len - 1] == ' ') b[--len] = '\0';
    snprintf(out, n, "%s %s", b, suffix);
}
