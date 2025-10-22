#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

BoxValue CodeGenerator::visit_expr(const ExprPtr& expr) {
    if (auto* literal = dynamic_cast<Literal*>(expr.get())) {
        return visit_literal(literal);
    } else if (auto* variable = dynamic_cast<Variable*>(expr.get())) {
        return visit_variable(variable);
    } else if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        return visit_assign(assign);
    } else if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        return visit_binary(binary);
    } else if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return visit_unary(unary);
    } else if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        return visit_logical(logical);
    } else if (auto* call = dynamic_cast<Call*>(expr.get())) {
        return visit_call(call);
    } else if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return visit_expr(grouping->expression);
    } else if (auto* array_lit = dynamic_cast<ArrayLiteral*>(expr.get())) {
        return visit_array_literal(array_lit);
    } else if (auto* dict_lit = dynamic_cast<DictLiteral*>(expr.get())) {
        return visit_dict_literal(dict_lit);
    } else if (auto* index_get = dynamic_cast<IndexGet*>(expr.get())) {
        return visit_index_get(index_get);
    } else if (auto* index_set = dynamic_cast<IndexSet*>(expr.get())) {
        return visit_index_set(index_set);
    } else {
        throw CodeGenError("Unknown expression type");
    }
}

BoxValue CodeGenerator::visit_literal(Literal* expr) {
    if (std::holds_alternative<double>(expr->value)) {
        double val = std::get<double>(expr->value);
        return BoxValue(llvm::ConstantFP::get(double_type, val), BoxType::NUMBER, false);
    } else if (std::holds_alternative<std::string>(expr->value)) {
        std::string val = std::get<std::string>(expr->value);
        return create_string_constant(val);
    } else if (std::holds_alternative<bool>(expr->value)) {
        bool val = std::get<bool>(expr->value);
        return BoxValue(llvm::ConstantInt::get(i1_type, val ? 1 : 0), BoxType::BOOL, false);
    } else if (std::holds_alternative<std::monostate>(expr->value)) {
        return BoxValue(llvm::ConstantFP::get(double_type, 0.0), BoxType::NIL, false);
    } else {
        throw CodeGenError("Unknown literal type", expr->token);
    }
}

BoxValue CodeGenerator::create_string_constant(const std::string& value) {
    std::vector<uint8_t> string_bytes(value.begin(), value.end());
    string_bytes.push_back(0);
    
    llvm::ArrayType* string_type = llvm::ArrayType::get(i8_type, string_bytes.size());
    llvm::Constant* string_const = llvm::ConstantDataArray::get(*context, string_bytes);
    
    llvm::GlobalVariable* global_str = new llvm::GlobalVariable(
        *module, string_type, true, llvm::GlobalValue::InternalLinkage,
        string_const, "str"
    );
    global_str->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    
    llvm::Value* str_ptr = builder->CreateConstGEP2_32(string_type, global_str, 0, 0);
    return BoxValue(str_ptr, BoxType::STRING, false);
}

BoxValue CodeGenerator::visit_variable(Variable* expr) {
    if (expr->name.lexeme == "len") {
        std::string hint = "'len' is a built-in keyword for getting array length.\n";
        hint += "       Use it like: len(array)";
        throw CodeGenError("'len' cannot be used as a variable", expr->name, hint);
    }

    auto var = env->get(expr->name.lexeme);
    if (!var.has_value()) {
        std::string hint = "Make sure '" + expr->name.lexeme + "' is declared before use.\n";
        hint += "       Example: var " + expr->name.lexeme + " = value;";
        throw CodeGenError("Undefined variable '" + expr->name.lexeme + "'", expr->name, hint);
    }

    // For arrays and dicts, return the pointer to the struct, not the loaded struct
    if (var->box_type == BoxType::ARRAY) {
        return BoxValue(var->ir_value, var->box_type, true, var->element_type);
    } else if (var->box_type == BoxType::DICT) {
        return BoxValue(var->ir_value, var->box_type, true, var->element_type, var->value_type);
    }

    // For other types, load the value
    llvm::Type* load_type = nullptr;
    if (var->box_type == BoxType::NUMBER) {
        load_type = double_type;
    } else if (var->box_type == BoxType::BOOL) {
        load_type = i1_type;
    } else if (var->box_type == BoxType::STRING) {
        load_type = i8_ptr_type;
    } else if (var->box_type == BoxType::POINTER) {
        load_type = i8_ptr_type;
    } else {
        load_type = double_type; // Default fallback
    }
    
    llvm::Value* loaded = builder->CreateLoad(load_type, var->ir_value, "load_" + expr->name.lexeme);
    return BoxValue(loaded, var->box_type, true, var->element_type);
}

BoxValue CodeGenerator::visit_assign(Assign* expr) {
    BoxValue value = visit_expr(expr->value);

    auto var = env->get(expr->name.lexeme);
    if (!var.has_value()) {
        std::string hint = "Variable '" + expr->name.lexeme + "' must be declared before assignment.\n";
        hint += "       Use: var " + expr->name.lexeme + " = value;";
        throw CodeGenError("Undefined variable '" + expr->name.lexeme + "'", expr->name, hint);
    }

    if (!var->is_mutable) {
        std::string hint = "Literal values and constants cannot be reassigned.";
        throw CodeGenError("Cannot assign to immutable variable '" + expr->name.lexeme + "'", expr->name, hint);
    }

    builder->CreateStore(value.ir_value, var->ir_value);
    return value;
}

