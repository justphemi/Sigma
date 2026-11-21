// Recursive-descent parser.
//
// Statements end at a newline (or ';'). Inside (), [] and map literals,
// newlines are ignored, so long expressions can be split across lines there.
// Outside of brackets, a binary operator must stay on the same line as its
// left operand; method chains may start a line with '.'.
#include "parser.h"

#include <algorithm>
#include <cctype>
#include <set>

#include "lexer.h"

namespace {

std::string describe(const Token& t) {
    switch (t.t) {
    case Tok::Eof: return "the end of the file";
    case Tok::Num: return "the number " + t.text;
    case Tok::Str: return "a string";
    case Tok::Ident: return "the name '" + t.text + "'";
    default: return "'" + t.text + "'";
    }
}

int width(const Token& t) { return std::max<int>(1, (int)t.text.size()); }

bool isWord(const Token& t) {
    return t.t != Tok::Str && t.t != Tok::Num && !t.text.empty() &&
           (isalpha((unsigned char)t.text[0]) || t.text[0] == '_');
}

class Parser {
public:
    Parser(std::vector<Token> toks, int depth) : t_(std::move(toks)), depth_(depth) {}

    std::vector<NodeP> program() {
        std::vector<NodeP> out;
        while (!is(Tok::Eof)) {
            if (is(Tok::Semi)) { next(); continue; }
            out.push_back(statement());
            endStatement();
        }
        return out;
    }

    NodeP soloExpr() {
        NodeP e = expr();
        if (!is(Tok::Eof)) fail(cur().loc, "found " + describe(cur()) + " where a {...} in a string should end", "", width(cur()));
        return e;
    }

private:
    std::vector<Token> t_;
    size_t p_ = 0;
    int depth_ = 0;      // > 0 inside (), [] or a map literal
    int blockDepth_ = 0;  // > 0 inside any { } block

    const Token& cur() const { return t_[p_]; }
    const Token& ahead(size_t k) const { return t_[std::min(p_ + k, t_.size() - 1)]; }
    bool is(Tok k) const { return cur().t == k; }
    bool sameLine() const { return depth_ > 0 || !cur().nl; }
    Token next() {
        Token tk = t_[p_];
        if (p_ + 1 < t_.size()) p_++;
        return tk;
    }

    [[noreturn]] void expected(const std::string& what, const std::string& hint = "") {
        fail(cur().loc, "expected " + what + ", but found " + describe(cur()), hint, width(cur()));
    }
    Token expect(Tok k, const std::string& what, const std::string& hint = "") {
        if (!is(k)) expected(what, hint);
        return next();
    }

    void endStatement() {
        if (is(Tok::Semi)) { next(); return; }
        if (is(Tok::RBrace) || is(Tok::Eof) || cur().nl) return;
        fail(cur().loc, "found " + describe(cur()) + " after the end of a statement",
             "put each statement on its own line (or separate them with ;)", width(cur()));
    }

    // ---------------------------------------------------------------- statements

    NodeP block(const std::string& what) {
        Loc open = cur().loc;
        if (!is(Tok::LBrace)) expected("'{' to start the " + what);
        next();
        int savedDepth = depth_;
        depth_ = 0;
        blockDepth_++;
        NodeP b = mk(NK::Block, open);
        while (!is(Tok::RBrace)) {
            if (is(Tok::Eof)) fail(open, "this '{' is never closed", "add a matching '}'");
            if (is(Tok::Semi)) { next(); continue; }
            b->kids.push_back(statement());
            endStatement();
        }
        next();
        blockDepth_--;
        depth_ = savedDepth;
        return b;
    }

