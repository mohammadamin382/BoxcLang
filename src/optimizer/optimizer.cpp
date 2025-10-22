#include "optimizer.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace box {

std::string ExprValue::to_string() const {
    if (is_constant) {
        if (std::holds_alternative<double>(value)) {
            return "Const(" + std::to_string(std::get<double>(value)) + ")";
        } else if (std::holds_alternative<bool>(value)) {
            return std::string("Const(") + (std::get<bool>(value) ? "true" : "false") + ")";
        }
    }
    return "Expr(...)";
}

std::vector<StmtPtr> ConstantFolder::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    std::vector<StmtPtr> result;
    
    for (const auto& stmt : statements) {
        auto optimized = fold_stmt(stmt);
        if (optimized) {
            result.push_back(optimized);
        }
    }
    
    return result;
}

StmtPtr ConstantFolder::fold_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        auto new_stmt = std::make_shared<ExprStmt>(fold_expr(expr_stmt->expression));
        return new_stmt;
    }
    
    if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        auto new_stmt = std::make_shared<PrintStmt>(
            fold_expr(print_stmt->expression),
            print_stmt->keyword
        );
        return new_stmt;
    }
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        std::optional<ExprPtr> init = std::nullopt;
        if (var_stmt->initializer.has_value()) {
            init = fold_expr(var_stmt->initializer.value());
        }
        return std::make_shared<VarStmt>(var_stmt->name, init);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            auto folded = fold_stmt(s);
            if (folded) {
                new_stmts.push_back(folded);
            }
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto folded_cond = fold_expr(if_stmt->condition);
        
        if (auto* lit = dynamic_cast<Literal*>(folded_cond.get())) {
            bool cond_val = false;
            
            if (std::holds_alternative<bool>(lit->value)) {
                cond_val = std::get<bool>(lit->value);
            } else if (std::holds_alternative<double>(lit->value)) {
                cond_val = (std::get<double>(lit->value) != 0.0);
            }
            
            modified = true;
            if (cond_val) {
                return fold_stmt(if_stmt->then_branch);
            } else if (if_stmt->else_branch.has_value()) {
                return fold_stmt(if_stmt->else_branch.value());
            } else {
                return nullptr;
            }
        }
        
        auto then_br = fold_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = fold_stmt(if_stmt->else_branch.value());
        }
        
        return std::make_shared<IfStmt>(folded_cond, then_br, else_br, if_stmt->keyword);
    }
    
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        auto folded_cond = fold_expr(while_stmt->condition);
        
        if (auto* lit = dynamic_cast<Literal*>(folded_cond.get())) {
            bool cond_val = false;
            
            if (std::holds_alternative<bool>(lit->value)) {
                cond_val = std::get<bool>(lit->value);
            } else if (std::holds_alternative<double>(lit->value)) {
                cond_val = (std::get<double>(lit->value) != 0.0);
            }
            
            if (!cond_val) {
                modified = true;
                return nullptr;
            }
        }
        
        return std::make_shared<WhileStmt>(
            folded_cond,
            fold_stmt(while_stmt->body),
            while_stmt->keyword
        );
    }
    
    if (auto* switch_stmt = dynamic_cast<SwitchStmt*>(stmt.get())) {
        auto folded_cond = fold_expr(switch_stmt->condition);
        
        std::vector<CaseClause> new_cases;
        for (const auto& case_clause : switch_stmt->cases) {
            auto folded_val = fold_expr(case_clause.value);
            std::vector<StmtPtr> case_stmts;
            for (const auto& s : case_clause.statements) {
                auto folded = fold_stmt(s);
                if (folded) {
                    case_stmts.push_back(folded);
                }
            }
            new_cases.emplace_back(folded_val, case_stmts);
        }
        
        std::optional<std::vector<StmtPtr>> new_default;
        if (switch_stmt->default_case.has_value()) {
            std::vector<StmtPtr> default_stmts;
            for (const auto& s : switch_stmt->default_case.value()) {
                auto folded = fold_stmt(s);
                if (folded) {
                    default_stmts.push_back(folded);
                }
            }
            new_default = default_stmts;
        }
        
        return std::make_shared<SwitchStmt>(
            switch_stmt->keyword,
            folded_cond,
            new_cases,
            new_default
        );
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            auto folded = fold_stmt(s);
            if (folded) {
                new_body.push_back(folded);
            }
        }
        return std::make_shared<FunctionStmt>(
            func_stmt->name,
            func_stmt->params,
            new_body
        );
    }
    
    if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        std::optional<ExprPtr> val = std::nullopt;
        if (ret_stmt->value.has_value()) {
            val = fold_expr(ret_stmt->value.value());
        }
        return std::make_shared<ReturnStmt>(ret_stmt->keyword, val);
    }
    
    return stmt;
}

