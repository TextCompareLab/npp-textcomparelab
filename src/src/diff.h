// TextCompareLab for Notepad++ - diff engine (no Windows dependencies, unit-testable)
// Copyright (C) 2026 the TextCompareLab project. GPL v2 or later.
#pragma once
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

namespace tcl {

struct Range { size_t pos; size_t len; };

// Work budget so a huge or totally different input can never freeze Notepad++.
inline long long& workBudget() { static long long b = 0; return b; }

struct CompareResult {
    std::vector<int> removedLines;   // lines only in A
    std::vector<int> addedLines;     // lines only in B
    std::vector<int> changedA;       // lines in A paired with a changed line in B
    std::vector<int> changedB;
    std::vector<Range> inlineA;      // absolute byte ranges (in A) of changed words
    std::vector<Range> inlineB;
    bool tooDifferent = false;       // diff limit hit: everything marked as changed
    bool identical = false;
    struct Row { char c; int ia, ib; }; // ' ' same, 'c' changed pair, '-' only A, '+' only B
    std::vector<Row> rows;
};

// Myers O(ND). keepA/keepB[i]=1 where element is common. Returns false if D exceeds maxD.
inline bool myers(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b,
                  std::vector<char>& keepA, std::vector<char>& keepB, int maxD) {
    const int n = (int)a.size(), m = (int)b.size();
    keepA.assign(n, 0); keepB.assign(m, 0);
    int pre = 0;
    while (pre < n && pre < m && a[pre] == b[pre]) { keepA[pre] = keepB[pre] = 1; ++pre; }
    int suf = 0;
    while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == b[m - 1 - suf]) {
        keepA[n - 1 - suf] = keepB[m - 1 - suf] = 1; ++suf; }
    const int N = n - pre - suf, M = m - pre - suf;
    if (N == 0 || M == 0) return true;
    const int Dmax = std::min(N + M, maxD);
    if (Dmax < 0) return false;
    std::vector<int> V(2 * Dmax + 3, 0);
    const int off = Dmax + 1;
    std::vector<std::vector<int>> trace;
    int foundD = -1;
    for (int d = 0; d <= Dmax && foundD < 0; ++d) {
        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && V[off + k - 1] < V[off + k + 1])) x = V[off + k + 1];
            else x = V[off + k - 1] + 1;
            int y = x - k;
            while (x < N && y < M && a[pre + x] == b[pre + y]) { ++x; ++y; --workBudget(); }
            if (--workBudget() < 0) { keepA.assign(n, 0); keepB.assign(m, 0); return false; }
            V[off + k] = x;
            if (x >= N && y >= M) { foundD = d; break; }
        }
        trace.emplace_back(V.begin() + off - d, V.begin() + off + d + 1);
    }
    if (foundD < 0) { keepA.assign(n, 0); keepB.assign(m, 0); return false; }
    int x = N, y = M;
    for (int d = foundD; d > 0; --d) {
        const std::vector<int>& P = trace[d - 1];
        const int pd = d - 1;
        int k = x - y, prevk;
        if (k == -d || (k != d && P[k - 1 + pd] < P[k + 1 + pd])) prevk = k + 1; else prevk = k - 1;
        int prevx = P[prevk + pd], prevy = prevx - prevk;
        while (x > prevx && y > prevy && x > 0 && y > 0 && (x - 1 >= prevx) && (y - 1 >= prevy)
               && a[pre + x - 1] == b[pre + y - 1] && !(x - 1 == prevx && y - 1 == prevy && false)) {
            // diagonal moves come after the single edit, so stop when we reach the edit point
            if (x - 1 < prevx || y - 1 < prevy) break;
            keepA[pre + x - 1] = keepB[pre + y - 1] = 1; --x; --y;
            if (x == prevx + (k > prevk ? 1 : 0) && y == prevy + (k > prevk ? 0 : 1)) break;
        }
        x = prevx; y = prevy;
    }
    while (x > 0 && y > 0) { keepA[pre + x - 1] = keepB[pre + y - 1] = 1; --x; --y; }
    return true;
}

struct LineInfo { size_t start, len; }; // len excludes the line break

inline std::vector<LineInfo> splitLines(const std::string& t) {
    std::vector<LineInfo> L; size_t s = 0, i = 0;
    while (i < t.size()) {
        if (t[i] == '\n') { L.push_back({s, i - s}); s = ++i; }
        else if (t[i] == '\r') { L.push_back({s, i - s}); ++i; if (i < t.size() && t[i] == '\n') ++i; s = i; }
        else ++i;
    }
    L.push_back({s, t.size() - s});
    return L;
}

