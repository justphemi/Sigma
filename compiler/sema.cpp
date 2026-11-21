#include "sema.h"

#include <cstring>

namespace {

const BuiltinInfo kBuiltins[] = {
    {"yap", 0, -1, DYN},        {"ask", 0, 1, DYN},         {"len", 1, 1, NUM},
    {"type", 1, 1, DYN},        {"num", 1, 1, DYN},         {"int", 1, 1, DYN},
    {"str", 1, 1, DYN},         {"abs", 1, 1, NUM},         {"floor", 1, 1, NUM},
    {"ceil", 1, 1, NUM},        {"round", 1, 1, NUM},       {"sqrt", 1, 1, NUM},
    {"pow", 2, 2, NUM},         {"min", 1, -1, DYN},        {"max", 1, -1, DYN},
    {"rand", 0, 0, NUM},        {"rand_int", 2, 2, NUM},    {"now", 0, 0, NUM},
    {"sleep", 1, 1, DYN},       {"args", 0, 0, DYN},        {"exit", 0, 1, DYN},
    {"read_file", 1, 1, DYN},   {"write_file", 2, 2, DYN},  {"append_file", 2, 2, DYN},
    {"file_exists", 1, 1, BOOL}, {"vibecheck", 1, 2, DYN},
};

const MethodInfo kMethods[] = {
    {"push", 1, 1},    {"pop", 0, 0},     {"insert", 2, 2},  {"del", 1, 1},     {"has", 1, 1},
    {"find", 1, 1},    {"join", 0, 1},    {"reverse", 0, 0}, {"sort", 0, 2},    {"map", 1, 1},
    {"filter", 1, 1},  {"each", 1, 1},    {"reduce", 2, 2},  {"slice", 1, 2},   {"sum", 0, 0},
    {"copy", 0, 0},    {"clear", 0, 0},   {"upper", 0, 0},   {"lower", 0, 0},   {"trim", 0, 0},
    {"split", 0, 1},   {"starts", 1, 1},  {"ends", 1, 1},    {"replace", 2, 2}, {"repeat", 1, 1},
    {"chars", 0, 0},   {"keys", 0, 0},    {"values", 0, 0},  {"get", 1, 2},
};

// Sigma 0.1 built-ins -> their new names.
const std::map<std::string, std::string> kRenamedBuiltins = {
    {"check_type", "type"}, {"to_int", "int"},       {"to_dec", "num"},
    {"to_str", "str"},      {"random", "rand_int"}, {"random_range", "rand_int"},
};

std::string argCount(int n) { return std::to_string(n) + (n == 1 ? " argument" : " arguments"); }

std::string arityText(int lo, int hi) {
    if (hi < 0) return "at least " + argCount(lo);
    if (lo == hi) return argCount(lo);
    return std::to_string(lo) + " to " + argCount(hi);
}

Ty join(Ty a, Ty b) {
    if (a == BOT) return b;
    if (b == BOT) return a;
    return a == b ? a : DYN;
}

// `a + b` is a number only if both sides are numbers; strings concatenate.
Ty plusTy(Ty a, Ty b) {
    bool an = a == NUM || a == BOT, bn = b == NUM || b == BOT;
    if (an && bn) return (a == BOT && b == BOT) ? BOT : NUM;
    return DYN;
}

struct Assigned {
    std::string name;
    Loc loc;
    enum Kind { Plain, Lock, Loop, Err } kind;
};

struct Ctx {
    int loops = 0;
    int tries = 0;
};

class Analyzer {
public:
    explicit Analyzer(Program& p) : P(p) {}

