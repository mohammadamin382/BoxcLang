#include "codegen.hpp"
#include <regex>
#include <sstream>

namespace box {

void CodeGenerator::visit_llvm_inline(LLVMInlineStmt* stmt) {
    if (!in_unsafe_block) {
        std::string hint = "llvm_inline() can only be used inside unsafe blocks.\n";
        hint += "       Wrap your code in: unsafe { ... }";
        throw CodeGenError("llvm_inline() requires unsafe context", stmt->keyword, hint);
    }

    std::string llvm_code = stmt->llvm_code;
    std::unordered_map<std::string, std::string> variables_map = stmt->variables_map;

    std::string processed_code = process_llvm_inline(llvm_code, variables_map);

    inject_llvm_ir(processed_code);
}

std::string CodeGenerator::process_llvm_inline(const std::string& llvm_code, 
                                               const std::unordered_map<std::string, std::string>& variables_map) {
    std::string processed = llvm_code;

    for (const auto& pair : variables_map) {
        const std::string& box_var = pair.first;
        const std::string& llvm_var = pair.second;
        auto var_value = env->get(box_var);
        if (var_value.has_value()) {
            std::string search = "$" + box_var;
            size_t pos = 0;
            while ((pos = processed.find(search, pos)) != std::string::npos) {
                processed.replace(pos, search.length(), llvm_var);
                pos += llvm_var.length();
            }
        }
    }

    return processed;
}

void CodeGenerator::inject_llvm_ir(const std::string& llvm_ir_code) {
    std::istringstream stream(llvm_ir_code);
    std::string line;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty() || line[0] == ';') {
            continue;
        }

        try {
            emit_llvm_instruction(line);
        } catch (const std::exception& e) {
            std::string hint = "Invalid LLVM IR instruction.\n";
            hint += "       Error: " + std::string(e.what()) + "\n";
            hint += "       Instruction: " + line;
            throw CodeGenError("LLVM IR injection failed", std::nullopt, hint);
        }
    }
}

void CodeGenerator::emit_llvm_instruction(const std::string& instruction) {
    std::string instr = instruction;
    
    size_t eq_pos = instr.find('=');
    if (eq_pos != std::string::npos) {
        std::string result_var = instr.substr(0, eq_pos);
        std::string operation = instr.substr(eq_pos + 1);
        
        result_var.erase(0, result_var.find_first_not_of(" \t"));
        result_var.erase(result_var.find_last_not_of(" \t") + 1);
        
        operation.erase(0, operation.find_first_not_of(" \t"));
        operation.erase(operation.find_last_not_of(" \t") + 1);

        if (operation.substr(0, 3) == "add") {
            emit_add_instruction(result_var, operation);
        } else if (operation.substr(0, 3) == "sub") {
            emit_sub_instruction(result_var, operation);
        } else if (operation.substr(0, 3) == "mul") {
            emit_mul_instruction(result_var, operation);
        } else if (operation.substr(0, 4) == "call") {
            emit_call_instruction(result_var, operation);
        } else {
            throw CodeGenError("Unsupported LLVM operation: " + operation);
        }
    }
    else if (instr.substr(0, 3) == "ret") {
        emit_ret_instruction(instr);
    }
    else if (instr.substr(0, 5) == "store") {
        emit_store_instruction(instr);
    }
    else if (instr.substr(0, 2) == "br") {
        emit_br_instruction(instr);
    }
    else {
        throw CodeGenError("Unsupported LLVM instruction: " + instr);
    }
}

