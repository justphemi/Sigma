#include "../include/ast.h"
#include <string>
#include <sstream>
#include <set>
#include <map>

class CodeGen {
    std::stringstream code;
    int indent = 0;
    std::set<std::string> constants;
    
    void emit(std::string s) {
        for (int i = 0; i < indent; i++) code << "  ";
        code << s << "\n";
    }
    
    std::string genExpr(ASTNode* node) {
        if (node->type == NODE_LITERAL) {
            if (node->value[0] == '"') {
                std::string escaped = node->value;
                std::string result;
                for (char c : escaped) {
                    if (c == '"') result += "\\\"";
                    else result += c;
                }
                return "sigma_make_literal(\"" + result + "\")";
            } else {
                return "sigma_make_literal(\"" + node->value + "\")";
            }
        }
        
        if (node->type == NODE_IDENT) {
            return node->value;
        }
        
        if (node->type == NODE_BINARY_OP) {
            std::string left = genExpr(node->children[0].get());
            std::string right = genExpr(node->children[1].get());
            
            if (node->value == "+") {
                return "sigma_add(" + left + ", " + right + ")";
            } else if (node->value == "-") {
                return "sigma_subtract(" + left + ", " + right + ")";
            } else if (node->value == "*") {
                return "sigma_multiply(" + left + ", " + right + ")";
            } else if (node->value == "/") {
                return "sigma_divide(" + left + ", " + right + ")";
            } else if (node->value == "%") {
                return "sigma_modulo(" + left + ", " + right + ")";
            } else if (node->value == "==") {
                return "sigma_equals(" + left + ", " + right + ")";
            } else if (node->value == "===") {
                return "sigma_strict_equals(" + left + ", " + right + ")";
            } else if (node->value == "!=") {
                return "sigma_not_equals(" + left + ", " + right + ")";
            } else if (node->value == "<") {
                return "sigma_less_than(" + left + ", " + right + ")";
            } else if (node->value == ">") {
                return "sigma_greater_than(" + left + ", " + right + ")";
            } else if (node->value == "<=") {
                return "sigma_less_equal(" + left + ", " + right + ")";
            } else if (node->value == ">=") {
                return "sigma_greater_equal(" + left + ", " + right + ")";
            } else if (node->value == "&&") {
                return "sigma_logical_and(" + left + ", " + right + ")";
            } else if (node->value == "||") {
                return "sigma_logical_or(" + left + ", " + right + ")";
            }
            
            return "sigma_make_nil()";
        }
        
        if (node->type == NODE_FUNC_CALL) {
            // Check for check_type() function
            if (node->value == "check_type" && node->children.size() == 1) {
                return "sigma_type_of(" + genExpr(node->children[0].get()) + ")";
            }
            // Check for to_int() function
            if (node->value == "to_int" && node->children.size() == 1) {
                return "sigma_to_int(" + genExpr(node->children[0].get()) + ")";
            }
            // Check for to_dec() function
            if (node->value == "to_dec" && node->children.size() == 1) {
                return "sigma_to_dec(" + genExpr(node->children[0].get()) + ")";
            }
            // Check for to_str() function
            if (node->value == "to_str" && node->children.size() == 1) {
                return "sigma_to_str(" + genExpr(node->children[0].get()) + ")";
            }
            // Check for random() function
            if (node->value == "random" && node->children.size() == 1) {
                return "sigma_random(" + genExpr(node->children[0].get()) + ")";
            }
            // Check for random_range() function
            if (node->value == "random_range" && node->children.size() == 2) {
                return "sigma_random_range(" + genExpr(node->children[0].get()) + ", " + genExpr(node->children[1].get()) + ")";
            }
            
            std::string call = node->value + "(";
            for (size_t i = 0; i < node->children.size(); i++) {
                call += genExpr(node->children[i].get());
                if (i < node->children.size() - 1) call += ", ";
            }
            call += ")";
            return call;
        }
        
        if (node->type == NODE_ARRAY) {
            std::string arrCode = "sigma_make_array()";
            std::string tempArr = "temp_arr_" + std::to_string(rand());
            emit("SigmaValue " + tempArr + " = " + arrCode + ";");
            for (auto& child : node->children) {
                emit("sigma_array_push(" + tempArr + ", " + genExpr(child.get()) + ");");
            }
            return tempArr;
        }
        
        if (node->type == NODE_OBJECT) {
            std::string objCode = "sigma_make_object()";
            std::string tempObj = "temp_obj_" + std::to_string(rand());
            emit("SigmaValue " + tempObj + " = " + objCode + ";");
            for (auto& child : node->children) {
                std::string key = child->value;
                std::string val = genExpr(child->children[0].get());
                emit("sigma_object_set(" + tempObj + ", \"" + key + "\", " + val + ");");
            }
            return tempObj;
        }
        
        if (node->type == NODE_MEMBER_ACCESS) {
            std::string obj = genExpr(node->children[0].get());
            std::string member = node->children[1]->value;
            
            if (member == "sort" && node->children.size() > 2) {
                std::string order = genExpr(node->children[2].get());
                return "sigma_array_sort(" + obj + ", " + order + ")";
            }
            
            return "sigma_object_get(" + obj + ", \"" + member + "\")";
        }
        
        if (node->type == NODE_INDEX_ACCESS) {
            std::string arr = genExpr(node->children[0].get());
            std::string idx = genExpr(node->children[1].get());
            return "sigma_array_get(" + arr + ", " + idx + ")";
        }
        
        return "sigma_make_nil()";
    }
    
