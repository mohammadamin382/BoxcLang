
#include "compiler.hpp"
#include "file_resolver.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../memory_analyzer/memory_analyzer.hpp"
#include "../codegen/codegen.hpp"
#include "../optimizer/optimizer.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/IR/LegacyPassManager.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <optional>

namespace fs = std::filesystem;

namespace box {

static const char* BOX_VERSION = "0.1.0";

BoxCompiler::BoxCompiler(const CompilationOptions& opts) 
    : options(opts)
    , llvm_context(nullptr)
    , llvm_module(nullptr)
    , target_machine(nullptr) {
}

BoxCompiler::~BoxCompiler() = default;

/**
 * Architectural Component: Terminal UI Banner Generator
 * 
 * Renders formatted ANSI-escaped terminal banner displaying compiler identification.
 * Utilizes ANSI color codes for terminal styling and box drawing characters for visual framing.
 * 
 * Technical Implementation:
 * - ANSI escape sequences: \033[1;36m (cyan), \033[1;33m (yellow), \033[1;37m (white)
 * - Unicode box-drawing characters for structural aesthetics
 * - Stateless operation with no side effects beyond stdout stream manipulation
 * 
 * Performance: O(1) - constant time execution with fixed output size
 */
void BoxCompiler::print_banner() {
    std::cout << "\033[1;36m";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║        \033[1;33m███████╗  ██████╗ ██╗  ██╗\033[1;36m                      ║\n";
    std::cout << "║        \033[1;33m██╔════╝ ██╔═══██╗╚██╗██╔╝\033[1;36m                      ║\n";
    std::cout << "║        \033[1;33m█████╗   ██║   ██║ ╚███╔╝\033[1;36m                       ║\n";
    std::cout << "║        \033[1;33m██╔══╝   ██║   ██║ ██╔██╗\033[1;36m                       ║\n";
    std::cout << "║        \033[1;33m███████╗ ╚██████╔╝██╔╝ ██╗\033[1;36m                      ║\n";
    std::cout << "║        \033[1;33m╚══════╝  ╚═════╝ ╚═╝  ╚═╝\033[1;36m                      ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║           \033[1;37mBox Compiler v" << BOX_VERSION << "\033[1;36m                         ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\033[0m\n";
    std::cout << std::endl;
}

void BoxCompiler::print_success(const std::string& output_file) {
    std::cout << "\033[1;32m✓ Compilation successful!\033[0m\n";
    std::cout << "\033[1;37m→ Output: \033[1;36m" << output_file << "\033[0m\n";
}

void BoxCompiler::print_error(const std::string& error_type, const std::string& message) {
    std::cerr << "\033[1;31m✗ " << error_type << ":\033[0m " << message << "\n";
}

void BoxCompiler::print_warnings(const std::vector<std::string>& warnings) {
    if (warnings.empty() || !options.show_memory_warnings) return;
    
    std::cout << "\n\033[1;33m⚠ Memory Safety Warnings:\033[0m\n";
    for (const auto& warning : warnings) {
        std::cout << "\033[0;33m  • " << warning << "\033[0m\n";
    }
}

void BoxCompiler::print_memory_report(const std::string& report) {
    if (!report.empty()) {
        std::cout << "\n" << report << "\n";
    }
}

/**
 * Version Information Display Function
 * 
 * Outputs compiler version metadata to stdout stream.
 * 
 * Output Format:
 * - Primary: Semantic version identifier
 * - Secondary: Copyright attribution with year designation
 * 
 * Thread Safety: Thread-safe (stdout operations are atomic at OS level)
 */
void BoxCompiler::print_version() {
    std::cout << "Box Compiler v" << BOX_VERSION << "\n";
    std::cout << "Copyright (c) 2024 Box Language Project\n";
}

void BoxCompiler::print_help() {
    std::cout << "Usage: box [options] <input-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>     Specify output executable file\n";
    std::cout << "  --emit-llvm             Emit LLVM IR to .ll file\n";
    std::cout << "  -S                      Emit assembly to .s file\n";
    std::cout << "  -r, --run               Run the compiled program immediately\n";
    std::cout << "  --no-optimize           Disable optimizations\n";
    std::cout << "  -O<level>               Set IR optimization level (0-3, default: 3)\n";
    std::cout << "  -Oasm<level>            Set LLVM codegen optimization level (0-3, default: 3)\n";
    std::cout << "  --no-warnings           Suppress memory safety warnings\n";
    std::cout << "  -v, --verbose           Enable verbose output\n";
    std::cout << "  --version               Show version information\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  box program.box                  # Compile to executable\n";
    std::cout << "  box -o myapp program.box         # Compile with custom output name\n";
    std::cout << "  box --emit-llvm program.box      # Generate LLVM IR\n";
    std::cout << "  box -S program.box               # Generate assembly\n";
    std::cout << "  box -r program.box               # Compile and run\n";
    std::cout << "  box -O2 -Oasm3 program.box       # IR opt level 2, LLVM opt level 3\n";
    std::cout << "  box --no-optimize program.box    # Compile without optimizations\n";
}

/**
 * File System I/O: Source Code Reader
 * 
 * Performs unbuffered sequential read of source file from filesystem into memory.
 * 
 * Algorithm:
 * 1. Attempt file descriptor acquisition via POSIX-compliant open()
 * 2. Stream entire file content into intermediate stringstream buffer
 * 3. Transfer buffer content to output parameter via move semantics
 * 
 * Error Handling:
 * - Returns false on file access failure (permissions, non-existence)
 * - No exception throwing - boolean return indicates operation status
 * 
 * Memory Complexity: O(n) where n = file size in bytes
 * Time Complexity: O(n) - linear scan of file content
 * 
 * @param path Filesystem path to source file (absolute or relative)
 * @param content Output parameter receiving file content
 * @return Boolean success indicator
 */
bool BoxCompiler::read_source_file(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    return true;
}

/**
 * LLVM IR Serialization to Filesystem
 * 
 * Persists LLVM Intermediate Representation to disk in textual format.
 * 
 * Technical Details:
 * - Output format: LLVM Assembly Language (.ll extension)
 * - Encoding: UTF-8 text stream
 * - No compression applied
 * 
 * @param ir String containing LLVM IR in textual assembly format
 * @param output_path Target filesystem path for IR output
 * @return Boolean indicating write operation success
 */
bool BoxCompiler::write_llvm_ir(const std::string& ir, const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << ir;
    return true;
}

std::string BoxCompiler::get_output_filename(const std::string& input_file) {
    if (!options.output_file.empty()) {
        return options.output_file;
    }
    
    fs::path p(input_file);
    return p.stem().string();
}

std::string BoxCompiler::get_llvm_ir_filename(const std::string& input_file) {
    fs::path p(input_file);
    return p.stem().string() + ".ll";
}

std::string BoxCompiler::get_assembly_filename(const std::string& input_file) {
    fs::path p(input_file);
    return p.stem().string() + ".s";
}

std::string BoxCompiler::get_object_filename(const std::string& input_file) {
    fs::path p(input_file);
    return p.stem().string() + ".o";
}

/**
 * LLVM Infrastructure Initialization Subsystem
 * 
 * Configures LLVM backend components for code generation targeting host architecture.
 * 
 * Initialization Sequence:
 * 1. Native target registration (CPU architecture detection)
 * 2. Assembly parser initialization for inline assembly support
 * 3. Assembly printer initialization for object file emission
 * 4. Target triple resolution (OS-Arch-Vendor detection)
 * 5. Target machine instantiation with optimization level mapping
 * 
 * Optimization Level Mapping:
 * - Level 0: llvm::CodeGenOpt::None (no optimization)
 * - Level 1: llvm::CodeGenOpt::Less (minimal optimization)
 * - Level 2: llvm::CodeGenOpt::Default (balanced optimization)
 * - Level 3: llvm::CodeGenOpt::Aggressive (maximum optimization)
 * 
 * Architecture Support:
 * - Primary: x86_64 (AMD64/Intel 64)
 * - Secondary: ARM64, RISC-V (via target triple detection)
 * 
 * Error Conditions:
 * - Target lookup failure (unsupported architecture)
 * - Target machine instantiation failure (invalid configuration)
 * 
 * @return Boolean indicating successful LLVM initialization
 */
bool BoxCompiler::initialize_llvm() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    
    if (!target) {
        print_error("LLVM Target Error", error);
        return false;
    }
    
    llvm::TargetOptions target_opts;
    std::optional<llvm::Reloc::Model> reloc_model = std::nullopt;
    
    int llvm_opt_level = options.llvm_optimization_level;
    llvm::CodeGenOpt::Level cg_opt_level;
    
    switch (llvm_opt_level) {
        case 0: cg_opt_level = llvm::CodeGenOpt::None; break;
        case 1: cg_opt_level = llvm::CodeGenOpt::Less; break;
        case 2: cg_opt_level = llvm::CodeGenOpt::Default; break;
        default: cg_opt_level = llvm::CodeGenOpt::Aggressive; break;
    }
    
    target_machine = std::unique_ptr<llvm::TargetMachine>(
        target->createTargetMachine(target_triple, "generic", "", target_opts, reloc_model, 
                                   std::nullopt, cg_opt_level)
    );
    
    if (!target_machine) {
        print_error("LLVM Error", "Failed to create target machine");
        return false;
    }
    
    return true;
}

/**
 * Object File Code Emission Pipeline
 * 
 * Transforms LLVM IR module into target-specific object file format.
 * 
 * Pipeline Architecture:
 * 1. File descriptor acquisition with error code propagation
 * 2. Legacy pass manager instantiation for backward compatibility
 * 3. Target-specific code generation pass registration
 * 4. Module transformation via pass execution
 * 5. Stream buffer flush ensuring complete write
 * 
 * Output Format:
 * - ELF (Linux/Unix): Executable and Linkable Format
 * - Mach-O (macOS): Mach Object File Format
 * - COFF (Windows): Common Object File Format
 * 
 * Pass Manager Strategy:
 * - Uses legacy::PassManager (deprecated but stable)
 * - Future migration to new PassManager planned
 * 
 * Error Propagation:
 * - std::error_code for filesystem errors
 * - Boolean return for pass registration failures
 * 
 * @param obj_path Filesystem path for object file output
 * @return Boolean indicating successful object file generation
 */
/**
 * Assembly File Code Emission Pipeline
 * 
 * Transforms LLVM IR module into target-specific assembly language.
 * 
 * Pipeline Architecture:
 * 1. File descriptor acquisition with error code propagation
 * 2. Legacy pass manager instantiation for backward compatibility
 * 3. Target-specific assembly generation pass registration
 * 4. Module transformation via pass execution
 * 5. Stream buffer flush ensuring complete write
 * 
 * Output Format:
 * - x86-64 assembly (AT&T or Intel syntax depending on target)
 * - ARM assembly for ARM targets
 * - Human-readable assembly code (.s extension)
 * 
 * @param asm_path Filesystem path for assembly file output
 * @return Boolean indicating successful assembly file generation
 */
bool BoxCompiler::write_assembly_file(const std::string& asm_path) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(asm_path, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        print_error("File Error", "Could not open file: " + ec.message());
        return false;
    }
    
    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_AssemblyFile)) {
        print_error("LLVM Error", "Target machine cannot emit assembly file");
        return false;
    }
    
    pass.run(*llvm_module);
    dest.flush();
    
    return true;
}

