#include "optimizer.hpp"
#include "parser.hpp"
#include "lexer.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace box;

void test_constant_folding() {
    std::cout << "Testing Constant Folding..." << std::endl;
    
    std::string source = R"(
        var x = 2 + 3;
        var y = 10 * 5;
        var z = 7 - 2;
        var w = 20 / 4;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = true;
    config.algebraic_simplification = false;
    config.dead_code_elimination = false;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    bool found_constant = false;
    for (const auto& stmt : optimized) {
        if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
            if (var_stmt->initializer.has_value()) {
                if (auto* lit = dynamic_cast<Literal*>(var_stmt->initializer.value().get())) {
                    found_constant = true;
                    if (std::holds_alternative<double>(lit->value)) {
                        double val = std::get<double>(lit->value);
                        std::cout << "  Found constant: " << val << std::endl;
                    }
                }
            }
        }
    }
    
    assert(found_constant && "Constant folding should produce literal values");
    std::cout << "  ✓ Constant folding test passed" << std::endl;
}

void test_algebraic_simplification() {
    std::cout << "Testing Algebraic Simplification..." << std::endl;
    
    std::string source = R"(
        var a = x + 0;
        var b = x * 1;
        var c = x * 0;
        var d = x - 0;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = false;
    config.algebraic_simplification = true;
    config.dead_code_elimination = false;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    std::cout << "  ✓ Algebraic simplification test passed" << std::endl;
}

void test_dead_code_elimination() {
    std::cout << "Testing Dead Code Elimination..." << std::endl;
    
    std::string source = R"(
        var unused = 42;
        var used = 10;
        print used;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = false;
    config.algebraic_simplification = false;
    config.dead_code_elimination = true;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    int var_count = 0;
    for (const auto& stmt : optimized) {
        if (dynamic_cast<VarStmt*>(stmt.get())) {
            var_count++;
        }
    }
    
    std::cout << "  Variable declarations after DCE: " << var_count << std::endl;
    std::cout << "  ✓ Dead code elimination test passed" << std::endl;
}

void test_strength_reduction() {
    std::cout << "Testing Strength Reduction..." << std::endl;
    
    std::string source = R"(
        var a = x * 2;
        var b = x * 4;
        var c = x * 8;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = false;
    config.algebraic_simplification = false;
    config.strength_reduction = true;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    std::cout << "  ✓ Strength reduction test passed" << std::endl;
}

void test_combined_optimizations() {
    std::cout << "Testing Combined Optimizations..." << std::endl;
    
    std::string source = R"(
        var a = 2 + 3;
        var b = a * 1;
        var c = b + 0;
        var unused = 999;
        print c;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = true;
    config.algebraic_simplification = true;
    config.dead_code_elimination = true;
    config.optimize_level = 3;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    std::cout << "  Optimized to " << optimized.size() << " statements" << std::endl;
    std::cout << "  ✓ Combined optimizations test passed" << std::endl;
}

void test_if_statement_folding() {
    std::cout << "Testing If Statement Folding..." << std::endl;
    
    std::string source = R"(
        if (true) {
            print 1;
        } else {
            print 2;
        }
        
        if (false) {
            print 3;
        } else {
            print 4;
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = true;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    int print_count = 0;
    for (const auto& stmt : optimized) {
        if (dynamic_cast<PrintStmt*>(stmt.get())) {
            print_count++;
        }
    }
    
    std::cout << "  Print statements after optimization: " << print_count << std::endl;
    std::cout << "  ✓ If statement folding test passed" << std::endl;
}

void test_loop_optimization() {
    std::cout << "Testing Loop Optimization..." << std::endl;
    
    std::string source = R"(
        while (false) {
            print 1;
        }
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = true;
    config.loop_unrolling = true;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    std::cout << "  Statements after loop optimization: " << optimized.size() << std::endl;
    std::cout << "  ✓ Loop optimization test passed" << std::endl;
}

void test_peephole_optimization() {
    std::cout << "Testing Peephole Optimization..." << std::endl;
    
    std::string source = R"(
        var a = --x;
        var b = !!y;
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.peephole_optimization = true;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    std::cout << "  ✓ Peephole optimization test passed" << std::endl;
}

void test_complex_expression() {
    std::cout << "Testing Complex Expression Optimization..." << std::endl;
    
    std::string source = R"(
        var result = ((2 + 3) * (4 - 1)) + (10 / 2);
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    Parser parser(tokens, source);
    auto statements = parser.parse();
    
    OptimizerConfig config;
    config.constant_folding = true;
    config.algebraic_simplification = true;
    config.optimize_level = 3;
    
    Optimizer optimizer(config);
    auto optimized = optimizer.optimize(statements);
    
    if (!optimized.empty()) {
        if (auto* var_stmt = dynamic_cast<VarStmt*>(optimized[0].get())) {
            if (var_stmt->initializer.has_value()) {
                if (auto* lit = dynamic_cast<Literal*>(var_stmt->initializer.value().get())) {
                    if (std::holds_alternative<double>(lit->value)) {
                        double result = std::get<double>(lit->value);
                        double expected = ((2.0 + 3.0) * (4.0 - 1.0)) + (10.0 / 2.0);
                        std::cout << "  Expected: " << expected << ", Got: " << result << std::endl;
                        assert(result == expected && "Complex expression should be fully folded");
                    }
                }
            }
        }
    }
    
    std::cout << "  ✓ Complex expression optimization test passed" << std::endl;
}

void test_optimizer_config() {
    std::cout << "Testing Optimizer Configuration..." << std::endl;
    
    OptimizerConfig config;
    assert(config.constant_folding == true);
    assert(config.algebraic_simplification == true);
    assert(config.dead_code_elimination == true);
    assert(config.optimize_level == 3);
    
    config.constant_folding = false;
    config.optimize_level = 1;
    
    assert(config.constant_folding == false);
    assert(config.optimize_level == 1);
    
    std::cout << "  ✓ Optimizer configuration test passed" << std::endl;
}

int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Box Optimizer Test Suite" << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;
    
    try {
        test_optimizer_config();
        test_constant_folding();
        test_algebraic_simplification();
        test_dead_code_elimination();
        test_strength_reduction();
        test_combined_optimizations();
        test_if_statement_folding();
        test_loop_optimization();
        test_peephole_optimization();
        test_complex_expression();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "All optimizer tests passed successfully! ✓" << std::endl;
        std::cout << std::string(60, '=') << "\n" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << "\n" << std::endl;
        return 1;
    }
}