    void genStmt(ASTNode* node) {
        if (node->type == NODE_INPUT) {
            std::string varName = node->value;
            std::string prompt = "";
            
            if (!node->children.empty()) {
                std::string literal = node->children[0]->value;
                if (literal.length() >= 2 && literal[0] == '"' && literal[literal.length()-1] == '"') {
                    prompt = literal.substr(1, literal.length() - 2);
                }
            }
            
            emit("SigmaValue " + varName + " = sigma_input(\"" + prompt + "\");");
        }
        
        if (node->type == NODE_VAR_DECL) {
            bool isConstant = node->value.find("$fixed_") == 0;
            std::string varName = node->value;
            
            if (isConstant) {
                constants.insert(varName);
            }
            
            std::string value = genExpr(node->children[0].get());
            emit("SigmaValue " + varName + " = " + value + ";");
        }
        
        if (node->type == NODE_ASSIGNMENT) {
            std::string varName = node->children[0]->value;
            
            if (constants.count(varName) > 0) {
                emit("sigma_error(\"Cannot reassign constant variable: " + varName + "\");");
                emit("exit(1);");
                return;
            }
            
            if (node->children[0]->type == NODE_MEMBER_ACCESS) {
                std::string obj = genExpr(node->children[0]->children[0].get());
                std::string member = node->children[0]->children[1]->value;
                std::string value = genExpr(node->children[1].get());
                emit("sigma_object_set(" + obj + ", \"" + member + "\", " + value + ");");
            } else if (node->children[0]->type == NODE_INDEX_ACCESS) {
                std::string arr = genExpr(node->children[0]->children[0].get());
                std::string idx = genExpr(node->children[0]->children[1].get());
                std::string value = genExpr(node->children[1].get());
                emit("sigma_array_set(" + arr + ", " + idx + ", " + value + ");");
            } else {
                std::string value = genExpr(node->children[1].get());
                emit(varName + " = " + value + ";");
            }
        }
        
        if (node->type == NODE_YAP) {
            std::string expr = genExpr(node->children[0].get());
            emit("sigma_print(" + expr + ");");
        }
        
        if (node->type == NODE_RETURN) {
            emit("return " + genExpr(node->children[0].get()) + ";");
        }
        
        if (node->type == NODE_IF) {
            emit("if (sigma_is_truthy(" + genExpr(node->children[0].get()) + ")) {");
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
            
            emit("for (SigmaValue " + initVar + " = " + initVal + "; " + 
                 "sigma_is_truthy(" + cond + "); " + incVar + " = sigma_add(" + incVar + ", sigma_make_number(1))) {");
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
            emit("while (sigma_is_truthy(" + genExpr(node->children[0].get()) + ")) {");
            indent++;
            if (node->children[1]->type == NODE_BLOCK) {
                for (auto& child : node->children[1]->children) genStmt(child.get());
            } else {
                genStmt(node->children[1].get());
            }
            indent--;
            emit("}");
        }
        
        if (node->type == NODE_TRY_CATCH) {
            emit("if (1) {");
            indent++;
            for (auto& stmt : node->children[0]->children) {
                genStmt(stmt.get());
            }
            indent--;
            emit("}");
            
            if (node->children.size() > 1) {
                std::string errorVar = node->children[1]->value;
                emit("if (0) {");
                indent++;
                emit("SigmaValue " + errorVar + " = sigma_make_string(\"Error\");");
                for (auto& stmt : node->children[1]->children) {
                    genStmt(stmt.get());
                }
                indent--;
                emit("}");
            }
        }
        
        if (node->type == NODE_FUNC_DECL) {
            emit("SigmaValue " + node->value + "(");
            
            size_t paramCount = node->children.size() - 1;
            for (size_t i = 0; i < paramCount; i++) {
                code << "SigmaValue " << node->children[i]->value;
                if (i < paramCount - 1) code << ", ";
            }
            code << ") {\n";
            indent++;
            for (auto& stmt : node->children.back()->children) {
                genStmt(stmt.get());
            }
            emit("return sigma_make_nil();");
            indent--;
            emit("}");
        }
        
        if (node->type == NODE_UNARY_OP) {
            if (node->value == "++") {
                emit(node->children[0]->value + " = sigma_add(" + node->children[0]->value + ", sigma_make_number(1));");
            } else if (node->value == "--") {
                emit(node->children[0]->value + " = sigma_subtract(" + node->children[0]->value + ", sigma_make_number(1));");
            }
        }
        
        if (node->type == NODE_FUNC_CALL) {
            std::string call = node->value + "(";
            for (size_t i = 0; i < node->children.size(); i++) {
                call += genExpr(node->children[i].get());
                if (i < node->children.size() - 1) call += ", ";
            }
            call += ");";
            emit(call);
        }
    }
    
public:
    std::string generate(ASTNode* root) {
        code << "#include <stdio.h>\n";
        code << "#include <stdlib.h>\n";
        code << "#include <string.h>\n";
        code << "#include <math.h>\n";
        code << "#include <time.h>\n\n";
        
        // Type definitions
        code << "typedef enum {\n";
        code << "  TYPE_NIL,\n";
        code << "  TYPE_NUMBER,\n";
        code << "  TYPE_STRING,\n";
        code << "  TYPE_BOOL,\n";
        code << "  TYPE_ARRAY,\n";
        code << "  TYPE_OBJECT\n";
        code << "} SigmaType;\n\n";
        
        code << "typedef struct {\n";
        code << "  void** items;\n";
        code << "  int size;\n";
        code << "  int capacity;\n";
        code << "} SigmaArray;\n\n";
        
        code << "typedef struct {\n";
        code << "  char** keys;\n";
        code << "  void** values;\n";
        code << "  int size;\n";
        code << "  int capacity;\n";
        code << "} SigmaObject;\n\n";
        
        code << "typedef struct SigmaValue {\n";
        code << "  SigmaType type;\n";
        code << "  union {\n";
        code << "    double number;\n";
        code << "    char* string;\n";
        code << "    int boolean;\n";
        code << "    SigmaArray* array;\n";
        code << "    SigmaObject* object;\n";
        code << "  } as;\n";
        code << "} SigmaValue;\n\n";
        
        // Forward declarations
        code << "SigmaValue sigma_make_nil();\n";
        code << "SigmaValue sigma_make_number(double n);\n";
        code << "SigmaValue sigma_make_bool(int b);\n";
        code << "SigmaValue sigma_make_string(const char* s);\n";
        code << "SigmaValue sigma_make_literal(const char* literal);\n";
        code << "int sigma_is_truthy(SigmaValue v);\n";
        code << "void sigma_print(SigmaValue v);\n";
        code << "void sigma_error(const char* msg);\n";
        code << "SigmaValue sigma_input(const char* prompt);\n";
        code << "SigmaValue sigma_type_of(SigmaValue v);\n";
        code << "SigmaValue sigma_to_int(SigmaValue v);\n";
        code << "SigmaValue sigma_to_dec(SigmaValue v);\n";
        code << "SigmaValue sigma_to_str(SigmaValue v);\n";
        code << "SigmaValue sigma_random(SigmaValue digits);\n";
        code << "SigmaValue sigma_random_range(SigmaValue min, SigmaValue max);\n\n";
        
        // Basic functions
        code << "void sigma_error(const char* msg) {\n";
        code << "  fprintf(stderr, \"Error: %s\\n\", msg);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_make_nil() {\n";
        code << "  SigmaValue v; v.type = TYPE_NIL; return v;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_make_number(double n) {\n";
        code << "  SigmaValue v; v.type = TYPE_NUMBER; v.as.number = n; return v;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_make_bool(int b) {\n";
        code << "  SigmaValue v; v.type = TYPE_BOOL; v.as.boolean = b; return v;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_make_string(const char* s) {\n";
        code << "  SigmaValue v; v.type = TYPE_STRING;\n";
        code << "  v.as.string = malloc(strlen(s) + 1);\n";
        code << "  strcpy(v.as.string, s);\n";
        code << "  return v;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_make_literal(const char* literal) {\n";
        code << "  if (literal[0] == '\"') {\n";
        code << "    char* str = malloc(strlen(literal) - 1);\n";
        code << "    strncpy(str, literal + 1, strlen(literal) - 2);\n";
        code << "    str[strlen(literal) - 2] = '\\0';\n";
        code << "    return sigma_make_string(str);\n";
        code << "  } else if (strcmp(literal, \"true\") == 0) {\n";
        code << "    return sigma_make_bool(1);\n";
        code << "  } else if (strcmp(literal, \"false\") == 0) {\n";
        code << "    return sigma_make_bool(0);\n";
        code << "  } else {\n";
        code << "    return sigma_make_number(atof(literal));\n";
        code << "  }\n";
        code << "}\n\n";
        
        // Input function
        code << "SigmaValue sigma_input(const char* prompt) {\n";
        code << "  if (prompt && strlen(prompt) > 0) {\n";
        code << "    printf(\"%s\", prompt);\n";
        code << "    fflush(stdout);\n";
        code << "  }\n";
        code << "  char buffer[1024];\n";
        code << "  if (fgets(buffer, sizeof(buffer), stdin) == NULL) {\n";
        code << "    return sigma_make_string(\"\");\n";
        code << "  }\n";
        code << "  size_t len = strlen(buffer);\n";
        code << "  if (len > 0 && buffer[len-1] == '\\n') {\n";
        code << "    buffer[len-1] = '\\0';\n";
        code << "  }\n";
        code << "  return sigma_make_string(buffer);\n";
        code << "}\n\n";
        
        // Type function
        code << "SigmaValue sigma_type_of(SigmaValue v) {\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NIL: return sigma_make_string(\"nil\");\n";
        code << "    case TYPE_NUMBER: {\n";
        code << "      if (v.as.number == floor(v.as.number)) {\n";
        code << "        return sigma_make_string(\"int\");\n";
        code << "      } else {\n";
        code << "        return sigma_make_string(\"dec\");\n";
        code << "      }\n";
        code << "    }\n";
        code << "    case TYPE_STRING: return sigma_make_string(\"str\");\n";
        code << "    case TYPE_BOOL: return sigma_make_string(\"bool\");\n";
        code << "    case TYPE_ARRAY: return sigma_make_string(\"arr\");\n";
        code << "    case TYPE_OBJECT: return sigma_make_string(\"obj\");\n";
        code << "    default: return sigma_make_string(\"unknown\");\n";
        code << "  }\n";
        code << "}\n\n";
        
        // Type conversion functions
        code << "SigmaValue sigma_to_int(SigmaValue v) {\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NUMBER:\n";
        code << "      return sigma_make_number(floor(v.as.number));\n";
        code << "    case TYPE_STRING: {\n";
        code << "      double num = atof(v.as.string);\n";
        code << "      return sigma_make_number(floor(num));\n";
        code << "    }\n";
        code << "    case TYPE_BOOL:\n";
        code << "      return sigma_make_number(v.as.boolean ? 1.0 : 0.0);\n";
        code << "    default:\n";
        code << "      return sigma_make_number(0.0);\n";
        code << "  }\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_to_dec(SigmaValue v) {\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NUMBER:\n";
        code << "      return v;\n";
        code << "    case TYPE_STRING:\n";
        code << "      return sigma_make_number(atof(v.as.string));\n";
        code << "    case TYPE_BOOL:\n";
        code << "      return sigma_make_number(v.as.boolean ? 1.0 : 0.0);\n";
        code << "    default:\n";
        code << "      return sigma_make_number(0.0);\n";
        code << "  }\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_to_str(SigmaValue v) {\n";
        code << "  char buffer[64];\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NIL:\n";
        code << "      return sigma_make_string(\"nil\");\n";
        code << "    case TYPE_NUMBER:\n";
        code << "      if (v.as.number == floor(v.as.number)) {\n";
        code << "        sprintf(buffer, \"%.0f\", v.as.number);\n";
        code << "      } else {\n";
        code << "        sprintf(buffer, \"%g\", v.as.number);\n";
        code << "      }\n";
        code << "      return sigma_make_string(buffer);\n";
        code << "    case TYPE_STRING:\n";
        code << "      return v;\n";
        code << "    case TYPE_BOOL:\n";
        code << "      return sigma_make_string(v.as.boolean ? \"true\" : \"false\");\n";
        code << "    case TYPE_ARRAY:\n";
        code << "      return sigma_make_string(\"[array]\");\n";
        code << "    case TYPE_OBJECT:\n";
        code << "      return sigma_make_string(\"[object]\");\n";
        code << "    default:\n";
        code << "      return sigma_make_string(\"unknown\");\n";
        code << "  }\n";
        code << "}\n\n";
        
        // Random number functions
        code << "SigmaValue sigma_random(SigmaValue digits) {\n";
        code << "  static int seeded = 0;\n";
        code << "  if (!seeded) {\n";
        code << "    srand(time(NULL));\n";
        code << "    seeded = 1;\n";
        code << "  }\n";
        code << "  int d = (int)digits.as.number;\n";
        code << "  if (d <= 0) d = 1;\n";
        code << "  if (d > 9) d = 9;\n";
        code << "  int min = 1;\n";
        code << "  int max = 9;\n";
        code << "  for (int i = 1; i < d; i++) {\n";
        code << "    min *= 10;\n";
        code << "    max = max * 10 + 9;\n";
        code << "  }\n";
        code << "  int result = min + rand() % (max - min + 1);\n";
        code << "  return sigma_make_number((double)result);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_random_range(SigmaValue min_val, SigmaValue max_val) {\n";
        code << "  static int seeded = 0;\n";
        code << "  if (!seeded) {\n";
        code << "    srand(time(NULL));\n";
        code << "    seeded = 1;\n";
        code << "  }\n";
        code << "  int min = (int)min_val.as.number;\n";
        code << "  int max = (int)max_val.as.number;\n";
        code << "  if (min > max) {\n";
        code << "    int temp = min;\n";
        code << "    min = max;\n";
        code << "    max = temp;\n";
        code << "  }\n";
        code << "  int result = min + rand() % (max - min + 1);\n";
        code << "  return sigma_make_number((double)result);\n";
        code << "}\n\n";
        
        // Array functions
        code << "SigmaValue sigma_make_array() {\n";
        code << "  SigmaValue v;\n";
        code << "  v.type = TYPE_ARRAY;\n";
        code << "  v.as.array = malloc(sizeof(SigmaArray));\n";
        code << "  v.as.array->items = malloc(sizeof(void*) * 10);\n";
        code << "  v.as.array->size = 0;\n";
        code << "  v.as.array->capacity = 10;\n";
        code << "  return v;\n";
        code << "}\n\n";
        
        code << "void sigma_array_push(SigmaValue arr, SigmaValue val) {\n";
        code << "  if (arr.type != TYPE_ARRAY) return;\n";
        code << "  if (arr.as.array->size >= arr.as.array->capacity) {\n";
        code << "    arr.as.array->capacity *= 2;\n";
        code << "    arr.as.array->items = realloc(arr.as.array->items, sizeof(void*) * arr.as.array->capacity);\n";
        code << "  }\n";
        code << "  SigmaValue* newVal = malloc(sizeof(SigmaValue));\n";
        code << "  *newVal = val;\n";
        code << "  arr.as.array->items[arr.as.array->size++] = newVal;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_array_get(SigmaValue arr, SigmaValue idx) {\n";
        code << "  if (arr.type != TYPE_ARRAY || idx.type != TYPE_NUMBER) return sigma_make_nil();\n";
        code << "  int i = (int)idx.as.number;\n";
        code << "  if (i < 0 || i >= arr.as.array->size) return sigma_make_nil();\n";
        code << "  return *(SigmaValue*)arr.as.array->items[i];\n";
        code << "}\n\n";
        
        code << "void sigma_array_set(SigmaValue arr, SigmaValue idx, SigmaValue val) {\n";
        code << "  if (arr.type != TYPE_ARRAY || idx.type != TYPE_NUMBER) return;\n";
        code << "  int i = (int)idx.as.number;\n";
        code << "  if (i < 0 || i >= arr.as.array->size) return;\n";
        code << "  *(SigmaValue*)arr.as.array->items[i] = val;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_array_sort(SigmaValue arr, SigmaValue order) {\n";
        code << "  if (arr.type != TYPE_ARRAY) return arr;\n";
        code << "  int ascending = 1;\n";
        code << "  if (order.type == TYPE_STRING && strcmp(order.as.string, \"desc\") == 0) {\n";
        code << "    ascending = 0;\n";
        code << "  }\n";
        code << "  for (int i = 0; i < arr.as.array->size - 1; i++) {\n";
        code << "    for (int j = 0; j < arr.as.array->size - i - 1; j++) {\n";
        code << "      SigmaValue* a = (SigmaValue*)arr.as.array->items[j];\n";
        code << "      SigmaValue* b = (SigmaValue*)arr.as.array->items[j + 1];\n";
        code << "      int shouldSwap = 0;\n";
        code << "      if (ascending && a->type == TYPE_NUMBER && b->type == TYPE_NUMBER && a->as.number > b->as.number) shouldSwap = 1;\n";
        code << "      if (!ascending && a->type == TYPE_NUMBER && b->type == TYPE_NUMBER && a->as.number < b->as.number) shouldSwap = 1;\n";
        code << "      if (shouldSwap) {\n";
        code << "        void* temp = arr.as.array->items[j];\n";
        code << "        arr.as.array->items[j] = arr.as.array->items[j + 1];\n";
        code << "        arr.as.array->items[j + 1] = temp;\n";
        code << "      }\n";
        code << "    }\n";
        code << "  }\n";
        code << "  return arr;\n";
        code << "}\n\n";
        
        // Object functions
        code << "SigmaValue sigma_make_object() {\n";
        code << "  SigmaValue v;\n";
        code << "  v.type = TYPE_OBJECT;\n";
        code << "  v.as.object = malloc(sizeof(SigmaObject));\n";
        code << "  v.as.object->keys = malloc(sizeof(char*) * 10);\n";
        code << "  v.as.object->values = malloc(sizeof(void*) * 10);\n";
        code << "  v.as.object->size = 0;\n";
        code << "  v.as.object->capacity = 10;\n";
        code << "  return v;\n";
        code << "}\n\n";
        
        code << "void sigma_object_set(SigmaValue obj, const char* key, SigmaValue val) {\n";
        code << "  if (obj.type != TYPE_OBJECT) return;\n";
        code << "  for (int i = 0; i < obj.as.object->size; i++) {\n";
        code << "    if (strcmp(obj.as.object->keys[i], key) == 0) {\n";
        code << "      *(SigmaValue*)obj.as.object->values[i] = val;\n";
        code << "      return;\n";
        code << "    }\n";
        code << "  }\n";
        code << "  if (obj.as.object->size >= obj.as.object->capacity) {\n";
        code << "    obj.as.object->capacity *= 2;\n";
        code << "    obj.as.object->keys = realloc(obj.as.object->keys, sizeof(char*) * obj.as.object->capacity);\n";
        code << "    obj.as.object->values = realloc(obj.as.object->values, sizeof(void*) * obj.as.object->capacity);\n";
        code << "  }\n";
        code << "  obj.as.object->keys[obj.as.object->size] = malloc(strlen(key) + 1);\n";
        code << "  strcpy(obj.as.object->keys[obj.as.object->size], key);\n";
        code << "  SigmaValue* newVal = malloc(sizeof(SigmaValue));\n";
        code << "  *newVal = val;\n";
        code << "  obj.as.object->values[obj.as.object->size++] = newVal;\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_object_get(SigmaValue obj, const char* key) {\n";
        code << "  if (obj.type != TYPE_OBJECT) return sigma_make_nil();\n";
        code << "  for (int i = 0; i < obj.as.object->size; i++) {\n";
        code << "    if (strcmp(obj.as.object->keys[i], key) == 0) {\n";
        code << "      return *(SigmaValue*)obj.as.object->values[i];\n";
        code << "    }\n";
        code << "  }\n";
        code << "  return sigma_make_nil();\n";
        code << "}\n\n";
        
        // Arithmetic operations
        code << "SigmaValue sigma_add(SigmaValue a, SigmaValue b) {\n";
        code << "  if (a.type == TYPE_STRING || b.type == TYPE_STRING) {\n";
        code << "    char a_buf[64], b_buf[64];\n";
        code << "    char* a_str = a_buf;\n";
        code << "    char* b_str = b_buf;\n";
        code << "    if (a.type == TYPE_STRING) {\n";
        code << "      a_str = a.as.string;\n";
        code << "    } else if (a.type == TYPE_NUMBER) {\n";
        code << "      sprintf(a_buf, \"%g\", a.as.number);\n";
        code << "    } else if (a.type == TYPE_BOOL) {\n";
        code << "      strcpy(a_buf, a.as.boolean ? \"true\" : \"false\");\n";
        code << "    } else {\n";
        code << "      strcpy(a_buf, \"nil\");\n";
        code << "    }\n";
        code << "    if (b.type == TYPE_STRING) {\n";
        code << "      b_str = b.as.string;\n";
        code << "    } else if (b.type == TYPE_NUMBER) {\n";
        code << "      sprintf(b_buf, \"%g\", b.as.number);\n";
        code << "    } else if (b.type == TYPE_BOOL) {\n";
        code << "      strcpy(b_buf, b.as.boolean ? \"true\" : \"false\");\n";
        code << "    } else {\n";
        code << "      strcpy(b_buf, \"nil\");\n";
        code << "    }\n";
        code << "    char* result = malloc(strlen(a_str) + strlen(b_str) + 1);\n";
        code << "    strcpy(result, a_str);\n";
        code << "    strcat(result, b_str);\n";
        code << "    return sigma_make_string(result);\n";
        code << "  }\n";
        code << "  return sigma_make_number(a.as.number + b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_subtract(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_number(a.as.number - b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_multiply(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_number(a.as.number * b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_divide(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_number(a.as.number / b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_modulo(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_number(fmod(a.as.number, b.as.number));\n";
        code << "}\n\n";
        
        // Comparison and logical operations
        code << "SigmaValue sigma_equals(SigmaValue a, SigmaValue b) {\n";
        code << "  if (a.type != b.type) return sigma_make_bool(0);\n";
        code << "  if (a.type == TYPE_NUMBER) return sigma_make_bool(a.as.number == b.as.number);\n";
        code << "  if (a.type == TYPE_STRING) return sigma_make_bool(strcmp(a.as.string, b.as.string) == 0);\n";
        code << "  if (a.type == TYPE_BOOL) return sigma_make_bool(a.as.boolean == b.as.boolean);\n";
        code << "  return sigma_make_bool(0);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_strict_equals(SigmaValue a, SigmaValue b) {\n";
        code << "  if (a.type != b.type) return sigma_make_bool(0);\n";
        code << "  if (a.type == TYPE_NUMBER) return sigma_make_bool(a.as.number == b.as.number);\n";
        code << "  if (a.type == TYPE_STRING) return sigma_make_bool(strcmp(a.as.string, b.as.string) == 0);\n";
        code << "  if (a.type == TYPE_BOOL) return sigma_make_bool(a.as.boolean == b.as.boolean);\n";
        code << "  return sigma_make_bool(0);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_not_equals(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(!sigma_is_truthy(sigma_equals(a, b)));\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_less_than(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(a.as.number < b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_greater_than(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(a.as.number > b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_less_equal(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(a.as.number <= b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_greater_equal(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(a.as.number >= b.as.number);\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_logical_and(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(sigma_is_truthy(a) && sigma_is_truthy(b));\n";
        code << "}\n\n";
        
        code << "SigmaValue sigma_logical_or(SigmaValue a, SigmaValue b) {\n";
        code << "  return sigma_make_bool(sigma_is_truthy(a) || sigma_is_truthy(b));\n";
        code << "}\n\n";
        
        // Truthiness and printing
        code << "int sigma_is_truthy(SigmaValue v) {\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NIL: return 0;\n";
        code << "    case TYPE_BOOL: return v.as.boolean;\n";
        code << "    case TYPE_NUMBER: return v.as.number != 0;\n";
        code << "    case TYPE_STRING: return strlen(v.as.string) > 0;\n";
        code << "    default: return 1;\n";
        code << "  }\n";
        code << "}\n\n";
        
        code << "void sigma_print(SigmaValue v) {\n";
        code << "  switch (v.type) {\n";
        code << "    case TYPE_NIL: printf(\"nil\\n\"); break;\n";
        code << "    case TYPE_NUMBER: {\n";
        code << "      double num = v.as.number;\n";
        code << "      if (num == floor(num)) {\n";
        code << "        printf(\"%g\\n\", num);\n";
        code << "      } else {\n";
        code << "        printf(\"%.2f\\n\", num);\n";
        code << "      }\n";
        code << "      break;\n";
        code << "    }\n";
        code << "    case TYPE_STRING: printf(\"%s\\n\", v.as.string); break;\n";
        code << "    case TYPE_BOOL: printf(\"%s\\n\", v.as.boolean ? \"true\" : \"false\"); break;\n";
        code << "    case TYPE_ARRAY: {\n";
        code << "      printf(\"[\");\n";
        code << "      for (int i = 0; i < v.as.array->size; i++) {\n";
        code << "        SigmaValue elem = *(SigmaValue*)v.as.array->items[i];\n";
        code << "        if (elem.type == TYPE_NUMBER) printf(\"%g\", elem.as.number);\n";
        code << "        else if (elem.type == TYPE_STRING) printf(\"\\\"%s\\\"\", elem.as.string);\n";
        code << "        if (i < v.as.array->size - 1) printf(\", \");\n";
        code << "      }\n";
        code << "      printf(\"]\\n\");\n";
        code << "      break;\n";
        code << "    }\n";
        code << "    case TYPE_OBJECT: printf(\"<object>\\n\"); break;\n";
        code << "    default: printf(\"<unknown>\\n\"); break;\n";
        code << "  }\n";
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