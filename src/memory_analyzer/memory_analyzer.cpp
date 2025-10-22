#include "memory_analyzer.hpp"
#include <sstream>
#include <algorithm>
#include <queue>
#include <stack>

namespace box {

std::string memory_state_to_string(MemoryState state) {
    switch (state) {
        case MemoryState::UNINITIALIZED: return "uninitialized";
        case MemoryState::ALLOCATED: return "allocated";
        case MemoryState::FREED: return "freed";
        case MemoryState::INVALID: return "invalid";
        case MemoryState::UNKNOWN: return "unknown";
    }
    return "unknown";
}

std::string pointer_state_to_string(PointerState state) {
    switch (state) {
        case PointerState::NULL_PTR: return "null";
        case PointerState::VALID: return "valid";
        case PointerState::DANGLING: return "dangling";
        case PointerState::UNKNOWN: return "unknown";
    }
    return "unknown";
}

AllocationInfo::AllocationInfo()
    : var_name(""),
      allocation_token(Token{TokenType::END_OF_FILE, "", 0, 0}),
      state(MemoryState::UNKNOWN),
      freed_at(std::nullopt),
      size_expr(std::nullopt),
      is_array(false),
      ref_count(0),
      aliases() {}

AllocationInfo::AllocationInfo(
    const std::string& name,
    const Token& token,
    MemoryState s,
    bool array
) : var_name(name),
    allocation_token(token),
    state(s),
    freed_at(std::nullopt),
    size_expr(std::nullopt),
    is_array(array),
    ref_count(0),
    aliases() {}

bool AllocationInfo::operator==(const AllocationInfo& other) const {
    return var_name == other.var_name &&
           state == other.state &&
           is_array == other.is_array &&
           ref_count == other.ref_count;
}

PointerInfo::PointerInfo()
    : var_name(""),
      declaration_token(Token{TokenType::END_OF_FILE, "", 0, 0}),
      pointee_type(""),
      state(PointerState::UNKNOWN),
      points_to(std::nullopt),
      level(1) {}

PointerInfo::PointerInfo(
    const std::string& name,
    const Token& token,
    const std::string& type,
    PointerState s,
    int lvl
) : var_name(name),
    declaration_token(token),
    pointee_type(type),
    state(s),
    points_to(std::nullopt),
    level(lvl) {}

ControlFlowNode::ControlFlowNode(NodeType t, int id)
    : type(t),
      statement(nullptr),
      expression(nullptr),
      node_id(id) {}

ControlFlowPath::ControlFlowPath()
    : is_feasible(true) {}

MemorySafetyError::MemorySafetyError(
    const std::string& msg,
    const Token& tok,
    const std::optional<std::string>& h,
    const std::string& err_type
) : std::runtime_error(format_error(msg, tok, h, err_type)),
    message(msg),
    token(tok),
    hint(h),
    error_type(err_type) {}

std::string MemorySafetyError::format_error(
    const std::string& msg,
    const Token& tok,
    const std::optional<std::string>& hint,
    const std::string& error_type
) {
    std::ostringstream oss;
    oss << "\n" << std::string(70, '=') << "\n";
    oss << error_type << " at Line " << tok.line << ", Column " << tok.column << "\n";
    oss << std::string(70, '=') << "\n";
    oss << "Error: " << msg << "\n";
    
    if (hint) {
        oss << "\nHint: " << *hint << "\n";
    }
    
    oss << std::string(70, '=') << "\n";
    return oss.str();
}

MemorySafetyAnalyzer::MemorySafetyAnalyzer()
    : strict_mode(true),
      next_cfg_node_id(0) {
    current_scope_vars.push_back(std::unordered_set<std::string>());
    freed_in_scope.push_back(std::unordered_set<std::string>());
}

bool MemorySafetyAnalyzer::analyze(const std::vector<StmtPtr>& statements) {
    errors.clear();
    warnings.clear();
    
    try {
        for (const auto& stmt : statements) {
            analyze_stmt(stmt);
        }
        
        check_memory_leaks();
        
        CFGNodePtr cfg_entry = build_cfg(statements);
        if (cfg_entry) {
            perform_dataflow_analysis(cfg_entry);
        }
        
        if (!errors.empty()) {
            return false;
        }
        
        return true;
        
    } catch (const MemorySafetyError& e) {
        errors.push_back(e);
        return false;
    }
}

void MemorySafetyAnalyzer::enter_scope() {
    current_scope_vars.push_back(std::unordered_set<std::string>());
    freed_in_scope.push_back(std::unordered_set<std::string>());
}

void MemorySafetyAnalyzer::exit_scope() {
    if (current_scope_vars.size() > 1) {
        auto scope_vars = current_scope_vars.back();
        current_scope_vars.pop_back();
        
        auto freed = freed_in_scope.back();
        freed_in_scope.pop_back();
        
        for (const auto& var : scope_vars) {
            auto it = allocations.find(var);
            if (it != allocations.end()) {
                const auto& alloc = it->second;
                if (alloc.state == MemoryState::ALLOCATED && freed.find(var) == freed.end()) {
                    if (strict_mode) {
                        throw MemorySafetyError(
                            "Memory leak: Variable '" + var + "' goes out of scope without being freed",
                            alloc.allocation_token,
                            "Add 'free(" + var + ");' before the end of this scope",
                            "MEMORY LEAK"
                        );
                    } else {
                        warnings.push_back("Potential memory leak: " + var);
                    }
                }
            }
        }
    }
}

void MemorySafetyAnalyzer::analyze_stmt(const StmtPtr& stmt) {
    if (auto* var_stmt = get_stmt_as<VarStmt>(stmt)) {
        analyze_var_stmt(var_stmt);
    } else if (auto* expr_stmt = get_stmt_as<ExprStmt>(stmt)) {
        analyze_expr_stmt(expr_stmt);
    } else if (auto* block = get_stmt_as<Block>(stmt)) {
        analyze_block(block);
    } else if (auto* if_stmt = get_stmt_as<IfStmt>(stmt)) {
        analyze_if_stmt(if_stmt);
    } else if (auto* while_stmt = get_stmt_as<WhileStmt>(stmt)) {
        analyze_while_stmt(while_stmt);
    } else if (auto* func_stmt = get_stmt_as<FunctionStmt>(stmt)) {
        analyze_function_stmt(func_stmt);
    } else if (auto* ret_stmt = get_stmt_as<ReturnStmt>(stmt)) {
        analyze_return_stmt(ret_stmt);
    } else if (auto* print_stmt = get_stmt_as<PrintStmt>(stmt)) {
        analyze_expr(print_stmt->expression);
    } else if (auto* switch_stmt = get_stmt_as<SwitchStmt>(stmt)) {
        analyze_switch_stmt(switch_stmt);
    } else if (auto* unsafe_block = get_stmt_as<UnsafeBlock>(stmt)) {
        analyze_unsafe_block(unsafe_block);
    }
}

void MemorySafetyAnalyzer::analyze_var_stmt(const VarStmt* stmt) {
    std::string var_name = stmt->name.lexeme;
    current_scope_vars.back().insert(var_name);
    
    if (stmt->initializer) {
        analyze_expr(*stmt->initializer);
        
        if (auto* call_expr = get_expr_as<Call>(*stmt->initializer)) {
            if (auto* callee_var = get_expr_as<Variable>(call_expr->callee)) {
                std::string func_name = callee_var->name.lexeme;
                
                if (func_name == "malloc" || func_name == "calloc" || func_name == "realloc") {
                    auto it = allocations.find(var_name);
                    if (it != allocations.end()) {
                        const auto& old_alloc = it->second;
                        if (old_alloc.state == MemoryState::ALLOCATED) {
                            throw MemorySafetyError(
                                "Memory leak: '" + var_name + "' is being reassigned without freeing previous allocation",
                                stmt->name,
                                "Free the previous allocation first: free(" + var_name + ");",
                                "MEMORY LEAK"
                            );
                        }
                    }
                    
                    ExprPtr size_expr = nullptr;
                    if (!call_expr->arguments.empty()) {
                        size_expr = call_expr->arguments[0];
                    }
                    
                    AllocationInfo alloc_info(var_name, stmt->name, MemoryState::ALLOCATED, func_name == "calloc");
                    alloc_info.size_expr = size_expr;
                    allocations[var_name] = alloc_info;
                    
                } else if (func_name == "addr_of") {
                    if (!call_expr->arguments.empty()) {
                        if (auto* arg_var = get_expr_as<Variable>(call_expr->arguments[0])) {
                            std::string target_var = arg_var->name.lexeme;
                            
                            PointerInfo ptr_info(var_name, stmt->name, "number", PointerState::VALID);
                            ptr_info.points_to = target_var;
                            pointers[var_name] = ptr_info;
                            
                            auto it = allocations.find(target_var);
                            if (it != allocations.end()) {
                                it->second.ref_count++;
                                it->second.aliases.insert(var_name);
                            }
                        }
                    }
                }
            }
        }
    }
}

void MemorySafetyAnalyzer::analyze_expr_stmt(const ExprStmt* stmt) {
    analyze_expr(stmt->expression);
}

std::optional<std::string> MemorySafetyAnalyzer::analyze_expr(const ExprPtr& expr) {
    if (auto* call = get_expr_as<Call>(expr)) {
        return analyze_call(call);
    } else if (auto* var = get_expr_as<Variable>(expr)) {
        return check_variable_access(var);
    } else if (auto* assign = get_expr_as<Assign>(expr)) {
        return analyze_assign(assign);
    } else if (auto* binary = get_expr_as<Binary>(expr)) {
        analyze_expr(binary->left);
        analyze_expr(binary->right);
    } else if (auto* unary = get_expr_as<Unary>(expr)) {
        return analyze_unary(unary);
    } else if (auto* grouping = get_expr_as<Grouping>(expr)) {
        return analyze_expr(grouping->expression);
    } else if (auto* logical = get_expr_as<Logical>(expr)) {
        analyze_expr(logical->left);
        analyze_expr(logical->right);
    } else if (auto* array_lit = get_expr_as<ArrayLiteral>(expr)) {
        for (const auto& elem : array_lit->elements) {
            analyze_expr(elem);
        }
    } else if (auto* index_get = get_expr_as<IndexGet>(expr)) {
        analyze_expr(index_get->array);
        analyze_expr(index_get->index);
    } else if (auto* index_set = get_expr_as<IndexSet>(expr)) {
        analyze_expr(index_set->array);
        analyze_expr(index_set->index);
        analyze_expr(index_set->value);
    }
    
    return std::nullopt;
}

std::optional<std::string> MemorySafetyAnalyzer::analyze_call(const Call* expr) {
    if (auto* callee_var = get_expr_as<Variable>(expr->callee)) {
        std::string func_name = callee_var->name.lexeme;
        
        if (func_name == "free") {
            if (expr->arguments.size() != 1) {
                throw MemorySafetyError(
                    "free() expects exactly 1 argument, got " + std::to_string(expr->arguments.size()),
                    expr->paren,
                    "Usage: free(pointer);"
                );
            }
            
            if (auto* arg_var = get_expr_as<Variable>(expr->arguments[0])) {
                std::string var_name = arg_var->name.lexeme;
                
                auto it = allocations.find(var_name);
                if (it == allocations.end()) {
                    throw MemorySafetyError(
                        "Attempting to free non-allocated memory: '" + var_name + "'",
                        arg_var->name,
                        "Only pointers returned by malloc/calloc/realloc can be freed",
                        "INVALID FREE"
                    );
                }
                
                auto& alloc = it->second;
                
                if (alloc.state == MemoryState::FREED) {
                    throw MemorySafetyError(
                        "Double-free detected: '" + var_name + "' has already been freed",
                        arg_var->name,
                        alloc.freed_at ? "Previously freed at line " + std::to_string(alloc.freed_at->line) : "Previously freed",
                        "DOUBLE-FREE"
                    );
                }
                
                if (alloc.state != MemoryState::ALLOCATED) {
                    throw MemorySafetyError(
                        "Attempting to free memory in invalid state: '" + var_name + "'",
                        arg_var->name,
                        "Current state: " + memory_state_to_string(alloc.state)
                    );
                }
                
                alloc.state = MemoryState::FREED;
                alloc.freed_at = arg_var->name;
                freed_in_scope.back().insert(var_name);
                
                update_pointer_states_on_free(var_name);
            }
            
            return std::nullopt;
            
        } else if (func_name == "deref") {
            if (expr->arguments.size() != 1) {
                throw MemorySafetyError(
                    "deref() expects exactly 1 argument, got " + std::to_string(expr->arguments.size()),
                    expr->paren,
                    "Usage: deref(pointer);"
                );
            }
            
            if (auto* arg_var = get_expr_as<Variable>(expr->arguments[0])) {
                std::string var_name = arg_var->name.lexeme;
                
                auto alloc_it = allocations.find(var_name);
                if (alloc_it != allocations.end()) {
                    const auto& alloc = alloc_it->second;
                    
                    if (alloc.state == MemoryState::FREED) {
                        throw MemorySafetyError(
                            "Use-after-free: Dereferencing freed pointer '" + var_name + "'",
                            arg_var->name,
                            alloc.freed_at ? "Pointer was freed at line " + std::to_string(alloc.freed_at->line) : "Pointer was freed",
                            "USE-AFTER-FREE"
                        );
                    }
                    
                    if (alloc.state == MemoryState::UNINITIALIZED) {
                        throw MemorySafetyError(
                            "Dereferencing uninitialized pointer '" + var_name + "'",
                            arg_var->name,
                            "Initialize the pointer before dereferencing"
                        );
                    }
                }
                
                auto ptr_it = pointers.find(var_name);
                if (ptr_it != pointers.end()) {
                    const auto& ptr_info = ptr_it->second;
                    
                    if (ptr_info.state == PointerState::DANGLING) {
                        throw MemorySafetyError(
                            "Use-after-free: Dereferencing dangling pointer '" + var_name + "'",
                            arg_var->name,
                            "The memory this pointer refers to has been freed",
                            "USE-AFTER-FREE"
                        );
                    }
                    
                    if (ptr_info.state == PointerState::NULL_PTR) {
                        throw MemorySafetyError(
                            "Null pointer dereference: '" + var_name + "' is null",
                            arg_var->name,
                            "Check if pointer is null before dereferencing",
                            "NULL POINTER DEREFERENCE"
                        );
                    }
                }
            }
        }
        
        if (func_name != "free" && func_name != "deref" && 
            func_name != "malloc" && func_name != "calloc" && 
            func_name != "realloc" && func_name != "addr_of") {
            for (const auto& arg : expr->arguments) {
                analyze_expr(arg);
            }
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> MemorySafetyAnalyzer::analyze_assign(const Assign* expr) {
    std::string var_name = expr->name.lexeme;
    
    auto it = allocations.find(var_name);
    if (it != allocations.end()) {
        const auto& alloc = it->second;
        
        if (alloc.state == MemoryState::ALLOCATED) {
            if (auto* call_expr = get_expr_as<Call>(expr->value)) {
                if (auto* callee_var = get_expr_as<Variable>(call_expr->callee)) {
                    std::string func_name = callee_var->name.lexeme;
                    if (func_name == "malloc" || func_name == "calloc" || func_name == "realloc") {
                        throw MemorySafetyError(
                            "Memory leak: Reassigning '" + var_name + "' without freeing previous allocation",
                            expr->name,
                            "Free the previous allocation first: free(" + var_name + ");",
                            "MEMORY LEAK"
                        );
                    }
                }
            }
        }
    }
    
    analyze_expr(expr->value);
    return std::nullopt;
}

std::optional<std::string> MemorySafetyAnalyzer::analyze_unary(const Unary* expr) {
    analyze_expr(expr->right);
    return std::nullopt;
}

std::optional<std::string> MemorySafetyAnalyzer::check_variable_access(const Variable* expr) {
    std::string var_name = expr->name.lexeme;
    
    auto alloc_it = allocations.find(var_name);
    if (alloc_it != allocations.end()) {
        const auto& alloc = alloc_it->second;
        
        if (alloc.state == MemoryState::FREED) {
            throw MemorySafetyError(
                "Use-after-free: Accessing freed memory '" + var_name + "'",
                expr->name,
                alloc.freed_at ? "Memory was freed at line " + std::to_string(alloc.freed_at->line) : "Memory was freed",
                "USE-AFTER-FREE"
            );
        }
    }
    
    auto ptr_it = pointers.find(var_name);
    if (ptr_it != pointers.end()) {
        const auto& ptr_info = ptr_it->second;
        
        if (ptr_info.state == PointerState::DANGLING) {
            warnings.push_back(
                "Warning: Accessing dangling pointer '" + var_name + "' at line " + 
                std::to_string(expr->name.line)
            );
        }
    }
    
    return var_name;
}

void MemorySafetyAnalyzer::analyze_block(const Block* stmt) {
    enter_scope();
    
    for (const auto& s : stmt->statements) {
        analyze_stmt(s);
    }
    
    exit_scope();
}

void MemorySafetyAnalyzer::analyze_if_stmt(const IfStmt* stmt) {
    analyze_expr(stmt->condition);
    
    auto then_allocations = allocations;
    analyze_stmt(stmt->then_branch);
    auto then_freed = freed_in_scope.back();
    
    if (stmt->else_branch) {
        allocations = then_allocations;
        analyze_stmt(*stmt->else_branch);
        auto else_freed = freed_in_scope.back();
        
        for (const auto& var : then_freed) {
            if (else_freed.find(var) == else_freed.end()) {
                auto it = allocations.find(var);
                if (it != allocations.end() && it->second.state == MemoryState::FREED) {
                    it->second.state = MemoryState::ALLOCATED;
                }
            }
        }
    }
}

void MemorySafetyAnalyzer::analyze_while_stmt(const WhileStmt* stmt) {
    analyze_expr(stmt->condition);
    
    enter_scope();
    analyze_stmt(stmt->body);
    exit_scope();
}

void MemorySafetyAnalyzer::analyze_function_stmt(const FunctionStmt* stmt) {
    auto old_allocations = allocations;
    auto old_pointers = pointers;
    
    allocations.clear();
    pointers.clear();
    enter_scope();
    
    for (const auto& s : stmt->body) {
        analyze_stmt(s);
    }
    
    check_function_memory_leaks(stmt->name);
    
    perform_interprocedural_analysis(stmt);
    
    exit_scope();
    allocations = old_allocations;
    pointers = old_pointers;
}

void MemorySafetyAnalyzer::analyze_return_stmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        analyze_expr(*stmt->value);
    }
}

void MemorySafetyAnalyzer::analyze_switch_stmt(const SwitchStmt* stmt) {
    analyze_expr(stmt->condition);
    
    std::vector<std::unordered_map<std::string, AllocationInfo>> case_allocations;
    
    for (const auto& case_clause : stmt->cases) {
        analyze_expr(case_clause.value);
        
        auto case_alloc = allocations;
        for (const auto& case_stmt : case_clause.statements) {
            analyze_stmt(case_stmt);
        }
        case_allocations.push_back(allocations);
        allocations = case_alloc;
    }
    
    if (stmt->default_case) {
        for (const auto& default_stmt : *stmt->default_case) {
            analyze_stmt(default_stmt);
        }
    }
}

void MemorySafetyAnalyzer::analyze_unsafe_block(const UnsafeBlock* stmt) {
    bool old_strict = strict_mode;
    strict_mode = false;
    
    for (const auto& s : stmt->statements) {
        analyze_stmt(s);
    }
    
    strict_mode = old_strict;
}

void MemorySafetyAnalyzer::check_function_memory_leaks(const Token& func_name) {
    for (const auto& [var, alloc] : allocations) {
        if (alloc.state == MemoryState::ALLOCATED) {
            if (strict_mode) {
                throw MemorySafetyError(
                    "Memory leak in function '" + func_name.lexeme + "': Variable '" + var + "' is not freed before return",
                    alloc.allocation_token,
                    "Add 'free(" + var + ");' before all return statements",
                    "MEMORY LEAK"
                );
            }
        }
    }
}

void MemorySafetyAnalyzer::check_memory_leaks() {
    for (const auto& [var, alloc] : allocations) {
        if (alloc.state == MemoryState::ALLOCATED) {
            if (strict_mode) {
                throw MemorySafetyError(
                    "Memory leak: Variable '" + var + "' is never freed",
                    alloc.allocation_token,
                    "Add 'free(" + var + ");' before program exit",
                    "MEMORY LEAK"
                );
            } else {
                warnings.push_back("Warning: Potential memory leak - '" + var + "' may not be freed");
            }
        }
    }
}

std::string MemorySafetyAnalyzer::get_report() const {
    std::ostringstream report;
    
    if (!errors.empty()) {
        report << "\n=== MEMORY SAFETY ERRORS ===\n";
        for (const auto& error : errors) {
            report << error.what() << "\n";
        }
    }
    
    if (!warnings.empty()) {
        report << "\n=== WARNINGS ===\n";
        for (const auto& warning : warnings) {
            report << warning << "\n";
        }
    }
    
    if (errors.empty() && warnings.empty()) {
        report << "\n=== MEMORY SAFETY CHECK PASSED ===\n";
        report << "No memory safety issues detected.\n";
    }
    
    return report.str();
}

CFGNodePtr MemorySafetyAnalyzer::create_cfg_node(ControlFlowNode::NodeType type) {
    auto node = std::make_shared<ControlFlowNode>(type, next_cfg_node_id++);
    cfg_nodes.push_back(node);
    return node;
}

void MemorySafetyAnalyzer::connect_cfg_nodes(CFGNodePtr from, CFGNodePtr to) {
    from->successors.push_back(to);
    to->predecessors.push_back(from);
}

CFGNodePtr MemorySafetyAnalyzer::build_cfg(const std::vector<StmtPtr>& statements) {
    if (statements.empty()) {
        return nullptr;
    }
    
    CFGNodePtr entry = create_cfg_node(ControlFlowNode::NodeType::ENTRY);
    CFGNodePtr exit = create_cfg_node(ControlFlowNode::NodeType::EXIT);
    
    CFGNodePtr current_exit = exit;
    CFGNodePtr body = build_cfg_block(statements, current_exit);
    
    if (body) {
        connect_cfg_nodes(entry, body);
    } else {
        connect_cfg_nodes(entry, exit);
    }
    
    return entry;
}

CFGNodePtr MemorySafetyAnalyzer::build_cfg_block(
    const std::vector<StmtPtr>& statements, 
    CFGNodePtr& exit_node
) {
    if (statements.empty()) {
        return nullptr;
    }
    
    CFGNodePtr first_node = nullptr;
    CFGNodePtr prev_node = nullptr;
    
    for (const auto& stmt : statements) {
        CFGNodePtr stmt_node = build_cfg_stmt(stmt, exit_node);
        
        if (!first_node) {
            first_node = stmt_node;
        }
        
        if (prev_node && stmt_node) {
            connect_cfg_nodes(prev_node, stmt_node);
        }
        
        prev_node = stmt_node;
    }
    
    if (prev_node) {
        connect_cfg_nodes(prev_node, exit_node);
    }
    
    return first_node;
}

CFGNodePtr MemorySafetyAnalyzer::build_cfg_stmt(const StmtPtr& stmt, CFGNodePtr& exit_node) {
    if (auto* if_stmt = get_stmt_as<IfStmt>(stmt)) {
        CFGNodePtr branch_node = create_cfg_node(ControlFlowNode::NodeType::BRANCH);
        branch_node->statement = stmt;
        branch_node->expression = if_stmt->condition;
        
        CFGNodePtr merge_node = create_cfg_node(ControlFlowNode::NodeType::MERGE);
        
        CFGNodePtr then_exit = merge_node;
        CFGNodePtr then_body = build_cfg_stmt(if_stmt->then_branch, then_exit);
        connect_cfg_nodes(branch_node, then_body ? then_body : merge_node);
        
        if (if_stmt->else_branch) {
            CFGNodePtr else_exit = merge_node;
            CFGNodePtr else_body = build_cfg_stmt(*if_stmt->else_branch, else_exit);
            connect_cfg_nodes(branch_node, else_body ? else_body : merge_node);
        } else {
            connect_cfg_nodes(branch_node, merge_node);
        }
        
        return branch_node;
        
    } else if (auto* while_stmt = get_stmt_as<WhileStmt>(stmt)) {
        CFGNodePtr loop_header = create_cfg_node(ControlFlowNode::NodeType::LOOP_HEADER);
        loop_header->statement = stmt;
        loop_header->expression = while_stmt->condition;
        
        CFGNodePtr loop_exit = create_cfg_node(ControlFlowNode::NodeType::LOOP_EXIT);
        
        CFGNodePtr body_exit = loop_header;
        CFGNodePtr loop_body = build_cfg_stmt(while_stmt->body, body_exit);
        
        if (loop_body) {
            connect_cfg_nodes(loop_header, loop_body);
        } else {
            connect_cfg_nodes(loop_header, loop_header);
        }
        
        connect_cfg_nodes(loop_header, loop_exit);
        
        return loop_header;
        
    } else if (auto* block = get_stmt_as<Block>(stmt)) {
        return build_cfg_block(block->statements, exit_node);
        
    } else {
        CFGNodePtr stmt_node = create_cfg_node(ControlFlowNode::NodeType::STATEMENT);
        stmt_node->statement = stmt;
        return stmt_node;
    }
}

void MemorySafetyAnalyzer::perform_dataflow_analysis(CFGNodePtr entry) {
    if (!entry) return;
    
    std::queue<CFGNodePtr> worklist;
    std::unordered_set<int> in_worklist;
    
    worklist.push(entry);
    in_worklist.insert(entry->node_id);
    
    while (!worklist.empty()) {
        CFGNodePtr node = worklist.front();
        worklist.pop();
        in_worklist.erase(node->node_id);
        
        auto old_out = node->allocations_out;
        
        propagate_allocations(node);
        detect_memory_access_patterns(node);
        
        if (node->allocations_out != old_out) {
            for (const auto& succ : node->successors) {
                if (in_worklist.find(succ->node_id) == in_worklist.end()) {
                    worklist.push(succ);
                    in_worklist.insert(succ->node_id);
                }
            }
        }
    }
}

void MemorySafetyAnalyzer::propagate_allocations(CFGNodePtr node) {
    node->allocations_in.clear();
    
    for (const auto& pred : node->predecessors) {
        for (const auto& [var, alloc] : pred->allocations_out) {
            node->allocations_in[var] = alloc;
        }
    }
    
    node->allocations_out = node->allocations_in;
    
    if (node->statement) {
        if (auto* var_stmt = get_stmt_as<VarStmt>(node->statement)) {
            std::string var_name = var_stmt->name.lexeme;
            if (var_stmt->initializer) {
                if (auto* call_expr = get_expr_as<Call>(*var_stmt->initializer)) {
                    if (auto* callee = get_expr_as<Variable>(call_expr->callee)) {
                        std::string func_name = callee->name.lexeme;
                        if (func_name == "malloc" || func_name == "calloc" || func_name == "realloc") {
                            AllocationInfo alloc(var_name, var_stmt->name, MemoryState::ALLOCATED);
                            node->allocations_out[var_name] = alloc;
                        }
                    }
                }
            }
        } else if (auto* expr_stmt = get_stmt_as<ExprStmt>(node->statement)) {
            if (auto* call_expr = get_expr_as<Call>(expr_stmt->expression)) {
                if (auto* callee = get_expr_as<Variable>(call_expr->callee)) {
                    if (callee->name.lexeme == "free" && !call_expr->arguments.empty()) {
                        if (auto* arg = get_expr_as<Variable>(call_expr->arguments[0])) {
                            std::string var_name = arg->name.lexeme;
                            auto it = node->allocations_out.find(var_name);
                            if (it != node->allocations_out.end()) {
                                it->second.state = MemoryState::FREED;
                                node->freed_vars.insert(var_name);
                            }
                        }
                    }
                }
            }
        }
    }
}

void MemorySafetyAnalyzer::detect_memory_access_patterns(CFGNodePtr node) {
    if (!node->statement) return;
    
    std::unordered_set<std::string> accessed_vars;
    
    auto collect_accesses = [&](const ExprPtr& expr) {
        std::unordered_set<std::string> deps;
        analyze_memory_dependencies(expr, deps);
        for (const auto& dep : deps) {
            accessed_vars.insert(dep);
        }
    };
    
    if (auto* expr_stmt = get_stmt_as<ExprStmt>(node->statement)) {
        collect_accesses(expr_stmt->expression);
    } else if (auto* print_stmt = get_stmt_as<PrintStmt>(node->statement)) {
        collect_accesses(print_stmt->expression);
    } else if (auto* if_stmt = get_stmt_as<IfStmt>(node->statement)) {
        collect_accesses(if_stmt->condition);
    } else if (auto* while_stmt = get_stmt_as<WhileStmt>(node->statement)) {
        collect_accesses(while_stmt->condition);
    } else if (auto* ret_stmt = get_stmt_as<ReturnStmt>(node->statement)) {
        if (ret_stmt->value) {
            collect_accesses(*ret_stmt->value);
        }
    }
    
    for (const auto& var : accessed_vars) {
        auto it = node->allocations_in.find(var);
        if (it != node->allocations_in.end()) {
            if (it->second.state == MemoryState::FREED) {
                warnings.push_back(
                    "Potential use-after-free of '" + var + "' in CFG node " + 
                    std::to_string(node->node_id)
                );
            }
        }
    }
}

void MemorySafetyAnalyzer::analyze_memory_dependencies(
    const ExprPtr& expr,
    std::unordered_set<std::string>& deps
) {
    if (auto* var = get_expr_as<Variable>(expr)) {
        deps.insert(var->name.lexeme);
    } else if (auto* binary = get_expr_as<Binary>(expr)) {
        analyze_memory_dependencies(binary->left, deps);
        analyze_memory_dependencies(binary->right, deps);
    } else if (auto* unary = get_expr_as<Unary>(expr)) {
        analyze_memory_dependencies(unary->right, deps);
    } else if (auto* call = get_expr_as<Call>(expr)) {
        for (const auto& arg : call->arguments) {
            analyze_memory_dependencies(arg, deps);
        }
    } else if (auto* grouping = get_expr_as<Grouping>(expr)) {
        analyze_memory_dependencies(grouping->expression, deps);
    } else if (auto* index_get = get_expr_as<IndexGet>(expr)) {
        analyze_memory_dependencies(index_get->array, deps);
        analyze_memory_dependencies(index_get->index, deps);
    } else if (auto* index_set = get_expr_as<IndexSet>(expr)) {
        analyze_memory_dependencies(index_set->array, deps);
        analyze_memory_dependencies(index_set->index, deps);
        analyze_memory_dependencies(index_set->value, deps);
    }
}

void MemorySafetyAnalyzer::perform_interprocedural_analysis(const FunctionStmt* func) {
    CFGNodePtr func_entry = create_cfg_node(ControlFlowNode::NodeType::FUNCTION_CALL);
    CFGNodePtr func_exit = create_cfg_node(ControlFlowNode::NodeType::FUNCTION_RETURN);
    
    function_entry_nodes[func->name.lexeme] = func_entry;
    function_exit_nodes[func->name.lexeme] = func_exit;
    
    CFGNodePtr body_exit = func_exit;
    CFGNodePtr body = build_cfg_block(func->body, body_exit);
    
    if (body) {
        connect_cfg_nodes(func_entry, body);
    } else {
        connect_cfg_nodes(func_entry, func_exit);
    }
    
    perform_dataflow_analysis(func_entry);
    
    std::vector<ControlFlowPath> paths = enumerate_all_paths(func_entry, func_exit);
    for (const auto& path : paths) {
        analyze_path_memory_safety(path);
    }
}

std::vector<ControlFlowPath> MemorySafetyAnalyzer::enumerate_all_paths(
    CFGNodePtr entry,
    CFGNodePtr exit
) {
    std::vector<ControlFlowPath> all_paths;
    std::vector<CFGNodePtr> current_path;
    std::unordered_set<int> visited;
    
    enumerate_paths_recursive(entry, exit, current_path, visited, all_paths);
    
    return all_paths;
}

/**
 * @brief Recursive path enumeration with cycle detection
 * 
 * Architectural design:
 * - DFS-based traversal with backtracking
 * - Visited set prevents infinite loops in cyclic CFGs
 * - Path limit prevents exponential explosion
 * 
 * Performance characteristics:
 * - Time: O(V + E) for acyclic graphs
 * - Space: O(V) for recursion stack
 * - Early termination on path count threshold
 * 
 * @param current Current CFG node being explored
 * @param exit Target exit node
 * @param current_path Path accumulator (modified during recursion)
 * @param visited Cycle detection set
 * @param all_paths Output collection of complete paths
 */
void MemorySafetyAnalyzer::enumerate_paths_recursive(
    CFGNodePtr current,
    CFGNodePtr exit,
    std::vector<CFGNodePtr>& current_path,
    std::unordered_set<int>& visited,
    std::vector<ControlFlowPath>& all_paths
) {
    if (!current) return;
    
    constexpr size_t MAX_PATHS = 10000;
    if (all_paths.size() >= MAX_PATHS) {
        return;
    }
    
    if (visited.find(current->node_id) != visited.end()) {
        return;
    }
    
    constexpr size_t MAX_PATH_DEPTH = 1000;
    if (current_path.size() >= MAX_PATH_DEPTH) {
        return;
    }
    
    current_path.push_back(current);
    visited.insert(current->node_id);
    
    if (current == exit) {
        ControlFlowPath path;
        path.nodes = current_path;
        path.final_allocations = current->allocations_out;
        path.freed_vars = current->freed_vars;
        all_paths.push_back(path);
    } else {
        for (const auto& succ : current->successors) {
            enumerate_paths_recursive(succ, exit, current_path, visited, all_paths);
        }
    }
    
    current_path.pop_back();
    visited.erase(current->node_id);
}

void MemorySafetyAnalyzer::analyze_path_memory_safety(const ControlFlowPath& path) {
    if (!path.is_feasible) return;
    
    for (const auto& [var, alloc] : path.final_allocations) {
        if (alloc.state == MemoryState::ALLOCATED) {
            warnings.push_back(
                "Path-sensitive analysis: Potential leak of '" + var + 
                "' along execution path"
            );
        }
    }
}

void MemorySafetyAnalyzer::update_pointer_states_on_free(const std::string& var) {
    auto it = allocations.find(var);
    if (it != allocations.end()) {
        for (const auto& alias : it->second.aliases) {
            auto ptr_it = pointers.find(alias);
            if (ptr_it != pointers.end()) {
                ptr_it->second.state = PointerState::DANGLING;
            }
        }
    }
}

void MemorySafetyAnalyzer::add_error(const MemorySafetyError& error) {
    errors.push_back(error);
}

void MemorySafetyAnalyzer::add_warning(const std::string& warning) {
    warnings.push_back(warning);
}

}