ExprPtr ConstantFolder::fold_expr(const ExprPtr& expr) {
    if (!expr) return nullptr;
    
    if (dynamic_cast<Literal*>(expr.get())) {
        return expr;
    }
    
    if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return fold_expr(grouping->expression);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        auto folded_right = fold_expr(unary->right);
        
        if (auto* lit = dynamic_cast<Literal*>(folded_right.get())) {
            modified = true;
            
            if (unary->op.type == TokenType::MINUS) {
                if (std::holds_alternative<double>(lit->value)) {
                    double val = std::get<double>(lit->value);
                    return std::make_shared<Literal>(
                        LiteralValue(-val),
                        unary->op
                    );
                }
            } else if (unary->op.type == TokenType::BANG) {
                if (std::holds_alternative<bool>(lit->value)) {
                    bool val = std::get<bool>(lit->value);
                    return std::make_shared<Literal>(
                        LiteralValue(!val),
                        unary->op
                    );
                } else if (std::holds_alternative<double>(lit->value)) {
                    double val = std::get<double>(lit->value);
                    return std::make_shared<Literal>(
                        LiteralValue(val == 0.0),
                        unary->op
                    );
                }
            }
        }
        
        return std::make_shared<Unary>(unary->op, folded_right);
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        auto folded_left = fold_expr(binary->left);
        auto folded_right = fold_expr(binary->right);
        
        auto* left_lit = dynamic_cast<Literal*>(folded_left.get());
        auto* right_lit = dynamic_cast<Literal*>(folded_right.get());
        
        if (left_lit && right_lit) {
            bool left_is_num = std::holds_alternative<double>(left_lit->value);
            bool right_is_num = std::holds_alternative<double>(right_lit->value);
            
            if (left_is_num && right_is_num) {
                double left_val = std::get<double>(left_lit->value);
                double right_val = std::get<double>(right_lit->value);
                
                modified = true;
                TokenType op = binary->op.type;
                
                try {
                    if (op == TokenType::PLUS) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val + right_val), binary->op
                        );
                    } else if (op == TokenType::MINUS) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val - right_val), binary->op
                        );
                    } else if (op == TokenType::STAR) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val * right_val), binary->op
                        );
                    } else if (op == TokenType::SLASH) {
                        if (right_val != 0.0) {
                            return std::make_shared<Literal>(
                                LiteralValue(left_val / right_val), binary->op
                            );
                        }
                    } else if (op == TokenType::PERCENT) {
                        if (right_val != 0.0) {
                            return std::make_shared<Literal>(
                                LiteralValue(std::fmod(left_val, right_val)), binary->op
                            );
                        }
                    } else if (op == TokenType::LESS) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val < right_val), binary->op
                        );
                    } else if (op == TokenType::LESS_EQUAL) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val <= right_val), binary->op
                        );
                    } else if (op == TokenType::GREATER) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val > right_val), binary->op
                        );
                    } else if (op == TokenType::GREATER_EQUAL) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val >= right_val), binary->op
                        );
                    } else if (op == TokenType::EQUAL_EQUAL) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val == right_val), binary->op
                        );
                    } else if (op == TokenType::BANG_EQUAL) {
                        return std::make_shared<Literal>(
                            LiteralValue(left_val != right_val), binary->op
                        );
                    }
                } catch (...) {
                }
            }
            
            bool left_is_bool = std::holds_alternative<bool>(left_lit->value);
            bool right_is_bool = std::holds_alternative<bool>(right_lit->value);
            
            if (left_is_bool && right_is_bool) {
                bool left_val = std::get<bool>(left_lit->value);
                bool right_val = std::get<bool>(right_lit->value);
                
                modified = true;
                TokenType op = binary->op.type;
                
                if (op == TokenType::EQUAL_EQUAL) {
                    return std::make_shared<Literal>(
                        LiteralValue(left_val == right_val), binary->op
                    );
                } else if (op == TokenType::BANG_EQUAL) {
                    return std::make_shared<Literal>(
                        LiteralValue(left_val != right_val), binary->op
                    );
                }
            }
        }
        
        return std::make_shared<Binary>(folded_left, binary->op, folded_right);
    }
    
    if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        auto folded_left = fold_expr(logical->left);
        auto folded_right = fold_expr(logical->right);
        
        if (auto* lit = dynamic_cast<Literal*>(folded_left.get())) {
            bool is_truthy = false;
            
            if (std::holds_alternative<bool>(lit->value)) {
                is_truthy = std::get<bool>(lit->value);
            } else if (std::holds_alternative<double>(lit->value)) {
                is_truthy = (std::get<double>(lit->value) != 0.0);
            } else {
                is_truthy = true;
            }
            
            if (logical->op.type == TokenType::AND) {
                if (!is_truthy) {
                    modified = true;
                    return std::make_shared<Literal>(LiteralValue(false), logical->op);
                } else {
                    modified = true;
                    return folded_right;
                }
            } else if (logical->op.type == TokenType::OR) {
                if (is_truthy) {
                    modified = true;
                    return std::make_shared<Literal>(LiteralValue(true), logical->op);
                } else {
                    modified = true;
                    return folded_right;
                }
            }
        }
        
        return std::make_shared<Logical>(folded_left, logical->op, folded_right);
    }
    
    if (auto* array_lit = dynamic_cast<ArrayLiteral*>(expr.get())) {
        std::vector<ExprPtr> new_elems;
        for (const auto& elem : array_lit->elements) {
            new_elems.push_back(fold_expr(elem));
        }
        return std::make_shared<ArrayLiteral>(new_elems, array_lit->bracket);
    }
    
    if (auto* dict_lit = dynamic_cast<DictLiteral*>(expr.get())) {
        std::vector<std::pair<ExprPtr, ExprPtr>> new_pairs;
        for (const auto& [k, v] : dict_lit->pairs) {
            new_pairs.emplace_back(fold_expr(k), fold_expr(v));
        }
        return std::make_shared<DictLiteral>(new_pairs, dict_lit->brace);
    }
    
    if (auto* index_get = dynamic_cast<IndexGet*>(expr.get())) {
        return std::make_shared<IndexGet>(
            fold_expr(index_get->array),
            fold_expr(index_get->index),
            index_get->bracket
        );
    }
    
    if (auto* index_set = dynamic_cast<IndexSet*>(expr.get())) {
        return std::make_shared<IndexSet>(
            fold_expr(index_set->array),
            fold_expr(index_set->index),
            fold_expr(index_set->value),
            index_set->bracket
        );
    }
    
    if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        return std::make_shared<Assign>(
            assign->name,
            fold_expr(assign->value)
        );
    }
    
    if (auto* call = dynamic_cast<Call*>(expr.get())) {
        std::vector<ExprPtr> new_args;
        for (const auto& arg : call->arguments) {
            new_args.push_back(fold_expr(arg));
        }
        return std::make_shared<Call>(call->callee, call->paren, new_args);
    }
    
    return expr;
}

std::optional<double> ConstantFolder::get_numeric_value(const ExprPtr& expr) const {
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            return std::get<double>(lit->value);
        }
    }
    return std::nullopt;
}

std::optional<bool> ConstantFolder::get_boolean_value(const ExprPtr& expr) const {
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        if (std::holds_alternative<bool>(lit->value)) {
            return std::get<bool>(lit->value);
        }
    }
    return std::nullopt;
}

bool ConstantFolder::is_truthy(const ExprPtr& expr) const {
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        if (std::holds_alternative<bool>(lit->value)) {
            return std::get<bool>(lit->value);
        }
        if (std::holds_alternative<double>(lit->value)) {
            return std::get<double>(lit->value) != 0.0;
        }
    }
    return true;
}