    void run() {
        Func* m = newFunc();
        m->isMain = true;
        m->name = "main";
        P.main = m;

        for (auto& s : P.stmts) {
            if (s->k != NK::Func) continue;
            if (P.named.count(s->s))
                fail(s->loc, "function '" + s->s + "' is cooked twice", "rename one of them", (int)s->s.size());
            Func* f = newFunc();
            f->name = s->s;
            f->node = s.get();
            f->cname = "f_" + s->s;
            s->fn = f;
            P.named[s->s] = f;
            P.cooks.push_back(f);
            for (size_t i = 0; i < s->names.size(); i++) {
                Sym* ps = newSym(s->names[i], SymKind::Param, f, s->nameLocs[i]);
                f->params.push_back(ps);
                f->scope[ps->name] = ps;
            }
        }

        std::vector<Assigned> top;
        for (auto& s : P.stmts) collect(s.get(), top);
        declare(m, top);
        Ctx c;
        for (auto& s : P.stmts)
            if (s->k != NK::Func) stmt(m, s.get(), c);

        for (Func* f : P.cooks) {
            Node* body = f->node->kids[0].get();
            std::vector<Assigned> as;
            collect(body, as);
            declare(f, as);
            Ctx fc;
            stmt(f, body, fc);
        }

        for (auto& f : P.funcs)
            if (!f->isMain) f->fallsThrough = !terminates(f->node->kids[0].get());

        infer();

        for (auto& sp : P.syms) {
            Sym* s = sp.get();
            if (s->ty == BOT) s->ty = DYN;
            if (s->kind == SymKind::Capture) s->cname = "c_" + s->name;
            else if (s->owner->isMain && s->usedInFn) s->cname = "g_" + s->name;
            else s->cname = "v_" + s->name;
        }
        for (auto& f : P.funcs)
            if (f->ret == BOT || f->isLambda) f->ret = DYN;
    }

private:
    Program& P;
    int lambdaCount_ = 0;
    bool changed_ = false;

    Sym* newSym(const std::string& name, SymKind k, Func* owner, Loc loc) {
        P.syms.push_back(std::make_unique<Sym>());
        Sym* s = P.syms.back().get();
        s->name = name;
        s->kind = k;
        s->owner = owner;
        s->loc = loc;
        return s;
    }

    Func* newFunc() {
        P.funcs.push_back(std::make_unique<Func>());
        return P.funcs.back().get();
    }

    // ------------------------------------------------------------ declarations

    // Every name a function body assigns to (not descending into lambdas,
    // which only appear inside expressions).
    void collect(Node* n, std::vector<Assigned>& out) {
        switch (n->k) {
        case NK::Assign:
            // Only plain `x: value` declares; `x += 1` needs x to exist already.
            if (n->kids[0]->k == NK::Ident && n->s.empty())
                out.push_back({n->kids[0]->s, n->kids[0]->loc, Assigned::Plain});
            break;
        case NK::Lock: out.push_back({n->s, n->loc, Assigned::Lock}); break;
        case NK::ForIn:
            for (size_t i = 0; i < n->names.size(); i++) out.push_back({n->names[i], n->nameLocs[i], Assigned::Loop});
            collect(n->kids[1].get(), out);
            break;
        case NK::Try:
            if (!n->s.empty()) out.push_back({n->s, n->nameLocs[0], Assigned::Err});
            for (auto& k : n->kids) collect(k.get(), out);
            break;
        case NK::If:
            for (auto& k : n->kids)
                if (k->k == NK::Block) collect(k.get(), out);
            break;
        case NK::While: collect(n->kids.back().get(), out); break;
        case NK::Block:
            for (auto& k : n->kids) collect(k.get(), out);
            break;
        default: break;
        }
    }

    // A variable visible from an enclosing scope (no captures created).
    Sym* findOuter(Func* f, const std::string& name) {
        if (f->isLambda) {
            Func* p = f->parent;
            auto it = p->scope.find(name);
            if (it != p->scope.end()) return it->second;
            if (p->isMain) return nullptr;
            return findOuter(p, name);
        }
        auto it = P.main->scope.find(name);
        return it != P.main->scope.end() ? it->second : nullptr;
    }

    void declare(Func* f, const std::vector<Assigned>& as) {
        for (const auto& a : as) {
            auto it = f->scope.find(a.name);
            if (it != f->scope.end()) {
                if (a.kind == Assigned::Lock) {
                    if (it->second->kind == SymKind::Param)
                        fail(a.loc, "can't lock parameter '" + a.name + "'", "", (int)a.name.size());
                    it->second->locked = true;
                }
                continue;
            }
            if (P.named.count(a.name))
                fail(a.loc, "'" + a.name + "' is already the name of a function", "pick a different name for this variable",
                     (int)a.name.size());
            if (!f->isMain && a.kind == Assigned::Plain) {
                if (Sym* outer = findOuter(f, a.name)) {
                    if (outer->owner->isMain) continue;  // updates the global
                    fail(a.loc, "can't change '" + a.name + "' inside a lambda",
                         "lambdas get a read-only copy of outer variables - serve a new value instead, "
                         "or keep the data in a list/map (those are shared)",
                         (int)a.name.size());
                }
            }
            Sym* s = newSym(a.name, SymKind::Local, f, a.loc);
            s->locked = a.kind == Assigned::Lock;
            f->scope[a.name] = s;
            f->locals.push_back(s);
        }
    }

