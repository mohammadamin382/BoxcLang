#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

BoxValue CodeGenerator::dict_get(const BoxValue& dict_value, IndexGet* expr) {
    BoxValue key_value = visit_expr(expr->index);
    
    if (key_value.box_type != BoxType::NUMBER) {
        std::string hint = "Dictionary keys must be numbers.\n";
        hint += "       The key has type '" + key_value.box_type + "'.";
        throw CodeGenError("Dictionary key must be a number", expr->bracket, hint);
    }
    
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0);
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");
    
    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1);
    llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
    llvm::Value* entries_ptr = builder->CreateLoad(entry_ptr_type, entries_ptr_ptr, "dict_entries");
    
    llvm::Value* result_alloca = builder->CreateAlloca(double_type, nullptr, "dict_result");
    builder->CreateStore(llvm::ConstantFP::get(double_type, 0.0), result_alloca);
    
    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "dict_get_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "dict_get_body", current_function);
    llvm::BasicBlock* found_block = llvm::BasicBlock::Create(*context, "dict_get_found", current_function);
    llvm::BasicBlock* notfound_block = llvm::BasicBlock::Create(*context, "dict_get_notfound", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "dict_get_end", current_function);
    
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length);
    builder->CreateCondBr(cond, body_block, notfound_block);
    
    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);
    
    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_key_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0);
    llvm::Value* entry_key = builder->CreateLoad(double_type, entry_key_ptr);
    
    llvm::Value* key_match = builder->CreateFCmpOEQ(entry_key, key_value.ir_value);
    
    llvm::BasicBlock* check_next = llvm::BasicBlock::Create(*context, "check_next", current_function);
    builder->CreateCondBr(key_match, found_block, check_next);
    
    builder->SetInsertPoint(check_next);
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(found_block);
    i = builder->CreateLoad(i64_type, loop_var);
    entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_val_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 1);
    llvm::Value* entry_val = builder->CreateLoad(double_type, entry_val_ptr);
    builder->CreateStore(entry_val, result_alloca);
    builder->CreateBr(end_block);
    
    builder->SetInsertPoint(notfound_block);
    llvm::Value* error_msg = get_or_create_string_constant(
        "Runtime Error: Dictionary key not found at line " + std::to_string(expr->bracket.line) + "\n"
    );
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();
    
    builder->SetInsertPoint(end_block);
    llvm::Value* result = builder->CreateLoad(double_type, result_alloca);
    return BoxValue(result, BoxType::NUMBER, true);
}

BoxValue CodeGenerator::dict_set(const BoxValue& dict_value, IndexSet* expr) {
    BoxValue key_value = visit_expr(expr->index);
    BoxValue val_value = visit_expr(expr->value);
    
    if (key_value.box_type != BoxType::NUMBER) {
        std::string hint = "Dictionary keys must be numbers.\n";
        hint += "       The key has type '" + key_value.box_type + "'.";
        throw CodeGenError("Dictionary key must be a number", expr->bracket, hint);
    }
    
    if (val_value.box_type != BoxType::NUMBER) {
        std::string hint = "Dictionary values must be numbers.\n";
        hint += "       The value has type '" + val_value.box_type + "'.";
        throw CodeGenError("Dictionary value must be a number", expr->bracket, hint);
    }
    
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0);
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");
    
    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1);
    llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
    llvm::Value* entries_ptr = builder->CreateLoad(entry_ptr_type, entries_ptr_ptr, "dict_entries");
    
    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "dict_set_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "dict_set_body", current_function);
    llvm::BasicBlock* found_block = llvm::BasicBlock::Create(*context, "dict_set_found", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "dict_set_end", current_function);
    
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length);
    builder->CreateCondBr(cond, body_block, end_block);
    
    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);
    
    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_key_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0);
    llvm::Value* entry_key = builder->CreateLoad(double_type, entry_key_ptr);
    
    llvm::Value* key_match = builder->CreateFCmpOEQ(entry_key, key_value.ir_value);
    
    llvm::BasicBlock* check_next = llvm::BasicBlock::Create(*context, "check_next", current_function);
    builder->CreateCondBr(key_match, found_block, check_next);
    
    builder->SetInsertPoint(check_next);
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(found_block);
    i = builder->CreateLoad(i64_type, loop_var);
    entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_val_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 1);
    builder->CreateStore(val_value.ir_value, entry_val_ptr);
    builder->CreateBr(end_block);
    
    builder->SetInsertPoint(end_block);
    return val_value;
}

