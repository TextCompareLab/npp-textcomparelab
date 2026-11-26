// TextCompareLab for Notepad++ installer: portable core (no Windows calls), unit-tested on Linux.
// Copyright (C) 2026 the TextCompareLab project. GPL v2 or later.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
namespace tcli {
namespace fs = std::filesystem;
typedef std::vector<uint8_t> Bytes;

struct Result { bool ok = false; bool denied = false; std::string msg; };

inline bool readFile(const fs::path& p, Bytes& out, size_t maxBytes = (size_t)64 << 20) {
    std::error_code ec; auto sz = fs::file_size(p, ec);
    if (ec || sz > maxBytes) return false;
    FILE* f = nullptr;
#ifdef _WIN32
    f = _wfopen(p.c_str(), L"rb");
#else
    f = fopen(p.c_str(), "rb");
#endif
    if (!f) return false;
    out.resize((size_t)sz);
    size_t n = out.empty() ? 0 : fread(out.data(), 1, out.size(), f);
    fclose(f);
    return n == out.size();
}
inline bool writeFile(const fs::path& p, const Bytes& b) {
    FILE* f = nullptr;
#ifdef _WIN32
    f = _wfopen(p.c_str(), L"wb");
#else
    f = fopen(p.c_str(), "wb");
#endif
    if (!f) return false;
    size_t n = b.empty() ? 0 : fwrite(b.data(), 1, b.size(), f);
    bool ok = (fclose(f) == 0) && n == b.size();
    return ok;
}

// PE machine type: "x64", "x86", "arm64", or "" if not a PE file.
inline std::string peArch(const Bytes& b) {
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return "";
    uint32_t pe = b[0x3C] | (b[0x3D] << 8) | (b[0x3E] << 16) | ((uint32_t)b[0x3F] << 24);
    if ((size_t)pe + 6 > b.size() || memcmp(&b[pe], "PE\0\0", 4) != 0) return "";
    uint16_t m = b[pe + 4] | (b[pe + 5] << 8);
    if (m == 0x8664) return "x64";
    if (m == 0x14c) return "x86";
    if (m == 0xAA64) return "arm64";
    return "";
}

// FileVersion string ("1.1.3.0") from the UTF-16 version resource of a DLL or EXE; "" if none.
inline std::string fileVersion(const Bytes& b) {
    static const char key[] = "F\0i\0l\0e\0V\0e\0r\0s\0i\0o\0n\0";   // 11 UTF-16 chars
    const size_t kl = 22;
    for (size_t i = 0; i + kl + 4 < b.size(); i++) {
        if (memcmp(&b[i], key, kl) != 0) continue;
        if (i >= 2 && !(b[i - 1] == 0 && b[i - 2] == 0) ) { /* StringFileInfo entries are preceded by padding */ }
        size_t j = i + kl;
        while (j + 1 < b.size() && b[j] == 0 && b[j + 1] == 0) j += 2;   // key terminator and padding
        std::string v;
        while (j + 1 < b.size() && b[j + 1] == 0 && (b[j] == '.' || (b[j] >= '0' && b[j] <= '9')) && v.size() < 32) { v += (char)b[j]; j += 2; }
        if (!v.empty()) return v;
    }
    return "";
}
// <0, 0, >0 like strcmp on dotted numbers.
inline int cmpVersion(const std::string& a, const std::string& c) {
    auto parse = [](const std::string& s) { std::vector<long> v; long cur = 0; bool any = false;
        for (char ch : s) { if (ch == '.') { v.push_back(cur); cur = 0; any = false; } else if (ch >= '0' && ch <= '9') { cur = cur * 10 + (ch - '0'); any = true; } }
        if (any || !s.empty()) v.push_back(cur); return v; };
    auto x = parse(a), y = parse(c); size_t n = x.size() > y.size() ? x.size() : y.size();
    x.resize(n, 0); y.resize(n, 0);
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

inline fs::path pluginDir(const fs::path& npp) { return npp / "plugins" / "TextCompareLab"; }
inline fs::path pluginDll(const fs::path& npp) { return pluginDir(npp) / "TextCompareLab.dll"; }
inline bool looksLikeNpp(const fs::path& npp) { std::error_code ec; return fs::is_regular_file(npp / "notepad++.exe", ec); }

inline Result fromEc(const std::error_code& ec, const char* what) {
    Result r; r.msg = std::string(what) + ": " + ec.message();
    r.denied = (ec == std::errc::permission_denied || ec == std::errc::operation_not_permitted);
    return r;
}

// Installs the DLL. Keeps the previous DLL as TextCompareLab.dll.bak. Never touches anything else.
inline Result installDll(const fs::path& npp, const Bytes& dll) {
    Result r;
    if (!looksLikeNpp(npp)) { r.msg = "notepad++.exe was not found in that folder."; return r; }
    std::error_code ec;
    fs::path dir = pluginDir(npp), dst = pluginDll(npp), tmp = dir / "TextCompareLab.dll.new", bak = dir / "TextCompareLab.dll.bak";
    fs::create_directories(dir, ec); if (ec) return fromEc(ec, "Could not create the plugin folder");
    if (!writeFile(tmp, dll)) { std::error_code e2; fs::remove(tmp, e2); r.msg = "Could not write the plugin file (folder not writable?)."; r.denied = true; return r; }
    Bytes back; if (!readFile(tmp, back) || back != dll) { fs::remove(tmp, ec); r.msg = "The written file did not match; nothing was installed."; return r; }
    bool had = fs::exists(dst, ec);
    if (had) {
        fs::remove(bak, ec);
        ec.clear(); fs::rename(dst, bak, ec);
        if (ec) { std::error_code e2; fs::remove(tmp, e2); r = fromEc(ec, "Could not replace the existing plugin (is Notepad++ still running?)"); return r; }
    }
    ec.clear(); fs::rename(tmp, dst, ec);
    if (ec) {
        std::error_code e2; if (had) fs::rename(bak, dst, e2);   // restore
        fs::remove(tmp, e2);
        return fromEc(ec, "Could not finish the install");
    }
    r.ok = true; r.msg = "Installed."; return r;
}
// Removes only our DLL files and then our folder if it is empty.
inline Result uninstallDll(const fs::path& npp) {
    Result r; std::error_code ec; fs::path dir = pluginDir(npp);
    if (!fs::exists(dir, ec)) { r.ok = true; r.msg = "Nothing to remove."; return r; }
    for (const char* n : {"TextCompareLab.dll", "TextCompareLab.dll.bak", "TextCompareLab.dll.new"}) {
        fs::path p = dir / n; ec.clear();
        if (fs::exists(p, ec)) { fs::remove(p, ec); if (ec) return fromEc(ec, "Could not remove the plugin (is Notepad++ still running?)"); }
    }
    ec.clear(); if (fs::is_empty(dir, ec)) fs::remove(dir, ec);
    r.ok = true; r.msg = "Removed."; return r;
}
}  // namespace tcli
