#include "../include/token.h"
#include <vector>
#include <string>
#include <cctype>
#include <map>

class Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1, col = 1;
    
    std::map<std::string, TokenType> keywords = {
        {"fn", TOK_FN}, {"return", TOK_RETURN}, {"true", TOK_TRUE}, 
        {"false", TOK_FALSE}, {"yap", TOK_YAP},
        {"$if", TOK_IF}, {"$el", TOK_EL}, {"$for", TOK_FOR}, 
        {"$while", TOK_WHILE}, {"$time_start", TOK_TIME_START},
        {"$time_end", TOK_TIME_END}, {"$fixed", TOK_FIXED},
        {"$try", TOK_TRY}, {"catch", TOK_CATCH}, {"$in", TOK_IN}
    };
    
    char peek() { return pos < src.size() ? src[pos] : '\0'; }
    char advance() { col++; return src[pos++]; }
    
    void skipWhitespace() {
        while (isspace(peek())) {
            if (peek() == '\n') { line++; col = 0; }
            advance();
        }
    }
    
    void skipComment() {
        if (peek() == '-' && pos + 1 < src.size() && src[pos + 1] == '-') {
            if (pos + 2 < src.size() && src[pos + 2] == '-') {
                // Multi-line comment
                advance(); advance(); advance();
                while (pos + 2 < src.size()) {
                    if (src[pos] == '-' && src[pos+1] == '-' && src[pos+2] == '-') {
                        advance(); advance(); advance();
                        break;
                    }
                    if (peek() == '\n') line++;
                    advance();
                }
            } else {
                // Single-line comment
                while (peek() != '\n' && peek() != '\0') advance();
            }
        }
    }
    
    Token number() {
        std::string num;
        while (isdigit(peek()) || peek() == '.') num += advance();
        return {TOK_NUMBER, num, line, col};
    }
    
    Token string() {
        std::string str;
        advance(); // Skip opening quote
        while (peek() != '"' && peek() != '\0') {
            if (peek() == '\\') {
                advance();
                if (peek() == 'n') str += '\n';
                else if (peek() == 't') str += '\t';
                else str += peek();
                advance();
            } else {
                str += advance();
            }
        }
        advance(); // Skip closing quote
        return {TOK_STRING, str, line, col};
    }
    
    Token identifier() {
        std::string id;
        if (peek() == '$') id += advance();
        while (isalnum(peek()) || peek() == '_') id += advance();
        
        auto it = keywords.find(id);
        if (it != keywords.end()) return {it->second, id, line, col};
        return {TOK_IDENT, id, line, col};
    }
    
public:
    Lexer(std::string source) : src(source) {}
    
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        
        while (pos < src.size()) {
            skipWhitespace();
            skipComment();
            if (pos >= src.size()) break;
            
            char c = peek();
            
            if (isdigit(c)) tokens.push_back(number());
            else if (c == '"') tokens.push_back(string());
            else if (isalpha(c) || c == '_' || c == '$') tokens.push_back(identifier());
            else if (c == ':' && pos + 1 < src.size() && src[pos + 1] == ':') {
                advance(); advance();
                tokens.push_back({TOK_DCOLON, "::", line, col});
            }
            else if (c == ':') { advance(); tokens.push_back({TOK_COLON, ":", line, col}); }
            else if (c == '=' && pos + 1 < src.size() && src[pos + 1] == '>') {
                advance(); advance();
                tokens.push_back({TOK_ARROW, "=>", line, col});
            }
            else if (c == '=' && pos + 1 < src.size() && src[pos + 1] == '=') {
                advance(); advance();
                tokens.push_back({TOK_EQ, "==", line, col});
            }
            else if (c == '!' && pos + 1 < src.size() && src[pos + 1] == '=') {
                advance(); advance();
                tokens.push_back({TOK_NEQ, "!=", line, col});
            }
            else if (c == '<' && pos + 1 < src.size() && src[pos + 1] == '=') {
                advance(); advance();
                tokens.push_back({TOK_LTE, "<=", line, col});
            }
            else if (c == '>' && pos + 1 < src.size() && src[pos + 1] == '=') {
                advance(); advance();
                tokens.push_back({TOK_GTE, ">=", line, col});
            }
            else if (c == '+' && pos + 1 < src.size() && src[pos + 1] == '+') {
                advance(); advance();
                tokens.push_back({TOK_PLUSPLUS, "++", line, col});
            }
            else if (c == '-' && pos + 1 < src.size() && src[pos + 1] == '-' && !isdigit(src[pos-1])) {
                skipComment();
                continue;
            }
            else if (c == '=') { advance(); tokens.push_back({TOK_ASSIGN, "=", line, col}); }
            else if (c == '+') { advance(); tokens.push_back({TOK_PLUS, "+", line, col}); }
            else if (c == '-') { advance(); tokens.push_back({TOK_MINUS, "-", line, col}); }
            else if (c == '*') { advance(); tokens.push_back({TOK_STAR, "*", line, col}); }
            else if (c == '/') { advance(); tokens.push_back({TOK_SLASH, "/", line, col}); }
            else if (c == '<') { advance(); tokens.push_back({TOK_LT, "<", line, col}); }
            else if (c == '>') { advance(); tokens.push_back({TOK_GT, ">", line, col}); }
            else if (c == '(') { advance(); tokens.push_back({TOK_LPAREN, "(", line, col}); }
            else if (c == ')') { advance(); tokens.push_back({TOK_RPAREN, ")", line, col}); }
            else if (c == '{') { advance(); tokens.push_back({TOK_LBRACE, "{", line, col}); }
            else if (c == '}') { advance(); tokens.push_back({TOK_RBRACE, "}", line, col}); }
            else if (c == '[') { advance(); tokens.push_back({TOK_LBRACK, "[", line, col}); }
            else if (c == ']') { advance(); tokens.push_back({TOK_RBRACK, "]", line, col}); }
            else if (c == ',') { advance(); tokens.push_back({TOK_COMMA, ",", line, col}); }
            else if (c == '.') { advance(); tokens.push_back({TOK_DOT, ".", line, col}); }
            else { advance(); }
        }
        
        tokens.push_back({TOK_EOF, "", line, col});
        return tokens;
    }
};