std::vector<StmtPtr> AlgebraicSimplifier::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    std::vector<StmtPtr> result;
    
    for (const auto& stmt : statements) {
        auto simplified = simplify_stmt(stmt);
        if (simplified) {
            result.push_back(simplified);
        }
    }
    
    return result;
}

StmtPtr AlgebraicSimplifier::simplify_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        return std::make_shared<ExprStmt>(simplify_expr(expr_stmt->expression));
    }
    
    if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        return std::make_shared<PrintStmt>(
            simplify_expr(print_stmt->expression),
            print_stmt->keyword
        );
    }
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        std::optional<ExprPtr> init = std::nullopt;
        if (var_stmt->initializer.has_value()) {
            init = simplify_expr(var_stmt->initializer.value());
        }
        return std::make_shared<VarStmt>(var_stmt->name, init);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            auto simplified = simplify_stmt(s);
            if (simplified) {
                new_stmts.push_back(simplified);
            }
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto cond = simplify_expr(if_stmt->condition);
        auto then_br = simplify_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = simplify_stmt(if_stmt->else_branch.value());
        }
        return std::make_shared<IfStmt>(cond, then_br, else_br, if_stmt->keyword);
    }
    
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        return std::make_shared<WhileStmt>(
            simplify_expr(while_stmt->condition),
            simplify_stmt(while_stmt->body),
            while_stmt->keyword
        );
    }
    
    if (auto* switch_stmt = dynamic_cast<SwitchStmt*>(stmt.get())) {
        auto cond = simplify_expr(switch_stmt->condition);
        
        std::vector<CaseClause> new_cases;
        for (const auto& case_clause : switch_stmt->cases) {
            auto val = simplify_expr(case_clause.value);
            std::vector<StmtPtr> stmts;
            for (const auto& s : case_clause.statements) {
                auto simplified = simplify_stmt(s);
                if (simplified) {
                    stmts.push_back(simplified);
                }
            }
            new_cases.emplace_back(val, stmts);
        }
        
        std::optional<std::vector<StmtPtr>> new_default;
        if (switch_stmt->default_case.has_value()) {
            std::vector<StmtPtr> stmts;
            for (const auto& s : switch_stmt->default_case.value()) {
                auto simplified = simplify_stmt(s);
                if (simplified) {
                    stmts.push_back(simplified);
                }
            }
            new_default = stmts;
        }
        
        return std::make_shared<SwitchStmt>(switch_stmt->keyword, cond, new_cases, new_default);
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            auto simplified = simplify_stmt(s);
            if (simplified) {
                new_body.push_back(simplified);
            }
        }
        return std::make_shared<FunctionStmt>(func_stmt->name, func_stmt->params, new_body);
    }
    
    if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        std::optional<ExprPtr> val = std::nullopt;
        if (ret_stmt->value.has_value()) {
            val = simplify_expr(ret_stmt->value.value());
        }
        return std::make_shared<ReturnStmt>(ret_stmt->keyword, val);
    }
    
    return stmt;
}

ExprPtr AlgebraicSimplifier::simplify_expr(const ExprPtr& expr) {
    if (!expr) return nullptr;
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        auto left = simplify_expr(binary->left);
        auto right = simplify_expr(binary->right);
        
        TokenType op = binary->op.type;
        
        if (op == TokenType::PLUS) {
            if (is_zero(left)) {
                modified = true;
                return right;
            }
            if (is_zero(right)) {
                modified = true;
                return left;
            }
        }
        
        if (op == TokenType::MINUS) {
            if (is_zero(right)) {
                modified = true;
                return left;
            }
            if (are_equal_variables(left, right)) {
                modified = true;
                return create_literal(0.0);
            }
        }
        
        if (op == TokenType::STAR) {
            if (is_zero(left)) {
                modified = true;
                return create_literal(0.0);
            }
            if (is_zero(right)) {
                modified = true;
                return create_literal(0.0);
            }
            if (is_one(left)) {
                modified = true;
                return right;
            }
            if (is_one(right)) {
                modified = true;
                return left;
            }
            
            if (auto* right_lit = dynamic_cast<Literal*>(right.get())) {
                if (std::holds_alternative<double>(right_lit->value)) {
                    double val = std::get<double>(right_lit->value);
                    if (val == 2.0) {
                        modified = true;
                        Token plus_op(TokenType::PLUS, "+", 0, 0);
                        return std::make_shared<Binary>(left, plus_op, deep_copy_expr(left));
                    }
                    
                    if (is_power_of_two(val)) {
                        modified = true;
                        int power = static_cast<int>(std::log2(val));
                        ExprPtr result = left;
                        Token plus_op(TokenType::PLUS, "+", 0, 0);
                        for (int i = 0; i < power; ++i) {
                            result = std::make_shared<Binary>(
                                result, plus_op, deep_copy_expr(result)
                            );
                        }
                        return result;
                    }
                }
            }
        }
        
        if (op == TokenType::SLASH) {
            if (is_one(right)) {
                modified = true;
                return left;
            }
            if (are_equal_variables(left, right)) {
                modified = true;
                return create_literal(1.0);
            }
        }
        
        return std::make_shared<Binary>(left, binary->op, right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        auto right = simplify_expr(unary->right);
        
        if (unary->op.type == TokenType::MINUS) {
            if (auto* inner_unary = dynamic_cast<Unary*>(right.get())) {
                if (inner_unary->op.type == TokenType::MINUS) {
                    modified = true;
                    return inner_unary->right;
                }
            }
        }
        
        return std::make_shared<Unary>(unary->op, right);
    }
    
    if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return simplify_expr(grouping->expression);
    }
    
    if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        auto left = simplify_expr(logical->left);
        auto right = simplify_expr(logical->right);
        return std::make_shared<Logical>(left, logical->op, right);
    }
    
    if (auto* array_lit = dynamic_cast<ArrayLiteral*>(expr.get())) {
        std::vector<ExprPtr> new_elems;
        for (const auto& elem : array_lit->elements) {
            new_elems.push_back(simplify_expr(elem));
        }
        return std::make_shared<ArrayLiteral>(new_elems, array_lit->bracket);
    }
    
    if (auto* dict_lit = dynamic_cast<DictLiteral*>(expr.get())) {
        std::vector<std::pair<ExprPtr, ExprPtr>> new_pairs;
        for (const auto& [k, v] : dict_lit->pairs) {
            new_pairs.emplace_back(simplify_expr(k), simplify_expr(v));
        }
        return std::make_shared<DictLiteral>(new_pairs, dict_lit->brace);
    }
    
    if (auto* index_get = dynamic_cast<IndexGet*>(expr.get())) {
        return std::make_shared<IndexGet>(
            simplify_expr(index_get->array),
            simplify_expr(index_get->index),
            index_get->bracket
        );
    }
    
    if (auto* index_set = dynamic_cast<IndexSet*>(expr.get())) {
        return std::make_shared<IndexSet>(
            simplify_expr(index_set->array),
            simplify_expr(index_set->index),
            simplify_expr(index_set->value),
            index_set->bracket
        );
    }
    
    if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        return std::make_shared<Assign>(assign->name, simplify_expr(assign->value));
    }
    
    if (auto* call = dynamic_cast<Call*>(expr.get())) {
        std::vector<ExprPtr> new_args;
        for (const auto& arg : call->arguments) {
            new_args.push_back(simplify_expr(arg));
        }
        return std::make_shared<Call>(call->callee, call->paren, new_args);
    }
    
    return expr;
}