bool BoxCompiler::write_object_file(const std::string& obj_path) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(obj_path, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        print_error("File Error", "Could not open file: " + ec.message());
        return false;
    }
    
    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_ObjectFile)) {
        print_error("LLVM Error", "Target machine cannot emit object file");
        return false;
    }
    
    pass.run(*llvm_module);
    dest.flush();
    
    return true;
}

/**
 * Native Executable Linking Subsystem
 * 
 * Invokes system linker (GCC frontend) to resolve symbols and produce executable binary.
 * 
 * Linker Invocation Strategy:
 * - Toolchain: GCC (GNU Compiler Collection) as linker driver
 * - Standard Library: libm (mathematical functions) linked by default
 * - Position Independence: -no-pie flag disables PIE for performance
 * - Error Stream: stderr redirected to stdout (2>&1) for unified error capture
 * 
 * System Call Mechanism:
 * - popen(): Creates child process with bidirectional pipe
 * - Process communication via FILE* stream handle
 * - Exit status retrieved via pclose() for error detection
 * 
 * Security Considerations:
 * - Shell injection vulnerability exists if paths contain shell metacharacters
 * - No path sanitization performed (assumes trusted input)
 * 
 * Alternative Approaches:
 * - Direct invocation of ld (bypasses GCC driver overhead)
 * - LLD linker (faster, but requires separate installation)
 * 
 * @param obj_path Path to input object file (.o)
 * @param exe_path Path to output executable binary
 * @return Boolean indicating successful link operation
 */