    Sym* makeCapture(Func* f, Sym* outer) {
        Sym* c = newSym(outer->name, SymKind::Capture, f, outer->loc);
        c->outer = outer;
        f->scope[outer->name] = c;
        f->captures.push_back(c);
        return c;
    }

    // Name lookup for reads. Creates captures for lambdas as needed.
    Sym* lookup(Func* f, const std::string& name) {
        auto it = f->scope.find(name);
        if (it != f->scope.end()) return it->second;
        if (f->isMain) return nullptr;
        if (!f->isLambda) {
            auto g = P.main->scope.find(name);
            if (g == P.main->scope.end()) return nullptr;
            g->second->usedInFn = true;
            return g->second;
        }
        Sym* o = lookup(f->parent, name);
        if (!o) return nullptr;
        if (o->owner->isMain) {
            o->usedInFn = true;
            return o;
        }
        return makeCapture(f, o);
    }

    // Like lookup, but ignores top-level variables.
    Sym* lookupLocal(Func* f, const std::string& name) {
        if (f->isMain) return nullptr;
        auto it = f->scope.find(name);
        if (it != f->scope.end()) return it->second;
        if (!f->isLambda) return nullptr;
        Sym* o = lookupLocal(f->parent, name);
        return o ? makeCapture(f, o) : nullptr;
    }

    std::vector<std::string> visibleNames(Func* f) {
        std::vector<std::string> v;
        for (Func* g = f; g; g = g->parent)
            for (auto& kv : g->scope) v.push_back(kv.first);
        for (auto& kv : P.main->scope) v.push_back(kv.first);
        for (auto& kv : P.named) v.push_back(kv.first);
        for (const auto& b : kBuiltins) v.push_back(b.name);
        return v;
    }

    [[noreturn]] void notDefined(Func* f, const std::string& name, Loc loc) {
        std::string best = closestName(name, visibleNames(f));
        fail(loc, "'" + name + "' is not defined",
             best.empty() ? "give it a value first, like  " + name + ": 0" : "did you mean '" + best + "'?",
             (int)name.size());
    }

    [[noreturn]] void unknownFunction(Func* f, const std::string& name, Loc loc) {
        auto r = kRenamedBuiltins.find(name);
        if (r != kRenamedBuiltins.end())
            fail(loc, "'" + name + "' was renamed to '" + r->second + "' in Sigma 0.2", "", (int)name.size());
        std::string best = closestName(name, visibleNames(f));
        fail(loc, "there's no function called '" + name + "'",
             best.empty() ? "define it with  cook " + name + "(...) { ... }" : "did you mean '" + best + "'?",
             (int)name.size());
    }

    // ------------------------------------------------------------ resolution

    void markVolatile(Func* f, Sym* s, const Ctx& c) {
        if (c.tries > 0 && s->owner == f) s->isVolatile = true;
    }

