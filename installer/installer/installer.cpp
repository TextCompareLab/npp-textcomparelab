// TextCompareLab for Notepad++ installer. Copyright (C) 2026 the TextCompareLab project. GPL v2 or later.
// Installs TextCompareLab.dll into the user's Notepad++ plugins folder. No network, no registry writes,
// never closes Notepad++, never changes Windows security settings.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include "core.h"
using namespace tcli;

#define VERSION_STR "1.1.3"
static const wchar_t* TITLE = L"TextCompareLab for Notepad++ installer";

static bool resource(int id, Bytes& out) {
    HRSRC r = ::FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!r) return false;
    HGLOBAL g = ::LoadResource(nullptr, r); DWORD n = ::SizeofResource(nullptr, r);
    const uint8_t* p = (const uint8_t*)::LockResource(g);
    if (!g || !p || !n) return false;
    out.assign(p, p + n); return true;
}
static int box(const std::wstring& t, UINT flags) { return ::MessageBoxW(nullptr, t.c_str(), TITLE, flags); }
static std::wstring w(const std::string& s) { return std::wstring(s.begin(), s.end()); }

static bool regPath(HKEY root, REGSAM view, std::wstring& out) {
    HKEY k; if (::RegOpenKeyExW(root, L"SOFTWARE\\Notepad++", 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return false;
    wchar_t buf[1024]; DWORD sz = sizeof(buf) - 2, type = 0;
    LONG rc = ::RegQueryValueExW(k, nullptr, nullptr, &type, (BYTE*)buf, &sz); ::RegCloseKey(k);
    if (rc != ERROR_SUCCESS || type != REG_SZ) return false;
    buf[sz / 2] = 0; out = buf; return !out.empty();
}
static std::wstring env(const wchar_t* n) { wchar_t b[1024]; DWORD r = ::GetEnvironmentVariableW(n, b, 1024); return (r > 0 && r < 1024) ? std::wstring(b) : L""; }
static std::vector<fs::path> candidates() {
    std::vector<fs::path> c; std::wstring p;
    if (regPath(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, p)) c.push_back(p);
    if (regPath(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, p)) c.push_back(p);
    if (regPath(HKEY_CURRENT_USER, 0, p)) c.push_back(p);
    for (const wchar_t* v : {L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)"}) { std::wstring b = env(v); if (!b.empty()) c.push_back(fs::path(b) / L"Notepad++"); }
    std::vector<fs::path> out;
    for (auto& x : c) { bool dup = false; for (auto& y : out) if (_wcsicmp(x.c_str(), y.c_str()) == 0) dup = true; if (!dup && looksLikeNpp(x)) out.push_back(x); }
    return out;
}
static bool pickFolder(fs::path& out) {
    BROWSEINFOW bi = {}; bi.lpszTitle = L"Choose the Notepad++ folder (the one that contains notepad++.exe; for a portable copy, its own folder)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolderW(&bi); if (!pidl) return false;
    wchar_t b[MAX_PATH]; bool ok = ::SHGetPathFromIDListW(pidl, b) != 0; ::CoTaskMemFree(pidl);
    if (ok) out = b; return ok;
}
static bool nppRunning() {
    HANDLE s = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (s == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e = {}; e.dwSize = sizeof(e); bool f = false;
    if (::Process32FirstW(s, &e)) do { if (_wcsicmp(e.szExeFile, L"notepad++.exe") == 0) { f = true; break; } } while (::Process32NextW(s, &e));
    ::CloseHandle(s); return f;
}
static bool isElevated() {
    HANDLE t; if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &t)) return false;
    TOKEN_ELEVATION el = {}; DWORD n = 0; bool r = ::GetTokenInformation(t, TokenElevation, &el, sizeof(el), &n) && el.TokenIsElevated; ::CloseHandle(t); return r;
}
// Relaunch this installer with administrator rights (the user sees the normal Windows approval prompt).
static bool relaunchElevated(const fs::path& npp, const wchar_t* action) {
    wchar_t exe[MAX_PATH]; ::GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring args = std::wstring(L"/elevated /action ") + action + L" /dir \"" + npp.wstring() + L"\"";
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"runas"; sei.lpFile = exe; sei.lpParameters = args.c_str(); sei.nShow = SW_SHOWNORMAL;
    return ::ShellExecuteExW(&sei) != 0;
}
static bool waitNppClosed() {
    while (nppRunning()) {
        int r = box(L"Notepad++ is running.\n\nSave your work and close Notepad++ yourself, then click Retry. This installer never closes it for you.", MB_RETRYCANCEL | MB_ICONINFORMATION);
        if (r != IDRETRY) return false;
    }
    return true;
}
static std::string archOfNpp(const fs::path& npp) {
    Bytes b; if (!readFile(npp / "notepad++.exe", b, (size_t)256 << 20)) return ""; return peArch(b);
}
static bool writeNotices(const fs::path& npp) {
    Bytes a, b; bool ok = true;
    if (resource(201, a)) ok &= writeFile(pluginDir(npp) / "LICENSE-GPL2.txt", a);
    if (resource(202, b)) ok &= writeFile(pluginDir(npp) / "THIRD-PARTY-NOTICES.txt", b);
    return ok;
}
static int doInstall(const fs::path& npp, bool quiet, bool elevated) {
    std::string arch = archOfNpp(npp);
    if (arch.empty()) { if (!quiet) box(L"Could not read notepad++.exe to see whether it is 32-bit, 64-bit or ARM64. Nothing was installed.", MB_OK | MB_ICONWARNING); return 3; }
    Bytes dll; int id = arch == "x64" ? 101 : arch == "x86" ? 102 : 103;
    if (!resource(id, dll)) { if (!quiet) box(L"The installer is missing its plugin file. Nothing was installed.", MB_OK | MB_ICONERROR); return 4; }
    if (!quiet && !waitNppClosed()) return 1;
    Result r = installDll(npp, dll);
    if (!r.ok && r.denied && !elevated && !quiet) {
        if (box(L"Windows needs administrator permission to write into this folder.\n\nClick OK to approve that in the next Windows prompt, or Cancel to stop. Nothing has been changed yet.", MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
            return relaunchElevated(npp, L"install") ? 0 : 5;
        return 1;
    }
    if (!r.ok) { if (!quiet) box(L"Not installed.\n\n" + w(r.msg) + L"\n\nYour Notepad++ was not changed (a previous version of the plugin, if any, was kept).", MB_OK | MB_ICONWARNING); return 6; }
    writeNotices(npp);
    if (!quiet) box(L"TextCompareLab " VERSION_STR L" (" + w(arch) + L") is installed in:\n" + (pluginDir(npp)).wstring() + L"\n\nStart Notepad++ and open the Plugins menu > TextCompareLab. This is a beta that has been tested only in part inside Notepad++.", MB_OK | MB_ICONINFORMATION);
    return 0;
}
static int doUninstall(const fs::path& npp, bool quiet, bool elevated) {
    if (!quiet && !waitNppClosed()) return 1;
    Result r = uninstallDll(npp);
    if (!r.ok && r.denied && !elevated && !quiet) {
        if (box(L"Windows needs administrator permission to remove the plugin from this folder.\n\nClick OK to approve that in the next Windows prompt.", MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
            return relaunchElevated(npp, L"uninstall") ? 0 : 5;
        return 1;
    }
    if (!r.ok) { if (!quiet) box(L"Not removed.\n\n" + w(r.msg), MB_OK | MB_ICONWARNING); return 6; }
    std::error_code ec; fs::remove(pluginDir(npp) / "LICENSE-GPL2.txt", ec); fs::remove(pluginDir(npp) / "THIRD-PARTY-NOTICES.txt", ec); fs::remove(pluginDir(npp), ec);  // only succeeds if empty
    if (!quiet) box(L"TextCompareLab was removed. Other plugins were not touched.", MB_OK | MB_ICONINFORMATION);
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0; LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    bool quiet = false, elevated = false; std::wstring action = L"", dir;
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"/quiet") quiet = true; else if (a == L"/elevated") elevated = true;
        else if (a == L"/action" && i + 1 < argc) action = argv[++i];
        else if (a == L"/dir" && i + 1 < argc) dir = argv[++i];
    }
    if (!dir.empty()) {     // scripted or elevated run: the folder was already chosen
        fs::path npp = dir; if (!looksLikeNpp(npp)) { if (!quiet) box(L"notepad++.exe was not found in that folder. Nothing was changed.", MB_OK | MB_ICONWARNING); return 2; }
        return action == L"uninstall" ? doUninstall(npp, quiet, elevated) : doInstall(npp, quiet, elevated);
    }
    box(L"This installs the TextCompareLab plugin (version " VERSION_STR L" beta) for Notepad++.\n\nIt copies one plugin file into your Notepad++ plugins folder. It does not connect to the internet, does not change Windows settings, and never closes Notepad++. The plugin is free software (GPL v2 or later), unsigned, and only partly tested inside Notepad++.", MB_OK | MB_ICONINFORMATION);
    auto c = candidates(); fs::path npp;
    for (;;) {
        if (!c.empty() && npp.empty()) {
            int r = box(L"Install into this Notepad++ folder?\n\n" + c[0].wstring() + L"\n\nYes = install here, No = choose another folder, Cancel = stop.", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (r == IDYES) npp = c[0]; else if (r == IDCANCEL) return 1;
        }
        if (npp.empty()) {
            if (!pickFolder(npp)) return 1;
            if (!looksLikeNpp(npp)) { box(L"notepad++.exe was not found in that folder. Please choose the folder that contains notepad++.exe.", MB_OK | MB_ICONWARNING); npp.clear(); c.clear(); continue; }
        }
        break;
    }
    Bytes inst; std::string have; if (readFile(pluginDll(npp), inst)) have = fileVersion(inst);
    if (fs::exists(pluginDll(npp))) {
        std::wstring hv = have.empty() ? L"an unknown version" : w(have);
        int cmp = have.empty() ? -1 : cmpVersion(have, VERSION_STR);
        std::wstring msg = L"TextCompareLab " + hv + L" is already installed here.\n\n";
        if (cmp == 0) msg += L"Yes = reinstall the same version, No = uninstall it, Cancel = stop.";
        else if (cmp < 0) msg += L"Yes = replace it with " VERSION_STR L" (the old file is kept as TextCompareLab.dll.bak), No = uninstall it, Cancel = stop.";
        else msg += L"The installed version is NEWER than this installer. Yes = replace it with the older " VERSION_STR L" anyway, No = uninstall it, Cancel = stop.";
        int r = box(msg, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return 1;
        if (r == IDNO) return doUninstall(npp, false, false);
    }
    return doInstall(npp, false, false);
}