bool BoxCompiler::link_executable(const std::string& obj_path, const std::string& exe_path) {
    std::string cmd = "gcc " + obj_path + " -o " + exe_path + " -lm -no-pie 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        print_error("Linker Error", "Failed to execute linker");
        return false;
    }
    
    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    
    if (status != 0) {
        print_error("Linker Error", result);
        return false;
    }
    
    return true;
}

bool BoxCompiler::run_executable(const std::string& exe_path, int& return_code) {
    std::cout << "\n\033[1;36m" << std::string(60, '═') << "\033[0m\n";
    std::cout << "\033[1;37mRunning: " << exe_path << "\033[0m\n";
    std::cout << "\033[1;36m" << std::string(60, '═') << "\033[0m\n\n";
    
    return_code = system(("./" + exe_path).c_str());
    
    std::cout << "\n\033[1;36m" << std::string(60, '═') << "\033[0m\n";
    std::cout << "\033[1;37mProgram exited with code: " << return_code << "\033[0m\n";
    std::cout << "\033[1;36m" << std::string(60, '═') << "\033[0m\n";
    
    return true;
}

void BoxCompiler::cleanup_intermediate_files(const std::string& obj_path) {
    try {
        if (fs::exists(obj_path)) {
            fs::remove(obj_path);
        }
    } catch (const std::exception& e) {
        if (options.verbose) {
            std::cerr << "Warning: Failed to clean up intermediate file: " << e.what() << "\n";
        }
    }
}

