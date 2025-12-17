#include "../include/ast.h"
#include <string>
#include <sstream>
#include <set>

class CodeGen {
    std::stringstream code;
    int indent = 0;
    std::set<std::string> stringVars;
    
    void emit(std::string s) {
        for (int i = 0; i < indent; i++) code << "  ";
        code << s << "\n";
    }
    
    bool isStringLiteral(std::string s) {
        return s.length() > 0 && s[0] == '"';
    }
    
    bool isStringVar(std::string varName) {
        return stringVars.find(varName) != stringVars.end();
    }
    
    std::string genExpr(ASTNode* node) {
        if (node->type == NODE_LITERAL) {
            return node->value;
        }
        if (node->type == NODE_IDENT) {
            return node->value;
        }
        
        if (node->type == NODE_BINARY_OP) {
            std::string left = genExpr(node->children[0].get());
            std::string right = genExpr(node->children[1].get());
            
            // String concatenation
            if (node->value == "+" && (isStringLiteral(left) || isStringLiteral(right) || 
                isStringVar(node->children[0]->value) || isStringVar(node->children[1]->value))) {
                return "sigma_concat(" + left + ", " + right + ")";
            }
            
            return "(" + left + " " + node->value + " " + right + ")";
        }
        
        if (node->type == NODE_FUNC_CALL) {
            std::string call = node->value + "(";
            for (size_t i = 0; i < node->children.size(); i++) {
                call += genExpr(node->children[i].get());
                if (i < node->children.size() - 1) call += ", ";
            }
            call += ")";
            return call;
        }
        
        if (node->type == NODE_ARRAY) {
            return "NULL";
        }
        
        if (node->type == NODE_MEMBER_ACCESS) {
            return genExpr(node->children[0].get()) + "." + genExpr(node->children[1].get());
        }
        
        return "0";
    }
    
    void genStmt(ASTNode* node) {
        if (node->type == NODE_VAR_DECL) {
            std::string value = genExpr(node->children[0].get());
            if (isStringLiteral(value)) {
                stringVars.insert(node->value);
                emit("char* " + node->value + " = " + value + ";");
            } else if (value == "true" || value == "false") {
                emit("int " + node->value + " = " + (value == "true" ? "1" : "0") + ";");
            } else {
                emit("double " + node->value + " = " + value + ";");
            }
        }
        
        if (node->type == NODE_YAP) {
            std::string expr = genExpr(node->children[0].get());
            
            // Check if it's a string literal
            if (isStringLiteral(expr)) {
                emit("printf(\"%s\\n\", " + expr + ");");
            }
            // Check if it's a string variable
            else if (node->children[0]->type == NODE_IDENT && isStringVar(node->children[0]->value)) {
                emit("printf(\"%s\\n\", " + expr + ");");
            }
            // Check if it's a concatenation result (function call to sigma_concat)
            else if (expr.find("sigma_concat") != std::string::npos) {
                emit("printf(\"%s\\n\", " + expr + ");");
            }
            // Otherwise it's a number
            else {
                emit("printf(\"%.2f\\n\", (double)" + expr + ");");
            }
        }
        
        if (node->type == NODE_RETURN) {
            emit("return " + genExpr(node->children[0].get()) + ";");
        }
        
        if (node->type == NODE_IF) {
            emit("if (" + genExpr(node->children[0].get()) + ") {");
            indent++;
            if (node->children[1]->type == NODE_BLOCK) {
                for (auto& child : node->children[1]->children) genStmt(child.get());
            } else {
                genStmt(node->children[1].get());
            }
            indent--;
            
            for (size_t i = 2; i < node->children.size(); i++) {
                emit("} else {");
                indent++;
                if (node->children[i]->type == NODE_BLOCK) {
                    for (auto& child : node->children[i]->children) genStmt(child.get());
                } else {
                    genStmt(node->children[i].get());
                }
                indent--;
            }
            emit("}");
        }
        
        if (node->type == NODE_FOR) {
            std::string initVar = node->children[0]->value;
            std::string initVal = genExpr(node->children[0]->children[0].get());
            std::string cond = genExpr(node->children[1].get());
            std::string incVar = node->children[2]->children[0]->value;
            
            emit("for (double " + initVar + " = " + initVal + "; " + cond + "; " + incVar + "++) {");
            indent++;
            if (node->children[3]->type == NODE_BLOCK) {
                for (auto& child : node->children[3]->children) genStmt(child.get());
            } else {
                genStmt(node->children[3].get());
            }
            indent--;
            emit("}");
        }
        
        if (node->type == NODE_WHILE) {
            emit("while (" + genExpr(node->children[0].get()) + ") {");
            indent++;
            if (node->children[1]->type == NODE_BLOCK) {
                for (auto& child : node->children[1]->children) genStmt(child.get());
            } else {
                genStmt(node->children[1].get());
            }
            indent--;
            emit("}");
        }
        
        if (node->type == NODE_FUNC_DECL) {
            emit("double " + node->value + "(");
            for (size_t i = 0; i < node->children.size() - 1; i++) {
                code << "double " << node->children[i]->value;
                if (i < node->children.size() - 2) code << ", ";
            }
            code << ") {\n";
            indent++;
            for (auto& stmt : node->children.back()->children) {
                genStmt(stmt.get());
            }
            indent--;
            emit("}");
        }
        
        if (node->type == NODE_UNARY_OP) {
            emit(node->children[0]->value + node->value + ";");
        }
    }
    
public:
    std::string generate(ASTNode* root) {
        code << "#include <stdio.h>\n";
        code << "#include <stdlib.h>\n";
        code << "#include <string.h>\n";
        code << "#include <time.h>\n\n";
        
        // String concatenation helper
        code << "char* sigma_concat(char* a, char* b) {\n";
        code << "  char* result = malloc(strlen(a) + strlen(b) + 1);\n";
        code << "  strcpy(result, a);\n";
        code << "  strcat(result, b);\n";
        code << "  return result;\n";
        code << "}\n\n";
        
        code << "char* sigma_num_to_str(double n) {\n";
        code << "  char* buf = malloc(64);\n";
        code << "  sprintf(buf, \"%.2f\", n);\n";
        code << "  return buf;\n";
        code << "}\n\n";
        
        // Generate functions
        for (auto& child : root->children) {
            if (child->type == NODE_FUNC_DECL) genStmt(child.get());
        }
        
        // Main function
        code << "int main() {\n";
        indent++;
        for (auto& child : root->children) {
            if (child->type != NODE_FUNC_DECL) genStmt(child.get());
        }
        emit("return 0;");
        indent--;
        code << "}\n";
        
        return code.str();
    }
};