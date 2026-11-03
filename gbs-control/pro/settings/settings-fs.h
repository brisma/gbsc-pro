#pragma once
#include <stdint.h>
#include <stddef.h>

// Settings files are small; anything past this is ignored (keeps boot bounded on corrupt files)
static const size_t SETTINGS_MAX_FILE_BYTES = 8192;

// Streams one file: nothing is visible until commit(), which replaces the target atomically
class SettingsFileWriter {
public:
    virtual ~SettingsFileWriter() {}
    virtual bool write(const char* data, size_t len) = 0;   // false after any failure (commit then fails)
    virtual bool commit() = 0;
    virtual void abort() = 0;
};

class SettingsFs {
public:
    virtual ~SettingsFs() {}
    virtual bool exists(const char* path) = 0;
    virtual bool remove(const char* path) = 0;
    virtual bool rename(const char* from, const char* to) = 0;
    virtual bool writeAll(const char* path, const char* data, size_t len) = 0;
    virtual bool readLines(const char* path, void (*onLine)(const char* line, void* ctx), void* ctx,
                           size_t maxBytes = SETTINGS_MAX_FILE_BYTES) = 0;
    virtual size_t readAt(const char* path, size_t offset, char* buf, size_t len) = 0;   // 0 at EOF / missing
    virtual size_t fileSize(const char* path) = 0;                                       // 0 when missing
    // Default: buffers in RAM and commits through settingsAtomicWrite (fine for tests and small files);
    // the LittleFS implementation streams to <path>.tmp instead. Caller deletes the writer.
    virtual SettingsFileWriter* openWriter(const char* path);
};

// Assembles lines byte by byte; lines longer than 127 chars are dropped
struct LineAssembler { char line[128]; size_t n; bool overflow; };
void lineAssemblerInit(LineAssembler& a);
void lineAssemblerFeed(LineAssembler& a, char c, void (*onLine)(const char*, void*), void* ctx);
void lineAssemblerFinish(LineAssembler& a, void (*onLine)(const char*, void*), void* ctx);

bool settingsAtomicWrite(SettingsFs& fs, const char* path, const char* data, size_t len);
void settingsRecoverTmp(SettingsFs& fs, const char* path);
void settingsSplitLines(const char* data, size_t len, void (*onLine)(const char*, void*), void* ctx);

// Growable text buffer; any failed append sets `failed` and makes later appends no-ops
struct TextBuf { char* data; size_t len; size_t cap; size_t maxCap; bool failed; };
void textBufInit(TextBuf& b, size_t maxCap = 0);   // 0 = no limit
bool textBufAppend(TextBuf& b, const char* s);
void textBufFree(TextBuf& b);
void textBufSink(const char* text, void* ctx);   // SettingsSink adapter, ctx = TextBuf*
