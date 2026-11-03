#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host-fs.h"
#include "golden.h"

static bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[512];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

static bool writeFile(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return true;
}

static bool updateRequested() {   // only GOLDEN_UPDATE=1: GOLDEN_UPDATE=0 used to rewrite them too
    const char* v = getenv("GOLDEN_UPDATE");
    return v && strcmp(v, "1") == 0;
}

bool goldenCheck(const char* name, const std::string& actual) { return goldenCheckFile("osd", name, "txt", actual); }

bool goldenCheckFile(const char* subdir, const char* name, const char* ext, const std::string& actual) {
    std::string dir = std::string(GOLDEN_DIR) + "/" + subdir;
    hostMakeDir(GOLDEN_DIR);
    hostMakeDir(dir);
    std::string path = dir + "/" + name + "." + ext;
    std::string expected;
    if (updateRequested()) { writeFile(path, actual); return true; }
    if (!readFile(path, expected)) {
        writeFile(path, actual);
        printf("golden created: review %s and re-run\n", path.c_str());
        return false;
    }
    if (expected == actual) return true;
    writeFile(dir + "/" + name + ".actual." + ext, actual);
    size_t line = 1;
    for (size_t i = 0; i < expected.size() && i < actual.size() && expected[i] == actual[i]; i++)
        if (expected[i] == '\n') line++;
    printf("golden %s differs at line %u (see %s.actual.%s)\n", name, (unsigned)line, name, ext);
    return false;
}