    NodeP statement() {
        const Token tk = cur();
        switch (tk.t) {
        case Tok::Yoink: {
            if (blockDepth_ > 0) fail(tk.loc, "yoink only works at the top level of a file", "", 5);
            next();
            Token path = expect(Tok::Str, "a file path like \"utils.sgm\" after yoink");
            if (!path.parts.empty()) fail(path.loc, "yoink paths can't use {interpolation}");
            NodeP n = mk(NK::Yoink, tk.loc);
            n->s = path.text;
            return n;
        }
        case Tok::Cook:
            if (blockDepth_ > 0)
                fail(tk.loc, "functions can only be cooked at the top level",
                     "inside a block, use a lambda instead:  helper: (x) => x * 2", 4);
            return funcDecl();
        case Tok::Lock: {
            next();
            Token name = expect(Tok::Ident, "a name after 'lock'");
            expect(Tok::Colon, "':' after the name", "constants look like:  lock PI: 3.14159");
            NodeP n = mk(NK::Lock, name.loc);
            n->s = name.text;
            n->kids.push_back(expr());
            return n;
        }
        case Tok::Fr: return ifStmt();
        case Tok::Nah:
            fail(tk.loc, "'nah' without an 'fr' before it", "nah (else) has to come right after an fr { ... } block", 3);
        case Tok::Oops:
            fail(tk.loc, "'oops' without a 'yolo' before it", "write  yolo { ... } oops e { ... }", 4);
        case Tok::Grind: return grind();
        case Tok::Dip: next(); return mk(NK::Dip, tk.loc);
        case Tok::Skip: next(); return mk(NK::Skip, tk.loc);
        case Tok::Serve: {
            next();
            NodeP n = mk(NK::Serve, tk.loc);
            if (!is(Tok::RBrace) && !is(Tok::Eof) && !is(Tok::Semi) && !cur().nl) n->kids.push_back(expr());
            return n;
        }
        case Tok::Yeet: {
            next();
            NodeP n = mk(NK::Yeet, tk.loc);
            n->kids.push_back(expr());
            return n;
        }
        case Tok::Yolo: {
            next();
            NodeP n = mk(NK::Try, tk.loc);
            n->kids.push_back(block("yolo block"));
            if (is(Tok::Oops)) {
                next();
                if (is(Tok::Ident)) {
                    n->s = cur().text;
                    n->nameLocs.push_back(cur().loc);
                    next();
                }
                n->kids.push_back(block("oops block"));
            } else if (is(Tok::Ident) && cur().text == "catch") {
                fail(cur().loc, "'catch' is old Sigma syntax", "write  oops e { ... }", 5);
            }
            return n;
        }
        default: break;
        }

        if (tk.t == Tok::Ident) {
            if (tk.text == "fn" && ahead(1).t == Tok::Ident)
                fail(tk.loc, "'fn' is old Sigma syntax", "cook a function like this:  cook add(a, b) { serve a + b }", 2);
            if (tk.text == "return" && !ahead(1).nl)
                fail(tk.loc, "Sigma uses 'serve' to return a value", "write  serve a + b", 6);
        }

        NodeP e = expr();
        if (is(Tok::Colon)) {
            checkTarget(e.get());
            next();
            NodeP n = mk(NK::Assign, e->loc);
            n->kids.push_back(std::move(e));
            n->kids.push_back(expr());
            return n;
        }
        static const std::pair<Tok, const char*> kCompound[] = {
            {Tok::PlusEq, "+"}, {Tok::MinusEq, "-"}, {Tok::StarEq, "*"}, {Tok::SlashEq, "/"}, {Tok::PercentEq, "%"}};
        for (const auto& c : kCompound) {
            if (is(c.first)) {
                checkTarget(e.get());
                next();
                NodeP n = mk(NK::Assign, e->loc);
                n->s = c.second;
                n->kids.push_back(std::move(e));
                n->kids.push_back(expr());
                return n;
            }
        }
        if (is(Tok::PlusPlus) && !cur().nl) {
            checkTarget(e.get());
            Loc l = next().loc;
            NodeP n = mk(NK::Assign, e->loc);
            n->s = "+";
            n->kids.push_back(std::move(e));
            NodeP one = mk(NK::Num, l);
            one->num = 1;
            n->kids.push_back(std::move(one));
            return n;
        }
        if (e->k == NK::Call || e->k == NK::MethodCall) {
            NodeP n = mk(NK::ExprStmt, e->loc);
            n->kids.push_back(std::move(e));
            return n;
        }
        std::string hint = e->k == NK::Ident ? "to give it a value, write  " + e->s + ": something"
                                             : "only function calls can stand alone on a line";
        fail(e->loc, "this line doesn't do anything", hint);
    }

