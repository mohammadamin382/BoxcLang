#ifndef BOX_OPTIMIZER_HPP
#define BOX_OPTIMIZER_HPP

#include "../parser/parser.hpp"
#include "../lexer/token.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include <cmath>

namespace box {

struct OptimizerConfig {
    bool constant_folding = true;
    bool constant_propagation = true;
    bool dead_code_elimination = true;
    bool common_subexpression_elimination = true;
    bool loop_invariant_code_motion = true;
    bool loop_unrolling = true;
    int loop_unroll_threshold = 32;
    bool strength_reduction = true;
    bool function_inlining = true;
    int inline_threshold = 10;
    bool algebraic_simplification = true;
    bool peephole_optimization = true;
    int optimize_level = 3;
    bool aggressive_optimization = true;
    bool loop_fusion = false;
    bool loop_interchange = false;
};

class ExprValue {
public:
    bool is_constant;
    std::variant<std::monostate, double, bool, std::string> value;
    ExprPtr expr;

    ExprValue() : is_constant(false), value(std::monostate{}), expr(nullptr) {}
    
    ExprValue(bool constant, double val) 
        : is_constant(constant), value(val), expr(nullptr) {}
    
    ExprValue(bool constant, bool val) 
        : is_constant(constant), value(val), expr(nullptr) {}
    
    ExprValue(ExprPtr e) 
        : is_constant(false), value(std::monostate{}), expr(e) {}
    
    std::string to_string() const;
};

class OptimizationPass {
public:
    explicit OptimizationPass(const OptimizerConfig& cfg) 
        : config(cfg), modified(false) {}
    
    virtual ~OptimizationPass() = default;
    
    virtual std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) = 0;
    
    bool was_modified() const { return modified; }

protected:
    OptimizerConfig config;
    bool modified;
};

class ConstantFolder : public OptimizationPass {
public:
    explicit ConstantFolder(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    StmtPtr fold_stmt(const StmtPtr& stmt);
    ExprPtr fold_expr(const ExprPtr& expr);
    
    std::optional<double> get_numeric_value(const ExprPtr& expr) const;
    std::optional<bool> get_boolean_value(const ExprPtr& expr) const;
    bool is_truthy(const ExprPtr& expr) const;
};

class AlgebraicSimplifier : public OptimizationPass {
public:
    explicit AlgebraicSimplifier(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    StmtPtr simplify_stmt(const StmtPtr& stmt);
    ExprPtr simplify_expr(const ExprPtr& expr);
    
    bool is_power_of_two(double n) const;
    bool is_zero(const ExprPtr& expr) const;
    bool is_one(const ExprPtr& expr) const;
    bool are_equal_variables(const ExprPtr& a, const ExprPtr& b) const;
    ExprPtr create_literal(double value) const;
    ExprPtr deep_copy_expr(const ExprPtr& expr) const;
};

class DeadCodeEliminator : public OptimizationPass {
public:
    explicit DeadCodeEliminator(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    std::unordered_set<std::string> used_vars;
    std::unordered_map<std::string, StmtPtr> defined_vars;
    std::unordered_set<std::string> essential_vars;
    
    void analyze_stmt(const StmtPtr& stmt);
    void analyze_expr(const ExprPtr& expr);
    bool should_keep_stmt(const StmtPtr& stmt) const;
    bool has_side_effects(const ExprPtr& expr) const;
    StmtPtr eliminate_stmt(const StmtPtr& stmt);
    void mark_essential_variables(const std::vector<StmtPtr>& statements);
};

class CommonSubexpressionEliminator : public OptimizationPass {
public:
    explicit CommonSubexpressionEliminator(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg), temp_counter(0) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    std::unordered_map<std::string, ExprPtr> expr_cache;
    int temp_counter;
    
    StmtPtr process_stmt(const StmtPtr& stmt);
    ExprPtr process_expr(const ExprPtr& expr);
    std::string expr_to_string(const ExprPtr& expr) const;
    size_t expr_hash(const ExprPtr& expr) const;
    bool exprs_equal(const ExprPtr& a, const ExprPtr& b) const;
};

class LoopOptimizer : public OptimizationPass {
public:
    explicit LoopOptimizer(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    StmtPtr optimize_stmt(const StmtPtr& stmt);
    StmtPtr try_unroll_loop(const StmtPtr& stmt);
    StmtPtr extract_loop_invariant_code(const StmtPtr& stmt);
    
    bool can_unroll(const StmtPtr& stmt) const;
    std::optional<int> get_iteration_count(const StmtPtr& stmt) const;
    StmtPtr unroll_loop(const StmtPtr& stmt, int iterations);
    
    bool is_loop_invariant(const ExprPtr& expr, const std::unordered_set<std::string>& loop_vars) const;
    std::unordered_set<std::string> find_modified_vars(const StmtPtr& stmt) const;
    void collect_modified_vars(const StmtPtr& stmt, std::unordered_set<std::string>& vars) const;
};

class StrengthReducer : public OptimizationPass {
public:
    explicit StrengthReducer(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    StmtPtr reduce_stmt(const StmtPtr& stmt);
    ExprPtr reduce_expr(const ExprPtr& expr);
    
    ExprPtr reduce_multiplication(const ExprPtr& left, const ExprPtr& right, const Token& op);
    ExprPtr reduce_division(const ExprPtr& left, const ExprPtr& right, const Token& op);
    ExprPtr reduce_modulo(const ExprPtr& left, const ExprPtr& right, const Token& op);
    
    bool is_power_of_two(double n) const;
    int log2_int(int n) const;
};

class FunctionInliner : public OptimizationPass {
public:
    explicit FunctionInliner(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    std::unordered_map<std::string, StmtPtr> function_definitions;
    
    void collect_functions(const std::vector<StmtPtr>& statements);
    StmtPtr inline_stmt(const StmtPtr& stmt);
    ExprPtr inline_expr(const ExprPtr& expr);
    
    bool should_inline(const std::string& func_name) const;
    int calculate_complexity(const StmtPtr& stmt) const;
    ExprPtr substitute_params(const ExprPtr& expr, 
                             const std::vector<Token>& params, 
                             const std::vector<ExprPtr>& args);
};

class PeepholeOptimizer : public OptimizationPass {
public:
    explicit PeepholeOptimizer(const OptimizerConfig& cfg) 
        : OptimizationPass(cfg) {}
    
    std::vector<StmtPtr> run(const std::vector<StmtPtr>& statements) override;

private:
    StmtPtr optimize_stmt(const StmtPtr& stmt);
    ExprPtr optimize_expr(const ExprPtr& expr);
    
    ExprPtr optimize_double_negation(const ExprPtr& expr);
    ExprPtr optimize_comparison_chain(const ExprPtr& expr);
    ExprPtr optimize_boolean_operations(const ExprPtr& expr);
};

class Optimizer {
public:
    explicit Optimizer(const OptimizerConfig& cfg = OptimizerConfig());
    
    std::vector<StmtPtr> optimize(const std::vector<StmtPtr>& statements);
    
    OptimizerConfig& get_config() { return config; }
    const OptimizerConfig& get_config() const { return config; }

private:
    OptimizerConfig config;
    std::vector<std::unique_ptr<OptimizationPass>> passes;
    
    void initialize_passes();
};

}

#endif
