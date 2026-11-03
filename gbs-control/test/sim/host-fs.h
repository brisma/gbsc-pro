// File helpers for the native tests on macOS, Linux and Windows (MinGW): directories, a temporary directory, removal.
// No <windows.h>: it clashes with the Arduino shim's types.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

// true when the directory exists afterwards (made now or before)
inline bool hostMakeDir(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR);
}

inline std::string hostTempBase() {
#ifdef _WIN32
    const char* t = getenv("TEMP");
    if (!t || !*t) t = getenv("TMP");
    std::string s = t && *t ? t : ".";
    for (auto& c : s) if (c == '\\') c = '/';
#else
    const char* t = getenv("TMPDIR");
    std::string s = t && *t ? t : "/tmp";
#endif
    while (s.size() > 1 && s[s.size() - 1] == '/') s.erase(s.size() - 1);
    return s;
}

// A new empty directory <temp>/<prefix>..., "" on failure
inline std::string hostTempDir(const char* prefix) {
#ifdef _WIN32
    for (unsigned i = 0; i < 1000; i++) {
        std::string p = hostTempBase() + "/" + prefix + std::to_string(_getpid()) + "-" + std::to_string(i);
        if (_mkdir(p.c_str()) == 0) return p;
    }
    return "";
#else
    std::string tmpl = hostTempBase() + "/" + prefix + "XXXXXX";
    return mkdtemp(&tmpl[0]) ? tmpl : "";
#endif
}

// The directory, its files and subdirectories
inline void hostRemoveTree(const std::string& path) {
#ifdef _WIN32
    struct _finddata_t fd;
    intptr_t h = _findfirst((path + "/*").c_str(), &fd);
    if (h != -1) {
        do {
            std::string n = fd.name;
            if (n == "." || n == "..") continue;
            std::string p = path + "/" + n;
            if (fd.attrib & _A_SUBDIR) hostRemoveTree(p);
            else remove(p.c_str());
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
    _rmdir(path.c_str());
#else
    DIR* d = opendir(path.c_str());
    if (d) {
        while (struct dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            std::string p = path + "/" + n;
            struct stat st;
            if (lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) hostRemoveTree(p);
            else remove(p.c_str());
        }
        closedir(d);
    }
    rmdir(path.c_str());
#endif
}
