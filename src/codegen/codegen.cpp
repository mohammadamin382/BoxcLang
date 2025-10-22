#include "codegen.hpp"
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include <regex>

namespace box {

CodeGenerator::CodeGenerator(bool optimize, int optimize_level)
    : context(std::make_unique<llvm::LLVMContext>())
    , module(std::make_unique<llvm::Module>("box_module", *context))
    , builder(std::make_unique<llvm::IRBuilder<>>(*context))
    , env(std::make_shared<Environment>())
    , current_function(nullptr)
    , break_block(nullptr)
    , in_unsafe_block(false)
    , optimize(optimize)
    , optimize_level(optimize_level) {

    if (this->optimize) {
        OptimizerConfig config;
        config.optimize_level = optimize_level;
        optimizer = std::make_unique<Optimizer>(config);
    }

    double_type = llvm::Type::getDoubleTy(*context);
    i8_type = llvm::Type::getInt8Ty(*context);
    i1_type = llvm::Type::getInt1Ty(*context);
    i32_type = llvm::Type::getInt32Ty(*context);
    i64_type = llvm::Type::getInt64Ty(*context);
    void_type = llvm::Type::getVoidTy(*context);
    i8_ptr_type = llvm::PointerType::get(i8_type, 0);
    double_ptr_type = llvm::PointerType::get(double_type, 0);

    array_struct_type = llvm::StructType::create(*context, {
        i64_type,
        double_ptr_type
    }, "ArrayStruct");
    array_ptr_type = llvm::PointerType::get(array_struct_type, 0);

    dict_entry_type = llvm::StructType::create(*context, {
        double_type,
        double_type,
        i1_type
    }, "DictEntry");

    dict_struct_type = llvm::StructType::create(*context, {
        i64_type,
        llvm::PointerType::get(dict_entry_type, 0)
    }, "DictStruct");
    dict_ptr_type = llvm::PointerType::get(dict_struct_type, 0);

    declare_runtime_functions();
}

void CodeGenerator::declare_runtime_functions() {
    auto printf_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type}, true);
    printf_func = llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf", module.get());

    auto scanf_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type}, true);
    scanf_func = llvm::Function::Create(scanf_ty, llvm::Function::ExternalLinkage, "scanf", module.get());

    auto malloc_ty = llvm::FunctionType::get(i8_ptr_type, {i64_type}, false);
    malloc_func = llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc", module.get());

    auto free_ty = llvm::FunctionType::get(void_type, {i8_ptr_type}, false);
    free_func = llvm::Function::Create(free_ty, llvm::Function::ExternalLinkage, "free", module.get());

    auto memset_ty = llvm::FunctionType::get(i8_ptr_type, {i8_ptr_type, i32_type, i64_type}, false);
    memset_func = llvm::Function::Create(memset_ty, llvm::Function::ExternalLinkage, "memset", module.get());

    auto exit_ty = llvm::FunctionType::get(void_type, {i32_type}, false);
    exit_func = llvm::Function::Create(exit_ty, llvm::Function::ExternalLinkage, "exit", module.get());

    auto strcmp_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type, i8_ptr_type}, false);
    strcmp_func = llvm::Function::Create(strcmp_ty, llvm::Function::ExternalLinkage, "strcmp", module.get());

    auto fopen_ty = llvm::FunctionType::get(i8_ptr_type, {i8_ptr_type, i8_ptr_type}, false);
    fopen_func = llvm::Function::Create(fopen_ty, llvm::Function::ExternalLinkage, "fopen", module.get());

    auto fclose_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type}, false);
    fclose_func = llvm::Function::Create(fclose_ty, llvm::Function::ExternalLinkage, "fclose", module.get());

    auto fgets_ty = llvm::FunctionType::get(i8_ptr_type, {i8_ptr_type, i32_type, i8_ptr_type}, false);
    fgets_func = llvm::Function::Create(fgets_ty, llvm::Function::ExternalLinkage, "fgets", module.get());

    auto fputs_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type, i8_ptr_type}, false);
    fputs_func = llvm::Function::Create(fputs_ty, llvm::Function::ExternalLinkage, "fputs", module.get());

    auto fread_ty = llvm::FunctionType::get(i64_type, {i8_ptr_type, i64_type, i64_type, i8_ptr_type}, false);
    fread_func = llvm::Function::Create(fread_ty, llvm::Function::ExternalLinkage, "fread", module.get());

    auto fwrite_ty = llvm::FunctionType::get(i64_type, {i8_ptr_type, i64_type, i64_type, i8_ptr_type}, false);
    fwrite_func = llvm::Function::Create(fwrite_ty, llvm::Function::ExternalLinkage, "fwrite", module.get());

    auto fseek_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type, i64_type, i32_type}, false);
    fseek_func = llvm::Function::Create(fseek_ty, llvm::Function::ExternalLinkage, "fseek", module.get());

    auto ftell_ty = llvm::FunctionType::get(i64_type, {i8_ptr_type}, false);
    ftell_func = llvm::Function::Create(ftell_ty, llvm::Function::ExternalLinkage, "ftell", module.get());

    auto rewind_ty = llvm::FunctionType::get(void_type, {i8_ptr_type}, false);
    rewind_func = llvm::Function::Create(rewind_ty, llvm::Function::ExternalLinkage, "rewind", module.get());

    auto feof_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type}, false);
    feof_func = llvm::Function::Create(feof_ty, llvm::Function::ExternalLinkage, "feof", module.get());

    auto remove_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type}, false);
    remove_func = llvm::Function::Create(remove_ty, llvm::Function::ExternalLinkage, "remove", module.get());

    auto strlen_ty = llvm::FunctionType::get(i64_type, {i8_ptr_type}, false);
    strlen_func = llvm::Function::Create(strlen_ty, llvm::Function::ExternalLinkage, "strlen", module.get());

    auto strcpy_ty = llvm::FunctionType::get(i8_ptr_type, {i8_ptr_type, i8_ptr_type}, false);
    strcpy_func = llvm::Function::Create(strcpy_ty, llvm::Function::ExternalLinkage, "strcpy", module.get());

    auto strcat_ty = llvm::FunctionType::get(i8_ptr_type, {i8_ptr_type, i8_ptr_type}, false);
    strcat_func = llvm::Function::Create(strcat_ty, llvm::Function::ExternalLinkage, "strcat", module.get());

    auto access_ty = llvm::FunctionType::get(i32_type, {i8_ptr_type, i32_type}, false);
    access_func = llvm::Function::Create(access_ty, llvm::Function::ExternalLinkage, "access", module.get());

    stdin_global = new llvm::GlobalVariable(*module, i8_ptr_type, false, 
                                            llvm::GlobalValue::ExternalLinkage, nullptr, "stdin");

    stdout_global = new llvm::GlobalVariable(*module, i8_ptr_type, false, 
                                             llvm::GlobalValue::ExternalLinkage, nullptr, "stdout");
}

