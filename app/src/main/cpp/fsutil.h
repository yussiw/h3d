#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fsu {

inline bool isDir(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
inline bool exists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}
inline bool mkdirs(const std::string& p) {
    std::string cur;
    for (size_t i = 0; i <= p.size(); i++) {
        if (i == p.size() || p[i] == '/') {
            if (!cur.empty() && !isDir(cur)) mkdir(cur.c_str(), 0777);
        }
        if (i < p.size()) cur += p[i];
    }
    return isDir(p);
}
// lista arquivos (dirsOnly=false) ou pastas (dirsOnly=true), ordenados
inline std::vector<std::string> list(const std::string& dir, bool dirsOnly = false) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        bool isd = isDir(dir + "/" + n);
        if (dirsOnly != isd) continue;
        out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}
inline bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
inline bool writeFile(const std::string& path, const std::string& s) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << s;
    return (bool)f;
}
inline void removeAll(const std::string& p) {
    if (isDir(p)) {
        for (auto& n : list(p, true)) removeAll(p + "/" + n);
        for (auto& n : list(p, false)) unlink((p + "/" + n).c_str());
        rmdir(p.c_str());
    } else {
        unlink(p.c_str());
    }
}
inline void copyDir(const std::string& src, const std::string& dst) {
    mkdirs(dst);
    for (auto& n : list(src, false)) {
        std::string data;
        if (readFile(src + "/" + n, data)) writeFile(dst + "/" + n, data);
    }
    for (auto& n : list(src, true)) copyDir(src + "/" + n, dst + "/" + n);
}
inline std::string ext(const std::string& name) {
    size_t d = name.rfind('.');
    if (d == std::string::npos) return "";
    std::string e = name.substr(d + 1);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}
inline std::string stem(const std::string& name) {
    size_t d = name.rfind('.');
    return d == std::string::npos ? name : name.substr(0, d);
}

}  // namespace fsu
