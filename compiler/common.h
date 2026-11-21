// Shared compiler infrastructure: source locations, source files and
// friendly compile errors.
#pragma once

#include <exception>
#include <string>
#include <vector>

#define SIGMA_VERSION "0.2.0"

struct Loc {
    int file = 0;
    int line = 0;
    int col = 0;
};

struct SourceFile {
    std::string path;
    std::string text;
};

// Every file the compiler has read (the main program plus anything yoinked).
struct Sources {
    std::vector<SourceFile> files;
    int add(const std::string& path, const std::string& text);
    std::string lineText(int file, int line) const;
};
extern Sources g_sources;

// A compile error with a source location and an optional hint, printed like:
//
//   error: 'totl' is not defined
//    --> sum.sgm:3:5
//     |
//   3 |     totl += i
//     |     ^^^^
//     = hint: did you mean 'total'?
struct CompileError : std::exception {
    Loc loc;
    std::string msg;
    std::string hint;
    int width;
    CompileError(Loc l, std::string m, std::string h, int w)
        : loc(l), msg(std::move(m)), hint(std::move(h)), width(w) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

[[noreturn]] inline void fail(Loc l, const std::string& msg, const std::string& hint = "", int width = 1) {
    throw CompileError(l, msg, hint, width);
}

void printError(const CompileError& e);

// Closest candidate by edit distance, or "" if nothing is close enough.
std::string closestName(const std::string& name, const std::vector<std::string>& candidates);

// Runtime line info passed into generated code: (file << 20) | line.
inline int encodeLoc(Loc l) { return (l.file << 20) | (l.line & 0xFFFFF); }

// Formats a number exactly the way the runtime prints it (so 1.50 -> "1.5").
std::string formatNumber(double d);