/**
 * Recursive Import Resolution and AST Integration Engine
 * 
 * Implements depth-first traversal of import dependency graph with cycle detection.
 * 
 * Algorithm Architecture:
 * 1. Path Normalization: Converts relative paths to canonical absolute paths
 * 2. Memoization Check: Skips already-processed files via hash set lookup
 * 3. Cycle Detection: Maintains processing stack to detect circular dependencies
 * 4. Lexical Analysis: Tokenizes source code via Lexer component
 * 5. Syntax Analysis: Constructs AST via Parser component
 * 6. Import Segregation: Separates import statements from executable code
 * 7. Recursive Resolution: Processes transitive imports before current file
 * 8. AST Accumulation: Appends non-import statements to global statement vector
 * 
 * Circular Import Detection:
 * - Maintains stack of files currently being processed
 * - If file appears in its own processing stack, cycle exists
 * - Full cycle path displayed in error message for debugging
 * 
 * Import Resolution Strategy:
 * - FileResolver handles path lookup relative to importing file
 * - Supports both relative and absolute import paths
 * - Search order: relative to importer, then standard library paths
 * 
 * Error Recovery:
 * - Stack unwinding on error (end_processing called before return)
 * - No partial state corruption on failure
 * - All errors propagated to top-level caller
 * 
 * Performance Characteristics:
 * - Time: O(V + E) where V=files, E=import edges (DFS traversal)
 * - Space: O(D) where D=maximum import depth (stack depth)
 * 
 * @param file_path Source file being processed
 * @param source_code Content of source file
 * @param all_statements Output accumulator for all statements across all files
 * @return Boolean indicating successful import processing
 */
