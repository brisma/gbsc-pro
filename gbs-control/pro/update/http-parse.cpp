#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http-parse.h"
#include "../util/pgm.h"

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

static bool lowerEqN(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) if (lower(a[i]) != lower(b[i])) return false;
    return true;
}

bool urlSplit(const char* url, UrlParts& u) {
    const char* p;
    if (!strncmp(url, "https://", 8)) { u.tls = true; u.port = 443; p = url + 8; }
    else if (!strncmp(url, "http://", 7)) { u.tls = false; u.port = 80; p = url + 7; }
    else return false;
    const char* slash = strchr(p, '/');
    if (!slash) return false;
    const char* colon = (const char*)memchr(p, ':', (size_t)(slash - p));
    const char* hostEnd = colon ? colon : slash;
    size_t hostLen = (size_t)(hostEnd - p);
    if (!hostLen || hostLen >= sizeof(u.host)) return false;
    memcpy(u.host, p, hostLen);
    u.host[hostLen] = '\0';
    if (colon) {
        char* end;
        long port = strtol(colon + 1, &end, 10);
        if (end != slash || port <= 0 || port > 65535) return false;
        u.port = (uint16_t)port;
    }
    u.path = slash;
    return true;
}

int httpStatus(const char* line) {
    if (strncmp(line, "HTTP/1.", 7) != 0 || !line[7] || line[8] != ' ') return -1;
    const char* s = line + 9;
    if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9' || s[2] < '0' || s[2] > '9') return -1;
    return (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
}

bool httpHeader(const char* line, const char* name, const char*& value) {
    size_t n = strlen(name);
    if (!lowerEqN(line, name, n) || line[n] != ':') return false;
    value = line + n + 1;
    while (*value == ' ' || *value == '\t') value++;
    return true;
}


bool httpRequest(const UrlParts& u, const char* userAgent, char* out, size_t n) {
    char port[8] = "";
    if (u.port != 80) snprintf(port, sizeof(port), ":%u", (unsigned)u.port);
    int w = snprintf_P(out, n, PSTR("GET %s HTTP/1.0\r\nHost: %s%s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n"),
                       u.path, u.host, port, userAgent);
    return w > 0 && (size_t)w < n;
}

bool HttpResponse::begin(uint32_t maxBody) {
    end();
    maxBody_ = maxBody;
    clen_ = got_ = 0;
    lineLen_ = 0;
    status_ = 0;
    first_ = true;
    haveLen_ = body_ = false;
    line_ = new (std::nothrow) char[kLine];
    result_ = line_ ? HTTP_MORE : HTTP_E_MEMORY;
    return line_ != nullptr;
}

void HttpResponse::end() {
    delete[] line_;
    line_ = nullptr;
}

int HttpResponse::headerLine() {
    line_[lineLen_] = '\0';
    if (first_) {
        first_ = false;
        status_ = httpStatus(line_);
        return status_ == 200 ? HTTP_MORE : HTTP_E_STATUS;
    }
    if (!lineLen_) {                                    // end of the headers
        if (!haveLen_) return HTTP_E_LENGTH;
        if (clen_ > maxBody_) return HTTP_E_TOO_BIG;
        body_ = true;
        return HTTP_MORE;
    }
    const char* v;
    if (httpHeader(line_, "content-length", v)) {
        char* end;
        unsigned long n = strtoul(v, &end, 10);
        if (end == v || n > 0xFFFFFFFFUL) return HTTP_E_LENGTH;
        clen_ = (uint32_t)n;
        haveLen_ = true;
    }
    return HTTP_MORE;
}

int HttpResponse::feed(const uint8_t* d, size_t n, HttpSink sink, void* ctx) {
    if (result_ < 0) return result_;
    if (result_ == HTTP_DONE) return result_ = (n ? HTTP_E_EXTRA : HTTP_DONE);
    size_t i = 0;
    while (!body_ && i < n) {
        char c = (char)d[i++];
        if (c == '\n') {
            if (lineLen_ && line_[lineLen_ - 1] == '\r') lineLen_--;
            int r = headerLine();
            lineLen_ = 0;
            if (r < 0) return result_ = r;
        } else {
            if (lineLen_ >= kLine - 1) return result_ = HTTP_E_HEADER;
            line_[lineLen_++] = c;
        }
    }
    if (!body_) return HTTP_MORE;
    size_t left = n - i, want = clen_ - got_;
    size_t take = left < want ? left : want;
    if (take) {
        if (!sink(d + i, take, ctx)) return result_ = HTTP_E_SINK;
        got_ += (uint32_t)take;
    }
    if (left > take) return result_ = HTTP_E_EXTRA;
    if (got_ == clen_) result_ = HTTP_DONE;
    return result_;
}

