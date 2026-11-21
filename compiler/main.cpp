// `sig` - the Sigma command line tool.
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "codegen.h"
#include "common.h"
#include "parser.h"
#include "runtime_embed.h"  // generated at build time: SIGMA_RUNTIME
#include "sema.h"

namespace {

const std::vector<std::string> kCFlags = {"-O2", "-std=gnu11", "-fno-math-errno", "-w"};

void usage(FILE* out) {
    fprintf(out,
            "Sigma %s - a fast, compiled language with a clean syntax\n"
            "\n"
            "usage:\n"
            "  sig <file.sgm> [args...]        compile (cached) and run\n"
            "  sig run <file.sgm> [args...]    same as above\n"
            "  sig build <file.sgm> [-o name]  make a standalone native executable\n"
            "  sig check <file.sgm>            check for errors without running\n"
            "  sig emit <file.sgm>             print the generated C code\n"
            "  sig clean                       delete the compile cache\n"
            "  sig version                     print the version\n"
            "\n"
            "environment:\n"
            "  SIGMA_CC     C compiler to use (default: $CC, then cc)\n"
            "  SIGMA_CACHE  cache directory (default: ~/.cache/sigma)\n",
            SIGMA_VERSION);
}

[[noreturn]] void die(const std::string& msg) {
    fprintf(stderr, "%serror:%s %s\n", isatty(2) ? "\033[1;31m" : "", isatty(2) ? "\033[0m" : "", msg.c_str());
    exit(1);
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    f << data;
    return (bool)f;
}

std::string dirOf(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

// Reads a file, parses it and splices in everything it yoinks (each file once).
void load(const std::string& path, Loc from, std::vector<NodeP>& out, std::set<std::string>& seen) {
    char real[PATH_MAX];
    std::string key = realpath(path.c_str(), real) ? std::string(real) : path;
    if (!seen.insert(key).second) return;
    std::string text;
    if (!readFile(path, text)) {
        std::string why = strerror(errno);
        if (from.line > 0) fail(from, "can't yoink '" + path + "': " + why, "paths are relative to the file doing the yoink");
        die("can't open '" + path + "': " + why);
    }
    int fid = g_sources.add(path, text);
    for (auto& s : parseFile(fid, text)) {
        if (s->k == NK::Yoink) {
            std::string target = s->s;
            if (target.empty() || target[0] != '/') target = dirOf(path) + "/" + target;
            load(target, s->loc, out, seen);
        } else {
            out.push_back(std::move(s));
        }
    }
}

std::string compileToC(const std::string& path) {
    Program prog;
    std::set<std::string> seen;
    try {
        load(path, Loc{}, prog.stmts, seen);
        analyze(prog);
        return generateC(prog, SIGMA_RUNTIME);
    } catch (const CompileError& e) {
        printError(e);
        exit(1);
    }
}

std::string ccName() {
    if (const char* c = getenv("SIGMA_CC"); c && *c) return c;
    if (const char* c = getenv("CC"); c && *c) return c;
    return "cc";
}

// Runs a command (no shell). Returns its exit status, or -1 if it couldn't start.
int runCommand(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return -1;
    if (!WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

void compileC(const std::string& cfile, const std::string& out) {
    std::string cc = ccName();
    std::vector<std::string> args{cc};
    args.insert(args.end(), kCFlags.begin(), kCFlags.end());
    args.insert(args.end(), {"-o", out, cfile, "-lm"});
    int rc = runCommand(args);
    if (rc == 127 || rc == -1)
        die("couldn't run the C compiler '" + cc + "'. Sigma needs clang or gcc installed"
#ifdef __APPLE__
            " (run: xcode-select --install)"
#endif
            ".");
    if (rc != 0)
        die("internal compiler error: the C compiler rejected the generated code.\n"
            "       This is a bug in Sigma - please report it with your .sgm file.");
}

void mkdirs(const std::string& path) {
    std::string cur;
    std::stringstream ss(path);
    std::string part;
    if (!path.empty() && path[0] == '/') cur = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        cur += part + "/";
        mkdir(cur.c_str(), 0755);
    }
}

std::string cacheDir() {
    if (const char* d = getenv("SIGMA_CACHE"); d && *d) return d;
    if (const char* x = getenv("XDG_CACHE_HOME"); x && *x) return std::string(x) + "/sigma";
    if (const char* h = getenv("HOME"); h && *h) return std::string(h) + "/.cache/sigma";
    const char* t = getenv("TMPDIR");
    return std::string(t && *t ? t : "/tmp") + "/sigma-cache";
}

std::string hashHex(const std::string& data) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char buf[17];
    snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
    return buf;
}

int cmdRun(const std::string& path, int argc, char** argv, int firstArg) {
    std::string csrc = compileToC(path);
    std::string key = csrc + "\n" + ccName();
    for (const auto& f : kCFlags) key += " " + f;
    std::string dir = cacheDir();
    mkdirs(dir);
    std::string bin = dir + "/" + hashHex(key);
    if (access(bin.c_str(), X_OK) != 0) {
        std::string tag = std::to_string(getpid());
        std::string cfile = bin + "." + tag + ".c", tmpBin = bin + "." + tag + ".tmp";
        if (!writeFile(cfile, csrc)) die("can't write to the cache directory " + dir + " (set SIGMA_CACHE to change it)");
        compileC(cfile, tmpBin);
        unlink(cfile.c_str());
        if (rename(tmpBin.c_str(), bin.c_str()) != 0) die("can't write to the cache directory " + dir);
    }
    std::vector<char*> args{const_cast<char*>(path.c_str())};
    for (int i = firstArg; i < argc; i++) args.push_back(argv[i]);
    args.push_back(nullptr);
    fflush(stdout);
    execv(bin.c_str(), args.data());
    die("couldn't start the compiled program: " + std::string(strerror(errno)));
}

int cmdBuild(const std::string& path, std::string out) {
    if (out.empty()) {
        size_t slash = path.find_last_of('/');
        out = slash == std::string::npos ? path : path.substr(slash + 1);
        if (out.size() > 4 && out.compare(out.size() - 4, 4, ".sgm") == 0) out.resize(out.size() - 4);
        if (out == path) out += ".out";
    }
    std::string csrc = compileToC(path);
    char tmpl[] = "/tmp/sigma-build-XXXXXX";
    char* dir = mkdtemp(tmpl);
    if (!dir) die("can't create a temporary directory");
    std::string cfile = std::string(dir) + "/program.c";
    if (!writeFile(cfile, csrc)) die("can't write " + cfile);
    compileC(cfile, out);
    unlink(cfile.c_str());
    rmdir(dir);
    printf("built %s\n", out.c_str());
    return 0;
}

int cmdClean() {
    std::string dir = cacheDir();
    int rc = runCommand({"rm", "-rf", dir});
    if (rc != 0) die("couldn't remove " + dir);
    printf("removed %s\n", dir.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(stderr);
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        printf("sigma %s\n", SIGMA_VERSION);
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        usage(stdout);
        return 0;
    }
    if (cmd == "clean") return cmdClean();

    bool known = cmd == "run" || cmd == "build" || cmd == "check" || cmd == "emit";
    if (!known) {
        struct stat st;
        if (stat(cmd.c_str(), &st) != 0) {
            if (cmd.find('.') == std::string::npos && cmd.find('/') == std::string::npos)
                die("unknown command '" + cmd + "' (try: sig help)");
            die("can't open '" + cmd + "': " + strerror(errno));
        }
        return cmdRun(cmd, argc, argv, 2);
    }
    if (argc < 3) die("'sig " + cmd + "' needs a file, like:  sig " + cmd + " hello.sgm");
    std::string path = argv[2];
    if (cmd == "run") return cmdRun(path, argc, argv, 3);
    if (cmd == "check") {
        compileToC(path);
        printf("%s: no errors\n", path.c_str());
        return 0;
    }
    if (cmd == "emit") {
        fputs(compileToC(path).c_str(), stdout);
        return 0;
    }
    std::string out;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else die(std::string("unexpected argument '") + argv[i] + "' (usage: sig build <file.sgm> [-o name])");
    }
    return cmdBuild(path, out);
}
