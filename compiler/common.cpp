#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

Sources g_sources;

int Sources::add(const std::string& path, const std::string& text) {
    files.push_back({path, text});
    return (int)files.size() - 1;
}

std::string Sources::lineText(int file, int line) const {
    if (file < 0 || file >= (int)files.size()) return "";
    const std::string& t = files[file].text;
    size_t pos = 0;
    for (int l = 1; l < line && pos != std::string::npos; l++) {
        pos = t.find('\n', pos);
        if (pos != std::string::npos) pos++;
    }
    if (pos == std::string::npos || pos > t.size()) return "";
    size_t end = t.find('\n', pos);
    std::string s = t.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

void printError(const CompileError& e) {
    bool color = isatty(2);
    const char* red = color ? "\033[1;31m" : "";
    const char* blue = color ? "\033[1;34m" : "";
    const char* bold = color ? "\033[1m" : "";
    const char* cyan = color ? "\033[36m" : "";
    const char* reset = color ? "\033[0m" : "";

    fprintf(stderr, "%serror%s%s: %s%s\n", red, reset, bold, e.msg.c_str(), reset);
    std::string pad = " ";
    if (e.loc.line > 0 && e.loc.file < (int)g_sources.files.size()) {
        std::string num = std::to_string(e.loc.line);
        pad = std::string(num.size(), ' ');
        std::string raw = g_sources.lineText(e.loc.file, e.loc.line);
        // Expand tabs so the caret lines up.
        std::string text;
        int caretCol = 0;
        for (size_t i = 0; i < raw.size(); i++) {
            if ((int)i == e.loc.col - 1) caretCol = (int)text.size();
            if (raw[i] == '\t') text += "    ";
            else text += raw[i];
        }
        if (e.loc.col - 1 >= (int)raw.size()) caretCol = (int)text.size();
        fprintf(stderr, "%s%s-->%s %s:%d:%d\n", pad.c_str(), blue, reset,
                g_sources.files[e.loc.file].path.c_str(), e.loc.line, e.loc.col);
        fprintf(stderr, "%s %s|%s\n", pad.c_str(), blue, reset);
        fprintf(stderr, "%s%s |%s %s\n", blue, num.c_str(), reset, text.c_str());
        fprintf(stderr, "%s %s|%s %s%s%s%s\n", pad.c_str(), blue, reset, std::string(caretCol, ' ').c_str(), red,
                std::string(std::max(1, e.width), '^').c_str(), reset);
    }
    if (!e.hint.empty())
        fprintf(stderr, "%s %s=%s %shint:%s %s\n", pad.c_str(), blue, reset, cyan, reset, e.hint.c_str());
}

static size_t editDistance(const std::string& a, const std::string& b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); j++) {
            size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

std::string closestName(const std::string& name, const std::vector<std::string>& candidates) {
    std::string best;
    size_t bestD = (size_t)-1;
    size_t limit = std::max<size_t>(1, name.size() / 3);
    for (const auto& c : candidates) {
        if (c == name) continue;
        size_t d = editDistance(name, c);
        if (d <= limit && d < bestD) {
            bestD = d;
            best = c;
        }
    }
    return best;
}

std::string formatNumber(double n) {
    char buf[64];
    if (n == 0) return "0";
    if (std::isnan(n)) return "nan";
    if (std::isinf(n)) return n > 0 ? "inf" : "-inf";
    if (std::fabs(n) <= 9007199254740992.0 && n == std::floor(n)) {
        snprintf(buf, sizeof buf, "%lld", (long long)n);
        return buf;
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, n);
        if (strtod(buf, nullptr) == n) break;
    }
    return buf;
}