    void checkTarget(Node* e) {
        if (e->k == NK::Ident || e->k == NK::Field || e->k == NK::Index) return;
        fail(e->loc, "can't assign to this", "the left side must be a name (x), a field (a.b) or an index (a[0])");
    }

    NodeP funcDecl() {
        next();  // cook
        Token name = expect(Tok::Ident, "a function name after 'cook'");
        if (is(Tok::Colon))
            fail(cur().loc, "that's the old function syntax", "Sigma 0.2:  cook " + name.text + "(a, b) { ... }");
        NodeP n = mk(NK::Func, name.loc);
        n->s = name.text;
        expect(Tok::LParen, "'(' after the function name", "functions look like:  cook " + name.text + "(a, b) { ... }");
        params(n.get());
        n->kids.push_back(block("function body"));
        return n;
    }

    void params(Node* n) {  // after '(' up to and including ')'
        std::set<std::string> seen;
        while (!is(Tok::RParen)) {
            Token pn = expect(Tok::Ident, "a parameter name");
            if (!seen.insert(pn.text).second) fail(pn.loc, "parameter '" + pn.text + "' is listed twice", "", width(pn));
            n->names.push_back(pn.text);
            n->nameLocs.push_back(pn.loc);
            if (is(Tok::Comma)) next();
            else if (!is(Tok::RParen)) expected("',' or ')'");
        }
        next();
    }

    NodeP ifStmt() {
        NodeP n = mk(NK::If, cur().loc);
        next();
        n->kids.push_back(expr());
        n->kids.push_back(block("fr block"));
        while (is(Tok::Nah)) {
            next();
            if (is(Tok::Fr)) {
                next();
                n->kids.push_back(expr());
                n->kids.push_back(block("nah fr block"));
            } else {
                n->kids.push_back(block("nah block"));
                break;
            }
        }
        return n;
    }

    NodeP grind() {
        Loc l = next().loc;
        if (is(Tok::LBrace)) {
            NodeP n = mk(NK::While, l);
            n->kids.push_back(block("grind loop"));
            return n;
        }
        if (is(Tok::Ident) && (ahead(1).t == Tok::In || ahead(1).t == Tok::Comma)) {
            NodeP n = mk(NK::ForIn, l);
            Token a = next();
            n->names.push_back(a.text);
            n->nameLocs.push_back(a.loc);
            if (is(Tok::Comma)) {
                next();
                Token b = expect(Tok::Ident, "a second loop variable name");
                if (b.text == a.text) fail(b.loc, "both loop variables are called '" + a.text + "'", "", width(b));
                n->names.push_back(b.text);
                n->nameLocs.push_back(b.loc);
            }
            expect(Tok::In, "'in'", "loops look like:  grind item in items { ... }");
            n->kids.push_back(expr());
            n->kids.push_back(block("grind loop"));
            return n;
        }
        NodeP n = mk(NK::While, l);
        n->kids.push_back(expr());
        n->kids.push_back(block("grind loop"));
        return n;
    }

    // ---------------------------------------------------------------- expressions

    NodeP expr() { return orExpr(); }

    NodeP binary(NK k, const std::string& op, Loc l, NodeP a, NodeP b) {
        NodeP n = mk(k, l);
        n->s = op;
        n->kids.push_back(std::move(a));
        n->kids.push_back(std::move(b));
        return n;
    }

    NodeP orExpr() {
        NodeP l = andExpr();
        while ((is(Tok::Or) || is(Tok::OrOr)) && sameLine()) {
            Loc ol = next().loc;
            l = binary(NK::Or, "or", ol, std::move(l), andExpr());
        }
        return l;
    }

    NodeP andExpr() {
        NodeP l = notExpr();
        while ((is(Tok::And) || is(Tok::AndAnd)) && sameLine()) {
            Loc ol = next().loc;
            l = binary(NK::And, "and", ol, std::move(l), notExpr());
        }
        return l;
    }

    NodeP notExpr() {
        if (is(Tok::Not) || is(Tok::Bang)) {
            Loc l = next().loc;
            NodeP n = mk(NK::Unary, l);
            n->s = "not";
            n->kids.push_back(notExpr());
            return n;
        }
        return cmpExpr();
    }