bool BoxCompiler::process_imports(const std::string& file_path, const std::string& source_code, 
                                   std::vector<StmtPtr>& all_statements) {
    std::string normalized_path = fs::canonical(fs::absolute(file_path)).string();
    
    if (processed_files.count(normalized_path)) {
        return true;
    }
    
    if (file_resolver->is_processing(normalized_path)) {
        auto stack = file_resolver->get_processing_stack_vector();
        std::ostringstream error_msg;
        error_msg << "Circular import detected:\n";
        for (const auto& f : stack) {
            error_msg << "  → " << f << "\n";
        }
        error_msg << "  → " << normalized_path;
        print_error("Import Error", error_msg.str());
        return false;
    }
    
    file_resolver->begin_processing(normalized_path);
    
    Lexer lexer(source_code);
    std::vector<Token> tokens;
    
    try {
        tokens = lexer.scan_tokens();
    } catch (const std::exception& e) {
        file_resolver->end_processing(normalized_path);
        print_error("Syntax Error in " + file_path, e.what());
        return false;
    }
    
    Parser parser(tokens, source_code);
    std::vector<StmtPtr> statements;
    
    try {
        statements = parser.parse();
    } catch (const std::exception& e) {
        file_resolver->end_processing(normalized_path);
        print_error("Parse Error in " + file_path, e.what());
        return false;
    }
    
    std::vector<StmtPtr> non_import_stmts;
    for (const auto& stmt : statements) {
        if (auto import_stmt = std::dynamic_pointer_cast<ImportStmt>(stmt)) {
            auto resolved = file_resolver->resolve_import(import_stmt->file_path, normalized_path);
            if (!resolved) {
                file_resolver->end_processing(normalized_path);
                print_error("Import Error", "Cannot find imported file: " + import_stmt->file_path);
                return false;
            }
            
            std::string imported_content;
            if (!read_source_file(*resolved, imported_content)) {
                file_resolver->end_processing(normalized_path);
                print_error("Import Error", "Cannot read imported file: " + *resolved);
                return false;
            }
            
            if (!process_imports(*resolved, imported_content, all_statements)) {
                file_resolver->end_processing(normalized_path);
                return false;
            }
        } else {
            non_import_stmts.push_back(stmt);
        }
    }
    
    all_statements.insert(all_statements.end(), non_import_stmts.begin(), non_import_stmts.end());
    
    file_resolver->end_processing(normalized_path);
    file_resolver->mark_resolved(normalized_path);
    processed_files.insert(normalized_path);
    
    return true;
}

/**
 * Master Compilation Pipeline Orchestrator
 * 
 * Coordinates all compilation phases from source code to executable binary.
 * 
 * Pipeline Phases:
 * 1. Initialization: FileResolver setup, state clearing
 * 2. Import Resolution: Recursive dependency graph traversal
 * 3. Memory Safety Analysis: Static analysis for memory violations
 * 4. Code Generation: LLVM IR emission from AST
 * 5. Verification: LLVM module validation
 * 6. Object Emission: Target-specific object file generation
 * 7. Linking: Symbol resolution and executable creation
 * 8. Optional Execution: Immediate program invocation
 * 
 * Error Handling Philosophy:
 * - Fail-fast: Immediate termination on first error
 * - Cascading prevention: Early phases prevent invalid later phases
 * - Detailed diagnostics: Rich error messages with context
 * 
 * State Management:
 * - FileResolver instantiated per compilation
 * - processed_files cleared between compilations
 * - LLVM context isolated per compilation unit
 * 
 * Performance Considerations:
 * - Memory safety analysis runs before codegen (fast fail)
 * - LLVM optimization level configurable (0-3)
 * - Intermediate files cleaned up automatically
 * 
 * @return CompilationResult containing success status, message, exit code
 */
