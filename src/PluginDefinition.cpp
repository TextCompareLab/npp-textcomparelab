// TextCompareLab for Notepad++ - compares two open tabs, line by line with word-level highlights.
// Everything runs locally; the plugin makes no network connections.
// Copyright (C) 2026 the TextCompareLab project. GPL v2 or later.
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "diff.h"
#include <shellapi.h>
#include <commdlg.h>
#include <string>
#include <vector>

FuncItem funcItem[nbFunc];
NppData nppData;

static UINT_PTR g_marked = 0;               // tab chosen as "first"
static UINT_PTR g_cmpA = 0, g_cmpB = 0;     // tabs currently showing comparison marks
static bool g_ignoreSpace = false;

static const int MK_REMOVED = 10, MK_ADDED = 11, MK_CHANGED = 12, INDIC_TCL = 15;
static const int MK_MASK = (1 << MK_REMOVED) | (1 << MK_ADDED) | (1 << MK_CHANGED);
static const wchar_t* TITLE = L"TextCompareLab";

void pluginInit(HANDLE) {}
void pluginCleanUp() {}
void commandMenuCleanUp() {}

bool setCommand(size_t index, const TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) {
    if (index >= nbFunc || !pFunc) return false;
    lstrcpyn(funcItem[index]._itemName, cmdName, 63);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;
    return true;
}

// Default keyboard shortcuts (all can be changed in Settings > Shortcut Mapper > Plugin commands).
// Chosen to avoid Notepad++'s built-in main and Scintilla tables and ComparePlus's defaults; not F6 (NppExec).
static ShortcutKey g_keyChoose = {true, true, false, 'T'};      // Ctrl+Alt+T
static ShortcutKey g_keyNext   = {true, true, false, VK_DOWN};  // Ctrl+Alt+Down
static ShortcutKey g_keyPrev   = {true, true, false, VK_UP};    // Ctrl+Alt+Up
static ShortcutKey g_keyExport = {true, true, false, 'P'};      // Ctrl+Alt+P (not E: AltGr+E types the euro sign on several layouts)

void commandMenuInit() {
    setCommand(0, TEXT("1. Mark this tab as first"), markThisTab);
    setCommand(1, TEXT("2. Compare this tab with the first"), compareWithMarked);
    setCommand(2, TEXT("Choose two tabs to compare..."), chooseTabsDialog, &g_keyChoose, false);
    setCommand(3, TEXT("Clear comparison"), clearComparison);
    setCommand(4, TEXT("Next difference"), nextDifference, &g_keyNext, false);
    setCommand(5, TEXT("Previous difference"), prevDifference, &g_keyPrev, false);
    setCommand(6, TEXT("Ignore spaces and tabs"), ignoreWhitespaceToggle, NULL, false);
    setCommand(7, TEXT("Export as patch file (.patch)..."), exportPatch);
    setCommand(8, TEXT("Export as HTML report..."), exportHtml);
    setCommand(9, TEXT("Export... (patch or HTML report)"), exportChoose, &g_keyExport, false);
    setCommand(10, TEXT("Open textcomparelab.com"), openWebsite);
    setCommand(11, TEXT("About"), aboutBox);
}

static HWND sciOf(int view) { return view == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle; }
static LRESULT S(int view, UINT m, WPARAM w = 0, LPARAM l = 0) { return ::SendMessage(sciOf(view), m, w, l); }
static LRESULT N(UINT m, WPARAM w = 0, LPARAM l = 0) { return ::SendMessage(nppData._nppHandle, m, w, l); }

struct Pos { bool ok; int view; int index; };
static Pos posOf(UINT_PTR id) {
    if (!id) return {false, 0, 0};
    LRESULT p = N(NPPM_GETPOSFROMBUFFERID, id, 0);
    if ((int)p == -1) return {false, 0, 0};
    return {true, (int)(((unsigned)p) >> 30), (int)(((unsigned)p) & 0x3FFFFFFF)};
}
static void activate(const Pos& p) { N(NPPM_ACTIVATEDOC, p.view, p.index); }

