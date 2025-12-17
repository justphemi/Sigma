#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>

enum NodeType {
    NODE_PROGRAM, NODE_VAR_DECL, NODE_FUNC_DECL, NODE_FUNC_CALL,
    NODE_IF, NODE_FOR, NODE_WHILE, NODE_RETURN,
    NODE_BINARY_OP, NODE_UNARY_OP, NODE_LITERAL, NODE_IDENT,
    NODE_ARRAY, NODE_OBJECT, NODE_MEMBER_ACCESS, NODE_INDEX_ACCESS,
    NODE_YAP, NODE_BLOCK
};

struct ASTNode {
    NodeType type;
    std::string value;
    std::vector<std::unique_ptr<ASTNode>> children;
    
    ASTNode(NodeType t) : type(t) {}
    ASTNode(NodeType t, std::string v) : type(t), value(v) {}
};

#endif