BoxValue CodeGenerator::dict_has(const BoxValue& dict_value, const BoxValue& key_value) {
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0);
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");
    
    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1);
    llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
    llvm::Value* entries_ptr = builder->CreateLoad(entry_ptr_type, entries_ptr_ptr, "dict_entries");
    
    llvm::Value* result_alloca = builder->CreateAlloca(i1_type, nullptr, "has_result");
    builder->CreateStore(llvm::ConstantInt::get(i1_type, 0), result_alloca);
    
    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "has_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "has_body", current_function);
    llvm::BasicBlock* found_block = llvm::BasicBlock::Create(*context, "has_found", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "has_end", current_function);
    
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length);
    builder->CreateCondBr(cond, body_block, end_block);
    
    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);
    
    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_key_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0);
    llvm::Value* entry_key = builder->CreateLoad(double_type, entry_key_ptr);
    
    llvm::Value* key_match = builder->CreateFCmpOEQ(entry_key, key_value.ir_value);
    
    llvm::BasicBlock* check_next = llvm::BasicBlock::Create(*context, "has_check_next", current_function);
    builder->CreateCondBr(key_match, found_block, check_next);
    
    builder->SetInsertPoint(check_next);
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(found_block);
    builder->CreateStore(llvm::ConstantInt::get(i1_type, 1), result_alloca);
    builder->CreateBr(end_block);
    
    builder->SetInsertPoint(end_block);
    llvm::Value* result = builder->CreateLoad(i1_type, result_alloca, "has_value");
    return BoxValue(result, BoxType::BOOL);
}

BoxValue CodeGenerator::dict_keys(const BoxValue& dict_value) {
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0);
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");
    
    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1);
    llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
    llvm::Value* entries_ptr = builder->CreateLoad(entry_ptr_type, entries_ptr_ptr, "dict_entries");
    
    llvm::Value* array_struct = builder->CreateAlloca(array_struct_type, nullptr, "keys_array");
    
    llvm::Value* array_length_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 0);
    builder->CreateStore(length, array_length_ptr);
    
    llvm::Value* array_size = builder->CreateMul(length, llvm::ConstantInt::get(i64_type, 8));
    llvm::Value* raw_ptr = builder->CreateCall(malloc_func, {array_size}, "keys_data_raw");
    llvm::Value* data_ptr = builder->CreateBitCast(raw_ptr, double_ptr_type, "keys_data");
    
    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "keys_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "keys_body", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "keys_end", current_function);
    
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length);
    builder->CreateCondBr(cond, body_block, end_block);
    
    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);
    
    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_key_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0);
    llvm::Value* entry_key = builder->CreateLoad(double_type, entry_key_ptr);
    
    llvm::Value* key_dest_ptr = builder->CreateGEP(double_type, data_ptr, i);
    builder->CreateStore(entry_key, key_dest_ptr);
    
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(end_block);
    llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 1);
    builder->CreateStore(data_ptr, data_ptr_ptr);
    
    return BoxValue(array_struct, BoxType::ARRAY, true, BoxType::NUMBER);
}

BoxValue CodeGenerator::dict_values(const BoxValue& dict_value) {
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0);
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");
    
    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1);
    llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
    llvm::Value* entries_ptr = builder->CreateLoad(entry_ptr_type, entries_ptr_ptr, "dict_entries");
    
    llvm::Value* array_struct = builder->CreateAlloca(array_struct_type, nullptr, "values_array");
    
    llvm::Value* array_length_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 0);
    builder->CreateStore(length, array_length_ptr);
    
    llvm::Value* array_size = builder->CreateMul(length, llvm::ConstantInt::get(i64_type, 8));
    llvm::Value* raw_ptr = builder->CreateCall(malloc_func, {array_size}, "values_data_raw");
    llvm::Value* data_ptr = builder->CreateBitCast(raw_ptr, double_ptr_type, "values_data");
    
    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "values_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "values_body", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "values_end", current_function);
    
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length);
    builder->CreateCondBr(cond, body_block, end_block);
    
    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);
    
    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i);
    llvm::Value* entry_value_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 1);
    llvm::Value* entry_value = builder->CreateLoad(double_type, entry_value_ptr);
    
    llvm::Value* value_dest_ptr = builder->CreateGEP(double_type, data_ptr, i);
    builder->CreateStore(entry_value, value_dest_ptr);
    
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);
    
    builder->SetInsertPoint(end_block);
    llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 1);
    builder->CreateStore(data_ptr, data_ptr_ptr);
    
    return BoxValue(array_struct, BoxType::ARRAY, true, BoxType::NUMBER);
}

}
