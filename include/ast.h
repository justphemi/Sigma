#pragma once
#include <string>
#include <vector>
#include <memory>

enum ASTNodeType {
    NODE_PROGRAM,
    NODE_LITERAL,
    NODE_IDENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_VAR_DECL,
    NODE_ASSIGNMENT,
    NODE_FUNC_DECL,
    NODE_FUNC_CALL,
    NODE_RETURN,
    NODE_IF,
    NODE_FOR,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_YAP,
    NODE_ARRAY,
    NODE_OBJECT,
    NODE_MEMBER_ACCESS,
    NODE_INDEX_ACCESS,
    NODE_TRY_CATCH,
    NODE_INPUT
};

struct ASTNode {
    ASTNodeType type;
    std::string value;
    std::vector<std::unique_ptr<ASTNode>> children;
    
    ASTNode(ASTNodeType t, std::string v = "") : type(t), value(v) {}
};