BoxValue CodeGenerator::visit_binary(Binary* expr) {
    BoxValue left = visit_expr(expr->left);
    BoxValue right = visit_expr(expr->right);

    TokenType op_type = expr->op.type;

    if (op_type == TokenType::PLUS) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFAdd(left.ir_value, right.ir_value, "add");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '+' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '+' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::MINUS) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFSub(left.ir_value, right.ir_value, "sub");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '-' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '-' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::STAR) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFMul(left.ir_value, right.ir_value, "mul");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '*' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '*' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::SLASH) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            check_division_by_zero(right.ir_value, expr->op, "division");
            llvm::Value* result = builder->CreateFDiv(left.ir_value, right.ir_value, "div");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '/' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '/' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::PERCENT) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            check_division_by_zero(right.ir_value, expr->op, "modulo");
            llvm::Value* result = builder->CreateFRem(left.ir_value, right.ir_value, "mod");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '%' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '%' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::LESS) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpOLT(left.ir_value, right.ir_value, "lt");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '<' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '<' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::LESS_EQUAL) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpOLE(left.ir_value, right.ir_value, "le");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '<=' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '<=' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::GREATER) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpOGT(left.ir_value, right.ir_value, "gt");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '>' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '>' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::GREATER_EQUAL) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpOGE(left.ir_value, right.ir_value, "ge");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '>=' operator requires both operands to be numbers.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be numbers for '>=' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::EQUAL_EQUAL) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpOEQ(left.ir_value, right.ir_value, "eq");
            return BoxValue(result, BoxType::BOOL);
        } else if (left.box_type == BoxType::BOOL && right.box_type == BoxType::BOOL) {
            llvm::Value* result = builder->CreateICmpEQ(left.ir_value, right.ir_value, "eq");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '==' operator requires both operands to be the same type.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be of same type for '==' operator", expr->op, hint);
        }
    }
    else if (op_type == TokenType::BANG_EQUAL) {
        if (left.box_type == BoxType::NUMBER && right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFCmpONE(left.ir_value, right.ir_value, "ne");
            return BoxValue(result, BoxType::BOOL);
        } else if (left.box_type == BoxType::BOOL && right.box_type == BoxType::BOOL) {
            llvm::Value* result = builder->CreateICmpNE(left.ir_value, right.ir_value, "ne");
            return BoxValue(result, BoxType::BOOL);
        } else {
            std::string hint = "The '!=' operator requires both operands to be the same type.\n";
            hint += "       Left: " + left.box_type + ", Right: " + right.box_type;
            throw CodeGenError("Operands must be of same type for '!=' operator", expr->op, hint);
        }
    }
    else {
        throw CodeGenError("Unknown binary operator: " + expr->op.lexeme, expr->op);
    }
}

BoxValue CodeGenerator::visit_unary(Unary* expr) {
    BoxValue right = visit_expr(expr->right);

    if (expr->op.type == TokenType::MINUS) {
        if (right.box_type == BoxType::NUMBER) {
            llvm::Value* result = builder->CreateFNeg(right.ir_value, "neg");
            return BoxValue(result, BoxType::NUMBER);
        } else {
            std::string hint = "The '-' operator requires a number operand.\n";
            hint += "       Operand type: " + right.box_type;
            throw CodeGenError("Operand must be a number for '-' operator", expr->op, hint);
        }
    }
    else if (expr->op.type == TokenType::BANG) {
        llvm::Value* bool_val = to_boolean(right);
        llvm::Value* result = builder->CreateNot(bool_val, "not");
        return BoxValue(result, BoxType::BOOL);
    }
    else {
        throw CodeGenError("Unknown unary operator: " + expr->op.lexeme, expr->op);
    }
}

BoxValue CodeGenerator::visit_logical(Logical* expr) {
    BoxValue left = visit_expr(expr->left);
    llvm::Value* left_bool = to_boolean(left);

    if (expr->op.type == TokenType::AND) {
        llvm::BasicBlock* right_block = llvm::BasicBlock::Create(*context, "and_right", current_function);
        llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "and_merge", current_function);

        llvm::Value* result_ptr = builder->CreateAlloca(i1_type, nullptr, "and_result");
        builder->CreateStore(left_bool, result_ptr);

        builder->CreateCondBr(left_bool, right_block, merge_block);

        builder->SetInsertPoint(right_block);
        BoxValue right = visit_expr(expr->right);
        llvm::Value* right_bool = to_boolean(right);
        builder->CreateStore(right_bool, result_ptr);
        builder->CreateBr(merge_block);

        builder->SetInsertPoint(merge_block);
        llvm::Value* result = builder->CreateLoad(i1_type, result_ptr, "and_value");
        return BoxValue(result, BoxType::BOOL);
    }
    else if (expr->op.type == TokenType::OR) {
        llvm::BasicBlock* right_block = llvm::BasicBlock::Create(*context, "or_right", current_function);
        llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "or_merge", current_function);

        llvm::Value* result_ptr = builder->CreateAlloca(i1_type, nullptr, "or_result");
        builder->CreateStore(left_bool, result_ptr);

        builder->CreateCondBr(left_bool, merge_block, right_block);

        builder->SetInsertPoint(right_block);
        BoxValue right = visit_expr(expr->right);
        llvm::Value* right_bool = to_boolean(right);
        builder->CreateStore(right_bool, result_ptr);
        builder->CreateBr(merge_block);

        builder->SetInsertPoint(merge_block);
        llvm::Value* result = builder->CreateLoad(i1_type, result_ptr, "or_value");
        return BoxValue(result, BoxType::BOOL);
    }
    else {
        throw CodeGenError("Unknown logical operator: " + expr->op.lexeme, expr->op);
    }
}

}