bool AlgebraicSimplifier::is_power_of_two(double n) const {
    if (n <= 0.0 || n != static_cast<double>(static_cast<int>(n))) {
        return false;
    }
    int val = static_cast<int>(n);
    return (val & (val - 1)) == 0;
}

bool AlgebraicSimplifier::is_zero(const ExprPtr& expr) const {
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            return std::get<double>(lit->value) == 0.0;
        }
    }
    return false;
}

bool AlgebraicSimplifier::is_one(const ExprPtr& expr) const {
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            return std::get<double>(lit->value) == 1.0;
        }
    }
    return false;
}

bool AlgebraicSimplifier::are_equal_variables(const ExprPtr& a, const ExprPtr& b) const {
    auto* var_a = dynamic_cast<Variable*>(a.get());
    auto* var_b = dynamic_cast<Variable*>(b.get());
    
    if (var_a && var_b) {
        return var_a->name.lexeme == var_b->name.lexeme;
    }
    return false;
}

ExprPtr AlgebraicSimplifier::create_literal(double value) const {
    std::string val_str = std::to_string(value);
    Token dummy(TokenType::NUMBER, val_str, LiteralValue(value), 0, 0);
    return std::make_shared<Literal>(LiteralValue(value), dummy);
}

ExprPtr AlgebraicSimplifier::deep_copy_expr(const ExprPtr& expr) const {
    if (!expr) return nullptr;
    
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        return std::make_shared<Literal>(lit->value, lit->token);
    }
    
    if (auto* var = dynamic_cast<Variable*>(expr.get())) {
        return std::make_shared<Variable>(var->name);
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        return std::make_shared<Binary>(
            deep_copy_expr(binary->left),
            binary->op,
            deep_copy_expr(binary->right)
        );
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return std::make_shared<Unary>(unary->op, deep_copy_expr(unary->right));
    }
    
    return expr;
}

std::vector<StmtPtr> DeadCodeEliminator::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    used_vars.clear();
    defined_vars.clear();
    essential_vars.clear();
    
    mark_essential_variables(statements);
    
    for (const auto& stmt : statements) {
        analyze_stmt(stmt);
    }
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        if (should_keep_stmt(stmt)) {
            result.push_back(eliminate_stmt(stmt));
        }
    }
    
    return result;
}

void DeadCodeEliminator::mark_essential_variables(const std::vector<StmtPtr>& statements) {
    for (const auto& stmt : statements) {
        if (dynamic_cast<PrintStmt*>(stmt.get()) ||
            dynamic_cast<ReturnStmt*>(stmt.get()) ||
            dynamic_cast<Call*>(stmt.get())) {
            }
    }
}

void DeadCodeEliminator::analyze_stmt(const StmtPtr& stmt) {
    if (!stmt) return;
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        defined_vars[var_stmt->name.lexeme] = stmt;
        if (var_stmt->initializer.has_value()) {
            analyze_expr(var_stmt->initializer.value());
        }
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        analyze_expr(expr_stmt->expression);
    } else if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        analyze_expr(print_stmt->expression);
    } else if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        for (const auto& s : block->statements) {
            analyze_stmt(s);
        }
    } else if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        analyze_expr(if_stmt->condition);
        analyze_stmt(if_stmt->then_branch);
        if (if_stmt->else_branch.has_value()) {
            analyze_stmt(if_stmt->else_branch.value());
        }
    } else if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        analyze_expr(while_stmt->condition);
        analyze_stmt(while_stmt->body);
    } else if (auto* switch_stmt = dynamic_cast<SwitchStmt*>(stmt.get())) {
        analyze_expr(switch_stmt->condition);
        for (const auto& case_clause : switch_stmt->cases) {
            analyze_expr(case_clause.value);
            for (const auto& s : case_clause.statements) {
                analyze_stmt(s);
            }
        }
        if (switch_stmt->default_case.has_value()) {
            for (const auto& s : switch_stmt->default_case.value()) {
                analyze_stmt(s);
            }
        }
    } else if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        for (const auto& s : func_stmt->body) {
            analyze_stmt(s);
        }
    } else if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        if (ret_stmt->value.has_value()) {
            analyze_expr(ret_stmt->value.value());
        }
    }
}

void DeadCodeEliminator::analyze_expr(const ExprPtr& expr) {
    if (!expr) return;
    
    if (auto* var = dynamic_cast<Variable*>(expr.get())) {
        used_vars.insert(var->name.lexeme);
    } else if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        used_vars.insert(assign->name.lexeme);
        analyze_expr(assign->value);
    } else if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        analyze_expr(binary->left);
        analyze_expr(binary->right);
    } else if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        analyze_expr(unary->right);
    } else if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        analyze_expr(logical->left);
        analyze_expr(logical->right);
    } else if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        analyze_expr(grouping->expression);
    } else if (auto* call = dynamic_cast<Call*>(expr.get())) {
        for (const auto& arg : call->arguments) {
            analyze_expr(arg);
        }
    } else if (auto* array_lit = dynamic_cast<ArrayLiteral*>(expr.get())) {
        for (const auto& elem : array_lit->elements) {
            analyze_expr(elem);
        }
    } else if (auto* dict_lit = dynamic_cast<DictLiteral*>(expr.get())) {
        for (const auto& [k, v] : dict_lit->pairs) {
            analyze_expr(k);
            analyze_expr(v);
        }
    } else if (auto* index_get = dynamic_cast<IndexGet*>(expr.get())) {
        analyze_expr(index_get->array);
        analyze_expr(index_get->index);
    } else if (auto* index_set = dynamic_cast<IndexSet*>(expr.get())) {
        analyze_expr(index_set->array);
        analyze_expr(index_set->index);
        analyze_expr(index_set->value);
    }
}