    void stmt(Func* f, Node* n, Ctx& c) {
        switch (n->k) {
        case NK::Assign: {
            Node* t = n->kids[0].get();
            if (t->k == NK::Ident) {
                Sym* s = lookup(f, t->s);
                if (!s) notDefined(f, t->s, t->loc);
                if (s->locked)
                    fail(t->loc, "'" + t->s + "' is locked, so it can't be changed",
                         "it was declared with  lock " + t->s + ": ...", (int)t->s.size());
                t->sym = s;
                markVolatile(f, s, c);
            } else {
                for (auto& k : t->kids) expr(f, k.get(), c);
            }
            expr(f, n->kids[1].get(), c);
            break;
        }
        case NK::Lock: {
            Sym* s = f->scope.at(n->s);
            if (s->lockSeen) fail(n->loc, "'" + n->s + "' is already locked", "", (int)n->s.size());
            s->lockSeen = true;
            n->sym = s;
            markVolatile(f, s, c);
            expr(f, n->kids[0].get(), c);
            break;
        }
        case NK::ExprStmt:
        case NK::Yeet: expr(f, n->kids[0].get(), c); break;
        case NK::If:
            for (auto& k : n->kids) {
                if (k->k == NK::Block) stmt(f, k.get(), c);
                else expr(f, k.get(), c);
            }
            break;
        case NK::While:
            if (n->kids.size() == 2) expr(f, n->kids[0].get(), c);
            c.loops++;
            stmt(f, n->kids.back().get(), c);
            c.loops--;
            break;
        case NK::ForIn: {
            expr(f, n->kids[0].get(), c);
            if (n->kids[0]->k == NK::Range && n->names.size() == 2)
                fail(n->nameLocs[1], "a range loop gives one number at a time", "write  grind i in 0..10 { ... }",
                     (int)n->names[1].size());
            n->sym = f->scope.at(n->names[0]);
            if (n->names.size() == 2) n->sym2 = f->scope.at(n->names[1]);
            for (Sym* s : {n->sym, n->sym2}) {
                if (!s) continue;
                if (s->locked) fail(n->loc, "'" + s->name + "' is locked, so it can't be a loop variable");
                markVolatile(f, s, c);
            }
            c.loops++;
            stmt(f, n->kids[1].get(), c);
            c.loops--;
            break;
        }
        case NK::Dip:
        case NK::Skip:
            if (!c.loops)
                fail(n->loc, std::string("'") + (n->k == NK::Dip ? "dip" : "skip") + "' only works inside a grind loop",
                     "", 4);
            break;
        case NK::Serve:
            if (f->isMain) fail(n->loc, "'serve' only works inside a function", "to stop the program early, use exit()", 5);
            if (!n->kids.empty()) expr(f, n->kids[0].get(), c);
            break;
        case NK::Try:
            c.tries++;
            stmt(f, n->kids[0].get(), c);
            c.tries--;
            if (!n->s.empty()) {
                n->sym = f->scope.at(n->s);
                markVolatile(f, n->sym, c);
            }
            if (n->kids.size() > 1) stmt(f, n->kids[1].get(), c);
            break;
        case NK::Block:
            for (auto& k : n->kids) stmt(f, k.get(), c);
            break;
        case NK::Func:
        case NK::Yoink: break;
        default: fail(n->loc, "internal error: unexpected statement");
        }
    }

    void checkArgs(Node* call, const std::string& what, int lo, int hi, Loc loc, int w, const std::string& hint) {
        int argc = (int)call->kids.size() - (call->k == NK::Call ? 1 : 1);
        if (argc < lo || (hi >= 0 && argc > hi))
            fail(loc, what + " takes " + arityText(lo, hi) + ", but got " + std::to_string(argc), hint, w);
    }

