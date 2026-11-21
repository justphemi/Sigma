#include "lexer.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, Tok> kKeywords = {
    {"fr", Tok::Fr},       {"nah", Tok::Nah},     {"grind", Tok::Grind}, {"in", Tok::In},
    {"dip", Tok::Dip},     {"skip", Tok::Skip},   {"cook", Tok::Cook},   {"serve", Tok::Serve},
    {"lock", Tok::Lock},   {"yolo", Tok::Yolo},   {"oops", Tok::Oops},   {"yeet", Tok::Yeet},
    {"yoink", Tok::Yoink}, {"true", Tok::True},   {"false", Tok::False}, {"nocap", Tok::True},
    {"cap", Tok::False},   {"nil", Tok::Nil},     {"and", Tok::And},     {"or", Tok::Or},
    {"not", Tok::Not},
};

// Sigma 0.1 keywords -> what to write now.
const std::unordered_map<std::string, std::string> kLegacy = {
    {"$if", "fr <condition> { ... }"},
    {"$el", "nah { ... }"},
    {"$for", "grind i in 0..10 { ... }"},
    {"$while", "grind <condition> { ... }"},
    {"$fixed", "lock NAME: value"},
    {"$try", "yolo { ... } oops e { ... }"},
    {"$in", "name: ask(\"prompt\")"},
    {"$time_start", "start: now()"},
    {"$time_end", "now() - start"},
};

struct OpDef {
    const char* s;
    Tok t;
};
// Longest first.
const OpDef kOps[] = {
    {"..=", Tok::DotDotEq}, {"**", Tok::StarStar}, {"..", Tok::DotDot}, {"=>", Tok::Arrow},
    {"==", Tok::Eq},        {"!=", Tok::Neq},      {"<=", Tok::Le},     {">=", Tok::Ge},
    {"&&", Tok::AndAnd},    {"||", Tok::OrOr},     {"+=", Tok::PlusEq}, {"-=", Tok::MinusEq},
    {"*=", Tok::StarEq},    {"/=", Tok::SlashEq},  {"%=", Tok::PercentEq}, {"++", Tok::PlusPlus},
    {"+", Tok::Plus},       {"-", Tok::Minus},     {"*", Tok::Star},    {"/", Tok::Slash},
    {"%", Tok::Percent},    {"(", Tok::LParen},    {")", Tok::RParen},  {"{", Tok::LBrace},
    {"}", Tok::RBrace},     {"[", Tok::LBrack},    {"]", Tok::RBrack},  {",", Tok::Comma},
    {".", Tok::Dot},        {":", Tok::Colon},     {";", Tok::Semi},    {"<", Tok::Lt},
    {">", Tok::Gt},         {"!", Tok::Bang},
};

class Lexer {
public:
    Lexer(int file, const std::string& src, int line, int col) : file_(file), s_(src), line_(line), col_(col) {}

    std::vector<Token> run() {
        // A "#!" first line lets a .sgm file run directly as a script.
        if (line_ == 1 && col_ == 1 && s_.compare(0, 2, "#!") == 0)
            while (i_ < s_.size() && peek() != '\n') get();
        for (;;) {
            skipSpace();
            if (i_ >= s_.size()) break;
            char c = peek();
            if (isdigit((unsigned char)c)) number();
            else if (c == '"' || c == '\'') string();
            else if (isalpha((unsigned char)c) || c == '_' || c == '$') ident();
            else op();
        }
        Token eof;
        eof.t = Tok::Eof;
        eof.loc = here();
        eof.nl = true;
        out_.push_back(eof);
        return std::move(out_);
    }

private:
    int file_;
    const std::string& s_;
    size_t i_ = 0;
    int line_, col_;
    bool nl_ = true;
    std::vector<Token> out_;