std::string CodeGenerator::generate(const std::vector<StmtPtr>& statements) {
    std::vector<StmtPtr> stmts = statements;

    if (optimizer) {
        stmts = optimizer->optimize(stmts);
    }

    auto func_type = llvm::FunctionType::get(i32_type, false);
    auto main_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "main", module.get());
    auto block = llvm::BasicBlock::Create(*context, "entry", main_func);
    builder->SetInsertPoint(block);
    current_function = main_func;

    try {
        for (const auto& stmt : stmts) {
            visit_stmt(stmt);
        }

        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateRet(llvm::ConstantInt::get(i32_type, 0));
        }
    } catch (const CodeGenError&) {
        throw;
    } catch (const std::exception& e) {
        throw CodeGenError("Unexpected error during code generation: " + std::string(e.what()));
    }

    std::string ir_string;
    llvm::raw_string_ostream os(ir_string);
    module->print(os, nullptr);
    return os.str();
}

void CodeGenerator::visit_stmt(const StmtPtr& stmt) {
    if (builder->GetInsertBlock()->getTerminator()) {
        return;
    }

    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        visit_expr(expr_stmt->expression);
    } else if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        visit_print_stmt(print_stmt);
    } else if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        visit_var_stmt(var_stmt);
    } else if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        visit_block(block);
    } else if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        visit_if_stmt(if_stmt);
    } else if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        visit_while_stmt(while_stmt);
    } else if (auto* switch_stmt = dynamic_cast<SwitchStmt*>(stmt.get())) {
        visit_switch_stmt(switch_stmt);
    } else if (auto* function_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        visit_function_stmt(function_stmt);
    } else if (auto* return_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        visit_return_stmt(return_stmt);
    } else if (auto* break_stmt = dynamic_cast<BreakStmt*>(stmt.get())) {
        visit_break_stmt(break_stmt);
    } else if (auto* unsafe_block = dynamic_cast<UnsafeBlock*>(stmt.get())) {
        visit_unsafe_block(unsafe_block);
    } else if (auto* llvm_inline = dynamic_cast<LLVMInlineStmt*>(stmt.get())) {
        visit_llvm_inline(llvm_inline);
    } else {
        throw CodeGenError("Unknown statement type");
    }
}

}