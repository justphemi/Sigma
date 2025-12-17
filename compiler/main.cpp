#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "lexer.cpp"
#include "parser.cpp"
#include "codegen.cpp"

std::string readFile(std::string path) {
    std::ifstream file(path);
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

void writeFile(std::string path, std::string content) {
    std::ofstream file(path);
    file << content;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: sig <file.sgm>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    std::string source = readFile(filename);
    
    try {
        // Lexing
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        // Parsing
        Parser parser(tokens);
        auto ast = parser.parse();
        
        // Code Generation
        CodeGen codegen;
        std::string cCode = codegen.generate(ast.get());
        
        // Write C code to temp file
        std::string tempFile = "/tmp/sigma_temp.c";
        writeFile(tempFile, cCode);
        
        // Compile with GCC
        std::string compileCmd = "gcc -O3 " + tempFile + " -o /tmp/sigma_out -lm";
        int compileResult = system(compileCmd.c_str());
        
        if (compileResult != 0) {
            std::cerr << "Compilation failed!\n";
            return 1;
        }
        
        // Run the compiled program
        int runResult = system("/tmp/sigma_out");
        
        return runResult;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}