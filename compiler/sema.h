// Semantic analysis: name resolution, error checking and type inference.
//
// Scoping (Python-like, but friendlier):
//   * A variable belongs to the function it's assigned in (blocks don't
//     create scopes, so a value set inside `fr` is visible after it).
//   * Top-level variables are globals. Functions can read AND update them.
//   * Loop variables, `oops` variables and parameters are always local.
//   * Lambdas capture outer function variables by value (read-only).
//
// Type inference: every variable, parameter and function return gets one of
// NUM, BOOL or DYN. NUM/BOOL values compile to raw C doubles/bools; only DYN
// values pay for the 16-byte tagged SigValue. Inference is an optimistic
// fixpoint: everything starts at BOT ("no evidence yet") and is widened until
// nothing changes.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ast.h"

enum Ty { BOT = 0, NUM, BOOL, DYN };

enum class SymKind { Local, Param, Capture };

struct Func;

struct Sym {
    std::string name;
    SymKind kind = SymKind::Local;
    Ty ty = BOT;
    bool locked = false;
    bool lockSeen = false;
    bool isVolatile = false;  // assigned inside yolo: must survive longjmp
    bool usedInFn = false;    // top-level variable used from a function => C global
    Func* owner = nullptr;
    Sym* outer = nullptr;     // for captures: the captured variable
    Loc loc;
    std::string cname;
};

struct Func {
    std::string name;
    Node* node = nullptr;
    bool isMain = false;
    bool isLambda = false;
    bool escapes = false;     // used as a value, so it can be called dynamically
    Func* parent = nullptr;   // lambdas: the enclosing function
    std::vector<Sym*> params;
    std::map<std::string, Sym*> scope;
    std::vector<Sym*> locals;
    std::vector<Sym*> captures;
    Ty ret = BOT;
    bool fallsThrough = true;
    int id = 0;
    std::string cname;
};

struct Program {
    std::vector<NodeP> stmts;
    Func* main = nullptr;
    std::vector<std::unique_ptr<Func>> funcs;  // owns everything (main, cooks, lambdas)
    std::vector<Func*> cooks;                  // top-level functions, in source order
    std::map<std::string, Func*> named;
    std::vector<std::unique_ptr<Sym>> syms;
};

struct BuiltinInfo {
    const char* name;
    int minArgs;
    int maxArgs;  // -1 = any number
    Ty ret;
};

struct MethodInfo {
    const char* name;
    int minArgs;
    int maxArgs;
};

const BuiltinInfo* findBuiltin(const std::string& name);
const BuiltinInfo* builtinAt(int index);
int builtinIndex(const std::string& name);
const MethodInfo* findMethod(const std::string& name);

void analyze(Program& prog);

// The static type of an expression (valid after analyze()).
Ty exprTy(const Node* e);
