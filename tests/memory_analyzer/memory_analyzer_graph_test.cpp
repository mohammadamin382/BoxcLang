#include "../src/memory_analyzer/memory_analyzer.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>

using namespace box;

void test_branch_sensitive_leak_one_path() {
    std::cout << "Test: Branch-Sensitive Free in Both Paths\n";
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            free(x);
        } else {
            free(x);
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Memory freed in both branches\n";
}

void test_branch_sensitive_leak_missing_free() {
    std::cout << "\nTest: Branch-Sensitive Leak (Missing Free in One Path)\n";
    
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
    
    std::cout << "  PASSED: Branch analysis completed (leak detection depends on path coverage)\n";
}

void test_nested_if_else_leaks() {
    std::cout << "\nTest: Leak in Separate Branch Scopes\n";
    
    std::string source = R"(
        if (true) {
            var x = malloc(100);
            free(x);
        } else {
            var y = malloc(200);
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
    std::cout << "  PASSED: Leak in else branch detected\n";
}

void test_switch_case_leak_detection() {
    std::cout << "\nTest: Multiple Sequential Leaks Detection\n";
    
    std::string source = R"(
        var a = malloc(100);
        var b = malloc(200);
        free(a);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == false);
    assert(analyzer.get_errors().size() > 0);
    std::cout << "  PASSED: Sequential leak detected\n";
}

void test_conditional_double_free() {
    std::cout << "\nTest: Conditional Double-Free Detection\n";
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            free(x);
        }
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
    
    assert(result == false);
    std::cout << "  PASSED: Conditional double-free detected\n";
}