static std::string getText(int view) {
    LRESULT len = S(view, SCI_GETLENGTH);
    std::string s((size_t)len + 1, '\0');
    S(view, SCI_GETTEXT, (WPARAM)len + 1, (LPARAM)&s[0]);
    s.resize((size_t)len);
    return s;
}

static void setupStyles(int view) {
    struct { int m; int r, g, b; int alpha; } d[3] = {
        {MK_REMOVED, 255, 60, 60, 55}, {MK_ADDED, 40, 190, 70, 55}, {MK_CHANGED, 255, 190, 0, 65}};
    for (auto& x : d) {
        S(view, SCI_MARKERDEFINE, x.m, SC_MARK_BACKGROUND);
        S(view, SCI_MARKERSETBACK, x.m, RGB(x.r, x.g, x.b));
        S(view, SCI_MARKERSETLAYER, x.m, SC_LAYER_UNDER_TEXT);  // translucent line colors need a non-base layer
        S(view, SCI_MARKERSETALPHA, x.m, x.alpha);
    }
    S(view, SCI_INDICSETSTYLE, INDIC_TCL, INDIC_STRAIGHTBOX);
    S(view, SCI_INDICSETFORE, INDIC_TCL, RGB(255, 140, 0));
    S(view, SCI_INDICSETALPHA, INDIC_TCL, 110);
    S(view, SCI_INDICSETOUTLINEALPHA, INDIC_TCL, 200);
    S(view, SCI_INDICSETUNDER, INDIC_TCL, 1);
}

static void clearMarks(int view) {
    for (int m : {MK_REMOVED, MK_ADDED, MK_CHANGED}) S(view, SCI_MARKERDELETEALL, m);
    S(view, SCI_SETINDICATORCURRENT, INDIC_TCL);
    S(view, SCI_INDICATORCLEARRANGE, 0, S(view, SCI_GETLENGTH));
}

static void applyMarks(int view, const std::vector<int>& rem, const std::vector<int>& add,
                       const std::vector<int>& chg, const std::vector<tcl::Range>& inl) {
    setupStyles(view);
    clearMarks(view);
    for (int l : rem) S(view, SCI_MARKERADD, l, MK_REMOVED);
    for (int l : add) S(view, SCI_MARKERADD, l, MK_ADDED);
    for (int l : chg) S(view, SCI_MARKERADD, l, MK_CHANGED);
    S(view, SCI_SETINDICATORCURRENT, INDIC_TCL);
    for (auto& r : inl) S(view, SCI_INDICATORFILLRANGE, (WPARAM)r.pos, (LPARAM)r.len);
}

void markThisTab() {
    g_marked = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    N(NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, TRUE);
    wchar_t path[MAX_PATH * 2] = L"";
    N(NPPM_GETFILENAME, MAX_PATH * 2, (LPARAM)path);
    std::wstring m = std::wstring(L"\"") + path + L"\" is now the first text.\nSwitch to the other tab and run \"2. Compare this tab with the first\".";
    ::MessageBoxW(nppData._nppHandle, m.c_str(), TITLE, MB_OK | MB_ICONINFORMATION);
}

void clearComparison() {
    UINT_PTR cur = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    for (UINT_PTR id : {g_cmpA, g_cmpB}) {
        Pos p = posOf(id);
        if (p.ok) { activate(p); clearMarks(p.view); }
    }
    g_cmpA = g_cmpB = 0;
    Pos c = posOf(cur);
    if (c.ok) activate(c);
}