bool DeadCodeEliminator::should_keep_stmt(const StmtPtr& stmt) const {
    if (!stmt) return false;
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        if (var_stmt->initializer.has_value() && 
            has_side_effects(var_stmt->initializer.value())) {
            return true;
        }
        return used_vars.count(var_stmt->name.lexeme) > 0 ||
               essential_vars.count(var_stmt->name.lexeme) > 0;
    }
    
    return true;
}

bool DeadCodeEliminator::has_side_effects(const ExprPtr& expr) const {
    if (!expr) return false;
    
    if (dynamic_cast<Call*>(expr.get())) return true;
    if (dynamic_cast<Assign*>(expr.get())) return true;
    if (dynamic_cast<IndexSet*>(expr.get())) return true;
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        return has_side_effects(binary->left) || has_side_effects(binary->right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return has_side_effects(unary->right);
    }
    
    if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        return has_side_effects(logical->left) || has_side_effects(logical->right);
    }
    
    if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return has_side_effects(grouping->expression);
    }
    
    if (auto* array_lit = dynamic_cast<ArrayLiteral*>(expr.get())) {
        for (const auto& elem : array_lit->elements) {
            if (has_side_effects(elem)) return true;
        }
    }
    
    if (auto* dict_lit = dynamic_cast<DictLiteral*>(expr.get())) {
        for (const auto& [k, v] : dict_lit->pairs) {
            if (has_side_effects(k) || has_side_effects(v)) return true;
        }
    }
    
    if (auto* index_get = dynamic_cast<IndexGet*>(expr.get())) {
        return has_side_effects(index_get->array) || has_side_effects(index_get->index);
    }
    
    return false;
}

StmtPtr DeadCodeEliminator::eliminate_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            if (should_keep_stmt(s)) {
                new_stmts.push_back(eliminate_stmt(s));
            }
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto then_br = eliminate_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = eliminate_stmt(if_stmt->else_branch.value());
        }
        return std::make_shared<IfStmt>(
            if_stmt->condition, then_br, else_br, if_stmt->keyword
        );
    }
    
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        return std::make_shared<WhileStmt>(
            while_stmt->condition,
            eliminate_stmt(while_stmt->body),
            while_stmt->keyword
        );
    }
    
    if (auto* switch_stmt = dynamic_cast<SwitchStmt*>(stmt.get())) {
        std::vector<CaseClause> new_cases;
        for (const auto& case_clause : switch_stmt->cases) {
            std::vector<StmtPtr> stmts;
            for (const auto& s : case_clause.statements) {
                if (should_keep_stmt(s)) {
                    stmts.push_back(eliminate_stmt(s));
                }
            }
            new_cases.emplace_back(case_clause.value, stmts);
        }
        
        std::optional<std::vector<StmtPtr>> new_default;
        if (switch_stmt->default_case.has_value()) {
            std::vector<StmtPtr> stmts;
            for (const auto& s : switch_stmt->default_case.value()) {
                if (should_keep_stmt(s)) {
                    stmts.push_back(eliminate_stmt(s));
                }
            }
            new_default = stmts;
        }
        
        return std::make_shared<SwitchStmt>(
            switch_stmt->keyword, switch_stmt->condition, new_cases, new_default
        );
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            if (should_keep_stmt(s)) {
                new_body.push_back(eliminate_stmt(s));
            }
        }
        return std::make_shared<FunctionStmt>(
            func_stmt->name, func_stmt->params, new_body
        );
    }
    
    return stmt;
}

std::vector<StmtPtr> CommonSubexpressionEliminator::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    expr_cache.clear();
    temp_counter = 0;
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        result.push_back(process_stmt(stmt));
    }
    
    return result;
}

StmtPtr CommonSubexpressionEliminator::process_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            new_stmts.push_back(process_stmt(s));
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto cond = process_expr(if_stmt->condition);
        auto then_br = process_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = process_stmt(if_stmt->else_branch.value());
        }
        return std::make_shared<IfStmt>(cond, then_br, else_br, if_stmt->keyword);
    }
    
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        return std::make_shared<WhileStmt>(
            process_expr(while_stmt->condition),
            process_stmt(while_stmt->body),
            while_stmt->keyword
        );
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        auto old_cache = expr_cache;
        expr_cache.clear();
        
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            new_body.push_back(process_stmt(s));
        }
        
        expr_cache = old_cache;
        return std::make_shared<FunctionStmt>(func_stmt->name, func_stmt->params, new_body);
    }
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        return std::make_shared<ExprStmt>(process_expr(expr_stmt->expression));
    }
    
    if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        return std::make_shared<PrintStmt>(
            process_expr(print_stmt->expression),
            print_stmt->keyword
        );
    }
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        std::optional<ExprPtr> init = std::nullopt;
        if (var_stmt->initializer.has_value()) {
            init = process_expr(var_stmt->initializer.value());
        }
        return std::make_shared<VarStmt>(var_stmt->name, init);
    }
    
    if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        std::optional<ExprPtr> val = std::nullopt;
        if (ret_stmt->value.has_value()) {
            val = process_expr(ret_stmt->value.value());
        }
        return std::make_shared<ReturnStmt>(ret_stmt->keyword, val);
    }
    
    return stmt;
}

ExprPtr CommonSubexpressionEliminator::process_expr(const ExprPtr& expr) {
    if (!expr) return nullptr;
    
    if (dynamic_cast<Literal*>(expr.get()) || dynamic_cast<Variable*>(expr.get())) {
        return expr;
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        auto left = process_expr(binary->left);
        auto right = process_expr(binary->right);
        return std::make_shared<Binary>(left, binary->op, right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        auto right = process_expr(unary->right);
        return std::make_shared<Unary>(unary->op, right);
    }
    
    if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        auto left = process_expr(logical->left);
        auto right = process_expr(logical->right);
        return std::make_shared<Logical>(left, logical->op, right);
    }
    
    if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return std::make_shared<Grouping>(process_expr(grouping->expression));
    }
    
    return expr;
}