    void expr(Func* f, Node* e, Ctx& c) {
        switch (e->k) {
        case NK::Ident: {
            if (Sym* s = lookup(f, e->s)) {
                e->sym = s;
                return;
            }
            auto it = P.named.find(e->s);
            if (it != P.named.end()) {
                e->fn = it->second;
                it->second->escapes = true;
                return;
            }
            if (findBuiltin(e->s))
                fail(e->loc, "'" + e->s + "' is a built-in function and can't be passed around as a value (yet)",
                     "wrap it in a lambda:  x => " + e->s + "(x)", (int)e->s.size());
            notDefined(f, e->s, e->loc);
        }
        case NK::Call: {
            Node* callee = e->kids[0].get();
            if (callee->k == NK::Ident) {
                const std::string& name = callee->s;
                int w = (int)name.size();
                // Order: local variables > cooked functions > built-ins > top-level variables.
                if (Sym* s = lookupLocal(f, name)) {
                    callee->sym = s;
                } else if (P.named.count(name)) {
                    Func* F = P.named[name];
                    e->fn = F;
                    std::string sig;
                    for (size_t i = 0; i < F->params.size(); i++) sig += (i ? ", " : "") + F->params[i]->name;
                    checkArgs(e, "'" + name + "'", (int)F->params.size(), (int)F->params.size(), callee->loc, w,
                              "it's defined as  cook " + name + "(" + sig + ")");
                } else if (int bi = builtinIndex(name); bi >= 0) {
                    e->builtin = bi;
                    e->s = name;
                    checkArgs(e, name + "()", kBuiltins[bi].minArgs, kBuiltins[bi].maxArgs, callee->loc, w, "");
                } else if (Sym* g = lookup(f, name)) {
                    callee->sym = g;
                } else {
                    unknownFunction(f, name, callee->loc);
                }
            } else {
                expr(f, callee, c);
            }
            for (size_t i = 1; i < e->kids.size(); i++) expr(f, e->kids[i].get(), c);
            return;
        }
        case NK::MethodCall: {
            Node* obj = e->kids[0].get();
            if (obj->k == NK::Ident && e->s == "run" && P.named.count(obj->s) && !lookup(f, obj->s))
                fail(e->loc, "'.run()' is old Sigma syntax", "call functions directly:  " + obj->s + "(...)", 3);
            expr(f, obj, c);
            if (const MethodInfo* m = findMethod(e->s))
                checkArgs(e, "." + e->s + "()", m->minArgs, m->maxArgs, e->loc, (int)e->s.size(), "");
            for (size_t i = 1; i < e->kids.size(); i++) expr(f, e->kids[i].get(), c);
            return;
        }
        case NK::Lambda: {
            Func* L = newFunc();
            L->isLambda = true;
            L->parent = f;
            L->node = e;
            L->id = ++lambdaCount_;
            L->name = "lambda";
            L->cname = "l_" + std::to_string(L->id);
            e->fn = L;
            for (size_t i = 0; i < e->names.size(); i++) {
                Sym* ps = newSym(e->names[i], SymKind::Param, L, e->nameLocs[i]);
                L->params.push_back(ps);
                L->scope[ps->name] = ps;
            }
            std::vector<Assigned> as;
            collect(e->kids[0].get(), as);
            declare(L, as);
            Ctx lc;
            stmt(L, e->kids[0].get(), lc);
            return;
        }
        default:
            for (auto& k : e->kids) expr(f, k.get(), c);
            return;
        }
    }

    // ------------------------------------------------------------ control flow

    static bool hasDip(const Node* n) {  // a dip that would leave *this* loop
        if (n->k == NK::Dip) return true;
        if (n->k == NK::While || n->k == NK::ForIn || n->k == NK::Lambda) return false;
        for (auto& k : n->kids)
            if (hasDip(k.get())) return true;
        return false;
    }

    static bool terminates(const Node* block) {
        if (block->kids.empty()) return false;
        const Node* last = block->kids.back().get();
        switch (last->k) {
        case NK::Serve:
        case NK::Yeet: return true;
        case NK::If: {
            if (last->kids.size() % 2 == 0) return false;  // no final nah
            for (size_t i = 1; i + 1 < last->kids.size(); i += 2)
                if (!terminates(last->kids[i].get())) return false;
            return terminates(last->kids.back().get());
        }
        case NK::Try: return last->kids.size() > 1 && terminates(last->kids[0].get()) && terminates(last->kids[1].get());
        case NK::While: return last->kids.size() == 1 && !hasDip(last->kids[0].get());
        default: return false;
        }
    }

    // ------------------------------------------------------------ type inference

    void widen(Sym* s, Ty t) {
        Ty n = join(s->ty, t);
        if (n != s->ty) {
            s->ty = n;
            changed_ = true;
        }
    }

    void widenRet(Func* f, Ty t) {
        if (f->isLambda) return;
        Ty n = join(f->ret, t);
        if (n != f->ret) {
            f->ret = n;
            changed_ = true;
        }
    }

    void infer() {
        for (int iter = 0; iter < 200; iter++) {
            changed_ = false;
            for (auto& fp : P.funcs) {
                Func* f = fp.get();
                if (f->isLambda || f->escapes)
                    for (Sym* p : f->params) widen(p, DYN);
                for (Sym* cap : f->captures) widen(cap, cap->outer->ty);
                if (f->isMain) {
                    for (auto& s : P.stmts)
                        if (s->k != NK::Func) istmt(f, s.get());
                } else {
                    istmt(f, f->node->kids[0].get());
                    if (f->fallsThrough) widenRet(f, DYN);
                }
            }
            if (!changed_) break;
        }
    }

