#ifndef BOX_PARSER_HPP
#define BOX_PARSER_HPP

#include "../lexer/token.hpp"
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace box {

class Expr;
class Stmt;

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

class Expr {
public:
    virtual ~Expr() = default;
};

class Literal : public Expr {
public:
    LiteralValue value;
    Token token;

    Literal(LiteralValue val, const Token& tok)
        : value(val), token(tok) {}
};

class Variable : public Expr {
public:
    Token name;

    explicit Variable(const Token& n)
        : name(n) {}
};

class Assign : public Expr {
public:
    Token name;
    ExprPtr value;

    Assign(const Token& n, ExprPtr v)
        : name(n), value(std::move(v)) {}
};

class Binary : public Expr {
public:
    ExprPtr left;
    Token op;
    ExprPtr right;

    Binary(ExprPtr l, const Token& o, ExprPtr r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
};

class Unary : public Expr {
public:
    Token op;
    ExprPtr right;

    Unary(const Token& o, ExprPtr r)
        : op(o), right(std::move(r)) {}
};

class Logical : public Expr {
public:
    ExprPtr left;
    Token op;
    ExprPtr right;

    Logical(ExprPtr l, const Token& o, ExprPtr r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
};

class Call : public Expr {
public:
    ExprPtr callee;
    Token paren;
    std::vector<ExprPtr> arguments;

    Call(ExprPtr c, const Token& p, std::vector<ExprPtr> args)
        : callee(std::move(c)), paren(p), arguments(std::move(args)) {}
};

class Grouping : public Expr {
public:
    ExprPtr expression;

    explicit Grouping(ExprPtr expr)
        : expression(std::move(expr)) {}
};

class ArrayLiteral : public Expr {
public:
    std::vector<ExprPtr> elements;
    Token bracket;

    ArrayLiteral(std::vector<ExprPtr> elems, const Token& brack)
        : elements(std::move(elems)), bracket(brack) {}
};

class IndexGet : public Expr {
public:
    ExprPtr array;
    ExprPtr index;
    Token bracket;

    IndexGet(ExprPtr arr, ExprPtr idx, const Token& brack)
        : array(std::move(arr)), index(std::move(idx)), bracket(brack) {}
};

class IndexSet : public Expr {
public:
    ExprPtr array;
    ExprPtr index;
    ExprPtr value;
    Token bracket;

    IndexSet(ExprPtr arr, ExprPtr idx, ExprPtr val, const Token& brack)
        : array(std::move(arr)), index(std::move(idx)), value(std::move(val)), bracket(brack) {}
};

class DictLiteral : public Expr {
public:
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
    Token brace;

    DictLiteral(std::vector<std::pair<ExprPtr, ExprPtr>> p, const Token& b)
        : pairs(std::move(p)), brace(b) {}
};

class Stmt {
public:
    virtual ~Stmt() = default;
};

class ExprStmt : public Stmt {
public:
    ExprPtr expression;

    explicit ExprStmt(ExprPtr expr)
        : expression(std::move(expr)) {}
};

class PrintStmt : public Stmt {
public:
    ExprPtr expression;
    Token keyword;

    PrintStmt(ExprPtr expr, const Token& kw)
        : expression(std::move(expr)), keyword(kw) {}
};

class VarStmt : public Stmt {
public:
    Token name;
    std::optional<ExprPtr> initializer;

    VarStmt(const Token& n, std::optional<ExprPtr> init)
        : name(n), initializer(std::move(init)) {}
};

class Block : public Stmt {
public:
    std::vector<StmtPtr> statements;
    Token opening_brace;

    Block(std::vector<StmtPtr> stmts, const Token& brace)
        : statements(std::move(stmts)), opening_brace(brace) {}
};

class IfStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr then_branch;
    std::optional<StmtPtr> else_branch;
    Token keyword;

    IfStmt(ExprPtr cond, StmtPtr then_br, std::optional<StmtPtr> else_br, const Token& kw)
        : condition(std::move(cond)), then_branch(std::move(then_br)), 
          else_branch(std::move(else_br)), keyword(kw) {}
};

class WhileStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr body;
    Token keyword;

    WhileStmt(ExprPtr cond, StmtPtr b, const Token& kw)
        : condition(std::move(cond)), body(std::move(b)), keyword(kw) {}
};

class FunctionStmt : public Stmt {
public:
    Token name;
    std::vector<Token> params;
    std::vector<StmtPtr> body;

    FunctionStmt(const Token& n, std::vector<Token> p, std::vector<StmtPtr> b)
        : name(n), params(std::move(p)), body(std::move(b)) {}
};

class ReturnStmt : public Stmt {
public:
    Token keyword;
    std::optional<ExprPtr> value;

    ReturnStmt(const Token& kw, std::optional<ExprPtr> val)
        : keyword(kw), value(std::move(val)) {}
};

class BreakStmt : public Stmt {
public:
    Token keyword;

    explicit BreakStmt(const Token& kw)
        : keyword(kw) {}
};

class CaseClause {
public:
    ExprPtr value;
    std::vector<StmtPtr> statements;

    CaseClause(ExprPtr val, std::vector<StmtPtr> stmts)
        : value(std::move(val)), statements(std::move(stmts)) {}
};

class SwitchStmt : public Stmt {
public:
    Token keyword;
    ExprPtr condition;
    std::vector<CaseClause> cases;
    std::optional<std::vector<StmtPtr>> default_case;

    SwitchStmt(const Token& kw, ExprPtr cond, std::vector<CaseClause> c, 
               std::optional<std::vector<StmtPtr>> def)
        : keyword(kw), condition(std::move(cond)), cases(std::move(c)), 
          default_case(std::move(def)) {}
};

class UnsafeBlock : public Stmt {
public:
    Token keyword;
    std::vector<StmtPtr> statements;

    UnsafeBlock(const Token& kw, std::vector<StmtPtr> stmts)
        : keyword(kw), statements(std::move(stmts)) {}
};

class LLVMInlineStmt : public Stmt {
public:
    Token keyword;
    std::string llvm_code;
    std::unordered_map<std::string, std::string> variables_map;

    LLVMInlineStmt(const Token& kw, const std::string& code, 
                   const std::unordered_map<std::string, std::string>& vars)
        : keyword(kw), llvm_code(code), variables_map(vars) {}
};

class ImportStmt : public Stmt {
public:
    Token keyword;
    std::string file_path;
    Token path_token;

    ImportStmt(const Token& kw, const std::string& path, const Token& path_tok)
        : keyword(kw), file_path(path), path_token(path_tok) {}
};

class ParserError : public std::runtime_error {
public:
    Token token;
    std::string message;
    std::optional<std::string> hint;

    ParserError(const Token& tok, const std::string& msg, 
                const std::optional<std::string>& h = std::nullopt,
                const std::optional<std::string>& source = std::nullopt);

    static std::string format_error(const Token& tok, const std::string& msg,
                                    const std::optional<std::string>& hint,
                                    const std::optional<std::string>& source);
};

class Parser {
public:
    static constexpr size_t MAX_ARGUMENTS = 255;
    static constexpr size_t MAX_PARAMETERS = 255;
    static constexpr size_t MAX_LOOP_DEPTH = 100;
    static constexpr size_t MAX_BLOCK_DEPTH = 100;

    Parser(const std::vector<Token>& tokens, const std::string& source = "");

    std::vector<StmtPtr> parse();

private:
    std::vector<Token> tokens;
    std::string source;
    size_t current;
    std::vector<ParserError> errors;
    int loop_depth;
    int block_depth;
    int function_depth;
    bool in_unsafe_block;

    StmtPtr declaration();
    StmtPtr var_declaration();
    StmtPtr function(const std::string& kind);
    StmtPtr statement();
    StmtPtr print_statement();
    StmtPtr if_statement();
    StmtPtr while_statement();
    StmtPtr for_statement();
    StmtPtr return_statement();
    StmtPtr break_statement();
    StmtPtr switch_statement();
    StmtPtr unsafe_statement();
    StmtPtr llvm_inline_statement();
    StmtPtr import_statement();
    std::vector<StmtPtr> block();
    StmtPtr expression_statement();

    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr or_expr();
    ExprPtr and_expr();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr finish_call(ExprPtr callee);
    ExprPtr finish_index(ExprPtr array);
    ExprPtr array_literal();
    ExprPtr dict_literal();
    ExprPtr primary();

    bool match(const std::vector<TokenType>& types);
    template<typename... Args>
    bool match(Args... types);
    bool check(TokenType type) const;
    Token advance();
    bool is_at_end() const;
    Token peek() const;
    Token previous() const;
    Token consume(TokenType type, const std::string& message);
    ParserError error(const Token& token, const std::string& message, 
                     const std::optional<std::string>& hint = std::nullopt);
    void synchronize();
};

template<typename... Args>
bool Parser::match(Args... types) {
    std::vector<TokenType> type_vec = {types...};
    return match(type_vec);
}

}

#endif
