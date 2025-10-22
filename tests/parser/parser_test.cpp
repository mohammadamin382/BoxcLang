#include "../src/parser/parser.hpp"
#include "../src/lexer/lexer.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace box;

void test_variable_declaration() {
    std::cout << "Testing variable declarations..." << std::endl;
    
    std::string source = "var x = 42; var y;";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 2);
    assert(dynamic_cast<VarStmt*>(statements[0].get()) != nullptr);
    assert(dynamic_cast<VarStmt*>(statements[1].get()) != nullptr);
    
    std::cout << "✓ Variable declarations passed" << std::endl;
}

void test_function_declaration() {
    std::cout << "Testing function declarations..." << std::endl;
    
    std::string source = "fun add(a, b) { return a + b; }";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* func = dynamic_cast<FunctionStmt*>(statements[0].get());
    assert(func != nullptr);
    assert(func->name.lexeme == "add");
    assert(func->params.size() == 2);
    assert(func->params[0].lexeme == "a");
    assert(func->params[1].lexeme == "b");
    
    std::cout << "✓ Function declarations passed" << std::endl;
}

void test_if_statement() {
    std::cout << "Testing if statements..." << std::endl;
    
    std::string source = "if (x > 5) { print x; } else { print 0; }";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* if_stmt = dynamic_cast<IfStmt*>(statements[0].get());
    assert(if_stmt != nullptr);
    assert(if_stmt->then_branch != nullptr);
    assert(if_stmt->else_branch.has_value());
    
    std::cout << "✓ If statements passed" << std::endl;
}

void test_while_loop() {
    std::cout << "Testing while loops..." << std::endl;
    
    std::string source = "while (x < 10) { x = x + 1; }";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* while_stmt = dynamic_cast<WhileStmt*>(statements[0].get());
    assert(while_stmt != nullptr);
    assert(while_stmt->condition != nullptr);
    assert(while_stmt->body != nullptr);
    
    std::cout << "✓ While loops passed" << std::endl;
}

void test_for_loop() {
    std::cout << "Testing for loops..." << std::endl;
    
    std::string source = "for (var i = 0; i < 10; i = i + 1) { print i; }";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    
    std::cout << "✓ For loops passed" << std::endl;
}

void test_expressions() {
    std::cout << "Testing expressions..." << std::endl;
    
    std::string source = "var x = 1 + 2 * 3 - 4 / 2;";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* var_stmt = dynamic_cast<VarStmt*>(statements[0].get());
    assert(var_stmt != nullptr);
    assert(var_stmt->initializer.has_value());
    
    std::cout << "✓ Expressions passed" << std::endl;
}

void test_array_literal() {
    std::cout << "Testing array literals..." << std::endl;
    
    std::string source = "var arr = [1, 2, 3, 4, 5];";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* var_stmt = dynamic_cast<VarStmt*>(statements[0].get());
    assert(var_stmt != nullptr);
    assert(var_stmt->initializer.has_value());
    
    auto* arr_lit = dynamic_cast<ArrayLiteral*>(var_stmt->initializer.value().get());
    assert(arr_lit != nullptr);
    assert(arr_lit->elements.size() == 5);
    
    std::cout << "✓ Array literals passed" << std::endl;
}

void test_dict_literal() {
    std::cout << "Testing dictionary literals..." << std::endl;
    
    std::string source = "var dict = {\"name\": \"John\", \"age\": 30};";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* var_stmt = dynamic_cast<VarStmt*>(statements[0].get());
    assert(var_stmt != nullptr);
    assert(var_stmt->initializer.has_value());
    
    auto* dict_lit = dynamic_cast<DictLiteral*>(var_stmt->initializer.value().get());
    assert(dict_lit != nullptr);
    assert(dict_lit->pairs.size() == 2);
    
    std::cout << "✓ Dictionary literals passed" << std::endl;
}

void test_switch_statement() {
    std::cout << "Testing switch statements..." << std::endl;
    
    std::string source = R"(
        switch (x) {
            case 1:
                print "one";
                break;
            case 2:
                print "two";
                break;
            default:
                print "other";
        }
    )";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* switch_stmt = dynamic_cast<SwitchStmt*>(statements[0].get());
    assert(switch_stmt != nullptr);
    assert(switch_stmt->cases.size() == 2);
    assert(switch_stmt->default_case.has_value());
    
    std::cout << "✓ Switch statements passed" << std::endl;
}