    bool isCmp() const {
        switch (cur().t) {
        case Tok::Eq: case Tok::Neq: case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge: return true;
        default: return false;
        }
    }

    NodeP cmpExpr() {
        NodeP l = rangeExpr();
        if (isCmp() && sameLine()) {
            Token op = next();
            l = binary(NK::Binary, op.text, op.loc, std::move(l), rangeExpr());
            if (isCmp() && sameLine())
                fail(cur().loc, "comparisons can't be chained", "write  a < b and b < c", width(cur()));
        }
        return l;
    }

    NodeP rangeExpr() {
        NodeP l = addExpr();
        if ((is(Tok::DotDot) || is(Tok::DotDotEq)) && sameLine()) {
            Token op = next();
            NodeP n = binary(NK::Range, op.text, op.loc, std::move(l), addExpr());
            n->flag = op.t == Tok::DotDotEq;
            return n;
        }
        return l;
    }

    NodeP addExpr() {
        NodeP l = mulExpr();
        while ((is(Tok::Plus) || is(Tok::Minus)) && sameLine()) {
            Token op = next();
            l = binary(NK::Binary, op.text, op.loc, std::move(l), mulExpr());
        }
        return l;
    }

    NodeP mulExpr() {
        NodeP l = unary();
        while ((is(Tok::Star) || is(Tok::Slash) || is(Tok::Percent)) && sameLine()) {
            Token op = next();
            l = binary(NK::Binary, op.text, op.loc, std::move(l), unary());
        }
        return l;
    }

    NodeP unary() {
        if (is(Tok::Minus)) {
            Loc l = next().loc;
            NodeP inner = unary();
            if (inner->k == NK::Num) {  // fold -5 into a literal
                inner->num = -inner->num;
                inner->loc = l;
                return inner;
            }
            NodeP n = mk(NK::Unary, l);
            n->s = "-";
            n->kids.push_back(std::move(inner));
            return n;
        }
        return powExpr();
    }

    NodeP powExpr() {
        NodeP l = postfix();
        if (is(Tok::StarStar) && sameLine()) {
            Token op = next();
            return binary(NK::Binary, "**", op.loc, std::move(l), unary());
        }
        return l;
    }

    void args(Node* n) {  // after '(' up to and including ')'
        depth_++;
        while (!is(Tok::RParen)) {
            if (is(Tok::Eof)) expected("')'");
            n->kids.push_back(expr());
            if (is(Tok::Comma)) next();
            else if (!is(Tok::RParen)) expected("',' or ')'");
        }
        depth_--;
        next();
    }

    NodeP postfix() {
        NodeP e = primary();
        for (;;) {
            if (is(Tok::LParen) && sameLine()) {
                next();
                NodeP n = mk(NK::Call, e->loc);
                n->kids.push_back(std::move(e));
                args(n.get());
                e = std::move(n);
            } else if (is(Tok::LBrack) && sameLine()) {
                Loc l = next().loc;
                depth_++;
                NodeP idx = expr();
                depth_--;
                expect(Tok::RBrack, "']'");
                NodeP n = mk(NK::Index, l);
                n->kids.push_back(std::move(e));
                n->kids.push_back(std::move(idx));
                e = std::move(n);
            } else if (is(Tok::Dot)) {
                next();
                if (!isWord(cur())) expected("a field or method name after '.'");
                Token name = next();
                if (is(Tok::LParen) && !cur().nl) {
                    next();
                    NodeP n = mk(NK::MethodCall, name.loc);
                    n->s = name.text;
                    n->kids.push_back(std::move(e));
                    args(n.get());
                    e = std::move(n);
                } else {
                    NodeP n = mk(NK::Field, name.loc);
                    n->s = name.text;
                    n->kids.push_back(std::move(e));
                    e = std::move(n);
                }
            } else {
                break;
            }
        }
        return e;
    }

    bool lambdaAhead() const {  // at '(' : is this "(a, b) =>" ?
        int level = 0;
        for (size_t j = p_; j < t_.size(); j++) {
            if (t_[j].t == Tok::LParen) level++;
            else if (t_[j].t == Tok::RParen && --level == 0) return j + 1 < t_.size() && t_[j + 1].t == Tok::Arrow;
            else if (t_[j].t == Tok::Eof) return false;
        }
        return false;
    }

