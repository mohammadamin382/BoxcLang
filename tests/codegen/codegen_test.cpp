#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <cassert>

using namespace box;

void test_simple_program() {
    std::string source = R"(
        var x = 42;
        var y = 10;
        var z = x + y;
        print z;
    )";

    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    CodeGenerator codegen(false, 0);
    std::string llvm_ir = codegen.generate(statements);
    
    assert(llvm_ir.find("define i32 @main()") != std::string::npos);
    assert(llvm_ir.find("double 4.200000e+01") != std::string::npos);
    assert(llvm_ir.find("fadd") != std::string::npos);
    
    std::cout << "Test simple program: PASSED\n";
}

void test_function_generation() {
    std::string source = R"(
        fun add(a, b) {
            return a + b;
        }
        
        var result = add(5, 3);
        print result;
    )";

    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    CodeGenerator codegen(false, 0);
    std::string llvm_ir = codegen.generate(statements);
    
    assert(llvm_ir.find("define double @add(") != std::string::npos);
    assert(llvm_ir.find("call double @add(") != std::string::npos);
    
    std::cout << "Test function generation: PASSED\n";
}

void test_array_generation() {
    std::string source = R"(
        var arr = [1, 2, 3];
        print arr;
    )";

    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    CodeGenerator codegen(false, 0);
    std::string llvm_ir = codegen.generate(statements);
    
    assert(llvm_ir.find("ArrayStruct") != std::string::npos);
    assert(llvm_ir.find("malloc") != std::string::npos);
    
    std::cout << "Test array generation: PASSED\n";
}

void test_if_statement() {
    std::string source = R"(
        var x = 10;
        if (x > 5) {
            print true;
        } else {
            print false;
        }
    )";

    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    CodeGenerator codegen(false, 0);
    std::string llvm_ir = codegen.generate(statements);
    
    assert(llvm_ir.find("br i1") != std::string::npos);
    assert(llvm_ir.find("if_then") != std::string::npos);
    assert(llvm_ir.find("if_else") != std::string::npos);
    
    std::cout << "Test if statement: PASSED\n";
}

void test_while_loop() {
    std::string source = R"(
        var i = 0;
        while (i < 5) {
            print i;
            i = i + 1;
        }
    )";

    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    CodeGenerator codegen(false, 0);
    std::string llvm_ir = codegen.generate(statements);
    
    assert(llvm_ir.find("while_cond") != std::string::npos);
    assert(llvm_ir.find("while_body") != std::string::npos);
    
    std::cout << "Test while loop: PASSED\n";
}

int main() {
    try {
        std::cout << "Running codegen tests...\n";
        std::cout << "========================\n\n";
        
        test_simple_program();
        test_function_generation();
        test_array_generation();
        test_if_statement();
        test_while_loop();
        
        std::cout << "\n========================\n";
        std::cout << "All tests passed!\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
