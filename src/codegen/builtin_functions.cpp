#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

BoxValue CodeGenerator::builtin_malloc(const ExprPtr& size_expr) {
    BoxValue size_val = visit_expr(size_expr);

    if (size_val.box_type != BoxType::NUMBER) {
        std::string hint = "malloc() size must be a number.\n";
        hint += "       Got: " + size_val.box_type;
        throw CodeGenError("malloc() size must be a number", std::nullopt, hint);
    }

    llvm::Value* size_i64 = builder->CreateFPToSI(size_val.ir_value, i64_type, "malloc_size");

    llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
    llvm::Value* is_negative = builder->CreateICmpSLT(size_i64, zero, "size_negative");

    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "malloc_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "malloc_ok", current_function);

    builder->CreateCondBr(is_negative, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: malloc() size cannot be negative\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    llvm::Value* ptr = builder->CreateCall(malloc_func, {size_i64}, "malloc_ptr");

    return BoxValue(ptr, BoxType::POINTER, true);
}

BoxValue CodeGenerator::builtin_calloc(const ExprPtr& count_expr, const ExprPtr& size_expr) {
    BoxValue count_val = visit_expr(count_expr);
    BoxValue size_val = visit_expr(size_expr);

    if (count_val.box_type != BoxType::NUMBER) {
        throw CodeGenError("calloc() count must be a number");
    }

    if (size_val.box_type != BoxType::NUMBER) {
        throw CodeGenError("calloc() size must be a number");
    }

    llvm::Value* count_i64 = builder->CreateFPToSI(count_val.ir_value, i64_type, "calloc_count");
    llvm::Value* size_i64 = builder->CreateFPToSI(size_val.ir_value, i64_type, "calloc_size");

    llvm::Value* total_size = builder->CreateMul(count_i64, size_i64, "total_size");

    llvm::Value* ptr = builder->CreateCall(malloc_func, {total_size}, "calloc_ptr");

    builder->CreateCall(memset_func, {ptr, llvm::ConstantInt::get(i32_type, 0), total_size});

    return BoxValue(ptr, BoxType::POINTER, true);
}

BoxValue CodeGenerator::builtin_realloc(const ExprPtr& ptr_expr, const ExprPtr& new_size_expr) {
    BoxValue ptr_val = visit_expr(ptr_expr);
    BoxValue new_size_val = visit_expr(new_size_expr);

    if (ptr_val.box_type != BoxType::POINTER) {
        std::string hint = "realloc() first argument must be a pointer.\n";
        hint += "       Got: " + ptr_val.box_type;
        throw CodeGenError("realloc() requires a pointer", std::nullopt, hint);
    }

    if (new_size_val.box_type != BoxType::NUMBER) {
        std::string hint = "realloc() size must be a number.\n";
        hint += "       Got: " + new_size_val.box_type;
        throw CodeGenError("realloc() size must be a number", std::nullopt, hint);
    }

    llvm::Value* new_size_i64 = builder->CreateFPToSI(new_size_val.ir_value, i64_type, "realloc_size");

    llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
    llvm::Value* is_negative = builder->CreateICmpSLT(new_size_i64, zero, "size_negative");

    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "realloc_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "realloc_ok", current_function);

    builder->CreateCondBr(is_negative, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: realloc() size cannot be negative\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    llvm::FunctionType* realloc_type = llvm::FunctionType::get(
        i8_ptr_type,
        {i8_ptr_type, i64_type},
        false
    );
    llvm::FunctionCallee realloc_callee = module->getOrInsertFunction("realloc", realloc_type);
    llvm::Function* realloc_func_ptr = llvm::cast<llvm::Function>(realloc_callee.getCallee());

    llvm::Value* new_ptr = builder->CreateCall(realloc_func_ptr, {ptr_val.ir_value, new_size_i64}, "realloc_ptr");

    llvm::Value* null_check = builder->CreateICmpEQ(new_ptr, llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* null_error_block = llvm::BasicBlock::Create(*context, "realloc_null_error", current_function);
    llvm::BasicBlock* success_block = llvm::BasicBlock::Create(*context, "realloc_success", current_function);

    builder->CreateCondBr(null_check, null_error_block, success_block);

    builder->SetInsertPoint(null_error_block);
    llvm::Value* null_error_msg = get_or_create_string_constant("Runtime Error: realloc() failed - out of memory\n");
    builder->CreateCall(printf_func, {null_error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(success_block);

    return BoxValue(new_ptr, BoxType::POINTER, true);
}

BoxValue CodeGenerator::builtin_free(const ExprPtr& ptr_expr) {
    BoxValue ptr_val = visit_expr(ptr_expr);

    if (ptr_val.box_type != BoxType::POINTER) {
        std::string hint = "free() requires a pointer argument.\n";
        hint += "       Got: " + ptr_val.box_type;
        throw CodeGenError("free() requires a pointer", std::nullopt, hint);
    }

    builder->CreateCall(free_func, {ptr_val.ir_value});

    return BoxValue(llvm::ConstantFP::get(double_type, 0.0), BoxType::NIL);
}

BoxValue CodeGenerator::builtin_addr_of(const ExprPtr& var_expr) {
    if (!dynamic_cast<Variable*>(var_expr.get())) {
        std::string hint = "addr_of() requires a variable name.\n";
        hint += "       Example: addr_of(myvar)";
        throw CodeGenError("addr_of() requires a variable", std::nullopt, hint);
    }

    std::string var_name = dynamic_cast<Variable*>(var_expr.get())->name.lexeme;
    auto var = env->get(var_name);

    if (!var.has_value()) {
        std::string hint = "Variable '" + var_name + "' not found.\n";
        hint += "       Declare it first with: var " + var_name + " = value;";
        throw CodeGenError("Undefined variable '" + var_name + "'", 
                         dynamic_cast<Variable*>(var_expr.get())->name, hint);
    }

    if (var->box_type == BoxType::NUMBER) {
        llvm::Value* ptr_bitcast = builder->CreateBitCast(var->ir_value, i8_ptr_type, "addr_" + var_name);
        return BoxValue(ptr_bitcast, BoxType::POINTER, true);
    } else {
        std::string hint = "Can only take address of number variables currently.\n";
        hint += "       Variable '" + var_name + "' has type: " + var->box_type;
        throw CodeGenError("Cannot take address of " + var->box_type, 
                         dynamic_cast<Variable*>(var_expr.get())->name, hint);
    }
}

BoxValue CodeGenerator::builtin_deref(const ExprPtr& ptr_expr) {
    BoxValue ptr_val = visit_expr(ptr_expr);

    if (ptr_val.box_type != BoxType::POINTER) {
        std::string hint = "deref() requires a pointer argument.\n";
        hint += "       Got: " + ptr_val.box_type;
        throw CodeGenError("deref() requires a pointer", std::nullopt, hint);
    }

    llvm::Value* null_check = builder->CreateICmpEQ(ptr_val.ir_value, 
                                                     llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "deref_null_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "deref_ok", current_function);

    builder->CreateCondBr(null_check, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: Null pointer dereference\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    llvm::Value* double_ptr = builder->CreateBitCast(ptr_val.ir_value, double_ptr_type, "deref_cast");
    llvm::Value* value = builder->CreateLoad(double_type, double_ptr, "deref_value");

    return BoxValue(value, BoxType::NUMBER, true);
}

BoxValue CodeGenerator::builtin_input() {
    const size_t buffer_size = 4096;
    llvm::Type* buffer_type = llvm::ArrayType::get(i8_type, buffer_size);
    llvm::Value* buffer = builder->CreateAlloca(buffer_type, nullptr, "input_buffer");
    llvm::Value* buffer_ptr = builder->CreateConstGEP2_32(buffer_type, buffer, 0, 0);

    llvm::BasicBlock* current_block = builder->GetInsertBlock();

    llvm::Value* stdin_ptr = builder->CreateLoad(i8_ptr_type, stdin_global, "stdin");
    llvm::Value* result = builder->CreateCall(fgets_func, {buffer_ptr, 
                                               llvm::ConstantInt::get(i32_type, buffer_size), stdin_ptr});

    llvm::Value* null_check = builder->CreateICmpEQ(result, llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "input_error", current_function);
    llvm::BasicBlock* continue_block = llvm::BasicBlock::Create(*context, "input_ok", current_function);

    builder->CreateCondBr(null_check, error_block, continue_block);

    builder->SetInsertPoint(error_block);
    llvm::Type* empty_type = llvm::ArrayType::get(i8_type, 1);
    llvm::GlobalVariable* empty_global = new llvm::GlobalVariable(
        *module, empty_type, true, llvm::GlobalValue::InternalLinkage,
        llvm::ConstantDataArray::get(*context, llvm::ArrayRef<uint8_t>{0}), "empty_input");
    llvm::Value* empty_ptr = builder->CreateConstGEP2_32(empty_type, empty_global, 0, 0);
    builder->CreateBr(continue_block);

    builder->SetInsertPoint(continue_block);
    llvm::PHINode* result_phi = builder->CreatePHI(i8_ptr_type, 2, "input_result");
    result_phi->addIncoming(empty_ptr, error_block);
    result_phi->addIncoming(buffer_ptr, current_block);

    llvm::Value* input_len = builder->CreateCall(strlen_func, {result_phi});
    llvm::Value* zero_len = builder->CreateICmpEQ(input_len, llvm::ConstantInt::get(i64_type, 0));

    llvm::BasicBlock* remove_newline_block = llvm::BasicBlock::Create(*context, "remove_newline", current_function);
    llvm::BasicBlock* skip_newline = llvm::BasicBlock::Create(*context, "skip_newline", current_function);

    builder->CreateCondBr(zero_len, skip_newline, remove_newline_block);

    builder->SetInsertPoint(remove_newline_block);
    llvm::Value* last_char_idx = builder->CreateSub(input_len, llvm::ConstantInt::get(i64_type, 1));
    llvm::Value* last_char_ptr = builder->CreateGEP(i8_type, result_phi, last_char_idx);
    llvm::Value* last_char = builder->CreateLoad(i8_type, last_char_ptr);
    llvm::Value* is_newline = builder->CreateICmpEQ(last_char, llvm::ConstantInt::get(i8_type, 10));

    llvm::BasicBlock* replace_block = llvm::BasicBlock::Create(*context, "replace_newline", current_function);
    builder->CreateCondBr(is_newline, replace_block, skip_newline);

    builder->SetInsertPoint(replace_block);
    builder->CreateStore(llvm::ConstantInt::get(i8_type, 0), last_char_ptr);
    builder->CreateBr(skip_newline);

    builder->SetInsertPoint(skip_newline);

    return BoxValue(result_phi, BoxType::STRING);
}

BoxValue CodeGenerator::builtin_input_num() {
    llvm::Value* result_alloca = builder->CreateAlloca(double_type, nullptr, "input_num_result");

    llvm::Value* fmt_str = get_or_create_string_constant("%lf");
    llvm::Value* stdin_ptr = builder->CreateLoad(i8_ptr_type, stdin_global, "stdin");

    llvm::Value* scan_result = builder->CreateCall(scanf_func, {fmt_str, result_alloca});

    llvm::Value* success = builder->CreateICmpEQ(scan_result, llvm::ConstantInt::get(i32_type, 1));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "input_num_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "input_num_ok", current_function);

    builder->CreateCondBr(success, ok_block, error_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: Invalid number input\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);
    llvm::Value* result = builder->CreateLoad(double_type, result_alloca);

    return BoxValue(result, BoxType::NUMBER);
}

BoxValue CodeGenerator::builtin_read_file(const BoxValue& path) {
    llvm::Value* mode_str = get_or_create_string_constant("r");
    llvm::Value* file_ptr = builder->CreateCall(fopen_func, {path.ir_value, mode_str}, "file_handle");

    llvm::Value* null_check = builder->CreateICmpEQ(file_ptr, llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "file_open_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "file_open_ok", current_function);

    builder->CreateCondBr(null_check, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: Cannot open file for reading\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    builder->CreateCall(fseek_func, {file_ptr, llvm::ConstantInt::get(i64_type, 0), 
                                     llvm::ConstantInt::get(i32_type, 2)});
    llvm::Value* file_size = builder->CreateCall(ftell_func, {file_ptr}, "file_size");
    builder->CreateCall(rewind_func, {file_ptr});

    llvm::Value* buffer_size = builder->CreateAdd(file_size, llvm::ConstantInt::get(i64_type, 1));
    llvm::Value* buffer = builder->CreateCall(malloc_func, {buffer_size}, "file_buffer");

    builder->CreateCall(fread_func, {buffer, llvm::ConstantInt::get(i64_type, 1), file_size, file_ptr});

    llvm::Value* null_term_ptr = builder->CreateGEP(i8_type, buffer, file_size);
    builder->CreateStore(llvm::ConstantInt::get(i8_type, 0), null_term_ptr);

    builder->CreateCall(fclose_func, {file_ptr});

    return BoxValue(buffer, BoxType::STRING);
}

BoxValue CodeGenerator::builtin_write_file(const BoxValue& path, const BoxValue& content) {
    llvm::Value* mode_str = get_or_create_string_constant("w");
    llvm::Value* file_ptr = builder->CreateCall(fopen_func, {path.ir_value, mode_str}, "file_handle");

    llvm::Value* null_check = builder->CreateICmpEQ(file_ptr, llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "file_write_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "file_write_ok", current_function);

    builder->CreateCondBr(null_check, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: Cannot open file for writing\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    builder->CreateCall(fputs_func, {content.ir_value, file_ptr});
    builder->CreateCall(fclose_func, {file_ptr});

    return BoxValue(llvm::ConstantFP::get(double_type, 1.0), BoxType::NUMBER);
}

BoxValue CodeGenerator::builtin_append_file(const BoxValue& path, const BoxValue& content) {
    llvm::Value* mode_str = get_or_create_string_constant("a");
    llvm::Value* file_ptr = builder->CreateCall(fopen_func, {path.ir_value, mode_str}, "file_handle");

    llvm::Value* null_check = builder->CreateICmpEQ(file_ptr, llvm::ConstantPointerNull::get(i8_ptr_type));
    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "file_append_error", current_function);
    llvm::BasicBlock* ok_block = llvm::BasicBlock::Create(*context, "file_append_ok", current_function);

    builder->CreateCondBr(null_check, error_block, ok_block);

    builder->SetInsertPoint(error_block);
    llvm::Value* error_msg = get_or_create_string_constant("Runtime Error: Cannot open file for appending\n");
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(ok_block);

    builder->CreateCall(fputs_func, {content.ir_value, file_ptr});
    builder->CreateCall(fclose_func, {file_ptr});

    return BoxValue(llvm::ConstantFP::get(double_type, 1.0), BoxType::NUMBER);
}

BoxValue CodeGenerator::builtin_file_exists(const BoxValue& path) {
    llvm::Value* result = builder->CreateCall(access_func, {path.ir_value, llvm::ConstantInt::get(i32_type, 0)});
    llvm::Value* exists = builder->CreateICmpEQ(result, llvm::ConstantInt::get(i32_type, 0));

    return BoxValue(exists, BoxType::BOOL);
}

}
