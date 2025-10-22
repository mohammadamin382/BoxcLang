#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

llvm::Value* CodeGenerator::get_or_create_string_constant(const std::string& value) {
    if (string_constants.find(value) != string_constants.end()) {
        llvm::GlobalVariable* global_str = string_constants[value];
        llvm::ArrayType* string_type = llvm::cast<llvm::ArrayType>(global_str->getValueType());
        return builder->CreateConstGEP2_32(string_type, global_str, 0, 0);
    }

    std::vector<uint8_t> string_bytes(value.begin(), value.end());
    string_bytes.push_back(0);

    llvm::ArrayType* string_type = llvm::ArrayType::get(i8_type, string_bytes.size());
    llvm::Constant* string_const = llvm::ConstantDataArray::get(*context, string_bytes);

    llvm::GlobalVariable* global_str = new llvm::GlobalVariable(
        *module, string_type, true, llvm::GlobalValue::InternalLinkage,
        string_const, "str_const"
    );
    global_str->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

    string_constants[value] = global_str;

    return builder->CreateConstGEP2_32(string_type, global_str, 0, 0);
}

void CodeGenerator::check_array_bounds(llvm::Value* index_i64, llvm::Value* length, const Token& token) {
    llvm::Value* is_negative = builder->CreateICmpSLT(index_i64, llvm::ConstantInt::get(i64_type, 0), "is_negative");
    llvm::Value* is_too_large = builder->CreateICmpSGE(index_i64, length, "is_too_large");
    llvm::Value* is_out_of_bounds = builder->CreateOr(is_negative, is_too_large, "is_out_of_bounds");

    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "bounds_error", current_function);
    llvm::BasicBlock* continue_block = llvm::BasicBlock::Create(*context, "bounds_ok", current_function);

    builder->CreateCondBr(is_out_of_bounds, error_block, continue_block);

    builder->SetInsertPoint(error_block);
    std::string error_message = "Runtime Error: Array index out of bounds at line " + std::to_string(token.line) + "\n";
    llvm::Value* error_msg = get_or_create_string_constant(error_message);
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(continue_block);
}

void CodeGenerator::check_division_by_zero(llvm::Value* divisor, const Token& token, const std::string& op_name) {
    llvm::Value* zero = llvm::ConstantFP::get(double_type, 0.0);
    llvm::Value* is_zero = builder->CreateFCmpOEQ(divisor, zero, "is_zero");

    llvm::BasicBlock* error_block = llvm::BasicBlock::Create(*context, "div_zero_error", current_function);
    llvm::BasicBlock* continue_block = llvm::BasicBlock::Create(*context, "div_ok", current_function);

    builder->CreateCondBr(is_zero, error_block, continue_block);

    builder->SetInsertPoint(error_block);
    std::string error_message = "Runtime Error: Division by zero at line " + std::to_string(token.line) + "\n";
    llvm::Value* error_msg = get_or_create_string_constant(error_message);
    builder->CreateCall(printf_func, {error_msg});
    builder->CreateCall(exit_func, {llvm::ConstantInt::get(i32_type, 1)});
    builder->CreateUnreachable();

    builder->SetInsertPoint(continue_block);
}

llvm::Value* CodeGenerator::to_boolean(const BoxValue& value) {
    if (value.box_type == BoxType::BOOL) {
        return value.ir_value;
    } else if (value.box_type == BoxType::NUMBER) {
        llvm::Value* zero = llvm::ConstantFP::get(double_type, 0.0);
        return builder->CreateFCmpONE(value.ir_value, zero, "tobool");
    } else if (value.box_type == BoxType::NIL) {
        return llvm::ConstantInt::get(i1_type, 0);
    } else {
        return llvm::ConstantInt::get(i1_type, 1);
    }
}

llvm::Value* CodeGenerator::to_number(const BoxValue& value) {
    if (value.box_type == BoxType::NUMBER) {
        return value.ir_value;
    } else if (value.box_type == BoxType::BOOL) {
        return builder->CreateUIToFP(value.ir_value, double_type, "bool_to_num");
    } else if (value.box_type == BoxType::NIL) {
        return llvm::ConstantFP::get(double_type, 0.0);
    } else {
        std::string hint = "Cannot convert " + value.box_type + " to number.";
        throw CodeGenError("Cannot convert " + value.box_type + " to number", std::nullopt, hint);
    }
}

}