    NodeP lambda() {
        NodeP n = mk(NK::Lambda, cur().loc);
        if (is(Tok::Ident)) {
            n->names.push_back(cur().text);
            n->nameLocs.push_back(cur().loc);
            next();
        } else {
            next();  // (
            params(n.get());
        }
        expect(Tok::Arrow, "'=>'");
        if (is(Tok::LBrace)) {
            n->kids.push_back(block("lambda body"));
        } else {
            Loc l = cur().loc;
            NodeP body = mk(NK::Block, l);
            NodeP serve = mk(NK::Serve, l);
            serve->kids.push_back(expr());
            body->kids.push_back(std::move(serve));
            n->kids.push_back(std::move(body));
        }
        return n;
    }

    NodeP primary() {
        const Token tk = cur();
        switch (tk.t) {
        case Tok::Num: {
            NodeP n = mk(NK::Num, tk.loc);
            n->num = tk.num;
            next();
            return n;
        }
        case Tok::Str: return stringLit();
        case Tok::True:
        case Tok::False: {
            NodeP n = mk(NK::Bool, tk.loc);
            n->flag = tk.t == Tok::True;
            next();
            return n;
        }
        case Tok::Nil: next(); return mk(NK::Nil, tk.loc);
        case Tok::Ident: {
            if (ahead(1).t == Tok::Arrow) return lambda();
            NodeP n = mk(NK::Ident, tk.loc);
            n->s = tk.text;
            next();
            return n;
        }
        case Tok::LParen: {
            if (lambdaAhead()) return lambda();
            next();
            depth_++;
            NodeP e = expr();
            depth_--;
            expect(Tok::RParen, "')'");
            return e;
        }
        case Tok::LBrack: {
            NodeP n = mk(NK::List, tk.loc);
            next();
            depth_++;
            while (!is(Tok::RBrack)) {
                if (is(Tok::Eof)) fail(tk.loc, "this '[' is never closed", "add a matching ']'");
                n->kids.push_back(expr());
                if (is(Tok::Comma)) next();
                else if (!is(Tok::RBrack)) expected("',' or ']'");
            }
            depth_--;
            next();
            return n;
        }
        case Tok::LBrace: return mapLit();
        default: break;
        }
        expected("a value");
    }

    NodeP mapLit() {
        Loc open = cur().loc;
        NodeP n = mk(NK::Map, open);
        next();
        depth_++;
        while (!is(Tok::RBrace)) {
            if (is(Tok::Eof)) fail(open, "this '{' is never closed", "add a matching '}'");
            Token key = cur();
            std::string k;
            if (key.t == Tok::Str) {
                if (!key.parts.empty()) fail(key.loc, "map keys can't use {interpolation}");
                k = key.text;
            } else if (key.t == Tok::Num) {
                k = formatNumber(key.num);
            } else if (isWord(key)) {
                k = key.text;
            } else {
                expected("a key name", "maps look like  {name: \"Sigma\", age: 1}");
            }
            next();
            expect(Tok::Colon, "':' after the key '" + k + "'", "maps look like  {name: \"Sigma\", age: 1}");
            n->names.push_back(k);
            n->nameLocs.push_back(key.loc);
            n->kids.push_back(expr());
            if (is(Tok::Comma)) next();
            else if (!is(Tok::RBrace) && !cur().nl) expected("',' or '}'");
        }
        depth_--;
        next();
        return n;
    }

    NodeP stringLit() {
        Token tk = next();
        if (tk.parts.empty()) {
            NodeP n = mk(NK::Str, tk.loc);
            n->s = tk.text;
            return n;
        }
        NodeP n = mk(NK::Interp, tk.loc);
        for (const auto& part : tk.parts) {
            if (!part.isExpr) {
                NodeP s = mk(NK::Str, part.loc);
                s->s = part.text;
                n->kids.push_back(std::move(s));
            } else {
                Parser sub(lex(part.loc.file, part.text, part.loc.line, part.loc.col), 1);
                n->kids.push_back(sub.soloExpr());
            }
        }
        return n;
    }
};

}  // namespace

std::vector<NodeP> parseFile(int file, const std::string& src) {
    return Parser(lex(file, src), 0).program();
}
