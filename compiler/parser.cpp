#include "../include/ast.h"
#include "../include/token.h"
#include <vector>
#include <memory>
#include <stdexcept>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    
    Token peek() { return tokens[pos]; }
    Token advance() { return tokens[pos++]; }
    bool check(TokenType type) { return peek().type == type; }
    Token expect(TokenType type) {
        if (!check(type)) throw std::runtime_error("Unexpected token");
        return advance();
    }
    
    std::unique_ptr<ASTNode> parsePrimary() {
        if (check(TOK_NUMBER)) {
            auto node = std::make_unique<ASTNode>(NODE_LITERAL, advance().value);
            return node;
        }
        if (check(TOK_STRING)) {
            auto node = std::make_unique<ASTNode>(NODE_LITERAL, "\"" + advance().value + "\"");
            return node;
        }
        if (check(TOK_TRUE) || check(TOK_FALSE)) {
            auto node = std::make_unique<ASTNode>(NODE_LITERAL, advance().value);
            return node;
        }
        if (check(TOK_LBRACK)) {
            auto node = std::make_unique<ASTNode>(NODE_ARRAY);
            advance();
            while (!check(TOK_RBRACK)) {
                node->children.push_back(parseExpression());
                if (check(TOK_COMMA)) advance();
            }
            expect(TOK_RBRACK);
            return node;
        }
        if (check(TOK_LBRACE)) {
            auto node = std::make_unique<ASTNode>(NODE_OBJECT);
            advance();
            while (!check(TOK_RBRACE)) {
                auto key = expect(TOK_IDENT).value;
                expect(TOK_DCOLON);
                auto val = parseExpression();
                auto pair = std::make_unique<ASTNode>(NODE_LITERAL, key);
                pair->children.push_back(std::move(val));
                node->children.push_back(std::move(pair));
                if (check(TOK_COMMA)) advance();
            }
            expect(TOK_RBRACE);
            return node;
        }
        if (check(TOK_IDENT)) {
            auto name = advance().value;
            
            // Check for function call (built-in functions like check_type, to_int, etc.)
            if (check(TOK_LPAREN)) {
                auto call = std::make_unique<ASTNode>(NODE_FUNC_CALL, name);
                advance(); // (
                while (!check(TOK_RPAREN)) {
                    call->children.push_back(parseExpression());
                    if (check(TOK_COMMA)) advance();
                }
                expect(TOK_RPAREN);
                return call;
            }
            
            auto node = std::make_unique<ASTNode>(NODE_IDENT, name);
            
            while (true) {
                if (check(TOK_DOT)) {
                    advance();
                    if (check(TOK_IDENT)) {
                        std::string method = advance().value;
                        if (method == "run" && check(TOK_LPAREN)) {
                            auto call = std::make_unique<ASTNode>(NODE_FUNC_CALL, name);
                            advance();
                            while (!check(TOK_RPAREN)) {
                                call->children.push_back(parseExpression());
                                if (check(TOK_COMMA)) advance();
                            }
                            expect(TOK_RPAREN);
                            return call;
                        } else if (method == "sort" && check(TOK_LPAREN)) {
                            // Array sort method
                            auto access = std::make_unique<ASTNode>(NODE_MEMBER_ACCESS);
                            access->children.push_back(std::move(node));
                            access->children.push_back(std::make_unique<ASTNode>(NODE_IDENT, method));
                            advance(); // (
                            if (!check(TOK_RPAREN)) {
                                access->children.push_back(parseExpression());
                            }
                            expect(TOK_RPAREN);
                            return access;
                        } else {
                            auto access = std::make_unique<ASTNode>(NODE_MEMBER_ACCESS);
                            access->children.push_back(std::move(node));
                            access->children.push_back(std::make_unique<ASTNode>(NODE_IDENT, method));
                            node = std::move(access);
                        }
                    }
                } else if (check(TOK_LBRACK)) {
                    advance();
                    auto idx = parseExpression();
                    expect(TOK_RBRACK);
                    auto access = std::make_unique<ASTNode>(NODE_INDEX_ACCESS);
                    access->children.push_back(std::move(node));
                    access->children.push_back(std::move(idx));
                    node = std::move(access);
                } else {
                    break;
                }
            }
            return node;
        }
        if (check(TOK_LPAREN)) {
            advance();
            auto node = parseExpression();
            expect(TOK_RPAREN);
            return node;
        }
        throw std::runtime_error("Unexpected token in expression");
    }
    
    std::unique_ptr<ASTNode> parseTerm() {
        auto left = parsePrimary();
        while (check(TOK_STAR) || check(TOK_SLASH)) {
            auto op = advance().value;
            auto right = parsePrimary();
            auto node = std::make_unique<ASTNode>(NODE_BINARY_OP, op);
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            left = std::move(node);
        }
        return left;
    }
    
    std::unique_ptr<ASTNode> parseExpression() {
        auto left = parseTerm();
        while (check(TOK_PLUS) || check(TOK_MINUS)) {
            auto op = advance().value;
            auto right = parseTerm();
            auto node = std::make_unique<ASTNode>(NODE_BINARY_OP, op);
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            left = std::move(node);
        }
        return left;
    }
    
    std::unique_ptr<ASTNode> parseComparison() {
        auto left = parseExpression();
        while (check(TOK_LT) || check(TOK_GT) || check(TOK_LTE) || 
               check(TOK_GTE) || check(TOK_EQ) || check(TOK_STRICT_EQ) || check(TOK_NEQ)) {
            auto op = advance().value;
            auto right = parseExpression();
            auto node = std::make_unique<ASTNode>(NODE_BINARY_OP, op);
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            left = std::move(node);
        }
        return left;
    }
    
    std::unique_ptr<ASTNode> parseLogical() {
        auto left = parseComparison();
        while (check(TOK_AND) || check(TOK_OR)) {
            auto op = advance().value;
            auto right = parseComparison();
            auto node = std::make_unique<ASTNode>(NODE_BINARY_OP, op);
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            left = std::move(node);
        }
        return left;
    }
    
    std::unique_ptr<ASTNode> parseStatement() {
        if (check(TOK_IN)) {
            advance();
            auto varName = expect(TOK_IDENT).value;
            expect(TOK_COLON);
            auto node = std::make_unique<ASTNode>(NODE_INPUT, varName);
            
            // Require prompt string
            if (check(TOK_STRING)) {
                node->children.push_back(std::make_unique<ASTNode>(NODE_LITERAL, "\"" + advance().value + "\""));
            } else {
                throw std::runtime_error("Expected prompt string after $in");
            }
            
            return node;
        }
        
        if (check(TOK_YAP)) {
            advance();
            auto node = std::make_unique<ASTNode>(NODE_YAP);
            expect(TOK_LPAREN);
            node->children.push_back(parseExpression());
            expect(TOK_RPAREN);
            return node;
        }
        
        if (check(TOK_RETURN)) {
            advance();
            auto node = std::make_unique<ASTNode>(NODE_RETURN);
            node->children.push_back(parseExpression());
            return node;
        }
        
        if (check(TOK_TRY)) {
            advance();
            expect(TOK_DCOLON);
            expect(TOK_LBRACE);
            
            auto node = std::make_unique<ASTNode>(NODE_TRY_CATCH);
            auto tryBlock = std::make_unique<ASTNode>(NODE_BLOCK);
            while (!check(TOK_RBRACE)) {
                tryBlock->children.push_back(parseStatement());
            }
            expect(TOK_RBRACE);
            node->children.push_back(std::move(tryBlock));
            
            if (check(TOK_CATCH)) {
                advance();
                expect(TOK_LPAREN);
                auto errorVar = expect(TOK_IDENT).value;
                expect(TOK_RPAREN);
                expect(TOK_DCOLON);
                expect(TOK_LBRACE);
                
                auto catchBlock = std::make_unique<ASTNode>(NODE_BLOCK, errorVar);
                while (!check(TOK_RBRACE)) {
                    catchBlock->children.push_back(parseStatement());
                }
                expect(TOK_RBRACE);
                node->children.push_back(std::move(catchBlock));
            }
            return node;
        }
        
        if (check(TOK_IF)) {
            advance();
            auto node = std::make_unique<ASTNode>(NODE_IF);
            node->children.push_back(parseLogical());
            expect(TOK_DCOLON);
            if (check(TOK_LBRACE)) {
                advance();
                auto block = std::make_unique<ASTNode>(NODE_BLOCK);
                while (!check(TOK_RBRACE)) block->children.push_back(parseStatement());
                expect(TOK_RBRACE);
                node->children.push_back(std::move(block));
            } else {
                node->children.push_back(parseStatement());
            }
            
            while (check(TOK_EL)) {
                advance();
                expect(TOK_DCOLON);
                if (check(TOK_LBRACE)) {
                    advance();
                    auto block = std::make_unique<ASTNode>(NODE_BLOCK);
                    while (!check(TOK_RBRACE)) block->children.push_back(parseStatement());
                    expect(TOK_RBRACE);
                    node->children.push_back(std::move(block));
                } else {
                    node->children.push_back(parseStatement());
                }
            }
            return node;
        }
        
        if (check(TOK_FOR)) {
            advance();
            expect(TOK_LPAREN);
            auto init = parseStatement();
            expect(TOK_COMMA);
            auto cond = parseLogical();
            expect(TOK_COMMA);
            auto inc = parseStatement();
            expect(TOK_RPAREN);
            expect(TOK_DCOLON);
            
            auto node = std::make_unique<ASTNode>(NODE_FOR);
            node->children.push_back(std::move(init));
            node->children.push_back(std::move(cond));
            node->children.push_back(std::move(inc));
            
            if (check(TOK_LBRACE)) {
                advance();
                auto block = std::make_unique<ASTNode>(NODE_BLOCK);
                while (!check(TOK_RBRACE)) block->children.push_back(parseStatement());
                expect(TOK_RBRACE);
                node->children.push_back(std::move(block));
            } else {
                node->children.push_back(parseStatement());
            }
            return node;
        }
        
        if (check(TOK_WHILE)) {
            advance();
            auto node = std::make_unique<ASTNode>(NODE_WHILE);
            node->children.push_back(parseLogical());
            expect(TOK_DCOLON);
            if (check(TOK_LBRACE)) {
                advance();
                auto block = std::make_unique<ASTNode>(NODE_BLOCK);
                while (!check(TOK_RBRACE)) block->children.push_back(parseStatement());
                expect(TOK_RBRACE);
                node->children.push_back(std::move(block));
            } else {
                node->children.push_back(parseStatement());
            }
            return node;
        }
        
        if (check(TOK_FN)) {
            advance();
            auto name = expect(TOK_IDENT).value;
            expect(TOK_COLON);
            expect(TOK_LPAREN);
            auto node = std::make_unique<ASTNode>(NODE_FUNC_DECL, name);
            while (!check(TOK_RPAREN)) {
                node->children.push_back(std::make_unique<ASTNode>(NODE_IDENT, expect(TOK_IDENT).value));
                if (check(TOK_COMMA)) advance();
            }
            expect(TOK_RPAREN);
            expect(TOK_LBRACE);
            auto body = std::make_unique<ASTNode>(NODE_BLOCK);
            while (!check(TOK_RBRACE)) body->children.push_back(parseStatement());
            expect(TOK_RBRACE);
            node->children.push_back(std::move(body));
            return node;
        }
        
        if (check(TOK_FIXED)) {
            advance();
            auto name = expect(TOK_IDENT).value;
            expect(TOK_COLON);
            auto node = std::make_unique<ASTNode>(NODE_VAR_DECL, "$fixed_" + name);
            node->children.push_back(parseExpression());
            return node;
        }
        
        if (check(TOK_IDENT)) {
            auto name = advance().value;
            
            // Check for direct function call (e.g., check_type(x), to_int(x))
            if (check(TOK_LPAREN)) {
                auto call = std::make_unique<ASTNode>(NODE_FUNC_CALL, name);
                advance(); // (
                while (!check(TOK_RPAREN)) {
                    call->children.push_back(parseExpression());
                    if (check(TOK_COMMA)) advance();
                }
                expect(TOK_RPAREN);
                return call;
            }
            
            // Check for function call with .run() or .sort()
            if (check(TOK_DOT)) {
                size_t savedPos = pos;
                advance();
                if (check(TOK_IDENT)) {
                    std::string method = advance().value;
                    if ((method == "run" || method == "sort") && check(TOK_LPAREN)) {
                        auto call = std::make_unique<ASTNode>(NODE_FUNC_CALL, name);
                        advance();
                        while (!check(TOK_RPAREN)) {
                            call->children.push_back(parseExpression());
                            if (check(TOK_COMMA)) advance();
                        }
                        expect(TOK_RPAREN);
                        return call;
                    }
                }
                pos = savedPos;
            }
            
            // Check for assignment (property or array element)
            if (check(TOK_DOT) || check(TOK_LBRACK)) {
                auto lhs = std::make_unique<ASTNode>(NODE_IDENT, name);
                
                // Parse member/index access chain
                while (check(TOK_DOT) || check(TOK_LBRACK)) {
                    if (check(TOK_DOT)) {
                        advance();
                        auto member = expect(TOK_IDENT).value;
                        auto access = std::make_unique<ASTNode>(NODE_MEMBER_ACCESS);
                        access->children.push_back(std::move(lhs));
                        access->children.push_back(std::make_unique<ASTNode>(NODE_IDENT, member));
                        lhs = std::move(access);
                    } else {
                        advance(); // [
                        auto idx = parseExpression();
                        expect(TOK_RBRACK);
                        auto access = std::make_unique<ASTNode>(NODE_INDEX_ACCESS);
                        access->children.push_back(std::move(lhs));
                        access->children.push_back(std::move(idx));
                        lhs = std::move(access);
                    }
                }
                
                if (check(TOK_COLON)) {
                    advance();
                    auto node = std::make_unique<ASTNode>(NODE_ASSIGNMENT);
                    node->children.push_back(std::move(lhs));
                    node->children.push_back(parseExpression());
                    return node;
                }
            }
            
            // Check for variable declaration
            if (check(TOK_COLON)) {
                advance();
                auto node = std::make_unique<ASTNode>(NODE_VAR_DECL, name);
                node->children.push_back(parseExpression());
                return node;
            }
            
            // Check for increment
            if (check(TOK_PLUSPLUS)) {
                advance();
                auto node = std::make_unique<ASTNode>(NODE_UNARY_OP, "++");
                node->children.push_back(std::make_unique<ASTNode>(NODE_IDENT, name));
                return node;
            }
        }
        
        throw std::runtime_error("Unknown statement");
    }
    
public:
    Parser(std::vector<Token> toks) : tokens(toks) {}
    
    std::unique_ptr<ASTNode> parse() {
        auto root = std::make_unique<ASTNode>(NODE_PROGRAM);
        while (!check(TOK_EOF)) {
            root->children.push_back(parseStatement());
        }
        return root;
    }
};