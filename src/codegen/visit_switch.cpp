#include "codegen.hpp"

namespace box {

void CodeGenerator::visit_switch_stmt(SwitchStmt* stmt) {
    BoxValue condition = visit_expr(stmt->condition);
    
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*context, "switch_end", current_function);
    llvm::BasicBlock* previous_break = break_block;
    break_block = end_block;

    std::vector<std::pair<CaseClause*, llvm::BasicBlock*>> case_blocks;
    for (size_t i = 0; i < stmt->cases.size(); i++) {
        llvm::BasicBlock* case_block = llvm::BasicBlock::Create(*context, "case_" + std::to_string(i), current_function);
        case_blocks.push_back({&stmt->cases[i], case_block});
    }

    llvm::BasicBlock* default_block = nullptr;
    if (stmt->default_case.has_value()) {
        default_block = llvm::BasicBlock::Create(*context, "default", current_function);
    }
    
    llvm::BasicBlock* next_check_block = llvm::BasicBlock::Create(*context, "case_check_0", current_function);
    builder->CreateBr(next_check_block);

    for (size_t i = 0; i < case_blocks.size(); i++) {
        builder->SetInsertPoint(next_check_block);
        
        CaseClause* case_clause = case_blocks[i].first;
        llvm::BasicBlock* case_block = case_blocks[i].second;
        
        BoxValue case_value = visit_expr(case_clause->value);
        
        llvm::Value* match = nullptr;
        if (condition.box_type == BoxType::NUMBER && case_value.box_type == BoxType::NUMBER) {
            match = builder->CreateFCmpOEQ(condition.ir_value, case_value.ir_value);
        } else if (condition.box_type == BoxType::STRING && case_value.box_type == BoxType::STRING) {
            match = string_compare(condition.ir_value, case_value.ir_value);
        } else if (condition.box_type == BoxType::BOOL && case_value.box_type == BoxType::BOOL) {
            match = builder->CreateICmpEQ(condition.ir_value, case_value.ir_value);
        } else {
            std::string hint = "Switch condition and case value types must match.\n";
            hint += "       Condition: " + condition.box_type + ", Case: " + case_value.box_type;
            throw CodeGenError("Type mismatch in switch/case", stmt->keyword, hint);
        }

        if (i < case_blocks.size() - 1) {
            next_check_block = llvm::BasicBlock::Create(*context, "case_check_" + std::to_string(i+1), current_function);
            builder->CreateCondBr(match, case_block, next_check_block);
        } else {
            if (default_block) {
                builder->CreateCondBr(match, case_block, default_block);
            } else {
                builder->CreateCondBr(match, case_block, end_block);
            }
        }
    }

    for (auto& [case_clause, case_block] : case_blocks) {
        builder->SetInsertPoint(case_block);
        for (const auto& case_stmt : case_clause->statements) {
            if (builder->GetInsertBlock()->getTerminator()) {
                break;
            }
            visit_stmt(case_stmt);
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(end_block);
        }
    }

    if (default_block) {
        builder->SetInsertPoint(default_block);
        for (const auto& default_stmt : stmt->default_case.value()) {
            if (builder->GetInsertBlock()->getTerminator()) {
                break;
            }
            visit_stmt(default_stmt);
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(end_block);
        }
    }

    builder->SetInsertPoint(end_block);
    break_block = previous_break;
}

llvm::Value* CodeGenerator::string_compare(llvm::Value* str1, llvm::Value* str2) {
    llvm::Value* result = builder->CreateCall(strcmp_func, {str1, str2});
    llvm::Value* zero = llvm::ConstantInt::get(result->getType(), 0);
    return builder->CreateICmpEQ(result, zero);
}

}