struct Interner {
    std::unordered_map<std::string, uint32_t> m;
    uint32_t id(const std::string& s) { auto it = m.find(s); if (it != m.end()) return it->second;
        uint32_t v = (uint32_t)m.size(); m.emplace(s, v); return v; }
};

inline void tokenize(const std::string& t, const LineInfo& li, std::vector<LineInfo>& out) {
    size_t i = li.start, e = li.start + li.len;
    auto isw = [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80; };
    auto iss = [](unsigned char c) { return c == ' ' || c == '\t'; };
    while (i < e) {
        size_t s = i; unsigned char c = (unsigned char)t[i];
        if (isw(c)) { while (i < e && isw((unsigned char)t[i])) ++i; }
        else if (iss(c)) { while (i < e && iss((unsigned char)t[i])) ++i; }
        else ++i;
        out.push_back({s, i - s});
    }
}

inline void addMerged(std::vector<Range>& v, size_t pos, size_t len) {
    if (!v.empty() && v.back().pos + v.back().len == pos) v.back().len += len; else v.push_back({pos, len});
}

inline CompareResult compare(const std::string& A, const std::string& B, bool ignoreSpace = false) {
    CompareResult r;
    workBudget() = 400000000LL;
    auto LA = splitLines(A), LB = splitLines(B);
    Interner in;
    auto norm = [&](const std::string& t, const LineInfo& li) {
        std::string s = t.substr(li.start, li.len);
        if (ignoreSpace) { std::string o; for (char c : s) if (c != ' ' && c != '\t') o += c; return o; }
        return s; };
    std::vector<uint32_t> a, b;
    for (auto& l : LA) a.push_back(in.id(norm(A, l)));
    for (auto& l : LB) b.push_back(in.id(norm(B, l)));
    std::vector<char> ka, kb;
    bool ok = myers(a, b, ka, kb, 4000);
    if (!ok) {
        r.tooDifferent = true;
        for (size_t i = 0; i < LA.size(); ++i) r.changedA.push_back((int)i);
        for (size_t j = 0; j < LB.size(); ++j) r.changedB.push_back((int)j);
        return r;
    }
    size_t i = 0, j = 0; bool any = false;
    while (i < ka.size() || j < kb.size()) {
        if (i < ka.size() && j < kb.size() && ka[i] && kb[j]) { r.rows.push_back({' ', (int)i, (int)j}); ++i; ++j; continue; }
        size_t i0 = i, j0 = j;
        while (i < ka.size() && !ka[i]) ++i;
        while (j < kb.size() && !kb[j]) ++j;
        size_t p = i - i0, q = j - j0, pair = std::min(p, q);
        any = true;
        for (size_t t = 0; t < pair; ++t) {
            r.changedA.push_back((int)(i0 + t)); r.changedB.push_back((int)(j0 + t));
            std::vector<LineInfo> ta, tb;
            tokenize(A, LA[i0 + t], ta); tokenize(B, LB[j0 + t], tb);
            Interner ti; std::vector<uint32_t> xa, xb;
            for (auto& x : ta) xa.push_back(ti.id(A.substr(x.start, x.len)));
            for (auto& x : tb) xb.push_back(ti.id(B.substr(x.start, x.len)));
            std::vector<char> wa, wb;
            if (!myers(xa, xb, wa, wb, 2000)) { wa.assign(xa.size(), 0); wb.assign(xb.size(), 0); }
            for (size_t u = 0; u < ta.size(); ++u) if (!wa[u]) addMerged(r.inlineA, ta[u].start, ta[u].len);
            for (size_t u = 0; u < tb.size(); ++u) if (!wb[u]) addMerged(r.inlineB, tb[u].start, tb[u].len);
        }
        for (size_t t = 0; t < pair; ++t) r.rows.push_back({'c', (int)(i0 + t), (int)(j0 + t)});
        for (size_t t = pair; t < p; ++t) { r.removedLines.push_back((int)(i0 + t)); r.rows.push_back({'-', (int)(i0 + t), -1}); }
        for (size_t t = pair; t < q; ++t) { r.addedLines.push_back((int)(j0 + t)); r.rows.push_back({'+', -1, (int)(j0 + t)}); }
    }
    r.identical = !any;
    return r;
}


// ---------------------------------------------------------------------------
// Unified diff (git/patch compatible) between two texts.
// Exact comparison: line endings are part of each line, so applying the patch to A
// reproduces B byte for byte. Returns status: 0 ok, 1 identical, 2 lone CR (old Mac line
// ends, not representable), 3 too different to diff.
struct PatchLine { std::string text; bool hasEol; };