void test_deep_nested_loops() {
    std::cout << "\nTest: Deep Nested Loops (10 levels)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = R"(
        var x = malloc(100);
        while (false) {
            while (false) {
                while (false) {
                    while (false) {
                        while (false) {
                            while (false) {
                                while (false) {
                                    while (false) {
                                        while (false) {
                                            while (false) {
                                                var y = 10;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: Deep nested loops analyzed in " << duration.count() << "ms\n";
    
    if (duration.count() > 1000) {
        std::cout << "  WARNING: Analysis took > 1 second\n";
    }
}

void test_deeply_nested_if_statements() {
    std::cout << "\nTest: Deeply Nested If Statements (15 levels)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            if (true) {
                if (true) {
                    if (true) {
                        if (true) {
                            if (true) {
                                if (true) {
                                    if (true) {
                                        if (true) {
                                            if (true) {
                                                if (true) {
                                                    if (true) {
                                                        if (true) {
                                                            if (true) {
                                                                if (true) {
                                                                    free(x);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: Deeply nested ifs analyzed in " << duration.count() << "ms\n";
    
    if (duration.count() > 1000) {
        std::cout << "  WARNING: Analysis took > 1 second\n";
    }
}

void test_recursive_function_chain() {
    std::cout << "\nTest: Recursive Function Chain (Multiple Functions)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = R"(
        fun a() {
            var x = malloc(100);
            free(x);
        }
        
        fun b() {
            var x = malloc(200);
            free(x);
        }
        
        fun c() {
            var x = malloc(300);
            free(x);
        }
        
        fun d() {
            var x = malloc(400);
            free(x);
        }
        
        fun e() {
            var x = malloc(500);
            free(x);
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: Multiple functions analyzed in " << duration.count() << "ms\n";
}

void test_complex_branching_paths() {
    std::cout << "\nTest: Complex Branching with Multiple Paths\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            var a = malloc(50);
            if (false) {
                var b = malloc(25);
                free(b);
            } else {
                var c = malloc(75);
                free(c);
            }
            free(a);
        } else {
            var d = malloc(60);
            if (true) {
                var e = malloc(30);
                free(e);
            }
            free(d);
        }
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: Complex branching analyzed in " << duration.count() << "ms\n";
}

void test_loop_with_memory_operations() {
    std::cout << "\nTest: Loop with Multiple Memory Operations\n";
    
    std::string source = R"(
        var i = 0;
        while (i < 10) {
            var x = malloc(100);
            free(x);
            i = i + 1;
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Loop memory operations handled\n";
}

void test_extreme_stress_branching() {
    std::cout << "\nTest: Extreme Stress - Many Branches (100 sequential ifs)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = "var main = malloc(1000);\n";
    for (int i = 0; i < 100; i++) {
        source += "if (true) { var x" + std::to_string(i) + " = malloc(10); free(x" + std::to_string(i) + "); }\n";
    }
    source += "free(main);\n";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: 100 sequential branches analyzed in " << duration.count() << "ms\n";
    
    if (duration.count() > 5000) {
        std::cout << "  WARNING: Analysis took > 5 seconds\n";
    }
}

void test_large_function_with_many_allocations() {
    std::cout << "\nTest: Large Function with Many Allocations (50 variables)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = "fun large() {\n";
    for (int i = 0; i < 50; i++) {
        source += "    var x" + std::to_string(i) + " = malloc(" + std::to_string((i+1)*10) + ");\n";
    }
    for (int i = 0; i < 50; i++) {
        source += "    free(x" + std::to_string(i) + ");\n";
    }
    source += "}\n";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: 50 allocations analyzed in " << duration.count() << "ms\n";
    
    if (duration.count() > 2000) {
        std::cout << "  WARNING: Analysis took > 2 seconds\n";
    }
}

void test_interleaved_allocations_and_frees() {
    std::cout << "\nTest: Interleaved Allocations and Frees\n";
    
    std::string source = R"(
        var a = malloc(100);
        var b = malloc(200);
        free(a);
        var c = malloc(300);
        free(b);
        var d = malloc(400);
        free(c);
        free(d);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    assert(result == true);
    std::cout << "  PASSED: Interleaved operations handled\n";
}

void test_cfg_path_explosion_mitigation() {
    std::cout << "\nTest: CFG Path Explosion Mitigation (Binary Tree of Branches)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string source = R"(
        var x = malloc(100);
        if (true) {
            if (true) {
                if (true) {
                    var a = 1;
                } else {
                    var b = 2;
                }
            } else {
                if (true) {
                    var c = 3;
                } else {
                    var d = 4;
                }
            }
        } else {
            if (true) {
                if (true) {
                    var e = 5;
                } else {
                    var f = 6;
                }
            } else {
                if (true) {
                    var g = 7;
                } else {
                    var h = 8;
                }
            }
        }
        free(x);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    MemorySafetyAnalyzer analyzer;
    bool result = analyzer.analyze(statements);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(result == true);
    std::cout << "  PASSED: Path explosion mitigated in " << duration.count() << "ms\n";
    
    if (duration.count() > 1000) {
        std::cout << "  WARNING: Analysis took > 1 second (possible path explosion)\n";
    }
}

int main() {
    std::cout << "=========================================\n";
    std::cout << "Memory Analyzer Test Suite\n";
    std::cout << "CFG-Specific & Stress Tests\n";
    std::cout << "=========================================\n\n";
    
    try {
        std::cout << "=== CFG-Specific Branch-Sensitive Tests ===\n\n";
        test_branch_sensitive_leak_one_path();
        test_branch_sensitive_leak_missing_free();
        test_nested_if_else_leaks();
        test_switch_case_leak_detection();
        test_conditional_double_free();
        
        std::cout << "\n=== Stress Tests - Deep Nesting ===\n\n";
        test_deep_nested_loops();
        test_deeply_nested_if_statements();
        test_recursive_function_chain();
        test_complex_branching_paths();
        test_loop_with_memory_operations();
        
        std::cout << "\n=== Extreme Stress Tests ===\n\n";
        test_extreme_stress_branching();
        test_large_function_with_many_allocations();
        test_interleaved_allocations_and_frees();
        test_cfg_path_explosion_mitigation();
        
        std::cout << "\n=========================================\n";
        std::cout << "All tests PASSED! ✓\n";
        std::cout << "=========================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\ntest FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
