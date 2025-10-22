#ifndef BOX_CODEGEN_HPP
#define BOX_CODEGEN_HPP

#include "box_value.hpp"
#include "environment.hpp"
#include "codegen_error.hpp"
#include "../parser/parser.hpp"
#include "../optimizer/optimizer.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace box {

class CodeGenerator {
public:
    explicit CodeGenerator(bool optimize = true, int optimize_level = 3);
    ~CodeGenerator() = default;

    std::string generate(const std::vector<StmtPtr>& statements);

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::shared_ptr<Environment> env;
    
    llvm::Function* current_function;
    std::unordered_map<std::string, std::pair<llvm::Function*, std::vector<std::string>>> functions;
    std::unordered_map<std::string, llvm::GlobalVariable*> string_constants;
    std::unordered_set<llvm::Value*> allocated_arrays;
    llvm::BasicBlock* break_block;
    bool in_unsafe_block;
    std::unordered_map<std::string, llvm::Value*> llvm_inline_vars;

    bool optimize;
    int optimize_level;
    std::unique_ptr<Optimizer> optimizer;

    llvm::Type* double_type;
    llvm::Type* i8_type;
    llvm::Type* i1_type;
    llvm::Type* i32_type;
    llvm::Type* i64_type;
    llvm::Type* void_type;
    llvm::PointerType* i8_ptr_type;
    llvm::PointerType* double_ptr_type;
    
    llvm::StructType* array_struct_type;
    llvm::PointerType* array_ptr_type;
    llvm::StructType* dict_entry_type;
    llvm::StructType* dict_struct_type;
    llvm::PointerType* dict_ptr_type;

    llvm::Function* printf_func;
    llvm::Function* scanf_func;
    llvm::Function* malloc_func;
    llvm::Function* free_func;
    llvm::Function* memset_func;
    llvm::Function* exit_func;
    llvm::Function* strcmp_func;
    llvm::Function* fopen_func;
    llvm::Function* fclose_func;
    llvm::Function* fgets_func;
    llvm::Function* fputs_func;
    llvm::Function* fread_func;
    llvm::Function* fwrite_func;
    llvm::Function* fseek_func;
    llvm::Function* ftell_func;
    llvm::Function* rewind_func;
    llvm::Function* feof_func;
    llvm::Function* remove_func;
    llvm::Function* strlen_func;
    llvm::Function* strcpy_func;
    llvm::Function* strcat_func;
    llvm::Function* access_func;
    llvm::GlobalVariable* stdin_global;
    llvm::GlobalVariable* stdout_global;

    void declare_runtime_functions();
    
    void visit_stmt(const StmtPtr& stmt);
    void visit_print_stmt(PrintStmt* stmt);
    void visit_var_stmt(VarStmt* stmt);
    void visit_block(Block* stmt);
    void visit_if_stmt(IfStmt* stmt);
    void visit_while_stmt(WhileStmt* stmt);
    void visit_function_stmt(FunctionStmt* stmt);
    void visit_return_stmt(ReturnStmt* stmt);
    void visit_break_stmt(BreakStmt* stmt);
    void visit_switch_stmt(SwitchStmt* stmt);
    void visit_unsafe_block(UnsafeBlock* stmt);
    void visit_llvm_inline(LLVMInlineStmt* stmt);

    BoxValue visit_expr(const ExprPtr& expr);
    BoxValue visit_literal(Literal* expr);
    BoxValue visit_variable(Variable* expr);
    BoxValue visit_assign(Assign* expr);
    BoxValue visit_binary(Binary* expr);
    BoxValue visit_unary(Unary* expr);
    BoxValue visit_logical(Logical* expr);
    BoxValue visit_call(Call* expr);
    BoxValue visit_array_literal(ArrayLiteral* expr);
    BoxValue visit_dict_literal(DictLiteral* expr);
    BoxValue visit_index_get(IndexGet* expr);
    BoxValue visit_index_set(IndexSet* expr);

    void print_array(const BoxValue& array_value);
    void print_dict(const BoxValue& dict_value);
    
    llvm::Value* get_or_create_string_constant(const std::string& value);
    BoxValue create_string_constant(const std::string& value);
    
    void check_array_bounds(llvm::Value* index_i64, llvm::Value* length, const Token& token);
    void check_division_by_zero(llvm::Value* divisor, const Token& token, const std::string& op_name);
    
    BoxValue array_get(const BoxValue& array_value, IndexGet* expr);
    BoxValue dict_get(const BoxValue& dict_value, IndexGet* expr);
    BoxValue array_set(const BoxValue& array_value, IndexSet* expr);
    BoxValue dict_set(const BoxValue& dict_value, IndexSet* expr);
    
    llvm::Value* string_compare(llvm::Value* str1, llvm::Value* str2);
    BoxValue dict_has(const BoxValue& dict_value, const BoxValue& key_value);
    BoxValue dict_keys(const BoxValue& dict_value);
    BoxValue dict_values(const BoxValue& dict_value);
    
    llvm::Value* to_boolean(const BoxValue& value);
    llvm::Value* to_number(const BoxValue& value);

    BoxValue builtin_malloc(const ExprPtr& size_expr);
    BoxValue builtin_calloc(const ExprPtr& count_expr, const ExprPtr& size_expr);
    BoxValue builtin_realloc(const ExprPtr& ptr_expr, const ExprPtr& new_size_expr);
    BoxValue builtin_free(const ExprPtr& ptr_expr);
    BoxValue builtin_addr_of(const ExprPtr& var_expr);
    BoxValue builtin_deref(const ExprPtr& ptr_expr);
    BoxValue builtin_input();
    BoxValue builtin_input_num();
    BoxValue builtin_read_file(const BoxValue& path);
    BoxValue builtin_write_file(const BoxValue& path, const BoxValue& content);
    BoxValue builtin_append_file(const BoxValue& path, const BoxValue& content);
    BoxValue builtin_file_exists(const BoxValue& path);

    std::string process_llvm_inline(const std::string& llvm_code, 
                                    const std::unordered_map<std::string, std::string>& variables_map);
    void inject_llvm_ir(const std::string& llvm_ir_code);
    void emit_llvm_instruction(const std::string& instruction);
    void emit_add_instruction(const std::string& result_var, const std::string& operation);
    void emit_sub_instruction(const std::string& result_var, const std::string& operation);
    void emit_mul_instruction(const std::string& result_var, const std::string& operation);
    void emit_call_instruction(const std::string& result_var, const std::string& operation);
    void emit_ret_instruction(const std::string& instruction);
    void emit_store_instruction(const std::string& instruction);
    void emit_br_instruction(const std::string& instruction);
    bool is_number(const std::string& s) const;
    llvm::Value* resolve_llvm_value(const std::string& value_str, const std::string& type_str);
};

}

#endif