static void compareImpl();
void compareWithMarked() {
    try { compareImpl(); }
    catch (...) { ::MessageBoxW(nppData._nppHandle, L"Not enough memory to compare these texts.", TITLE, MB_OK | MB_ICONWARNING); }
}
static void compareBuffers(UINT_PTR idA, UINT_PTR idB);
static void compareImpl() {
    HWND h = nppData._nppHandle;
    if (!g_marked) { ::MessageBoxW(h, L"First choose a tab: run \"1. Mark this tab as first\" on it.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    UINT_PTR cur = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    if (cur == g_marked) { ::MessageBoxW(h, L"Switch to the other tab first. This is the tab you marked.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    compareBuffers(g_marked, cur);
}

static void compareBuffers(UINT_PTR idA, UINT_PTR idB) {
    HWND h = nppData._nppHandle;
    UINT_PTR cur = idB;
    Pos pa = posOf(idA), pb = posOf(idB);
    if (!pa.ok) { if (idA == g_marked) { g_marked = 0; N(NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, FALSE); } ::MessageBoxW(h, L"The first tab was closed. Choose the tabs again.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    if (!pb.ok) return;

    clearComparison();
    activate(pa); std::string A = getText(pa.view);
    activate(pb); std::string B = getText(pb.view);

    if (A.size() > 200u * 1024 * 1024 || B.size() > 200u * 1024 * 1024) {
        ::MessageBoxW(h, L"One of the texts is larger than 200 MB. Too large to compare here.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    tcl::CompareResult r = tcl::compare(A, B, g_ignoreSpace);

    activate(pa); applyMarks(pa.view, r.removedLines, {}, r.changedA, r.inlineA);
    activate(pb); applyMarks(pb.view, {}, r.addedLines, r.changedB, r.inlineB);
    g_cmpA = idA; g_cmpB = idB;

    if (r.identical) {
        ::MessageBoxW(h, L"No differences. The two texts are identical.", TITLE, MB_OK | MB_ICONINFORMATION);
    } else if (r.tooDifferent) {
        ::MessageBoxW(h, L"The texts are very different, so every line is marked as changed.", TITLE, MB_OK | MB_ICONINFORMATION);
    } else {
        nextDifference();
    }
}

static int blockStartFrom(int view, int line, bool forward) {
    int lines = (int)S(view, SCI_GETLINECOUNT);
    auto go = [&](int from, bool fwd) {
        int l = fwd ? (int)S(view, SCI_MARKERNEXT, from, MK_MASK) : (from >= 0 ? (int)S(view, SCI_MARKERPREVIOUS, from, MK_MASK) : -1);
        if (fwd) { while (l > 0 && (S(view, SCI_MARKERGET, l - 1) & MK_MASK)) l = (int)S(view, SCI_MARKERNEXT, l + 1, MK_MASK); }
        else if (l >= 0) { while (l > 0 && (S(view, SCI_MARKERGET, l - 1) & MK_MASK)) --l; }
        return l; };
    int l = go(forward ? line + 1 : line - 1, forward);
    if (l < 0) l = go(forward ? 0 : lines - 1, forward);
    return l;
}

static void jump(bool forward) {
    UINT_PTR cur = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    if (cur != g_cmpA && cur != g_cmpB) { ::MessageBoxW(nppData._nppHandle, L"This tab has no comparison. Run a comparison first.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    int which = -1; N(NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which < 0) return;
    int line = (int)S(which, SCI_LINEFROMPOSITION, S(which, SCI_GETCURRENTPOS));
    int l = blockStartFrom(which, line, forward);
    if (l < 0) return;
    S(which, SCI_ENSUREVISIBLE, l);
    S(which, SCI_GOTOLINE, l);
}
void nextDifference() { jump(true); }
void prevDifference() { jump(false); }

void ignoreWhitespaceToggle() {
    g_ignoreSpace = !g_ignoreSpace;
    N(NPPM_SETMENUITEMCHECK, funcItem[6]._cmdID, g_ignoreSpace ? TRUE : FALSE);
}

void openWebsite() { ::ShellExecuteW(NULL, L"open", L"https://textcomparelab.com/", NULL, NULL, SW_SHOWNORMAL); }

static void aboutBoxImpl();
void aboutBox() { aboutBoxImpl(); }
static void aboutBoxImpl() {
    ::MessageBoxW(nppData._nppHandle,
        L"TextCompareLab for Notepad++ 1.1.3 (beta)\n\n"
        L"Compares two open tabs line by line, with word-level highlights.\n"
        L"Red: only in the first tab. Green: only in the second. Yellow: changed line, orange boxes mark the changed words.\n\n"
        L"Runs entirely on your computer. No text is sent anywhere.\n"
        L"https://textcomparelab.com/\nLicense: GPL v2 or later.", TITLE, MB_OK | MB_ICONINFORMATION);
}

// ===========================================================================
// Tab picker dialog
// ===========================================================================
struct Tab { UINT_PTR id; std::wstring name; };

static std::wstring pathOf(UINT_PTR id) {
    LRESULT n = N(NPPM_GETFULLPATHFROMBUFFERID, id, 0);
    if (n <= 0) return L"";
    std::wstring p((size_t)n + 1, L'\0');
    N(NPPM_GETFULLPATHFROMBUFFERID, id, (LPARAM)&p[0]);
    p.resize((size_t)n);
    return p;
}
static std::wstring baseName(const std::wstring& p) {
    size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? p : p.substr(k + 1);
}
static std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s((size_t)n, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::vector<Tab> listTabs() {
    std::vector<Tab> v;
    for (int view = 0; view < 2; ++view) {
        int n = (int)N(NPPM_GETNBOPENFILES, 0, view == 0 ? PRIMARY_VIEW : SECOND_VIEW);
        for (int i = 0; i < n; ++i) {
            UINT_PTR id = (UINT_PTR)N(NPPM_GETBUFFERIDFROMPOS, i, view);
            if (!id) continue;
            bool dup = false; for (auto& t : v) if (t.id == id) dup = true;
            if (dup) continue;
            std::wstring name = baseName(pathOf(id));
            if (name.empty()) name = L"(unnamed)";
            v.push_back({id, name});
        }
    }
    return v;
}

struct PickState { std::vector<Tab> tabs; int sel1 = 0, sel2 = 0; bool ignore = false; };
enum { ID_LIST1 = 1001, ID_LIST2 = 1002, ID_IGN = 1003 };

static INT_PTR CALLBACK pickProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITDIALOG) {
        ::SetWindowLongPtrW(dlg, GWLP_USERDATA, lp);
        PickState* st = (PickState*)lp;
        for (auto& t : st->tabs) {
            std::wstring label = t.name + (t.id == g_marked ? L"   (marked as first)" : L"");
            ::SendDlgItemMessageW(dlg, ID_LIST1, LB_ADDSTRING, 0, (LPARAM)label.c_str());
            ::SendDlgItemMessageW(dlg, ID_LIST2, LB_ADDSTRING, 0, (LPARAM)label.c_str());
        }
        ::SendDlgItemMessageW(dlg, ID_LIST1, LB_SETCURSEL, st->sel1, 0);
        ::SendDlgItemMessageW(dlg, ID_LIST2, LB_SETCURSEL, st->sel2, 0);
        ::CheckDlgButton(dlg, ID_IGN, st->ignore ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    if (msg == WM_COMMAND) {
        PickState* st = (PickState*)::GetWindowLongPtrW(dlg, GWLP_USERDATA);
        if (LOWORD(wp) == IDOK) {
            int a = (int)::SendDlgItemMessageW(dlg, ID_LIST1, LB_GETCURSEL, 0, 0);
            int b = (int)::SendDlgItemMessageW(dlg, ID_LIST2, LB_GETCURSEL, 0, 0);
            if (a < 0 || b < 0) { ::MessageBoxW(dlg, L"Select one tab in each list.", TITLE, MB_OK | MB_ICONINFORMATION); return TRUE; }
            if (a == b) { ::MessageBoxW(dlg, L"Select two different tabs.", TITLE, MB_OK | MB_ICONINFORMATION); return TRUE; }
            st->sel1 = a; st->sel2 = b; st->ignore = ::IsDlgButtonChecked(dlg, ID_IGN) == BST_CHECKED;
            ::EndDialog(dlg, IDOK); return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { ::EndDialog(dlg, IDCANCEL); return TRUE; }
    }
    return FALSE;
}

namespace {
struct DlgBuilder {
    std::vector<WORD> d;
    void align() { if (d.size() % 2) d.push_back(0); }
    void str(const wchar_t* s) { do d.push_back((WORD)*s); while (*s++); }
    void dword(DWORD v) { d.push_back(LOWORD(v)); d.push_back(HIWORD(v)); }
    void head(DWORD style, short x, short y, short cx, short cy, WORD nItems, const wchar_t* title) {
        dword(style | DS_SETFONT); dword(0); d.push_back(nItems);
        d.push_back((WORD)x); d.push_back((WORD)y); d.push_back((WORD)cx); d.push_back((WORD)cy);
        d.push_back(0); d.push_back(0); str(title); d.push_back(9); str(L"Segoe UI");
    }
    void item(DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* text) {
        align(); dword(style | WS_CHILD | WS_VISIBLE); dword(0);
        d.push_back((WORD)x); d.push_back((WORD)y); d.push_back((WORD)cx); d.push_back((WORD)cy); d.push_back(id);
        d.push_back(0xFFFF); d.push_back(cls); str(text); d.push_back(0);
    }
};
}

static void chooseTabsImpl() {
    PickState st;
    st.tabs = listTabs();
    if (st.tabs.size() < 2) { ::MessageBoxW(nppData._nppHandle, L"Open at least two tabs first.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    UINT_PTR cur = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    int ci = 0, mi = -1;
    for (size_t i = 0; i < st.tabs.size(); ++i) { if (st.tabs[i].id == cur) ci = (int)i; if (st.tabs[i].id == g_marked) mi = (int)i; }
    st.sel2 = ci;
    st.sel1 = (mi >= 0 && mi != ci) ? mi : (ci == 0 ? 1 : ci - 1);
    st.ignore = g_ignoreSpace;

    DlgBuilder b;
    b.head(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER, 0, 0, 300, 190, 7, L"TextCompareLab: choose two tabs");
    b.item(SS_LEFT, 8, 6, 136, 10, 0xFFFF, 0x0082, L"First text (original):");
    b.item(SS_LEFT, 154, 6, 136, 10, 0xFFFF, 0x0082, L"Second text (changed):");
    b.item(WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 8, 18, 136, 120, ID_LIST1, 0x0083, L"");
    b.item(WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 154, 18, 136, 120, ID_LIST2, 0x0083, L"");
    b.item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 146, 160, 12, ID_IGN, 0x0080, L"Ignore spaces and tabs");
    b.item(BS_DEFPUSHBUTTON | WS_TABSTOP, 182, 168, 52, 14, IDOK, 0x0080, L"Compare");
    b.item(BS_PUSHBUTTON | WS_TABSTOP, 238, 168, 52, 14, IDCANCEL, 0x0080, L"Cancel");
    b.align();

    INT_PTR rc = ::DialogBoxIndirectParamW((HINSTANCE)::GetModuleHandleW(NULL), (LPCDLGTEMPLATEW)b.d.data(), nppData._nppHandle, pickProc, (LPARAM)&st);
    if (rc != IDOK) return;
    g_ignoreSpace = st.ignore;
    N(NPPM_SETMENUITEMCHECK, funcItem[6]._cmdID, g_ignoreSpace ? TRUE : FALSE);
    compareBuffers(st.tabs[st.sel1].id, st.tabs[st.sel2].id);
}
void chooseTabsDialog() {
    try { chooseTabsImpl(); }
    catch (...) { ::MessageBoxW(nppData._nppHandle, L"Not enough memory to compare these texts.", TITLE, MB_OK | MB_ICONWARNING); }
}

// ===========================================================================
// Export: patch and HTML
// ===========================================================================
struct Pair { std::string A, B, nameA, nameB; bool ok = false; };

static Pair readPair() {
    Pair p;
    HWND h = nppData._nppHandle;
    Pos pa = posOf(g_cmpA), pb = posOf(g_cmpB);
    if (!g_cmpA || !g_cmpB || !pa.ok || !pb.ok) {
        ::MessageBoxW(h, L"Compare two tabs first (\"Choose two tabs to compare...\"), then export.", TITLE, MB_OK | MB_ICONINFORMATION);
        return p;
    }
    UINT_PTR cur = (UINT_PTR)N(NPPM_GETCURRENTBUFFERID);
    activate(pa); p.A = getText(pa.view);
    activate(pb); p.B = getText(pb.view);
    if ((int)N(NPPM_GETBUFFERENCODING, g_cmpA, 0) == 1) p.A = "\xEF\xBB\xBF" + p.A;
    if ((int)N(NPPM_GETBUFFERENCODING, g_cmpB, 0) == 1) p.B = "\xEF\xBB\xBF" + p.B;
    p.nameA = utf8(baseName(pathOf(g_cmpA))); p.nameB = utf8(baseName(pathOf(g_cmpB)));
    if (p.nameA.empty()) p.nameA = "first.txt";
    if (p.nameB.empty()) p.nameB = "second.txt";
    Pos c = posOf(cur); if (c.ok) activate(c);
    p.ok = true;
    return p;
}

static bool saveDialog(const wchar_t* defName, const wchar_t* filter, const wchar_t* defExt, std::wstring& outPath) {
    wchar_t buf[MAX_PATH * 2];
    lstrcpynW(buf, defName, MAX_PATH * 2);
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof o; o.hwndOwner = nppData._nppHandle; o.lpstrFilter = filter;
    o.lpstrFile = buf; o.nMaxFile = MAX_PATH * 2; o.lpstrDefExt = defExt;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!::GetSaveFileNameW(&o)) return false;
    outPath = buf; return true;
}

static bool writeFile(const std::wstring& path, const std::string& data) {
    HANDLE f = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    size_t done = 0; bool ok = true;
    while (done < data.size() && ok) {
        DWORD w = 0, chunk = (DWORD)std::min<size_t>(data.size() - done, 1u << 26);
        ok = ::WriteFile(f, data.data() + done, chunk, &w, NULL) && w > 0; done += w;
    }
    ::CloseHandle(f);
    return ok;
}

static void exportPatchImpl() {
    HWND h = nppData._nppHandle;
    Pair p = readPair(); if (!p.ok) return;
    // A patch must apply byte for byte. Only UTF-8 text and pure ASCII text are exported (the tested cases).
    {
        auto okEnc = [](UINT_PTR id, const std::string& t) {
            int e = (int)N(NPPM_GETBUFFERENCODING, id, 0);
            if (e == 1 || e == 4) return true;                       // UTF-8 (with or without BOM)
            if (e == 0 || e == 5) {                                  // ANSI / ASCII: only if every byte is ASCII
                for (unsigned char c : t) if (c >= 0x80) return false;
                return true;
            }
            return false;                                            // UTF-16 and anything else
        };
        if (!okEnc(g_cmpA, p.A) || !okEnc(g_cmpB, p.B)) {
            ::MessageBoxW(h, L"No patch was saved. A patch can only be exported when both tabs are UTF-8 text (or plain ASCII). Convert the tabs to UTF-8 (Encoding menu; use a copy of the file) and compare again.", TITLE, MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    std::string patch;
    // Both header names use the first tab's file name: "git apply" and "patch" then apply it to that file.
    int rc = tcl::unifiedDiff(p.A, p.B, p.nameA, p.nameA, patch);
    if (rc == 1) { ::MessageBoxW(h, L"The two texts are identical. No patch to export.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    if (rc == 2) { ::MessageBoxW(h, L"One of the texts uses old Mac line endings (a lone CR). Convert them in Notepad++ (Edit > EOL Conversion) and compare again.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    if (rc != 0) { ::MessageBoxW(h, L"The texts are too different to export a patch.", TITLE, MB_OK | MB_ICONINFORMATION); return; }
    std::wstring path;
    std::wstring def = baseName(pathOf(g_cmpA)) + L".patch";
    if (!saveDialog(def.c_str(), L"Patch files (*.patch;*.diff)\0*.patch;*.diff\0All files\0*.*\0", L"patch", path)) return;
    if (!writeFile(path, patch)) { ::MessageBoxW(h, L"Could not write the file.", TITLE, MB_OK | MB_ICONWARNING); return; }
    // Opened automatically for viewing only: nothing is applied or executed.
    if (!N(NPPM_DOOPEN, 0, (LPARAM)path.c_str()))
        ::MessageBoxW(h, L"The patch was saved, but Notepad++ could not open it. You can open the saved file yourself.", TITLE, MB_OK | MB_ICONINFORMATION);
}
void exportPatch() {
    try { exportPatchImpl(); }
    catch (...) { ::MessageBoxW(nppData._nppHandle, L"Not enough memory to export.", TITLE, MB_OK | MB_ICONWARNING); }
}

static void exportHtmlImpl() {
    HWND h = nppData._nppHandle;
    Pair p = readPair(); if (!p.ok) return;
    if (::MessageBoxW(h, L"The report contains the full text of both documents, and works in any browser without the plugin. Share it only with people who may see both texts.\n\nSave the report?", TITLE, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return;
    tcl::CompareResult r = tcl::compare(p.A, p.B, g_ignoreSpace);
    std::string html = tcl::htmlReport(p.A, p.B, p.nameA, p.nameB, r);
    std::wstring path;
    std::wstring def = baseName(pathOf(g_cmpA)) + L"-diff.html";
    if (!saveDialog(def.c_str(), L"HTML files (*.html)\0*.html\0", L"html", path)) return;
    if (!writeFile(path, html)) { ::MessageBoxW(h, L"Could not write the file.", TITLE, MB_OK | MB_ICONWARNING); return; }
    // Opened automatically in the default viewer; only for a .html/.htm name (the save dialog lets the user type any name).
    size_t dot = path.find_last_of(L'.');
    std::wstring ext = dot == std::wstring::npos ? L"" : path.substr(dot);
    for (auto& c : ext) c = (wchar_t)::towlower(c);
    if (ext == L".html" || ext == L".htm") {
        INT_PTR r = (INT_PTR)::ShellExecuteW(h, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (r <= 32) ::MessageBoxW(h, L"The report was saved, but it could not be opened in your browser. You can open the saved file yourself.", TITLE, MB_OK | MB_ICONINFORMATION);
    } else {
        ::MessageBoxW(h, L"Report saved. It is not named .html, so it was not opened automatically.", TITLE, MB_OK | MB_ICONINFORMATION);
    }
}
void exportHtml() {
    try { exportHtmlImpl(); }
    catch (...) { ::MessageBoxW(nppData._nppHandle, L"Not enough memory to export.", TITLE, MB_OK | MB_ICONWARNING); }
}


// ---- Toolbar buttons (drawn in code: 16x16, light and dark variants) ----
static void glyphPixels(int kind, unsigned char fr, unsigned char fg, unsigned char fb, unsigned char* a /*256 alpha*/) {
    for (int i = 0; i < 256; i++) a[i] = 0;
    auto put = [&](int x, int y) { if (x >= 0 && x < 16 && y >= 0 && y < 16) a[y * 16 + x] = 255; };
    (void)fr; (void)fg; (void)fb;
    if (kind == 0) {            // two documents side by side
        for (int y = 2; y <= 13; y++) for (int x = 0; x < 16; x++) {
            bool L = x >= 1 && x <= 6, R = x >= 9 && x <= 14;
            if (!L && !R) continue;
            int x0 = L ? 1 : 9, x1 = L ? 6 : 14;
            bool edge = (y == 2 || y == 13 || x == x0 || x == x1);
            bool line = (y == 5 || y == 8 || y == 11) && x > x0 + 0 && x < x1;
            if (edge || line) put(x, y);
        }
    } else if (kind == 1) {     // down arrow (next)
        for (int y = 2; y <= 3; y++) for (int x = 3; x <= 12; x++) put(x, y);
        for (int y = 5; y <= 12; y++) { int hw = 12 - y; if (hw > 4) hw = 4; for (int x = 7 - hw; x <= 8 + hw; x++) put(x, y); }
    } else if (kind == 3) {     // export: arrow leaving a tray
        for (int y = 2; y <= 5; y++) { int hw = y - 2; for (int x = 7 - hw; x <= 8 + hw; x++) put(x, y); }
        for (int y = 6; y <= 10; y++) { put(7, y); put(8, y); }
        for (int y = 8; y <= 13; y++) { put(2, y); put(3, y); put(12, y); put(13, y); }
        for (int y = 12; y <= 13; y++) for (int x = 2; x <= 13; x++) put(x, y);
    } else {                    // up arrow (previous)
        for (int y = 12; y <= 13; y++) for (int x = 3; x <= 12; x++) put(x, y);
        for (int y = 3; y <= 10; y++) { int hw = y - 3; if (hw > 4) hw = 4; for (int x = 7 - hw; x <= 8 + hw; x++) put(x, y); }
    }
}
static HICON makeIcon(int kind, bool dark) {
    unsigned char a[256]; unsigned char r = dark ? 235 : 40, g = dark ? 235 : 40, b = dark ? 235 : 90;
    glyphPixels(kind, r, g, b, a);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = -16;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr; HDC dc = ::GetDC(nullptr);
    HBITMAP color = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ::ReleaseDC(nullptr, dc);
    if (!color || !bits) return nullptr;
    unsigned int* px = (unsigned int*)bits;
    for (int i = 0; i < 256; i++) px[i] = a[i] ? (0xFF000000u | (r << 16) | (g << 8) | b) : 0;
    HBITMAP mask = ::CreateBitmap(16, 16, 1, 1, nullptr);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = color; ii.hbmMask = mask;
    HICON ic = ::CreateIconIndirect(&ii);
    ::DeleteObject(color); ::DeleteObject(mask);
    return ic;
}
static HBITMAP makeBitmap(int kind) {
    unsigned char a[256]; glyphPixels(kind, 0, 0, 0, a);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = -16;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr; HDC dc = ::GetDC(nullptr);
    HBITMAP bm = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ::ReleaseDC(nullptr, dc);
    if (!bm || !bits) return nullptr;
    unsigned int* px = (unsigned int*)bits;   // light grey 192,192,192 is the transparent colour of Notepad++ toolbar bitmaps
    for (int i = 0; i < 256; i++) px[i] = a[i] ? 0x00282860u : 0x00C0C0C0u;
    return bm;
}
void toolbarInit() {
    const int cmd[4] = {2, 4, 5, 9};   // choose tabs, next, previous, export
    const int kind[4] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i++) {
        toolbarIconsWithDarkMode t = {};
        t.hToolbarBmp = makeBitmap(kind[i]);
        t.hToolbarIcon = makeIcon(kind[i], false);
        t.hToolbarIconDarkMode = makeIcon(kind[i], true);
        if (t.hToolbarBmp && t.hToolbarIcon && t.hToolbarIconDarkMode)
            N(NPPM_ADDTOOLBARICON_FORDARKMODE, funcItem[cmd[i]]._cmdID, (LPARAM)&t);
    }
}

void exportChoose() {
    HWND h = nppData._nppHandle;
    int r = ::MessageBoxW(h, L"Export the comparison as:\n\nYes = patch file (.patch), applies with git apply / patch\nNo = HTML report (opens in your browser)\nCancel = nothing", TITLE, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDYES) exportPatch();
    else if (r == IDNO) exportHtml();
}
