#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

BoxValue CodeGenerator::visit_array_literal(ArrayLiteral* expr) {
    size_t length = expr->elements.size();

    llvm::Value* array_struct = builder->CreateAlloca(array_struct_type, nullptr, "array");

    std::optional<std::string> element_type;
    std::vector<BoxValue> evaluated_elements;

    for (size_t i = 0; i < expr->elements.size(); i++) {
        BoxValue elem_value = visit_expr(expr->elements[i]);

        if (elem_value.box_type != BoxType::NUMBER) {
            std::string hint = "Currently, arrays can only contain numbers.\n";
            hint += "       Element at index " + std::to_string(i) + " has type '" + elem_value.box_type + "'.";
            throw CodeGenError("Array elements must be numbers", expr->bracket, hint);
        }

        element_type = BoxType::NUMBER;
        evaluated_elements.push_back(elem_value);
    }

    llvm::Value* length_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 0, "length_ptr");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, length), length_ptr);

    if (length > 0) {
        llvm::Value* array_size = builder->CreateMul(llvm::ConstantInt::get(i64_type, length), 
                                                      llvm::ConstantInt::get(i64_type, 8), "array_size");
        llvm::Value* raw_ptr = builder->CreateCall(malloc_func, {array_size}, "array_data_raw");
        llvm::Value* data_ptr = builder->CreateBitCast(raw_ptr, double_ptr_type, "array_data");

        for (size_t i = 0; i < evaluated_elements.size(); i++) {
            llvm::Value* elem_ptr = builder->CreateGEP(double_type, data_ptr, 
                                                        llvm::ConstantInt::get(i64_type, i), 
                                                        "elem_" + std::to_string(i) + "_ptr");
            builder->CreateStore(evaluated_elements[i].ir_value, elem_ptr);
        }

        llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 1, "data_ptr_ptr");
        builder->CreateStore(data_ptr, data_ptr_ptr);
    } else {
        llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_struct, 1, "data_ptr_ptr");
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(double_ptr_type);
        builder->CreateStore(null_ptr, data_ptr_ptr);
    }

    return BoxValue(array_struct, BoxType::ARRAY, true, element_type);
}

BoxValue CodeGenerator::visit_dict_literal(DictLiteral* expr) {
    size_t length = expr->pairs.size();
    
    llvm::Value* dict_struct = builder->CreateAlloca(dict_struct_type, nullptr, "dict");
    
    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_struct, 0, "length_ptr");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, length), length_ptr);
    
    if (length > 0) {
        llvm::Value* entries_size = builder->CreateMul(llvm::ConstantInt::get(i64_type, length), 
                                                        llvm::ConstantInt::get(i64_type, 24), "entries_size");
        llvm::Value* raw_ptr = builder->CreateCall(malloc_func, {entries_size}, "dict_entries_raw");
        llvm::PointerType* entry_ptr_type = llvm::PointerType::get(dict_entry_type, 0);
        llvm::Value* entries_ptr = builder->CreateBitCast(raw_ptr, entry_ptr_type, "dict_entries");
        
        for (size_t i = 0; i < expr->pairs.size(); i++) {
            BoxValue key_value = visit_expr(expr->pairs[i].first);
            BoxValue val_value = visit_expr(expr->pairs[i].second);
            
            if (key_value.box_type != BoxType::NUMBER) {
                std::string hint = "Dictionary keys must be numbers.\n";
                hint += "       Key at index " + std::to_string(i) + " has type '" + key_value.box_type + "'.";
                throw CodeGenError("Dictionary keys must be numbers", expr->brace, hint);
            }
            
            if (val_value.box_type != BoxType::NUMBER) {
                std::string hint = "Dictionary values must be numbers.\n";
                hint += "       Value at index " + std::to_string(i) + " has type '" + val_value.box_type + "'.";
                throw CodeGenError("Dictionary values must be numbers", expr->brace, hint);
            }
            
            llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, 
                                                         llvm::ConstantInt::get(i64_type, i), 
                                                         "entry_" + std::to_string(i) + "_ptr");
            
            llvm::Value* key_field_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0, "key_field_ptr");
            builder->CreateStore(key_value.ir_value, key_field_ptr);
            
            llvm::Value* val_field_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 1, "val_field_ptr");
            builder->CreateStore(val_value.ir_value, val_field_ptr);
            
            llvm::Value* used_field_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 2, "used_field_ptr");
            builder->CreateStore(llvm::ConstantInt::get(i1_type, 1), used_field_ptr);
        }
        
        llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_struct, 1, "entries_ptr_ptr");
        builder->CreateStore(entries_ptr, entries_ptr_ptr);
    } else {
        llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_struct, 1, "entries_ptr_ptr");
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(dict_entry_type, 0));
        builder->CreateStore(null_ptr, entries_ptr_ptr);
    }
    
    return BoxValue(dict_struct, BoxType::DICT, true, BoxType::NUMBER, BoxType::NUMBER);
}

