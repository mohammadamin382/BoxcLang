
#include "compiler.hpp"
#include "lexer.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

void create_test_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
    file.close();
}

void cleanup_test_file(const std::string& filename) {
    if (fs::exists(filename)) {
        fs::remove(filename);
    }
}

void test_basic_compilation() {
    std::cout << "Testing basic compilation..." << std::flush;
    
    std::string test_file = "test_basic.box";
    std::string test_code = R"(
var x = 10;
var y = 20;
var result = x + y;
print(result);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_basic";
    options.optimize = true;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    assert(fs::exists("test_basic"));
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_basic");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_llvm_ir_emission() {
    std::cout << "Testing LLVM IR emission..." << std::flush;
    
    std::string test_file = "test_llvm.box";
    std::string test_code = R"(
var factorial = 5;
var result = 1;
while (factorial > 0) {
    result = result * factorial;
    factorial = factorial - 1;
}
print(result);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_llvm";
    options.emit_llvm = true;
    options.optimize = true;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    assert(fs::exists("test_llvm.ll"));
    assert(fs::exists("test_llvm"));
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_llvm.ll");
    cleanup_test_file("test_llvm");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_optimization_levels() {
    std::cout << "Testing optimization levels..." << std::flush;
    
    std::string test_file = "test_opt.box";
    std::string test_code = R"(
var x = 2 + 3;
var y = x * 4;
var z = y / 2;
print(z);
)";
    
    create_test_file(test_file, test_code);
    
    for (int opt_level = 0; opt_level <= 3; ++opt_level) {
        box::CompilationOptions options;
        options.input_file = test_file;
        options.output_file = "test_opt_" + std::to_string(opt_level);
        options.optimize = (opt_level > 0);
        options.optimization_level = opt_level;
        options.verbose = false;
        
        box::BoxCompiler compiler(options);
        box::CompilationResult result = compiler.compile();
        
        assert(result.success);
        cleanup_test_file(options.output_file);
    }
    
    cleanup_test_file(test_file);
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_array_compilation() {
    std::cout << "Testing array compilation..." << std::flush;
    
    std::string test_file = "test_array.box";
    std::string test_code = R"(
var arr = [1, 2, 3, 4, 5];
var sum = 0;
var i = 0;
while (i < len(arr)) {
    sum = sum + arr[i];
    i = i + 1;
}
print(sum);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_array";
    options.optimize = true;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_array");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_function_compilation() {
    std::cout << "Testing function compilation..." << std::flush;
    
    std::string test_file = "test_func.box";
    std::string test_code = R"(
fun fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

var result = fibonacci(10);
print(result);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_func";
    options.optimize = true;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_func");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_error_handling() {
    std::cout << "Testing error handling..." << std::flush;
    
    std::string test_file = "test_error.box";
    std::string test_code = R"(
var x = 10 +;
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_error";
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(!result.success);
    
    cleanup_test_file(test_file);
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_memory_safety_analysis() {
    std::cout << "Testing memory safety analysis..." << std::flush;
    
    std::string test_file = "test_memory.box";
    std::string test_code = R"(
var ptr = malloc(100);
free(ptr);
print(42);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_memory";
    options.show_memory_warnings = true;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_memory");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

void test_complex_program() {
    std::cout << "Testing complex program..." << std::flush;
    
    std::string test_file = "test_complex.box";
    std::string test_code = R"(
fun is_prime(n) {
    if (n <= 1) {
        return false;
    }
    if (n <= 3) {
        return true;
    }
    if (n % 2 == 0 or n % 3 == 0) {
        return false;
    }
    
    var i = 5;
    while (i * i <= n) {
        if (n % i == 0 or n % (i + 2) == 0) {
            return false;
        }
        i = i + 6;
    }
    return true;
}

var count = 0;
var i = 2;
while (i < 100) {
    if (is_prime(i)) {
        count = count + 1;
    }
    i = i + 1;
}

print(count);
)";
    
    create_test_file(test_file, test_code);
    
    box::CompilationOptions options;
    options.input_file = test_file;
    options.output_file = "test_complex";
    options.optimize = true;
    options.optimization_level = 3;
    options.verbose = false;
    
    box::BoxCompiler compiler(options);
    box::CompilationResult result = compiler.compile();
    
    assert(result.success);
    
    cleanup_test_file(test_file);
    cleanup_test_file("test_complex");
    
    std::cout << " \033[1;32m✓\033[0m" << std::endl;
}

int main() {
    std::cout << "\n\033[1;36m" << std::string(60, '=') << "\033[0m\n";
    std::cout << "\033[1;37m            Box Compiler Test Suite\033[0m\n";
    std::cout << "\033[1;36m" << std::string(60, '=') << "\033[0m\n\n";
    
    try {
        test_basic_compilation();
        test_llvm_ir_emission();
        test_optimization_levels();
        test_array_compilation();
        test_function_compilation();
        test_error_handling();
        test_memory_safety_analysis();
        test_complex_program();
        
        std::cout << "\n\033[1;36m" << std::string(60, '=') << "\033[0m\n";
        std::cout << "\033[1;32m        All Compiler Tests Passed Successfully! ✓\033[0m\n";
        std::cout << "\033[1;36m" << std::string(60, '=') << "\033[0m\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n\033[1;31mTest failed with error: " << e.what() << "\033[0m\n";
        return 1;
    }
}
