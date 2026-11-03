#include <stdlib.h>
#include <string.h>
#include "settings-fs.h"

static void tmpPath(const char* path, char* out, size_t n) {
    strncpy(out, path, n - 5); out[n - 5] = '\0'; strcat(out, ".tmp");
}

bool settingsAtomicWrite(SettingsFs& fs, const char* path, const char* data, size_t len) {
    char tmp[64]; tmpPath(path, tmp, sizeof(tmp));
    if (!fs.writeAll(tmp, data, len)) return false;
    return fs.rename(tmp, path);          // replaces the target atomically (lfs_rename): it never goes missing
}

void settingsRecoverTmp(SettingsFs& fs, const char* path) {
    char tmp[64]; tmpPath(path, tmp, sizeof(tmp));
    if (!fs.exists(tmp)) return;
    if (fs.exists(path)) fs.remove(tmp);
    else fs.rename(tmp, path);
}

void lineAssemblerInit(LineAssembler& a) { a.n = 0; a.overflow = false; }

void lineAssemblerFeed(LineAssembler& a, char c, void (*onLine)(const char*, void*), void* ctx) {
    if (c == '\n') {
        if (!a.overflow) {
            if (a.n && a.line[a.n - 1] == '\r') a.n--;
            a.line[a.n] = '\0';
            onLine(a.line, ctx);
        }
        a.n = 0; a.overflow = false;
    } else if (a.n < sizeof(a.line) - 1) {
        a.line[a.n++] = c;
    } else if (c == '\r') {
        // a full line of a CRLF file: the CR is not text (the ADV store counts it the same way), the \n ends it
    } else {
        a.overflow = true;
    }
}

void lineAssemblerFinish(LineAssembler& a, void (*onLine)(const char*, void*), void* ctx) {
    if (a.n && !a.overflow) lineAssemblerFeed(a, '\n', onLine, ctx);
    lineAssemblerInit(a);
}

void settingsSplitLines(const char* data, size_t len, void (*onLine)(const char*, void*), void* ctx) {
    LineAssembler a; lineAssemblerInit(a);
    if (len > SETTINGS_MAX_FILE_BYTES) len = SETTINGS_MAX_FILE_BYTES;
    for (size_t i = 0; i < len; i++) lineAssemblerFeed(a, data[i], onLine, ctx);
    lineAssemblerFinish(a, onLine, ctx);
}

void textBufInit(TextBuf& b, size_t maxCap) { b.data = nullptr; b.len = 0; b.cap = 0; b.maxCap = maxCap; b.failed = false; }

bool textBufAppend(TextBuf& b, const char* s) {
    if (b.failed) return false;
    size_t add = strlen(s);
    if (b.maxCap && b.len + add + 1 > b.maxCap) { b.failed = true; return false; }
    if (b.len + add + 1 > b.cap) {
        size_t cap = b.cap ? b.cap : 256;
        while (cap < b.len + add + 1) cap *= 2;
        if (b.maxCap && cap > b.maxCap) cap = b.maxCap;
        char* p = (char*)realloc(b.data, cap);
        if (!p) { b.failed = true; return false; }
        b.data = p; b.cap = cap;
    }
    memcpy(b.data + b.len, s, add + 1);
    b.len += add;
    return true;
}

void textBufFree(TextBuf& b) { size_t m = b.maxCap; free(b.data); textBufInit(b, m); }

void textBufSink(const char* text, void* ctx) { textBufAppend(*(TextBuf*)ctx, text); }

namespace {
class BufferedWriter : public SettingsFileWriter {
public:
    BufferedWriter(SettingsFs& fs, const char* path) : fs_(fs) {
        strncpy(path_, path, sizeof(path_) - 1);
        path_[sizeof(path_) - 1] = '\0';
        textBufInit(buf_);
    }
    ~BufferedWriter() override { textBufFree(buf_); }
    bool write(const char* data, size_t len) override {
        char chunk[65];
        while (len && !buf_.failed) {
            size_t n = len < 64 ? len : 64;
            memcpy(chunk, data, n); chunk[n] = '\0';
            textBufAppend(buf_, chunk);
            data += n; len -= n;
        }
        return !buf_.failed;
    }
    bool commit() override {
        if (buf_.failed) return false;
        return settingsAtomicWrite(fs_, path_, buf_.data ? buf_.data : "", buf_.len);
    }
    void abort() override {}
private:
    SettingsFs& fs_;
    char path_[64];
    TextBuf buf_;
};
} // namespace

SettingsFileWriter* SettingsFs::openWriter(const char* path) { return new BufferedWriter(*this, path); }