std::string CommonSubexpressionEliminator::expr_to_string(const ExprPtr& expr) const {
    if (!expr) return "null";
    
    if (auto* lit = dynamic_cast<Literal*>(expr.get())) {
        std::ostringstream oss;
        oss << "lit_";
        if (std::holds_alternative<double>(lit->value)) {
            oss << std::get<double>(lit->value);
        } else if (std::holds_alternative<bool>(lit->value)) {
            oss << (std::get<bool>(lit->value) ? "true" : "false");
        }
        return oss.str();
    }
    
    if (auto* var = dynamic_cast<Variable*>(expr.get())) {
        return "var_" + var->name.lexeme;
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        return "bin_" + expr_to_string(binary->left) + "_" + 
               binary->op.lexeme + "_" + expr_to_string(binary->right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return "un_" + unary->op.lexeme + "_" + expr_to_string(unary->right);
    }
    
    std::ostringstream oss;
    oss << reinterpret_cast<uintptr_t>(expr.get());
    return oss.str();
}

size_t CommonSubexpressionEliminator::expr_hash(const ExprPtr& expr) const {
    return std::hash<std::string>{}(expr_to_string(expr));
}

bool CommonSubexpressionEliminator::exprs_equal(const ExprPtr& a, const ExprPtr& b) const {
    return expr_to_string(a) == expr_to_string(b);
}

std::vector<StmtPtr> LoopOptimizer::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        result.push_back(optimize_stmt(stmt));
    }
    
    return result;
}

StmtPtr LoopOptimizer::optimize_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (dynamic_cast<WhileStmt*>(stmt.get())) {
        StmtPtr optimized = try_unroll_loop(stmt);
        
        if (auto* still_while = dynamic_cast<WhileStmt*>(optimized.get())) {
            return std::make_shared<WhileStmt>(
                still_while->condition,
                optimize_stmt(still_while->body),
                still_while->keyword
            );
        }
        
        return optimized;
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            new_stmts.push_back(optimize_stmt(s));
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto then_br = optimize_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = optimize_stmt(if_stmt->else_branch.value());
        }
        return std::make_shared<IfStmt>(
            if_stmt->condition, then_br, else_br, if_stmt->keyword
        );
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            new_body.push_back(optimize_stmt(s));
        }
        return std::make_shared<FunctionStmt>(
            func_stmt->name, func_stmt->params, new_body
        );
    }
    
    return stmt;
}

StmtPtr LoopOptimizer::try_unroll_loop(const StmtPtr& stmt) {
    if (!config.loop_unrolling) return stmt;
    
    if (!can_unroll(stmt)) return stmt;
    
    auto iter_count = get_iteration_count(stmt);
    if (iter_count.has_value() && 
        iter_count.value() > 0 && 
        iter_count.value() <= config.loop_unroll_threshold) {
        modified = true;
        return unroll_loop(stmt, iter_count.value());
    }
    
    return stmt;
}

StmtPtr LoopOptimizer::extract_loop_invariant_code(const StmtPtr& stmt) {
    return stmt;
}

bool LoopOptimizer::can_unroll([[maybe_unused]] const StmtPtr& stmt) const {
    return false;
}

std::optional<int> LoopOptimizer::get_iteration_count([[maybe_unused]] const StmtPtr& stmt) const {
    return std::nullopt;
}

StmtPtr LoopOptimizer::unroll_loop(const StmtPtr& stmt, int iterations) {
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        std::vector<StmtPtr> statements;
        
        for (int i = 0; i < iterations; ++i) {
            statements.push_back(while_stmt->body);
        }
        
        Token dummy_brace(TokenType::LBRACE, "{", 0, 0);
        return std::make_shared<Block>(statements, dummy_brace);
    }
    
    return stmt;
}

bool LoopOptimizer::is_loop_invariant(
    const ExprPtr& expr, 
    const std::unordered_set<std::string>& loop_vars) const {
    
    if (!expr) return true;
    
    if (auto* var = dynamic_cast<Variable*>(expr.get())) {
        return loop_vars.count(var->name.lexeme) == 0;
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        return is_loop_invariant(binary->left, loop_vars) &&
               is_loop_invariant(binary->right, loop_vars);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return is_loop_invariant(unary->right, loop_vars);
    }
    
    return true;
}

std::unordered_set<std::string> LoopOptimizer::find_modified_vars(const StmtPtr& stmt) const {
    std::unordered_set<std::string> vars;
    collect_modified_vars(stmt, vars);
    return vars;
}

void LoopOptimizer::collect_modified_vars(
    const StmtPtr& stmt, 
    std::unordered_set<std::string>& vars) const {
    
    if (!stmt) return;
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        vars.insert(var_stmt->name.lexeme);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        for (const auto& s : block->statements) {
            collect_modified_vars(s, vars);
        }
    }
}

std::vector<StmtPtr> StrengthReducer::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        result.push_back(reduce_stmt(stmt));
    }
    
    return result;
}

StmtPtr StrengthReducer::reduce_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        return std::make_shared<ExprStmt>(reduce_expr(expr_stmt->expression));
    }
    
    if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        return std::make_shared<PrintStmt>(
            reduce_expr(print_stmt->expression),
            print_stmt->keyword
        );
    }
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        std::optional<ExprPtr> init = std::nullopt;
        if (var_stmt->initializer.has_value()) {
            init = reduce_expr(var_stmt->initializer.value());
        }
        return std::make_shared<VarStmt>(var_stmt->name, init);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            new_stmts.push_back(reduce_stmt(s));
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        auto cond = reduce_expr(if_stmt->condition);
        auto then_br = reduce_stmt(if_stmt->then_branch);
        std::optional<StmtPtr> else_br = std::nullopt;
        if (if_stmt->else_branch.has_value()) {
            else_br = reduce_stmt(if_stmt->else_branch.value());
        }
        return std::make_shared<IfStmt>(cond, then_br, else_br, if_stmt->keyword);
    }
    
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        return std::make_shared<WhileStmt>(
            reduce_expr(while_stmt->condition),
            reduce_stmt(while_stmt->body),
            while_stmt->keyword
        );
    }
    
    if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        std::vector<StmtPtr> new_body;
        for (const auto& s : func_stmt->body) {
            new_body.push_back(reduce_stmt(s));
        }
        return std::make_shared<FunctionStmt>(
            func_stmt->name, func_stmt->params, new_body
        );
    }
    
    if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
        std::optional<ExprPtr> val = std::nullopt;
        if (ret_stmt->value.has_value()) {
            val = reduce_expr(ret_stmt->value.value());
        }
        return std::make_shared<ReturnStmt>(ret_stmt->keyword, val);
    }
    
    return stmt;
}