void test_function_call() {
    std::cout << "Testing function calls..." << std::endl;
    
    std::string source = "print add(1, 2);";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* print_stmt = dynamic_cast<PrintStmt*>(statements[0].get());
    assert(print_stmt != nullptr);
    
    auto* call = dynamic_cast<Call*>(print_stmt->expression.get());
    assert(call != nullptr);
    assert(call->arguments.size() == 2);
    
    std::cout << "✓ Function calls passed" << std::endl;
}

void test_array_indexing() {
    std::cout << "Testing array indexing..." << std::endl;
    
    std::string source = "var x = arr[0]; arr[1] = 42;";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 2);
    
    auto* var_stmt = dynamic_cast<VarStmt*>(statements[0].get());
    assert(var_stmt != nullptr);
    assert(var_stmt->initializer.has_value());
    auto* idx_get = dynamic_cast<IndexGet*>(var_stmt->initializer.value().get());
    assert(idx_get != nullptr);
    
    auto* expr_stmt = dynamic_cast<ExprStmt*>(statements[1].get());
    assert(expr_stmt != nullptr);
    auto* idx_set = dynamic_cast<IndexSet*>(expr_stmt->expression.get());
    assert(idx_set != nullptr);
    
    std::cout << "✓ Array indexing passed" << std::endl;
}

void test_unsafe_block() {
    std::cout << "Testing unsafe blocks..." << std::endl;
    
    std::string source = "unsafe { var x = 42; }";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* unsafe_block = dynamic_cast<UnsafeBlock*>(statements[0].get());
    assert(unsafe_block != nullptr);
    assert(unsafe_block->statements.size() == 1);
    
    std::cout << "✓ Unsafe blocks passed" << std::endl;
}

void test_llvm_inline() {
    std::cout << "Testing LLVM inline..." << std::endl;
    
    std::string source = R"(unsafe { llvm_inline("%result = add i32 5, 10"); })";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 1);
    auto* unsafe_block = dynamic_cast<UnsafeBlock*>(statements[0].get());
    assert(unsafe_block != nullptr);
    assert(unsafe_block->statements.size() == 1);
    
    auto* llvm_inline = dynamic_cast<LLVMInlineStmt*>(unsafe_block->statements[0].get());
    assert(llvm_inline != nullptr);
    assert(llvm_inline->llvm_code == "%result = add i32 5, 10");
    
    std::cout << "✓ LLVM inline passed" << std::endl;
}

void test_complex_program() {
    std::cout << "Testing complex program..." << std::endl;
    
    std::string source = R"(
        var x = 10;
        
        fun factorial(n) {
            if (n <= 1) {
                return 1;
            }
            return n * factorial(n - 1);
        }
        
        var result = factorial(5);
        print result;
        
        var arr = [1, 2, 3, 4, 5];
        for (var i = 0; i < len(arr); i = i + 1) {
            print arr[i];
        }
    )";
    
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    std::vector<StmtPtr> statements = parser.parse();
    
    assert(statements.size() == 6);
    
    std::cout << "✓ Complex program passed" << std::endl;
}

void test_error_recovery() {
    std::cout << "Testing error recovery..." << std::endl;
    
    std::string source = "var x = ; var y = 42;";
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    
    try {
        std::vector<StmtPtr> statements = parser.parse();
        assert(false);
    } catch (const std::runtime_error& e) {
        std::cout << "✓ Error recovery passed (caught expected error)" << std::endl;
    }
}

int main() {
    std::cout << "==================================================================" << std::endl;
    std::cout << "                    Box Parser C++ Test Suite                     " << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_variable_declaration();
        test_function_declaration();
        test_if_statement();
        test_while_loop();
        test_for_loop();
        test_expressions();
        test_array_literal();
        test_dict_literal();
        test_switch_statement();
        test_function_call();
        test_array_indexing();
        test_unsafe_block();
        test_llvm_inline();
        test_complex_program();
        test_error_recovery();
        
        std::cout << std::endl;
        std::cout << "==================================================================" << std::endl;
        std::cout << "                    ALL TESTS PASSED! ✓                          " << std::endl;
        std::cout << "==================================================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::endl;
        std::cerr << "==================================================================" << std::endl;
        std::cerr << "                    TEST FAILED! ✗                               " << std::endl;
        std::cerr << "==================================================================" << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