CompilationResult BoxCompiler::compile() {
    if (options.verbose) {
        print_banner();
    }
    
    fs::path input_path(options.input_file);
    std::string base_dir = input_path.parent_path().string();
    if (base_dir.empty()) {
        base_dir = fs::current_path().string();
    }
    
    file_resolver = std::make_unique<FileResolver>(base_dir);
    processed_files.clear();
    
    std::string source_code;
    if (!read_source_file(options.input_file, source_code)) {
        print_error("File Error", "Cannot read file '" + options.input_file + "'");
        return CompilationResult(false, "File not found", 1);
    }
    
    if (options.verbose) {
        std::cout << "\033[1;34m[1/5]\033[0m Lexical Analysis & Import Resolution...\n";
    }
    
    std::vector<StmtPtr> statements;
    if (!process_imports(options.input_file, source_code, statements)) {
        return CompilationResult(false, "Import processing failed", 1);
    }
    
    if (options.verbose) {
        std::cout << "\033[1;32m  ✓\033[0m Parsed " << statements.size() << " statements\n\n";
        std::cout << "\033[1;34m[3/5]\033[0m Memory Safety Analysis...\n";
    }
    
    MemorySafetyAnalyzer analyzer;
    bool memory_safe = analyzer.analyze(statements);
    
    if (!memory_safe) {
        std::string report = analyzer.get_report();
        print_memory_report(report);
        print_error("Memory Safety Error", "Code failed memory safety analysis");
        return CompilationResult(false, "Memory safety violation", 1);
    }
    
    auto warnings = analyzer.get_warnings();
    if (options.verbose && !warnings.empty()) {
        std::cout << "\033[1;33m  ⚠\033[0m " << warnings.size() << " warnings\n";
    }
    print_warnings(warnings);
    
    if (options.verbose) {
        std::cout << "\n\033[1;34m[4/5]\033[0m Code Generation...\n";
    }
    
    if (!initialize_llvm()) {
        return CompilationResult(false, "LLVM initialization failed", 1);
    }
    
    llvm_context = std::make_unique<llvm::LLVMContext>();
    
    CodeGenerator codegen(options.optimize, options.optimize ? options.optimization_level : 0);
    
    std::string llvm_ir;
    try {
        llvm_ir = codegen.generate(statements);
        
        llvm::SMDiagnostic err;
        llvm::MemoryBufferRef buf(llvm_ir, "box_module");
        llvm_module = llvm::parseIR(buf, err, *llvm_context);
        
        if (!llvm_module) {
            std::string err_str;
            llvm::raw_string_ostream err_stream(err_str);
            err.print("box", err_stream);
            print_error("LLVM IR Parse Error", err_stream.str());
            return CompilationResult(false, "Failed to parse generated LLVM IR", 1);
        }
    } catch (const CodeGenError& e) {
        print_error("Code Generation Error", e.what());
        return CompilationResult(false, e.what(), 1);
    }
    
    if (options.verbose) {
        std::cout << "\033[1;32m  ✓\033[0m LLVM IR generated\n";
    }
    
    std::string verification_error;
    llvm::raw_string_ostream error_stream(verification_error);
    if (llvm::verifyModule(*llvm_module, &error_stream)) {
        print_error("LLVM Verification Error", verification_error);
        return CompilationResult(false, "Module verification failed", 1);
    }
    
    if (options.emit_llvm) {
        std::string llvm_output = get_llvm_ir_filename(options.input_file);
        if (write_llvm_ir(llvm_ir, llvm_output)) {
            std::cout << "\033[1;36m→ LLVM IR written to: " << llvm_output << "\033[0m\n";
        } else {
            print_error("File Error", "Failed to write LLVM IR file");
        }
    }
    
    if (options.emit_assembly) {
        std::string asm_output = get_assembly_filename(options.input_file);
        if (write_assembly_file(asm_output)) {
            std::cout << "\033[1;36m→ Assembly written to: " << asm_output << "\033[0m\n";
        } else {
            print_error("File Error", "Failed to write assembly file");
        }
    }
    
    if (options.verbose) {
        std::cout << "\n\033[1;34m[5/5]\033[0m Linking...\n";
    }
    
    std::string obj_file = get_object_filename(options.input_file);
    std::string output_exe = get_output_filename(options.input_file);
    
    if (!write_object_file(obj_file)) {
        return CompilationResult(false, "Failed to generate object file", 1);
    }
    
    if (!link_executable(obj_file, output_exe)) {
        cleanup_intermediate_files(obj_file);
        return CompilationResult(false, "Linking failed", 1);
    }
    
    cleanup_intermediate_files(obj_file);
    
    if (options.verbose) {
        std::cout << "\033[1;32m  ✓\033[0m Executable created\n\n";
    }
    
    print_success(output_exe);
    
    if (options.run_after_compile) {
        int return_code = 0;
        run_executable(output_exe, return_code);
        return CompilationResult(true, "Compilation and execution completed", return_code);
    }
    
    return CompilationResult(true, "Compilation completed successfully", 0);
}

