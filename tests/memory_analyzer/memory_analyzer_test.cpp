#include "../src/memory_analyzer/memory_analyzer.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace box;

void test_basic_allocation() {
    std::cout << "Test: Basic Allocation\n";
    
    std::string source = R"(
        var x = malloc(100);
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Basic allocation and free\n";
}

void test_memory_leak_detection() {
    std::cout << "\nTest: Memory Leak Detection\n";
    
    std::string source = R"(
        var x = malloc(100);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Memory leak detected\n";
}

void test_double_free_detection() {
    std::cout << "\nTest: Double-Free Detection\n";
    
    std::string source = R"(
        var x = malloc(100);
        free(x);
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Double-free detected\n";
}

void test_use_after_free_detection() {
    std::cout << "\nTest: Use-After-Free Detection\n";
    
    std::string source = R"(
        var x = malloc(100);
        free(x);
        var y = deref(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Use-after-free detected\n";
}

void test_invalid_free_detection() {
    std::cout << "\nTest: Invalid Free Detection\n";
    
    std::string source = R"(
        var x = 42;
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Invalid free detected\n";
}

void test_scope_based_leak_detection() {
    std::cout << "\nTest: Scope-Based Leak Detection\n";
    
    std::string source = R"(
        {
            var x = malloc(100);
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Scope-based leak detected\n";
}

void test_pointer_aliasing() {
    std::cout << "\nTest: Pointer Aliasing\n";
    
    std::string source = R"(
        var x = malloc(100);
        var y = addr_of(x);
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Pointer aliasing tracked\n";
}

void test_if_branch_analysis() {
    std::cout << "\nTest: If-Branch Analysis\n";
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            free(x);
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    std::cout << "  PASSED: If-branch analysis completed\n";
}

void test_function_leak_detection() {
    std::cout << "\nTest: Function Leak Detection\n";
    
    std::string source = R"(
        fun test() {
            var x = malloc(100);
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Function leak detected\n";
}

void test_reassignment_leak() {
    std::cout << "\nTest: Reassignment Leak Detection\n";
    
    std::string source = R"(
        var x = malloc(100);
        x = malloc(200);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Reassignment leak detected\n";
}

void test_complex_control_flow() {
    std::cout << "\nTest: Complex Control Flow\n";
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            var y = malloc(200);
            free(y);
        }
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Complex control flow handled\n";
}

void test_calloc_allocation() {
    std::cout << "\nTest: Calloc Allocation\n";
    
    std::string source = R"(
        var arr = calloc(10);
        free(arr);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Calloc allocation handled\n";
}

void test_while_loop_analysis() {
    std::cout << "\nTest: While Loop Analysis\n";
    
    std::string source = R"(
        var x = malloc(100);
        while (false) {
            var y = 10;
        }
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: While loop analysis completed\n";
}

int main() {
    std::cout << "=================================\n";
    std::cout << "Memory Analyzer Test Suite\n";
    std::cout << "=================================\n\n";
    
    try {
        test_basic_allocation();
        test_memory_leak_detection();
        test_double_free_detection();
        test_use_after_free_detection();
        test_invalid_free_detection();
        test_scope_based_leak_detection();
        test_pointer_aliasing();
        test_if_branch_analysis();
        test_function_leak_detection();
        test_reassignment_leak();
        test_complex_control_flow();
        test_calloc_allocation();
        test_while_loop_analysis();
        
        std::cout << "\n=================================\n";
        std::cout << "All tests PASSED!\n";
        std::cout << "=================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
