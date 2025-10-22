#include "codegen.hpp"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>

namespace box {

void CodeGenerator::visit_print_stmt(PrintStmt* stmt) {
    BoxValue value = visit_expr(stmt->expression);

    if (value.box_type == BoxType::NUMBER) {
        llvm::Value* fmt_str = get_or_create_string_constant("%g\n");
        builder->CreateCall(printf_func, {fmt_str, value.ir_value});
    }
    else if (value.box_type == BoxType::STRING) {
        llvm::Value* fmt_str = get_or_create_string_constant("%s\n");
        builder->CreateCall(printf_func, {fmt_str, value.ir_value});
    }
    else if (value.box_type == BoxType::BOOL) {
        llvm::BasicBlock* true_block = llvm::BasicBlock::Create(*context, "print_true", current_function);
        llvm::BasicBlock* false_block = llvm::BasicBlock::Create(*context, "print_false", current_function);
        llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "print_merge", current_function);

        builder->CreateCondBr(value.ir_value, true_block, false_block);

        builder->SetInsertPoint(true_block);
        llvm::Value* true_str = get_or_create_string_constant("true\n");
        builder->CreateCall(printf_func, {true_str});
        builder->CreateBr(merge_block);

        builder->SetInsertPoint(false_block);
        llvm::Value* false_str = get_or_create_string_constant("false\n");
        builder->CreateCall(printf_func, {false_str});
        builder->CreateBr(merge_block);

        builder->SetInsertPoint(merge_block);
    }
    else if (value.box_type == BoxType::NIL) {
        llvm::Value* nil_str = get_or_create_string_constant("nil\n");
        builder->CreateCall(printf_func, {nil_str});
    }
    else if (value.box_type == BoxType::ARRAY) {
        print_array(value);
    }
    else if (value.box_type == BoxType::DICT) {
        print_dict(value);
    }
    else {
        std::string hint = "Supported types for print: numbers, strings, booleans, nil, arrays, dictionaries";
        throw CodeGenError("Cannot print value of type '" + value.box_type + "'", stmt->keyword, hint);
    }
}

void CodeGenerator::print_array(const BoxValue& array_value) {
    llvm::Value* bracket_open = get_or_create_string_constant("[");
    builder->CreateCall(printf_func, {bracket_open});

    llvm::Value* length_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 0, "length_ptr");
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "array_length");

    llvm::Value* data_ptr_ptr = builder->CreateStructGEP(array_struct_type, array_value.ir_value, 1, "data_ptr_ptr");
    llvm::Value* data_ptr = builder->CreateLoad(double_ptr_type, data_ptr_ptr, "array_data");

    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);

    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "print_array_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "print_array_body", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "print_array_end", current_function);

    builder->CreateBr(cond_block);

    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length, "loop_cond");
    builder->CreateCondBr(cond, body_block, end_block);

    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);

    llvm::Value* is_not_first = builder->CreateICmpSGT(i, llvm::ConstantInt::get(i64_type, 0));
    llvm::BasicBlock* comma_block = llvm::BasicBlock::Create(*context, "print_comma", current_function);
    llvm::BasicBlock* skip_comma = llvm::BasicBlock::Create(*context, "skip_comma", current_function);

    builder->CreateCondBr(is_not_first, comma_block, skip_comma);

    builder->SetInsertPoint(comma_block);
    llvm::Value* comma_str = get_or_create_string_constant(", ");
    builder->CreateCall(printf_func, {comma_str});
    builder->CreateBr(skip_comma);

    builder->SetInsertPoint(skip_comma);
    llvm::Value* elem_ptr = builder->CreateGEP(double_type, data_ptr, i, "elem_ptr");
    llvm::Value* elem = builder->CreateLoad(double_type, elem_ptr, "elem");
    llvm::Value* elem_fmt = get_or_create_string_constant("%g");
    builder->CreateCall(printf_func, {elem_fmt, elem});

    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);

    builder->SetInsertPoint(end_block);
    llvm::Value* bracket_close = get_or_create_string_constant("]\n");
    builder->CreateCall(printf_func, {bracket_close});
}