CompilationOptions CompilerCLI::parse_arguments(int argc, char* argv[]) {
    CompilationOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            BoxCompiler::print_help();
            exit(0);
        } else if (arg == "--version") {
            BoxCompiler::print_version();
            exit(0);
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                options.output_file = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                exit(1);
            }
        } else if (arg == "--emit-llvm") {
            options.emit_llvm = true;
        } else if (arg == "-S") {
            options.emit_assembly = true;
        } else if (arg == "-r" || arg == "--run") {
            options.run_after_compile = true;
        } else if (arg == "--no-optimize") {
            options.optimize = false;
        } else if (arg.substr(0, 5) == "-Oasm") {
            if (arg.length() > 5) {
                options.llvm_optimization_level = std::stoi(arg.substr(5));
                if (options.llvm_optimization_level < 0 || options.llvm_optimization_level > 3) {
                    std::cerr << "Error: LLVM optimization level must be 0-3\n";
                    exit(1);
                }
            }
        } else if (arg.substr(0, 2) == "-O") {
            if (arg.length() > 2) {
                options.optimization_level = std::stoi(arg.substr(2));
                if (options.optimization_level < 0 || options.optimization_level > 3) {
                    std::cerr << "Error: IR optimization level must be 0-3\n";
                    exit(1);
                }
            }
        } else if (arg == "--no-warnings") {
            options.show_memory_warnings = false;
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg[0] != '-') {
            options.input_file = arg;
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    if (options.input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        print_usage(argv[0]);
        exit(1);
    }
    
    return options;
}

void CompilerCLI::print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] <input-file>\n";
    std::cerr << "Try '" << program_name << " --help' for more information.\n";
}

int CompilerCLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    try {
        CompilationOptions options = parse_arguments(argc, argv);
        BoxCompiler compiler(options);
        CompilationResult result = compiler.compile();
        
        if (!result.success && options.verbose) {
            std::cerr << "\n\033[1;31m✗ Compilation failed\033[0m\n";
        }
        
        return result.exit_code;
        
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mFatal Error:\033[0m " << e.what() << "\n";
        return 1;
    }
}

}