ExprPtr StrengthReducer::reduce_expr(const ExprPtr& expr) {
    if (!expr) return nullptr;
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        auto left = reduce_expr(binary->left);
        auto right = reduce_expr(binary->right);
        
        if (binary->op.type == TokenType::STAR) {
            auto reduced = reduce_multiplication(left, right, binary->op);
            if (reduced) return reduced;
        } else if (binary->op.type == TokenType::SLASH) {
            auto reduced = reduce_division(left, right, binary->op);
            if (reduced) return reduced;
        } else if (binary->op.type == TokenType::PERCENT) {
            auto reduced = reduce_modulo(left, right, binary->op);
            if (reduced) return reduced;
        }
        
        return std::make_shared<Binary>(left, binary->op, right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        return std::make_shared<Unary>(unary->op, reduce_expr(unary->right));
    }
    
    if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        return std::make_shared<Assign>(assign->name, reduce_expr(assign->value));
    }
    
    return expr;
}

ExprPtr StrengthReducer::reduce_multiplication(
    const ExprPtr& left, 
    const ExprPtr& right, 
    [[maybe_unused]] const Token& op) {
    
    if (auto* lit = dynamic_cast<Literal*>(right.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            double val = std::get<double>(lit->value);
            
            if (is_power_of_two(val)) {
                modified = true;
                int shift_amount = log2_int(static_cast<int>(val));
                
                ExprPtr result = left;
                Token plus_op(TokenType::PLUS, "+", 0, 0);
                
                for (int i = 0; i < shift_amount; ++i) {
                    result = std::make_shared<Binary>(result, plus_op, result);
                }
                
                return result;
            }
        }
    }
    
    return nullptr;
}

ExprPtr StrengthReducer::reduce_division(
    const ExprPtr& left, 
    const ExprPtr& right, 
    const Token& op) {
    
    if (auto* lit = dynamic_cast<Literal*>(right.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            double val = std::get<double>(lit->value);
            
            if (is_power_of_two(val) && val >= 2.0) {
                modified = true;
                int shift_amount = log2_int(static_cast<int>(val));
                
                ExprPtr result = left;
                Token div_op(TokenType::SLASH, "/", op.line, op.column);
                
                for (int i = 0; i < shift_amount; ++i) {
                    auto two = std::make_shared<Literal>(
                        LiteralValue(2.0),
                        Token(TokenType::NUMBER, "2", op.line, op.column)
                    );
                    result = std::make_shared<Binary>(result, div_op, two);
                }
                
                return result;
            }
        }
    }
    
    return nullptr;
}

ExprPtr StrengthReducer::reduce_modulo(
    const ExprPtr& left, 
    const ExprPtr& right, 
    const Token& op) {
    
    if (auto* lit = dynamic_cast<Literal*>(right.get())) {
        if (std::holds_alternative<double>(lit->value)) {
            double val = std::get<double>(lit->value);
            
            if (is_power_of_two(val) && val >= 2.0) {
                modified = true;
                
                auto mask_value = std::make_shared<Literal>(
                    LiteralValue(val - 1.0),
                    Token(TokenType::NUMBER, std::to_string(val - 1.0), op.line, op.column)
                );
                
                Token dummy_op(TokenType::STAR, "*", op.line, op.column);
                auto zero = std::make_shared<Literal>(
                    LiteralValue(0.0),
                    Token(TokenType::NUMBER, "0", op.line, op.column)
                );
                auto mask_expr = std::make_shared<Binary>(mask_value, dummy_op, zero);
                mask_expr = std::make_shared<Binary>(mask_expr, Token(TokenType::PLUS, "+", op.line, op.column), mask_value);
                
                Token and_op(TokenType::STAR, "*", op.line, op.column);
                
                auto temp_div = std::make_shared<Binary>(left, Token(TokenType::SLASH, "/", op.line, op.column), right);
                auto temp_mult = std::make_shared<Binary>(temp_div, Token(TokenType::STAR, "*", op.line, op.column), right);
                auto result = std::make_shared<Binary>(left, Token(TokenType::MINUS, "-", op.line, op.column), temp_mult);
                
                return result;
            }
        }
    }
    
    return nullptr;
}

bool StrengthReducer::is_power_of_two(double n) const {
    if (n <= 0.0 || n != static_cast<double>(static_cast<int>(n))) {
        return false;
    }
    int val = static_cast<int>(n);
    return (val & (val - 1)) == 0;
}

int StrengthReducer::log2_int(int n) const {
    int result = 0;
    while (n > 1) {
        n >>= 1;
        result++;
    }
    return result;
}

std::vector<StmtPtr> FunctionInliner::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    function_definitions.clear();
    
    collect_functions(statements);
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        result.push_back(inline_stmt(stmt));
    }
    
    return result;
}

void FunctionInliner::collect_functions(const std::vector<StmtPtr>& statements) {
    for (const auto& stmt : statements) {
        if (auto* func_stmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
            function_definitions[func_stmt->name.lexeme] = stmt;
        }
    }
}

StmtPtr FunctionInliner::inline_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        return std::make_shared<ExprStmt>(inline_expr(expr_stmt->expression));
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            new_stmts.push_back(inline_stmt(s));
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    return stmt;
}

ExprPtr FunctionInliner::inline_expr(const ExprPtr& expr) {
    return expr;
}

bool FunctionInliner::should_inline(const std::string& func_name) const {
    auto it = function_definitions.find(func_name);
    if (it == function_definitions.end()) {
        return false;
    }
    
    int complexity = calculate_complexity(it->second);
    return complexity <= config.inline_threshold;
}