    char peek(size_t k = 0) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }
    char get() {
        char c = s_[i_++];
        if (c == '\n') { line_++; col_ = 1; }
        else col_++;
        return c;
    }
    Loc here() const { return {file_, line_, col_}; }

    Token& push(Tok t, Loc l, std::string text) {
        Token tk;
        tk.t = t;
        tk.loc = l;
        tk.text = std::move(text);
        tk.nl = nl_;
        nl_ = false;
        out_.push_back(std::move(tk));
        return out_.back();
    }

    void skipSpace() {
        for (;;) {
            char c = peek();
            if (c == '\n') { nl_ = true; get(); }
            else if (c == ' ' || c == '\t' || c == '\r') get();
            else if (c == '-' && peek(1) == '-') comment();
            else break;
        }
    }

    void comment() {
        Loc start = here();
        if (peek(2) == '-' && peek(3) != '-') {  // --- block comment ---
            get(); get(); get();
            for (;;) {
                if (i_ >= s_.size()) fail(start, "this block comment is never closed", "close it with another ---", 3);
                if (peek() == '-' && peek(1) == '-' && peek(2) == '-') { get(); get(); get(); return; }
                if (peek() == '\n') nl_ = true;
                get();
            }
        }
        // `x--` at the end of a line almost certainly meant "decrement".
        if (i_ > 0) {
            char before = s_[i_ - 1];
            if (isalnum((unsigned char)before) || before == '_' || before == ')' || before == ']') {
                size_t j = i_ + 2;
                while (j < s_.size() && (s_[j] == ' ' || s_[j] == '\t' || s_[j] == '\r')) j++;
                if (j >= s_.size() || s_[j] == '\n')
                    fail(start, "'--' starts a comment in Sigma", "to subtract one, write  x -= 1", 2);
            }
        }
        while (i_ < s_.size() && peek() != '\n') get();
    }

    void number() {
        Loc l = here();
        std::string t;
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            get(); get();
            while (isxdigit((unsigned char)peek()) || peek() == '_') {
                char c = get();
                if (c != '_') t += c;
            }
            if (t.empty()) fail(l, "this hex number has no digits", "", 2);
            push(Tok::Num, l, "0x" + t).num = (double)strtoull(t.c_str(), nullptr, 16);
            return;
        }
        auto digits = [&] {
            while (isdigit((unsigned char)peek()) || peek() == '_') {
                char c = get();
                if (c != '_') t += c;
            }
        };
        digits();
        if (peek() == '.' && isdigit((unsigned char)peek(1))) { t += get(); digits(); }
        if ((peek() == 'e' || peek() == 'E') &&
            (isdigit((unsigned char)peek(1)) || ((peek(1) == '+' || peek(1) == '-') && isdigit((unsigned char)peek(2))))) {
            t += get();
            if (peek() == '+' || peek() == '-') t += get();
            digits();
        }
        if (isalpha((unsigned char)peek()) || peek() == '_')
            fail(here(), "unexpected letter right after a number", "names can't start with a digit");
        push(Tok::Num, l, t).num = strtod(t.c_str(), nullptr);
    }

    void string() {
        Loc l = here();
        char q = get();
        std::vector<StrPart> parts;
        std::string cur;
        Loc curLoc = here();
        bool interpolated = false;
        for (;;) {
            if (i_ >= s_.size()) fail(l, "this string is never closed", std::string("add a closing ") + q);
            char c = peek();
            if (c == q) { get(); break; }
            if (c == '\\') {
                Loc el = here();
                get();
                char e = i_ < s_.size() ? get() : '\0';
                switch (e) {
                case 'n': cur += '\n'; break;
                case 't': cur += '\t'; break;
                case 'r': cur += '\r'; break;
                case '0': cur += '\0'; break;
                case '\\': case '"': case '\'': case '{': case '}': cur += e; break;
                default:
                    fail(el, std::string("unknown escape '\\") + e + "'",
                         "valid escapes: \\n \\t \\r \\0 \\\\ \\\" \\' \\{ \\}", 2);
                }
                continue;
            }
            if (c == '{') {
                interpolated = true;
                if (!cur.empty()) parts.push_back({false, cur, curLoc});
                cur.clear();
                Loc open = here();
                get();
                Loc el = here();
                std::string expr;
                int depth = 0;
                for (;;) {
                    if (i_ >= s_.size() || peek() == '\n')
                        fail(open, "this { in a string is never closed", "close it with }, or write \\{ for a literal brace");
                    char d = peek();
                    if (d == '"' || d == '\'') {  // a string inside the expression
                        char qq = get();
                        expr += qq;
                        while (i_ < s_.size() && peek() != qq && peek() != '\n') {
                            if (peek() == '\\') expr += get();
                            if (i_ < s_.size()) expr += get();
                        }
                        if (peek() == qq) expr += get();
                        continue;
                    }
                    if (d == '{') depth++;
                    if (d == '}') {
                        if (depth == 0) { get(); break; }
                        depth--;
                    }
                    expr += get();
                }
                if (expr.find_first_not_of(" \t") == std::string::npos)
                    fail(open, "empty {} in a string", "put a value inside, like \"hi {name}\" (or write \\{ for a brace)", 2);
                parts.push_back({true, expr, el});
                curLoc = here();
                continue;
            }
            cur += get();
        }
        Token& tk = push(Tok::Str, l, interpolated ? "" : cur);
        if (interpolated) {
            if (!cur.empty()) parts.push_back({false, cur, curLoc});
            tk.parts = std::move(parts);
        }
    }

    void ident() {
        Loc l = here();
        std::string t;
        if (peek() == '$') {
            t += get();
            while (isalnum((unsigned char)peek()) || peek() == '_') t += get();
            auto it = kLegacy.find(t);
            if (it != kLegacy.end())
                fail(l, "'" + t + "' is old Sigma syntax", "Sigma 0.2 writes this as:  " + it->second, (int)t.size());
            fail(l, "unexpected '$'", "Sigma 0.2 keywords don't use $ - see the README", 1);
        }
        while (isalnum((unsigned char)peek()) || peek() == '_') t += get();
        auto it = kKeywords.find(t);
        push(it != kKeywords.end() ? it->second : Tok::Ident, l, t);
    }

    void op() {
        Loc l = here();
        if (peek() == ':' && peek(1) == ':')
            fail(l, "'::' is old Sigma syntax",
                 "blocks use braces now:  fr x > 1 { ... }   and map entries use one colon:  {name: \"Sigma\"}", 2);
        if (peek() == '=' && peek(1) == '=' && peek(2) == '=')
            fail(l, "'===' isn't needed in Sigma", "'==' already compares type and value", 3);
        if (peek() == '=' && peek(1) != '=' && peek(1) != '>')
            fail(l, "Sigma uses ':' to assign", "write  x: 5  to assign, or  x == 5  to compare");
        for (const auto& o : kOps) {
            size_t n = strlen(o.s);
            if (s_.compare(i_, n, o.s) == 0) {
                for (size_t k = 0; k < n; k++) get();
                push(o.t, l, o.s);
                return;
            }
        }
        unsigned char c = (unsigned char)peek();
        if (c >= 0x80) fail(l, "unexpected non-ASCII character", "names and operators must be ASCII (strings can hold anything)");
        fail(l, std::string("unexpected character '") + (char)c + "'");
    }
};

}  // namespace

std::vector<Token> lex(int file, const std::string& src, int line, int col) {
    return Lexer(file, src, line, col).run();
}