inline std::vector<std::string> splitExact(const std::string& t, bool& loneCR) {
    std::vector<std::string> out; size_t s = 0, i = 0;
    while (i < t.size()) {
        if (t[i] == '\r') { if (i + 1 < t.size() && t[i + 1] == '\n') { i += 2; out.push_back(t.substr(s, i - s)); s = i; } else { loneCR = true; ++i; } }
        else if (t[i] == '\n') { ++i; out.push_back(t.substr(s, i - s)); s = i; }
        else ++i;
    }
    if (s < t.size()) out.push_back(t.substr(s));
    return out;
}

inline int unifiedDiff(const std::string& A, const std::string& B, const std::string& nameA,
                       const std::string& nameB, std::string& out, int ctx = 3) {
    out.clear();
    bool lone = false;
    auto LA = splitExact(A, lone), LB = splitExact(B, lone);
    if (lone) return 2;
    workBudget() = 400000000LL;
    Interner in; std::vector<uint32_t> a, b;
    for (auto& l : LA) a.push_back(in.id(l));
    for (auto& l : LB) b.push_back(in.id(l));
    std::vector<char> ka, kb;
    if (!myers(a, b, ka, kb, 20000)) return 3;
    struct Op { char c; size_t ia, ib; };
    std::vector<Op> ops; size_t i = 0, j = 0;
    while (i < ka.size() || j < kb.size()) {
        if (i < ka.size() && j < kb.size() && ka[i] && kb[j]) { ops.push_back({' ', i, j}); ++i; ++j; continue; }
        while (i < ka.size() && !ka[i]) { ops.push_back({'-', i, j}); ++i; }
        while (j < kb.size() && !kb[j]) { ops.push_back({'+', i, j}); ++j; }
    }
    std::vector<size_t> ch;
    for (size_t k = 0; k < ops.size(); ++k) if (ops[k].c != ' ') ch.push_back(k);
    if (ch.empty()) return 1;
    auto nm = [](const std::string& pre, const std::string& n) {
        return pre + n + (n.find(' ') != std::string::npos ? "\t" : "") + "\n"; };
    out += nm("--- a/", nameA); out += nm("+++ b/", nameB);
    size_t hs = 0, he = 0; bool open = false;
    std::vector<std::pair<size_t, size_t>> hunks;
    for (size_t k : ch) {
        size_t s = k > (size_t)ctx ? k - ctx : 0, e = std::min(ops.size(), k + ctx + 1);
        if (!open) { hs = s; he = e; open = true; }
        else if (s <= he) he = e;
        else { hunks.push_back({hs, he}); hs = s; he = e; }
    }
    hunks.push_back({hs, he});
    for (auto& h : hunks) {
        size_t oc = 0, nc = 0;
        for (size_t k = h.first; k < h.second; ++k) { if (ops[k].c != '+') ++oc; if (ops[k].c != '-') ++nc; }
        size_t aBefore = ops[h.first].ia, bBefore = ops[h.first].ib;
        size_t os = oc ? aBefore + 1 : aBefore, ns = nc ? bBefore + 1 : bBefore;
        out += "@@ -" + std::to_string(os) + (oc == 1 ? "" : "," + std::to_string(oc)) +
               " +" + std::to_string(ns) + (nc == 1 ? "" : "," + std::to_string(nc)) + " @@\n";
        for (size_t k = h.first; k < h.second; ++k) {
            const std::string& t = ops[k].c == '+' ? LB[ops[k].ib] : LA[ops[k].ia];
            out += ops[k].c; out += t;
            if (t.empty() || t.back() != '\n') out += "\n\\ No newline at end of file\n";
        }
    }
    return 0;
}


// ---------------------------------------------------------------------------
// Self-contained HTML report. No scripts, no external resources, all text escaped.
inline void appendEscaped(std::string& o, const std::string& t, size_t from, size_t len) {
    // Valid UTF-8 passes through; stray high bytes (legacy code pages) map to Latin-1 code points.
    size_t i = from, e = from + len;
    while (i < e) {
        unsigned char c = (unsigned char)t[i];
        int n = c < 0x80 ? 1 : (c >= 0xC2 && c <= 0xDF) ? 2 : (c >= 0xE0 && c <= 0xEF) ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
        bool ok = n > 0 && i + n <= e;
        for (int k = 1; ok && k < n; ++k) ok = ((unsigned char)t[i + k] & 0xC0) == 0x80;
        if (n == 1) {
            switch (c) {
                case '&': o += "&amp;"; break; case '<': o += "&lt;"; break; case '>': o += "&gt;"; break;
                case '"': o += "&quot;"; break; case '\'': o += "&#39;"; break;
                case '\t': o += "&#9;"; break;
                default: if (c < 0x20 || c == 0x7F) { o += "&#xFFFD;"; } else o += (char)c;
            }
            ++i;
        } else if (ok) { o.append(t, i, n); i += n; }
        else { char b[16]; snprintf(b, sizeof b, "&#%d;", c); o += b; ++i; }
    }
}