BoxValue CodeGenerator::visit_index_get(IndexGet* expr) {
    BoxValue container_value = visit_expr(expr->array);

    if (container_value.box_type == BoxType::DICT) {
        return dict_get(container_value, expr);
    } else if (container_value.box_type == BoxType::ARRAY) {
        return array_get(container_value, expr);
    } else {
        std::string hint = "Only arrays and dictionaries can be indexed with [].\n";
        hint += "       The expression has type '" + container_value.box_type + "'.";
        throw CodeGenError("Cannot index value", expr->bracket, hint);
    }
}

BoxValue CodeGenerator::visit_index_set(IndexSet* expr) {
    BoxValue container_value = visit_expr(expr->array);

    if (container_value.box_type == BoxType::DICT) {
        return dict_set(container_value, expr);
    } else if (container_value.box_type == BoxType::ARRAY) {
        return array_set(container_value, expr);
    } else {
        std::string hint = "Only arrays and dictionaries can be indexed for assignment.\n";
        hint += "       The expression has type '" + container_value.box_type + "'.";
        throw CodeGenError("Cannot index value for assignment", expr->bracket, hint);
    }
}

BoxValue CodeGenerator::array_get(const BoxValue& array_value, IndexGet* expr) {
    BoxValue index_value = visit_expr(expr->index);

    if (index_value.box_type != BoxType::NUMBER) {
        std::string hint = "Array indices must be numbers.\n";
        hint += "       The index has type '" + index_value.box_type + "'.";
        throw CodeGenError("Array index must be a number", expr->bracket, hint);
    }

    llvm::Value* length_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 0, "length_ptr");
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "array_length");

    llvm::Value* index_i64 = builder->CreateFPToSI(index_value.ir_value, i64_type, "index_i64");

    check_array_bounds(index_i64, length, expr->bracket);

    llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 1, "data_ptr_ptr");
    llvm::Value* data_ptr = builder->CreateLoad(double_ptr_type, data_ptr_ptr, "array_data");

    llvm::Value* elem_ptr = builder->CreateGEP(double_type, data_ptr, index_i64, "elem_ptr");
    llvm::Value* elem = builder->CreateLoad(double_type, elem_ptr, "elem_value");

    return BoxValue(elem, BoxType::NUMBER, true);
}

BoxValue CodeGenerator::array_set(const BoxValue& array_value, IndexSet* expr) {
    BoxValue index_value = visit_expr(expr->index);

    if (index_value.box_type != BoxType::NUMBER) {
        std::string hint = "Array indices must be numbers.\n";
        hint += "       The index has type '" + index_value.box_type + "'.";
        throw CodeGenError("Array index must be a number", expr->bracket, hint);
    }

    BoxValue value = visit_expr(expr->value);

    if (value.box_type != BoxType::NUMBER) {
        std::string hint = "Can only assign numbers to array elements.\n";
        hint += "       The value has type '" + value.box_type + "'.";
        throw CodeGenError("Array elements must be numbers", expr->bracket, hint);
    }

    llvm::Value* length_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 0, "length_ptr");
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "array_length");

    llvm::Value* index_i64 = builder->CreateFPToSI(index_value.ir_value, i64_type, "index_i64");

    check_array_bounds(index_i64, length, expr->bracket);

    llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 1, "data_ptr_ptr");
    llvm::Value* data_ptr = builder->CreateLoad(double_ptr_type, data_ptr_ptr, "array_data");

    llvm::Value* elem_ptr = builder->CreateGEP(double_type, data_ptr, index_i64, "elem_ptr");
    builder->CreateStore(value.ir_value, elem_ptr);

    return value;
}

}