void CodeGenerator::print_dict(const BoxValue& dict_value) {
    llvm::Value* brace_open = get_or_create_string_constant("{");
    builder->CreateCall(printf_func, {brace_open});

    llvm::Value* length_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 0, "length_ptr");
    llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "dict_length");

    llvm::Value* entries_ptr_ptr = builder->CreateStructGEP(dict_struct_type, dict_value.ir_value, 1, "entries_ptr_ptr");
    llvm::Value* entries_ptr = builder->CreateLoad(llvm::PointerType::get(dict_entry_type, 0), entries_ptr_ptr, "dict_entries");

    llvm::Value* loop_var = builder->CreateAlloca(i64_type, nullptr, "i");
    builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), loop_var);

    llvm::Value* first_entry = builder->CreateAlloca(i1_type, nullptr, "first_entry");
    builder->CreateStore(llvm::ConstantInt::get(i1_type, 1), first_entry);

    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "print_dict_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "print_dict_body", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "print_dict_end", current_function);

    builder->CreateBr(cond_block);

    builder->SetInsertPoint(cond_block);
    llvm::Value* i = builder->CreateLoad(i64_type, loop_var);
    llvm::Value* cond = builder->CreateICmpSLT(i, length, "loop_cond");
    builder->CreateCondBr(cond, body_block, end_block);

    builder->SetInsertPoint(body_block);
    i = builder->CreateLoad(i64_type, loop_var);

    llvm::Value* entry_ptr = builder->CreateGEP(dict_entry_type, entries_ptr, i, "entry_ptr");
    
    llvm::Value* key_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 0, "key_ptr");
    llvm::Value* value_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 1, "value_ptr");
    llvm::Value* used_ptr = builder->CreateStructGEP(dict_entry_type, entry_ptr, 2, "used_ptr");
    
    llvm::Value* used = builder->CreateLoad(i1_type, used_ptr, "used");
    
    llvm::BasicBlock* print_block = llvm::BasicBlock::Create(*context, "print_entry", current_function);
    llvm::BasicBlock* skip_block = llvm::BasicBlock::Create(*context, "skip_entry", current_function);
    
    builder->CreateCondBr(used, print_block, skip_block);
    
    builder->SetInsertPoint(print_block);
    
    llvm::Value* is_first = builder->CreateLoad(i1_type, first_entry, "is_first");
    llvm::BasicBlock* comma_block = llvm::BasicBlock::Create(*context, "print_comma", current_function);
    llvm::BasicBlock* skip_comma = llvm::BasicBlock::Create(*context, "skip_comma", current_function);
    
    builder->CreateCondBr(is_first, skip_comma, comma_block);
    
    builder->SetInsertPoint(comma_block);
    llvm::Value* comma_str = get_or_create_string_constant(", ");
    builder->CreateCall(printf_func, {comma_str});
    builder->CreateBr(skip_comma);
    
    builder->SetInsertPoint(skip_comma);
    
    builder->CreateStore(llvm::ConstantInt::get(i1_type, 0), first_entry);
    
    llvm::Value* key = builder->CreateLoad(double_type, key_ptr, "key");
    llvm::Value* value = builder->CreateLoad(double_type, value_ptr, "value");
    
    llvm::Value* entry_fmt = get_or_create_string_constant("%g: %g");
    builder->CreateCall(printf_func, {entry_fmt, key, value});
    builder->CreateBr(skip_block);
    
    builder->SetInsertPoint(skip_block);
    llvm::Value* i_next = builder->CreateAdd(i, llvm::ConstantInt::get(i64_type, 1));
    builder->CreateStore(i_next, loop_var);
    builder->CreateBr(cond_block);

    builder->SetInsertPoint(end_block);
    llvm::Value* brace_close = get_or_create_string_constant("}\n");
    builder->CreateCall(printf_func, {brace_close});
}

void CodeGenerator::visit_var_stmt(VarStmt* stmt) {
    std::string var_name = stmt->name.lexeme;

    if (env->exists_in_current_scope(var_name)) {
        std::string hint = "Variable '" + var_name + "' was already declared in this scope.\n";
        hint += "       Use a different name or assign to the existing variable.";
        throw CodeGenError("Variable '" + var_name + "' already declared in this scope", stmt->name, hint);
    }

    BoxValue value(nullptr, BoxType::NIL);
    if (stmt->initializer.has_value()) {
        value = visit_expr(stmt->initializer.value());
    } else {
        value = BoxValue(llvm::ConstantFP::get(double_type, 0.0), BoxType::NIL);
    }

    llvm::Value* var_ptr = builder->CreateAlloca(value.ir_value->getType(), nullptr, var_name);
    builder->CreateStore(value.ir_value, var_ptr);

    env->define(var_name, BoxValue(var_ptr, value.box_type, true, value.element_type));
}

void CodeGenerator::visit_block(Block* stmt) {
    auto previous = env;
    env = std::make_shared<Environment>(previous);

    try {
        for (const auto& statement : stmt->statements) {
            if (builder->GetInsertBlock()->getTerminator()) {
                break;
            }
            visit_stmt(statement);
        }
    } catch (...) {
        env = previous;
        throw;
    }

    env = previous;
}