inline std::string htmlReport(const std::string& A, const std::string& B, const std::string& nameA,
                              const std::string& nameB, const CompareResult& r) {
    auto LA = splitLines(A), LB = splitLines(B);
    std::string o;
    o += "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
         "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'\">"
         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Text comparison</title><style>"
         "body{font:15px/1.5 system-ui,sans-serif;margin:24px;color:#1d1b2e;background:#fff}"
         "h1{font-size:20px;margin:0 0 4px}.meta{color:#645f7e;margin-bottom:16px}"
         "table{border-collapse:collapse;width:100%;font:13.5px/1.45 ui-monospace,Consolas,monospace}"
         "td{padding:1px 8px;vertical-align:top;white-space:pre-wrap;word-break:break-word}"
         "td.n{color:#8a86a3;text-align:right;width:1%;white-space:nowrap;user-select:none}"
         "td.s{width:1%;user-select:none}"
         "tr.add td{background:#dcf7ec}tr.del td{background:#ffe4e2}tr.chg td{background:#fff5d6}"
         "mark{background:#ffb347;color:inherit;border-radius:3px}"
         ".k span{display:inline-block;padding:1px 8px;border-radius:6px;margin-right:6px}"
         "@media(prefers-color-scheme:dark){body{background:#0e0c1b;color:#ece9fb}tr.add td{background:#10301f}tr.del td{background:#3a1a20}tr.chg td{background:#3d3513}mark{background:#a8661c;color:#fff}}"
         "</style></head><body><h1>Text comparison</h1><div class=\"meta\">First: <b>";
    appendEscaped(o, nameA, 0, nameA.size()); o += "</b> &nbsp; Second: <b>"; appendEscaped(o, nameB, 0, nameB.size());
    o += "</b><br>Created by TextCompareLab for Notepad++. This file contains the full text of both documents.</div>"
         "<div class=\"k\"><span style=\"background:#ffe4e2\">only in first</span><span style=\"background:#dcf7ec\">only in second</span>"
         "<span style=\"background:#fff5d6\">changed</span><mark>changed words</mark></div><br><table>";
    if (r.identical) o += "<tr><td>The two texts are identical.</td></tr>";
    size_t pa = 0, pb = 0;
    auto cell = [&](const std::string& T, const LineInfo& L, const std::vector<Range>& rg, size_t& p) {
        std::string c; size_t pos = L.start, e = L.start + L.len;
        while (p < rg.size() && rg[p].pos + rg[p].len <= L.start) ++p;
        size_t q = p;
        while (q < rg.size() && rg[q].pos < e) {
            size_t rs = std::max(rg[q].pos, L.start), re = std::min(rg[q].pos + rg[q].len, e);
            if (rs > pos) appendEscaped(c, T, pos, rs - pos);
            if (re > rs) { c += "<mark>"; appendEscaped(c, T, rs, re - rs); c += "</mark>"; }
            pos = std::max(pos, re); ++q;
        }
        if (e > pos) appendEscaped(c, T, pos, e - pos);
        p = q; return c;
    };
    for (auto& w : r.rows) {
        std::string cls = w.c == ' ' ? "" : w.c == '+' ? "add" : w.c == '-' ? "del" : "chg";
        auto row = [&](const std::string& klass, int na, int nb, const char* sign, const std::string& txt) {
            o += "<tr class=\"" + klass + "\"><td class=\"n\">" + (na >= 0 ? std::to_string(na + 1) : "") + "</td><td class=\"n\">" +
                 (nb >= 0 ? std::to_string(nb + 1) : "") + "</td><td class=\"s\">" + sign + "</td><td dir=\"auto\">" + txt + "</td></tr>\n"; };
        if (w.c == ' ') { std::string c; appendEscaped(c, A, LA[w.ia].start, LA[w.ia].len); row("", w.ia, w.ib, " ", c); }
        else if (w.c == '-') { std::string c; appendEscaped(c, A, LA[w.ia].start, LA[w.ia].len); row("del", w.ia, -1, "-", c); }
        else if (w.c == '+') { std::string c; appendEscaped(c, B, LB[w.ib].start, LB[w.ib].len); row("add", -1, w.ib, "+", c); }
        else { row("chg", w.ia, -1, "-", cell(A, LA[w.ia], r.inlineA, pa)); row("chg", -1, w.ib, "+", cell(B, LB[w.ib], r.inlineB, pb)); }
    }
    o += "</table></body></html>\n";
    return o;
}

} // namespace tcl
