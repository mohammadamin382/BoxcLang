
#ifndef BOX_COMPILER_HPP
#define BOX_COMPILER_HPP

#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include "../parser/parser.hpp"

namespace box {

class Lexer;
class MemorySafetyAnalyzer;
class CodeGenerator;
class FileResolver;

struct CompilationOptions {
    std::string input_file;
    std::string output_file;
    bool emit_llvm;
    bool emit_assembly;
    bool run_after_compile;
    bool optimize;
    int optimization_level;
    int llvm_optimization_level;
    bool show_memory_warnings;
    bool verbose;
    
    CompilationOptions() 
        : emit_llvm(false)
        , emit_assembly(false)
        , run_after_compile(false)
        , optimize(true)
        , optimization_level(3)
        , llvm_optimization_level(3)
        , show_memory_warnings(true)
        , verbose(false) {}
};

class CompilationResult {
public:
    bool success;
    std::string message;
    int exit_code;
    std::vector<std::string> warnings;
    
    CompilationResult() : success(false), exit_code(1) {}
    CompilationResult(bool s, const std::string& msg, int code = 0) 
        : success(s), message(msg), exit_code(code) {}
};

class BoxCompiler {
private:
    CompilationOptions options;
    std::unique_ptr<llvm::LLVMContext> llvm_context;
    std::unique_ptr<llvm::Module> llvm_module;
    std::unique_ptr<llvm::TargetMachine> target_machine;
    std::unique_ptr<FileResolver> file_resolver;
    std::unordered_set<std::string> processed_files;
    
    bool initialize_llvm();
    bool read_source_file(const std::string& path, std::string& content);
    bool write_llvm_ir(const std::string& ir, const std::string& output_path);
    bool write_assembly_file(const std::string& asm_path);
    bool write_object_file(const std::string& obj_path);
    bool process_imports(const std::string& file_path, const std::string& source_code, 
                        std::vector<StmtPtr>& all_statements);
    bool link_executable(const std::string& obj_path, const std::string& exe_path);
    bool run_executable(const std::string& exe_path, int& return_code);
    void cleanup_intermediate_files(const std::string& obj_path);
    void print_banner();
    void print_success(const std::string& output_file);
    void print_error(const std::string& error_type, const std::string& message);
    void print_warnings(const std::vector<std::string>& warnings);
    void print_memory_report(const std::string& report);
    std::string get_output_filename(const std::string& input_file);
    std::string get_llvm_ir_filename(const std::string& input_file);
    std::string get_assembly_filename(const std::string& input_file);
    std::string get_object_filename(const std::string& input_file);
    
public:
    explicit BoxCompiler(const CompilationOptions& opts);
    ~BoxCompiler();
    
    CompilationResult compile();
    
    static void print_version();
    static void print_help();
};

class CompilerCLI {
public:
    static int run(int argc, char* argv[]);
    
private:
    static CompilationOptions parse_arguments(int argc, char* argv[]);
    static void print_usage(const char* program_name);
};

}

#endif

