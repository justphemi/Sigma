#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common.h"

// Node kinds. Child layout for each kind:
//
// Expressions
//   Num      num
//   Str      s (decoded text)
//   Interp   kids: parts (Str nodes or expressions)
//   Bool     flag
//   Nil
//   List     kids: items
//   Map      names: keys, kids: values
//   Range    kids[0]..kids[1], flag = inclusive (..=)
//   Ident    s
//   Unary    s = "-" | "not", kids[0]
//   Binary   s = operator, kids[0], kids[1]
//   And, Or  kids[0], kids[1]
//   Call     kids[0] = callee, kids[1..] = args
//   MethodCall kids[0] = object, s = method, kids[1..] = args
//   Field    kids[0] = object, s = field name
//   Index    kids[0] = object, kids[1] = index
//   Lambda   names = params, kids[0] = Block body
//
// Statements
//   Assign   s = "" (plain) or the compound operator ("+", "-", ...), kids[0] = target, kids[1] = value
//   Lock     s = name, kids[0] = value
//   ExprStmt kids[0]
//   If       kids = cond, block, cond, block, ..., [else block]
//   While    kids = [cond], block   (no cond = loop forever)
//   ForIn    names = 1 or 2 loop variables, kids[0] = iterable, kids[1] = block
//   Dip, Skip
//   Serve    kids = [value]
//   Yeet     kids[0]
//   Try      kids[0] = body, kids[1] = handler (optional), s = error variable (optional)
//   Func     s = name, names = params, kids[0] = body
//   Yoink    s = path
//   Block    kids = statements
enum class NK {
    Num, Str, Interp, Bool, Nil, List, Map, Range, Ident, Unary, Binary, And, Or,
    Call, MethodCall, Field, Index, Lambda,
    Assign, Lock, ExprStmt, If, While, ForIn, Dip, Skip, Serve, Yeet, Try, Func, Yoink, Block,
};

struct Sym;
struct Func;
struct Node;
using NodeP = std::unique_ptr<Node>;

struct Node {
    NK k;
    Loc loc;
    std::string s;
    double num = 0;
    bool flag = false;
    std::vector<NodeP> kids;
    std::vector<std::string> names;
    std::vector<Loc> nameLocs;

    // Filled in by semantic analysis.
    Sym* sym = nullptr;
    Sym* sym2 = nullptr;
    Func* fn = nullptr;
    int builtin = -1;

    Node(NK kind, Loc l) : k(kind), loc(l) {}
};

inline NodeP mk(NK k, Loc l) { return std::make_unique<Node>(k, l); }