void CodeGenerator::visit_if_stmt(IfStmt* stmt) {
    BoxValue condition = visit_expr(stmt->condition);
    llvm::Value* cond_bool = to_boolean(condition);

    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(*context, "if_then", current_function);
    llvm::BasicBlock* else_block = llvm::BasicBlock::Create(*context, "if_else", current_function);
    llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "if_merge", current_function);

    builder->CreateCondBr(cond_bool, then_block, else_block);

    builder->SetInsertPoint(then_block);
    visit_stmt(stmt->then_branch);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_block);
    }

    builder->SetInsertPoint(else_block);
    if (stmt->else_branch.has_value()) {
        visit_stmt(stmt->else_branch.value());
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_block);
    }

    builder->SetInsertPoint(merge_block);
}

void CodeGenerator::visit_while_stmt(WhileStmt* stmt) {
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "while_cond", current_function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "while_body", current_function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "while_end", current_function);

    llvm::BasicBlock* previous_break = break_block;
    break_block = end_block;

    builder->CreateBr(cond_block);

    builder->SetInsertPoint(cond_block);
    BoxValue condition = visit_expr(stmt->condition);
    llvm::Value* cond_bool = to_boolean(condition);
    builder->CreateCondBr(cond_bool, body_block, end_block);

    builder->SetInsertPoint(body_block);
    visit_stmt(stmt->body);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(cond_block);
    }

    builder->SetInsertPoint(end_block);
    break_block = previous_break;
}

void CodeGenerator::visit_function_stmt(FunctionStmt* stmt) {
    std::string func_name = stmt->name.lexeme;

    if (functions.find(func_name) != functions.end()) {
        std::string hint = "Function '" + func_name + "' was already declared.\n";
        hint += "       Use a different name or remove the duplicate declaration.";
        throw CodeGenError("Function '" + func_name + "' already declared", stmt->name, hint);
    }

    std::vector<llvm::Type*> param_types(stmt->params.size(), double_type);
    llvm::FunctionType* func_type = llvm::FunctionType::get(double_type, param_types, false);
    llvm::Function* func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func_name, module.get());

    std::vector<std::string> param_names;
    for (const auto& p : stmt->params) {
        param_names.push_back(p.lexeme);
    }
    functions[func_name] = {func, param_names};

    llvm::Function* previous_function = current_function;
    std::shared_ptr<Environment> previous_env = env;

    current_function = func;
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*context, "entry", func);
    builder->SetInsertPoint(entry_block);
    env = std::make_shared<Environment>(previous_env);

    size_t i = 0;
    for (auto& arg : func->args()) {
        arg.setName(stmt->params[i].lexeme);
        llvm::Value* param_ptr = builder->CreateAlloca(double_type, nullptr, stmt->params[i].lexeme);
        builder->CreateStore(&arg, param_ptr);
        env->define(stmt->params[i].lexeme, BoxValue(param_ptr, BoxType::NUMBER, true));
        i++;
    }

    bool has_explicit_return = false;
    for (const auto& statement : stmt->body) {
        if (builder->GetInsertBlock()->getTerminator()) {
            has_explicit_return = true;
            break;
        }
        visit_stmt(statement);
        if (builder->GetInsertBlock()->getTerminator()) {
            has_explicit_return = true;
        }
    }

    if (!has_explicit_return && !builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(llvm::ConstantFP::get(double_type, 0.0));
    }

    current_function = previous_function;
    env = previous_env;
    builder->SetInsertPoint(&current_function->back());
}

void CodeGenerator::visit_return_stmt(ReturnStmt* stmt) {
    if (stmt->value.has_value()) {
        BoxValue value = visit_expr(stmt->value.value());
        if (value.box_type == BoxType::NUMBER) {
            builder->CreateRet(value.ir_value);
        } else {
            builder->CreateRet(to_number(value));
        }
    } else {
        builder->CreateRet(llvm::ConstantFP::get(double_type, 0.0));
    }
}

void CodeGenerator::visit_break_stmt(BreakStmt* stmt) {
    if (break_block == nullptr) {
        std::string hint = "Break can only be used inside loops or switch statements.";
        throw CodeGenError("Break statement outside of loop or switch", stmt->keyword, hint);
    }
    builder->CreateBr(break_block);
}

void CodeGenerator::visit_unsafe_block(UnsafeBlock* stmt) {
    bool prev_unsafe = in_unsafe_block;
    in_unsafe_block = true;

    for (const auto& statement : stmt->statements) {
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
        visit_stmt(statement);
    }

    in_unsafe_block = prev_unsafe;
}

}