int FunctionInliner::calculate_complexity(const StmtPtr& stmt) const {
    if (!stmt) return 0;
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        int total = 0;
        for (const auto& s : block->statements) {
            total += calculate_complexity(s);
        }
        return total;
    }
    
    return 1;
}

ExprPtr FunctionInliner::substitute_params(
    const ExprPtr& expr,
    const std::vector<Token>& params,
    const std::vector<ExprPtr>& args) {
    
    if (!expr) return nullptr;
    
    if (auto* var = dynamic_cast<Variable*>(expr.get())) {
        for (size_t i = 0; i < params.size() && i < args.size(); ++i) {
            if (var->name.lexeme == params[i].lexeme) {
                return args[i];
            }
        }
        return expr;
    }
    
    if (auto* binary = dynamic_cast<Binary*>(expr.get())) {
        auto left = substitute_params(binary->left, params, args);
        auto right = substitute_params(binary->right, params, args);
        return std::make_shared<Binary>(left, binary->op, right);
    }
    
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        auto right = substitute_params(unary->right, params, args);
        return std::make_shared<Unary>(unary->op, right);
    }
    
    if (auto* logical = dynamic_cast<Logical*>(expr.get())) {
        auto left = substitute_params(logical->left, params, args);
        auto right = substitute_params(logical->right, params, args);
        return std::make_shared<Logical>(left, logical->op, right);
    }
    
    if (auto* assign = dynamic_cast<Assign*>(expr.get())) {
        auto value = substitute_params(assign->value, params, args);
        return std::make_shared<Assign>(assign->name, value);
    }
    
    if (auto* call = dynamic_cast<Call*>(expr.get())) {
        auto callee = substitute_params(call->callee, params, args);
        std::vector<ExprPtr> new_args;
        for (const auto& arg : call->arguments) {
            new_args.push_back(substitute_params(arg, params, args));
        }
        return std::make_shared<Call>(callee, call->paren, new_args);
    }
    
    if (auto* grouping = dynamic_cast<Grouping*>(expr.get())) {
        return std::make_shared<Grouping>(substitute_params(grouping->expression, params, args));
    }
    
    return expr;
}

std::vector<StmtPtr> PeepholeOptimizer::run(const std::vector<StmtPtr>& statements) {
    modified = false;
    
    std::vector<StmtPtr> result;
    for (const auto& stmt : statements) {
        result.push_back(optimize_stmt(stmt));
    }
    
    return result;
}

StmtPtr PeepholeOptimizer::optimize_stmt(const StmtPtr& stmt) {
    if (!stmt) return nullptr;
    
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        return std::make_shared<ExprStmt>(optimize_expr(expr_stmt->expression));
    }
    
    if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt.get())) {
        return std::make_shared<PrintStmt>(
            optimize_expr(print_stmt->expression),
            print_stmt->keyword
        );
    }
    
    if (auto* var_stmt = dynamic_cast<VarStmt*>(stmt.get())) {
        std::optional<ExprPtr> init = std::nullopt;
        if (var_stmt->initializer.has_value()) {
            init = optimize_expr(var_stmt->initializer.value());
        }
        return std::make_shared<VarStmt>(var_stmt->name, init);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt.get())) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->statements) {
            new_stmts.push_back(optimize_stmt(s));
        }
        return std::make_shared<Block>(new_stmts, block->opening_brace);
    }
    
    return stmt;
}

ExprPtr PeepholeOptimizer::optimize_expr(const ExprPtr& expr) {
    if (!expr) return nullptr;
    
    auto optimized = optimize_double_negation(expr);
    if (optimized != expr) {
        modified = true;
        return optimized;
    }
    
    optimized = optimize_boolean_operations(expr);
    if (optimized != expr) {
        modified = true;
        return optimized;
    }
    
    return expr;
}

ExprPtr PeepholeOptimizer::optimize_double_negation(const ExprPtr& expr) {
    if (auto* unary = dynamic_cast<Unary*>(expr.get())) {
        if (unary->op.type == TokenType::MINUS || unary->op.type == TokenType::BANG) {
            if (auto* inner_unary = dynamic_cast<Unary*>(unary->right.get())) {
                if (inner_unary->op.type == unary->op.type) {
                    return inner_unary->right;
                }
            }
        }
    }
    return expr;
}

ExprPtr PeepholeOptimizer::optimize_comparison_chain(const ExprPtr& expr) {
    return expr;
}

ExprPtr PeepholeOptimizer::optimize_boolean_operations(const ExprPtr& expr) {
    return expr;
}

Optimizer::Optimizer(const OptimizerConfig& cfg)
    : config(cfg) {
    initialize_passes();
}

void Optimizer::initialize_passes() {
    passes.clear();
    
    if (config.constant_folding) {
        passes.push_back(std::make_unique<ConstantFolder>(config));
    }
    
    if (config.algebraic_simplification) {
        passes.push_back(std::make_unique<AlgebraicSimplifier>(config));
    }
    
    if (config.dead_code_elimination) {
        passes.push_back(std::make_unique<DeadCodeEliminator>(config));
    }
    
    if (config.common_subexpression_elimination) {
        passes.push_back(std::make_unique<CommonSubexpressionEliminator>(config));
    }
    
    if (config.loop_unrolling || config.loop_invariant_code_motion) {
        passes.push_back(std::make_unique<LoopOptimizer>(config));
    }
    
    if (config.strength_reduction) {
        passes.push_back(std::make_unique<StrengthReducer>(config));
    }
    
    if (config.function_inlining) {
        passes.push_back(std::make_unique<FunctionInliner>(config));
    }
    
    if (config.peephole_optimization) {
        passes.push_back(std::make_unique<PeepholeOptimizer>(config));
    }
}

std::vector<StmtPtr> Optimizer::optimize(const std::vector<StmtPtr>& statements) {
    const int max_iterations = 10;
    int iteration = 0;
    
    std::vector<StmtPtr> current = statements;
    
    while (iteration < max_iterations) {
        iteration++;
        bool any_modified = false;
        
        for (auto& pass : passes) {
            current = pass->run(current);
            if (pass->was_modified()) {
                any_modified = true;
            }
        }
        
        if (!any_modified) {
            break;
        }
    }
    
    return current;
}

}