    void iexpr(Node* e) {
        if (e->k == NK::Lambda) return;  // analysed as its own function
        if (e->k == NK::Call && e->fn && !e->fn->escapes)
            for (size_t i = 0; i < e->fn->params.size(); i++) widen(e->fn->params[i], exprTy(e->kids[i + 1].get()));
        for (auto& k : e->kids) iexpr(k.get());
    }

    void istmt(Func* f, Node* n) {
        switch (n->k) {
        case NK::Assign: {
            Node* t = n->kids[0].get();
            Node* v = n->kids[1].get();
            for (auto& k : t->kids) iexpr(k.get());
            iexpr(v);
            if (t->k == NK::Ident) {
                Ty vt = exprTy(v);
                if (n->s.empty()) widen(t->sym, vt);
                else if (n->s == "+") widen(t->sym, plusTy(t->sym->ty, vt));
                else widen(t->sym, NUM);
            }
            break;
        }
        case NK::Lock:
            iexpr(n->kids[0].get());
            widen(n->sym, exprTy(n->kids[0].get()));
            break;
        case NK::ForIn:
            iexpr(n->kids[0].get());
            if (n->kids[0]->k == NK::Range) widen(n->sym, NUM);
            else {
                widen(n->sym, DYN);
                if (n->sym2) widen(n->sym2, DYN);
            }
            istmt(f, n->kids[1].get());
            break;
        case NK::Try:
            istmt(f, n->kids[0].get());
            if (n->sym) widen(n->sym, DYN);
            if (n->kids.size() > 1) istmt(f, n->kids[1].get());
            break;
        case NK::Serve:
            if (n->kids.empty()) widenRet(f, DYN);
            else {
                iexpr(n->kids[0].get());
                widenRet(f, exprTy(n->kids[0].get()));
            }
            break;
        case NK::If:
            for (auto& k : n->kids) {
                if (k->k == NK::Block) istmt(f, k.get());
                else iexpr(k.get());
            }
            break;
        case NK::While:
            if (n->kids.size() == 2) iexpr(n->kids[0].get());
            istmt(f, n->kids.back().get());
            break;
        case NK::Block:
            for (auto& k : n->kids) istmt(f, k.get());
            break;
        case NK::ExprStmt:
        case NK::Yeet: iexpr(n->kids[0].get()); break;
        default: break;
        }
    }
};

}  // namespace

const BuiltinInfo* findBuiltin(const std::string& name) {
    int i = builtinIndex(name);
    return i >= 0 ? &kBuiltins[i] : nullptr;
}

int builtinIndex(const std::string& name) {
    for (size_t i = 0; i < sizeof kBuiltins / sizeof kBuiltins[0]; i++)
        if (name == kBuiltins[i].name) return (int)i;
    return -1;
}

const BuiltinInfo* builtinAt(int index) { return &kBuiltins[index]; }

const MethodInfo* findMethod(const std::string& name) {
    for (const auto& m : kMethods)
        if (name == m.name) return &m;
    return nullptr;
}

Ty exprTy(const Node* e) {
    switch (e->k) {
    case NK::Num: return NUM;
    case NK::Bool: return BOOL;
    case NK::Ident: return e->sym ? e->sym->ty : DYN;
    case NK::Unary: return e->s == "-" ? NUM : BOOL;
    case NK::Binary: {
        const std::string& op = e->s;
        if (op == "+") return plusTy(exprTy(e->kids[0].get()), exprTy(e->kids[1].get()));
        if (op == "-" || op == "*" || op == "/" || op == "%" || op == "**") return NUM;
        return BOOL;
    }
    case NK::And:
    case NK::Or: return BOOL;
    case NK::Call:
        if (e->fn) return e->fn->ret;
        if (e->builtin >= 0) {
            const BuiltinInfo* b = builtinAt(e->builtin);
            if (!strcmp(b->name, "min") || !strcmp(b->name, "max")) {
                if (e->kids.size() < 3) return DYN;
                for (size_t i = 1; i < e->kids.size(); i++) {
                    Ty t = exprTy(e->kids[i].get());
                    if (t != NUM && t != BOT) return DYN;
                }
                return NUM;
            }
            return b->ret;
        }
        return DYN;
    default: return DYN;
    }
}

void analyze(Program& prog) { Analyzer(prog).run(); }