void CodeGenerator::emit_add_instruction(const std::string& result_var, const std::string& operation) {
    std::regex pattern(R"(add\s+(\w+)\s+(.+?),\s*(.+))");
    std::smatch match;
    
    if (std::regex_match(operation, match, pattern)) {
        std::string type_str = match[1].str();
        std::string op1 = match[2].str();
        std::string op2 = match[3].str();
        
        op1.erase(0, op1.find_first_not_of(" \t"));
        op1.erase(op1.find_last_not_of(" \t") + 1);
        op2.erase(0, op2.find_first_not_of(" \t"));
        op2.erase(op2.find_last_not_of(" \t") + 1);

        if (type_str == "i32") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i32");
            llvm::Value* val2 = resolve_llvm_value(op2, "i32");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateAdd(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "i64") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i64");
            llvm::Value* val2 = resolve_llvm_value(op2, "i64");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateAdd(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "double") {
            llvm::Value* val1 = resolve_llvm_value(op1, "double");
            llvm::Value* val2 = resolve_llvm_value(op2, "double");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateFAdd(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        }
    }
}

void CodeGenerator::emit_sub_instruction(const std::string& result_var, const std::string& operation) {
    std::regex pattern(R"(sub\s+(\w+)\s+(.+?),\s*(.+))");
    std::smatch match;
    
    if (std::regex_match(operation, match, pattern)) {
        std::string type_str = match[1].str();
        std::string op1 = match[2].str();
        std::string op2 = match[3].str();
        
        op1.erase(0, op1.find_first_not_of(" \t"));
        op1.erase(op1.find_last_not_of(" \t") + 1);
        op2.erase(0, op2.find_first_not_of(" \t"));
        op2.erase(op2.find_last_not_of(" \t") + 1);

        if (type_str == "i32") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i32");
            llvm::Value* val2 = resolve_llvm_value(op2, "i32");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateSub(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "i64") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i64");
            llvm::Value* val2 = resolve_llvm_value(op2, "i64");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateSub(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "double") {
            llvm::Value* val1 = resolve_llvm_value(op1, "double");
            llvm::Value* val2 = resolve_llvm_value(op2, "double");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateFSub(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        }
    }
}

void CodeGenerator::emit_mul_instruction(const std::string& result_var, const std::string& operation) {
    std::regex pattern(R"(mul\s+(\w+)\s+(.+?),\s*(.+))");
    std::smatch match;
    
    if (std::regex_match(operation, match, pattern)) {
        std::string type_str = match[1].str();
        std::string op1 = match[2].str();
        std::string op2 = match[3].str();
        
        op1.erase(0, op1.find_first_not_of(" \t"));
        op1.erase(op1.find_last_not_of(" \t") + 1);
        op2.erase(0, op2.find_first_not_of(" \t"));
        op2.erase(op2.find_last_not_of(" \t") + 1);

        if (type_str == "i32") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i32");
            llvm::Value* val2 = resolve_llvm_value(op2, "i32");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateMul(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "i64") {
            llvm::Value* val1 = resolve_llvm_value(op1, "i64");
            llvm::Value* val2 = resolve_llvm_value(op2, "i64");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateMul(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        } else if (type_str == "double") {
            llvm::Value* val1 = resolve_llvm_value(op1, "double");
            llvm::Value* val2 = resolve_llvm_value(op2, "double");
            std::string name = result_var;
            if (name[0] == '%') name = name.substr(1);
            llvm::Value* result = builder->CreateFMul(val1, val2, name);
            llvm_inline_vars[result_var] = result;
        }
    }
}

void CodeGenerator::emit_call_instruction(const std::string& result_var, const std::string& operation) {
    std::regex pattern(R"(call\s+(\w+)\s+@(\w+)\((.*?)\))");
    std::smatch match;
    
    if (std::regex_match(operation, match, pattern)) {
        std::string return_type_str = match[1].str();
        std::string func_name = match[2].str();
        std::string args_str = match[3].str();

        args_str.erase(0, args_str.find_first_not_of(" \t"));
        args_str.erase(args_str.find_last_not_of(" \t") + 1);

        if (functions.find(func_name) == functions.end()) {
            throw CodeGenError("Undefined function in llvm_inline: @" + func_name);
        }

        llvm::Function* func = functions[func_name].first;

        std::vector<llvm::Value*> args;
        if (!args_str.empty()) {
            std::istringstream arg_stream(args_str);
            std::string arg_part;
            while (std::getline(arg_stream, arg_part, ',')) {
                arg_part.erase(0, arg_part.find_first_not_of(" \t"));
                arg_part.erase(arg_part.find_last_not_of(" \t") + 1);
                
                std::regex arg_pattern(R"((\w+)\s+(.+))");
                std::smatch arg_match;
                if (std::regex_match(arg_part, arg_match, arg_pattern)) {
                    std::string arg_type = arg_match[1].str();
                    std::string arg_value = arg_match[2].str();
                    arg_value.erase(0, arg_value.find_first_not_of(" \t"));
                    arg_value.erase(arg_value.find_last_not_of(" \t") + 1);
                    llvm::Value* arg_val = resolve_llvm_value(arg_value, arg_type);
                    args.push_back(arg_val);
                }
            }
        }

        std::string name = result_var;
        if (name[0] == '%') name = name.substr(1);
        llvm::Value* result = builder->CreateCall(func, args, name);
        llvm_inline_vars[result_var] = result;
    }
}

void CodeGenerator::emit_ret_instruction(const std::string& instruction) {
    std::string trimmed = instruction;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

    if (trimmed == "ret void") {
        builder->CreateRetVoid();
        return;
    }

    std::regex pattern(R"(ret\s+(\w+)\s+(.+))");
    std::smatch match;
    
    if (std::regex_match(trimmed, match, pattern)) {
        std::string type_str = match[1].str();
        std::string value_str = match[2].str();
        
        value_str.erase(0, value_str.find_first_not_of(" \t"));
        value_str.erase(value_str.find_last_not_of(" \t") + 1);

        llvm::Value* ret_val = resolve_llvm_value(value_str, type_str);
        builder->CreateRet(ret_val);
    }
}

void CodeGenerator::emit_store_instruction(const std::string& instruction) {
    std::regex pattern(R"(store\s+(\w+)\s+(.+?),\s*(\w+)\*\s+(.+))");
    std::smatch match;
    
    if (std::regex_match(instruction, match, pattern)) {
        std::string value_type = match[1].str();
        std::string value_str = match[2].str();
        std::string pointer_type = match[3].str();
        std::string pointer_str = match[4].str();
        
        value_str.erase(0, value_str.find_first_not_of(" \t"));
        value_str.erase(value_str.find_last_not_of(" \t") + 1);
        pointer_str.erase(0, pointer_str.find_first_not_of(" \t"));
        pointer_str.erase(pointer_str.find_last_not_of(" \t") + 1);

        llvm::Value* value = resolve_llvm_value(value_str, value_type);

        llvm::Value* pointer = nullptr;
        if (llvm_inline_vars.find(pointer_str) != llvm_inline_vars.end()) {
            pointer = llvm_inline_vars[pointer_str];
        } else {
            throw CodeGenError("Undefined pointer in store: " + pointer_str);
        }

        builder->CreateStore(value, pointer);
    }
}

void CodeGenerator::emit_br_instruction(const std::string& instruction) {
    std::regex unconditional_pattern(R"(br\s+label\s+%(\w+))");
    std::smatch match;
    
    if (std::regex_match(instruction, match, unconditional_pattern)) {
        std::string label_name = match[1].str();

        llvm::BasicBlock* target_block = nullptr;
        for (auto& block : *current_function) {
            if (block.getName() == label_name) {
                target_block = &block;
                break;
            }
        }

        if (!target_block) {
            target_block = llvm::BasicBlock::Create(*context, label_name, current_function);
        }

        builder->CreateBr(target_block);
        return;
    }

    std::regex conditional_pattern(R"(br\s+i1\s+(.+?),\s*label\s+%(\w+),\s*label\s+%(\w+))");
    if (std::regex_match(instruction, match, conditional_pattern)) {
        std::string cond_str = match[1].str();
        std::string true_label = match[2].str();
        std::string false_label = match[3].str();
        
        cond_str.erase(0, cond_str.find_first_not_of(" \t"));
        cond_str.erase(cond_str.find_last_not_of(" \t") + 1);

        llvm::Value* cond = resolve_llvm_value(cond_str, "i1");

        llvm::BasicBlock* true_block = nullptr;
        llvm::BasicBlock* false_block = nullptr;

        for (auto& block : *current_function) {
            if (block.getName() == true_label) {
                true_block = &block;
            }
            if (block.getName() == false_label) {
                false_block = &block;
            }
        }

        if (!true_block) {
            true_block = llvm::BasicBlock::Create(*context, true_label, current_function);
        }
        if (!false_block) {
            false_block = llvm::BasicBlock::Create(*context, false_label, current_function);
        }

        builder->CreateCondBr(cond, true_block, false_block);
    }
}

bool CodeGenerator::is_number(const std::string& s) const {
    try {
        size_t pos;
        std::stod(s, &pos);
        return pos == s.length();
    } catch (...) {
        return false;
    }
}

llvm::Value* CodeGenerator::resolve_llvm_value(const std::string& value_str, const std::string& type_str) {
    std::string trimmed = value_str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

    if (trimmed[0] == '%') {
        if (llvm_inline_vars.find(trimmed) != llvm_inline_vars.end()) {
            return llvm_inline_vars[trimmed];
        }

        std::string var_name = trimmed.substr(1);
        auto box_var = env->get(var_name);
        if (box_var.has_value()) {
            return box_var->ir_value;
        }

        throw CodeGenError("Undefined variable in llvm_inline: " + trimmed);
    }

    // Handle numeric constants
    if (is_number(trimmed)) {
        if (type_str == "i32") {
            return llvm::ConstantInt::get(i32_type, std::stoi(trimmed));
        } else if (type_str == "i64") {
            return llvm::ConstantInt::get(i64_type, std::stoll(trimmed));
        } else if (type_str == "i1") {
            return llvm::ConstantInt::get(i1_type, std::stoi(trimmed));
        } else if (type_str == "double") {
            return llvm::ConstantFP::get(double_type, std::stod(trimmed));
        }
    }

    throw CodeGenError("Cannot resolve value in llvm_inline: " + trimmed);

    if (type_str == "i1") {
        std::string lower = trimmed;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true" || lower == "1") {
            return llvm::ConstantInt::get(i1_type, 1);
        } else {
            return llvm::ConstantInt::get(i1_type, 0);
        }
    } else if (type_str == "i32") {
        return llvm::ConstantInt::get(i32_type, std::stoi(trimmed));
    } else if (type_str == "i64") {
        return llvm::ConstantInt::get(i64_type, std::stoll(trimmed));
    } else if (type_str == "double") {
        return llvm::ConstantFP::get(double_type, std::stod(trimmed));
    } else {
        throw CodeGenError("Unsupported type in llvm_inline: " + type_str);
    }
}

}
