#ifndef BOX_MEMORY_ANALYZER_HPP
#define BOX_MEMORY_ANALYZER_HPP

#include "../lexer/token.hpp"
#include "../parser/parser.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <set>
#include <map>

namespace box {

enum class MemoryState {
    UNINITIALIZED,
    ALLOCATED,
    FREED,
    INVALID,
    UNKNOWN
};

enum class PointerState {
    NULL_PTR,
    VALID,
    DANGLING,
    UNKNOWN
};

std::string memory_state_to_string(MemoryState state);
std::string pointer_state_to_string(PointerState state);

struct AllocationInfo {
    std::string var_name;
    Token allocation_token;
    MemoryState state;
    std::optional<Token> freed_at;
    std::optional<ExprPtr> size_expr;
    bool is_array;
    int ref_count;
    std::unordered_set<std::string> aliases;

    AllocationInfo();
    
    AllocationInfo(
        const std::string& name,
        const Token& token,
        MemoryState s = MemoryState::ALLOCATED,
        bool array = false
    );
    
    bool operator==(const AllocationInfo& other) const;
};

struct PointerInfo {
    std::string var_name;
    Token declaration_token;
    std::string pointee_type;
    PointerState state;
    std::optional<std::string> points_to;
    int level;

    PointerInfo();
    
    PointerInfo(
        const std::string& name,
        const Token& token,
        const std::string& type,
        PointerState s = PointerState::VALID,
        int lvl = 1
    );
};

struct ControlFlowNode;
using CFGNodePtr = std::shared_ptr<ControlFlowNode>;

struct ControlFlowNode {
    enum class NodeType {
        ENTRY,
        EXIT,
        STATEMENT,
        BRANCH,
        MERGE,
        LOOP_HEADER,
        LOOP_BODY,
        LOOP_EXIT,
        FUNCTION_CALL,
        FUNCTION_RETURN
    };

    NodeType type;
    StmtPtr statement;
    ExprPtr expression;
    std::vector<CFGNodePtr> successors;
    std::vector<CFGNodePtr> predecessors;
    std::unordered_map<std::string, AllocationInfo> allocations_in;
    std::unordered_map<std::string, AllocationInfo> allocations_out;
    std::unordered_set<std::string> freed_vars;
    int node_id;

    explicit ControlFlowNode(NodeType t, int id = -1);
};

struct ControlFlowPath {
    std::vector<CFGNodePtr> nodes;
    std::unordered_map<std::string, AllocationInfo> final_allocations;
    std::unordered_set<std::string> freed_vars;
    bool is_feasible;

    ControlFlowPath();
};

class MemorySafetyError : public std::runtime_error {
public:
    std::string message;
    Token token;
    std::optional<std::string> hint;
    std::string error_type;

    MemorySafetyError(
        const std::string& msg,
        const Token& tok,
        const std::optional<std::string>& h = std::nullopt,
        const std::string& err_type = "MEMORY SAFETY ERROR"
    );

    static std::string format_error(
        const std::string& msg,
        const Token& tok,
        const std::optional<std::string>& hint,
        const std::string& error_type
    );
};

class MemorySafetyAnalyzer {
public:
    MemorySafetyAnalyzer();

    bool analyze(const std::vector<StmtPtr>& statements);
    std::string get_report() const;
    const std::vector<MemorySafetyError>& get_errors() const { return errors; }
    const std::vector<std::string>& get_warnings() const { return warnings; }

private:
    std::unordered_map<std::string, AllocationInfo> allocations;
    std::unordered_map<std::string, PointerInfo> pointers;
    std::vector<std::unordered_set<std::string>> current_scope_vars;
    std::vector<std::unordered_set<std::string>> freed_in_scope;
    std::vector<MemorySafetyError> errors;
    std::vector<std::string> warnings;
    bool strict_mode;

    int next_cfg_node_id;
    std::vector<CFGNodePtr> cfg_nodes;
    std::unordered_map<std::string, CFGNodePtr> function_entry_nodes;
    std::unordered_map<std::string, CFGNodePtr> function_exit_nodes;

    void enter_scope();
    void exit_scope();

    void analyze_stmt(const StmtPtr& stmt);
    void analyze_var_stmt(const VarStmt* stmt);
    void analyze_expr_stmt(const ExprStmt* stmt);
    void analyze_block(const Block* stmt);
    void analyze_if_stmt(const IfStmt* stmt);
    void analyze_while_stmt(const WhileStmt* stmt);
    void analyze_function_stmt(const FunctionStmt* stmt);
    void analyze_return_stmt(const ReturnStmt* stmt);
    void analyze_switch_stmt(const SwitchStmt* stmt);
    void analyze_unsafe_block(const UnsafeBlock* stmt);

    std::optional<std::string> analyze_expr(const ExprPtr& expr);
    std::optional<std::string> analyze_call(const Call* expr);
    std::optional<std::string> analyze_assign(const Assign* expr);
    std::optional<std::string> analyze_unary(const Unary* expr);
    std::optional<std::string> check_variable_access(const Variable* expr);

    void check_memory_leaks();
    void check_function_memory_leaks(const Token& func_name);

    CFGNodePtr build_cfg(const std::vector<StmtPtr>& statements);
    CFGNodePtr build_cfg_stmt(const StmtPtr& stmt, CFGNodePtr& exit_node);
    CFGNodePtr build_cfg_block(const std::vector<StmtPtr>& statements, CFGNodePtr& exit_node);
    
    void perform_dataflow_analysis(CFGNodePtr entry);
    void propagate_allocations(CFGNodePtr node);
    
    std::vector<ControlFlowPath> enumerate_all_paths(CFGNodePtr entry, CFGNodePtr exit);
    void enumerate_paths_recursive(
        CFGNodePtr current,
        CFGNodePtr exit,
        std::vector<CFGNodePtr>& current_path,
        std::unordered_set<int>& visited,
        std::vector<ControlFlowPath>& all_paths
    );
    
    void analyze_path_memory_safety(const ControlFlowPath& path);
    void detect_memory_access_patterns(CFGNodePtr node);
    
    void perform_interprocedural_analysis(const FunctionStmt* func);
    void analyze_memory_dependencies(const ExprPtr& expr, std::unordered_set<std::string>& deps);
    
    void track_pointer_aliases(const std::string& var, const ExprPtr& value);
    void update_pointer_states_on_free(const std::string& var);
    
    CFGNodePtr create_cfg_node(ControlFlowNode::NodeType type);
    void connect_cfg_nodes(CFGNodePtr from, CFGNodePtr to);
    
    void add_error(const MemorySafetyError& error);
    void add_warning(const std::string& warning);

    template<typename T>
    T* get_stmt_as(const StmtPtr& stmt) {
        return dynamic_cast<T*>(stmt.get());
    }

    template<typename T>
    const T* get_stmt_as_const(const StmtPtr& stmt) const {
        return dynamic_cast<const T*>(stmt.get());
    }

    template<typename T>
    T* get_expr_as(const ExprPtr& expr) {
        return dynamic_cast<T*>(expr.get());
    }

    template<typename T>
    const T* get_expr_as_const(const ExprPtr& expr) const {
        return dynamic_cast<const T*>(expr.get());
    }
};

}

#endif
