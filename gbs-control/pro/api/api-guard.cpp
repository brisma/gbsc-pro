#include <string.h>
#include "api-guard.h"

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool guardNormalizeHost(const char* in, char* out, size_t n) {
    if (!in || !n) return false;
    const char* p = strstr(in, "://");
    p = p ? p + 3 : in;
    if (*p == '[') return false;                       // IPv6 literal: the device is IPv4 only
    size_t len = 0;
    while (p[len] && p[len] != '/' && p[len] != ':') len++;
    while (len && p[len - 1] == '.') len--;
    if (!len || len >= n) return false;
    for (size_t i = 0; i < len; i++) out[i] = lower(p[i]);
    out[len] = '\0';
    return true;
}

static bool isIpv4(const char* s) {
    int parts = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); if (++digits > 3) return false; }
        if (v > 255) return false;
        parts++;
        if (*s == '.') { s++; if (!*s) return false; }
        else if (*s) return false;
    }
    return parts == 4;
}

static bool inList(const char* host, const char* list) {
    if (!list) return false;
    const char* p = list;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char* start = p;
        while (*p && *p != ',') p++;
        const char* end = p;
        while (end > start && end[-1] == ' ') end--;
        size_t len = (size_t)(end - start);
        if (len && len == strlen(host)) {
            size_t i = 0;
            while (i < len && lower(start[i]) == host[i]) i++;
            if (i == len) return true;
        }
    }
    return false;
}

bool guardHostAllowed(const char* rawHost, const char* hostname, const char* extraHosts) {
    char host[64];
    if (!guardNormalizeHost(rawHost, host, sizeof(host))) return false;
    if (isIpv4(host)) return true;
    size_t hn = strlen(hostname);   // the device hostname is lowercase
    if (!strcmp(host, hostname)) return true;
    if (!strncmp(host, hostname, hn) && !strcmp(host + hn, ".local")) return true;
    return inList(host, extraHosts);
}

bool guardOriginAllowed(const char* origin, const char* hostname, const char* extraHosts) {
    if (!origin || !strcmp(origin, "null")) return false;
    return guardHostAllowed(origin, hostname, extraHosts);
}

bool guardExtraHostsValid(const char* list) {
    if (!list) return false;
    size_t total = strlen(list);
    if (total > 63) return false;                   // sizeof(userOptions::extraHosts) - 1
    if (!total) return true;
    const char* p = list;
    for (;;) {
        while (*p == ' ') p++;                      // spaces around commas: inList() trims them too
        const char* start = p;
        while (*p && *p != ',' && *p != ' ') {
            char c = *p;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
            p++;
        }
        size_t len = (size_t)(p - start);
        while (*p == ' ') p++;
        if (*p && *p != ',') return false;          // a space inside a name
        if (!len || len > 63) return false;
        char name[64];
        for (size_t i = 0; i < len; i++) name[i] = lower(start[i]);
        name[len] = '\0';
        if (isIpv4(name) || !strcmp(name, "localhost")) return false;
        if (!*p) return true;
        p++;                                       // past the comma: an entry must follow
    }
}

bool guardHostLostByChange(const char* host, const char* hostname, const char* oldList, const char* newList) {
    return guardHostAllowed(host, hostname, oldList) && !guardHostAllowed(host, hostname, newList);
}
