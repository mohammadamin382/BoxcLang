#include "parser.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <unordered_set>

namespace box {

ParserError::ParserError(const Token& tok, const std::string& msg,
                        const std::optional<std::string>& h,
                        const std::optional<std::string>& source)
    : std::runtime_error(format_error(tok, msg, h, source))
    , token(tok)
    , message(msg)
    , hint(h) {}

std::string ParserError::format_error(const Token& tok, const std::string& msg,
                                     const std::optional<std::string>& hint,
                                     const std::optional<std::string>& source) {
    std::ostringstream oss;
    oss << "\n" << std::string(70, '=') << "\n";
    oss << "PARSER ERROR at Line " << tok.line << ", Column " << tok.column << "\n";
    oss << std::string(70, '=') << "\n";
    oss << "Error: " << msg << "\n";

    if (source) {
        std::istringstream source_stream(*source);
        std::string line;
        int current_line = 1;
        while (std::getline(source_stream, line)) {
            if (current_line == tok.line) {
                oss << "\n" << std::setw(4) << tok.line << " | " << line << "\n";
                oss << "     | " << std::string(tok.column - 1, ' ') << "^\n";
                break;
            }
            current_line++;
        }
    }

    if (hint) {
        oss << "\nHint: " << *hint << "\n";
    }

    oss << std::string(70, '=') << "\n";
    return oss.str();
}

Parser::Parser(const std::vector<Token>& tokens, const std::string& source)
    : tokens(tokens)
    , source(source)
    , current(0)
    , loop_depth(0)
    , block_depth(0)
    , function_depth(0)
    , in_unsafe_block(false) {}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> statements;

    while (!is_at_end()) {
        try {
            StmtPtr stmt = declaration();
            if (stmt) {
                statements.push_back(stmt);
            }
        } catch (const ParserError& e) {
            errors.push_back(e);
            synchronize();
        }
    }

    if (!errors.empty()) {
        std::ostringstream error_messages;
        for (const auto& e : errors) {
            error_messages << e.what();
        }

        std::ostringstream summary;
        summary << "\n" << std::string(70, '#') << "\n";
        summary << "COMPILATION FAILED: Found " << errors.size() << " parsing error(s)\n";
        summary << std::string(70, '#') << "\n";

        throw std::runtime_error(summary.str() + error_messages.str());
    }

    return statements;
}

StmtPtr Parser::declaration() {
    try {
        if (match(TokenType::IMPORT)) {
            return import_statement();
        }
        if (match(TokenType::VAR)) {
            return var_declaration();
        }
        if (match(TokenType::FUN)) {
            return function("function");
        }
        return statement();
    } catch (const ParserError&) {
        throw;
    }
}

StmtPtr Parser::var_declaration() {
    if (!check(TokenType::IDENTIFIER)) {
        std::string hint = "Variable declarations must follow this pattern:\n";
        hint += "       var variableName = value;\n";
        hint += "       var variableName;";
        throw error(peek(), "Expect variable name after 'var'", hint);
    }

    Token name = consume(TokenType::IDENTIFIER, "Expect variable name after 'var'");

    if (name.lexeme.length() > 255) {
        std::ostringstream hint;
        hint << "Variable names must be 255 characters or fewer.\n";
        hint << "       Current length: " << name.lexeme.length() << " characters.\n";
        hint << "       Use a shorter, more descriptive name.";
        throw error(name, "Variable name too long: '" + name.lexeme.substr(0, 50) + "...'", hint.str());
    }

    std::optional<ExprPtr> initializer;
    if (match(TokenType::EQUAL)) {
        try {
            initializer = expression();
        } catch (const ParserError&) {
            std::string hint = "Check the expression after '=' in variable declaration.\n";
            hint += "       Example: var x = 42;";
            throw error(previous(), "Invalid initializer expression", hint);
        }
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Variable declarations must end with a semicolon.\n";
        hint += "       Add ';' after the variable declaration.";
        throw error(peek(), "Expect ';' after variable declaration", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after variable declaration");
    return std::make_shared<VarStmt>(name, initializer);
}

StmtPtr Parser::function(const std::string& kind) {
    if (!check(TokenType::IDENTIFIER)) {
        std::ostringstream hint;
        hint << "Function declarations must have a name.\n";
        hint << "       Example: fun myFunction() { ... }";
        throw error(peek(), "Expect " + kind + " name", hint.str());
    }

    Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name");

    if (name.lexeme.length() > 255) {
        std::ostringstream hint;
        hint << "Function names must be 255 characters or fewer.\n";
        hint << "       Current length: " << name.lexeme.length() << " characters.";
        throw error(name, "Function name too long: '" + name.lexeme.substr(0, 50) + "...'", hint.str());
    }

    if (!check(TokenType::LPAREN)) {
        std::ostringstream hint;
        hint << "Function name must be followed by '('.\n";
        hint << "       Example: fun " << name.lexeme << "() { ... }";
        throw error(peek(), "Expect '(' after " + kind + " name", hint.str());
    }

    consume(TokenType::LPAREN, "Expect '(' after " + kind + " name");

    std::vector<Token> params;
    std::unordered_set<std::string> param_names;

    if (!check(TokenType::RPAREN)) {
        while (true) {
            if (params.size() >= MAX_PARAMETERS) {
                std::ostringstream hint;
                hint << "Box functions support up to " << MAX_PARAMETERS << " parameters.\n";
                hint << "       Consider restructuring your function to use fewer parameters.";
                throw error(peek(), "Cannot have more than " + std::to_string(MAX_PARAMETERS) + " parameters", hint.str());
            }

            if (!check(TokenType::IDENTIFIER)) {
                std::ostringstream hint;
                hint << "Function parameters must be identifiers.\n";
                hint << "       Example: fun " << name.lexeme << "(param1, param2) { ... }";
                throw error(peek(), "Expect parameter name", hint.str());
            }

            Token param = consume(TokenType::IDENTIFIER, "Expect parameter name");

            if (param_names.count(param.lexeme)) {
                std::ostringstream hint;
                hint << "Each parameter name must be unique within the function.\n";
                hint << "       Parameter '" << param.lexeme << "' is already defined.\n";
                hint << "       Use different names for each parameter.";
                throw error(param, "Duplicate parameter name '" + param.lexeme + "'", hint.str());
            }

            param_names.insert(param.lexeme);
            params.push_back(param);

            if (!match(TokenType::COMMA)) {
                break;
            }

            if (check(TokenType::RPAREN)) {
                std::ostringstream hint;
                hint << "Remove the trailing comma before ')'.\n";
                hint << "       Example: fun " << name.lexeme << "(a, b) not fun " << name.lexeme << "(a, b,)";
                throw error(peek(), "Trailing comma in parameter list", hint.str());
            }
        }
    }

    consume(TokenType::RPAREN, "Expect ')' after parameters");

    if (!check(TokenType::LBRACE)) {
        std::ostringstream hint;
        hint << "Function body must be enclosed in curly braces.\n";
        hint << "       Example: fun " << name.lexeme << "() { return 42; }";
        throw error(peek(), "Expect '{' before " + kind + " body", hint.str());
    }

    consume(TokenType::LBRACE, "Expect '{' before " + kind + " body");

    function_depth++;
    if (function_depth > 100) {
        function_depth--;
        std::string hint = "Function nesting is too deep (maximum 100 levels).\n";
        hint += "       Consider refactoring nested functions into separate top-level functions.";
        throw error(name, "Function nesting depth exceeds maximum", hint);
    }

    std::vector<StmtPtr> body;
    try {
        body = block();
    } catch (...) {
        function_depth--;
        throw;
    }
    function_depth--;

    return std::make_shared<FunctionStmt>(name, params, body);
}

StmtPtr Parser::statement() {
    if (match(TokenType::PRINT)) {
        return print_statement();
    }
    if (match(TokenType::IF)) {
        return if_statement();
    }
    if (match(TokenType::WHILE)) {
        return while_statement();
    }
    if (match(TokenType::FOR)) {
        return for_statement();
    }
    if (match(TokenType::SWITCH)) {
        return switch_statement();
    }
    if (match(TokenType::RETURN)) {
        return return_statement();
    }
    if (match(TokenType::BREAK)) {
        return break_statement();
    }
    if (match(TokenType::UNSAFE)) {
        return unsafe_statement();
    }
    if (match(TokenType::LLVM_INLINE)) {
        return llvm_inline_statement();
    }
    if (match(TokenType::LBRACE)) {
        Token opening_brace = previous();
        return std::make_shared<Block>(block(), opening_brace);
    }

    return expression_statement();
}

StmtPtr Parser::print_statement() {
    Token keyword = previous();

    ExprPtr value;
    try {
        value = expression();
    } catch (const ParserError&) {
        std::string hint = "The 'print' statement requires a valid expression.\n";
        hint += "       Example: print \"Hello\"; or print 42;";
        throw error(keyword, "Invalid expression in print statement", hint);
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Print statements must end with a semicolon.\n";
        hint += "       Example: print value;";
        throw error(peek(), "Expect ';' after value in print statement", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after value in print statement");
    return std::make_shared<PrintStmt>(value, keyword);
}

StmtPtr Parser::if_statement() {
    Token keyword = previous();

    if (!check(TokenType::LPAREN)) {
        std::string hint = "If statements require a condition in parentheses.\n";
        hint += "       Example: if (x > 5) { ... }";
        throw error(peek(), "Expect '(' after 'if'", hint);
    }

    consume(TokenType::LPAREN, "Expect '(' after 'if'");

    ExprPtr condition;
    try {
        condition = expression();
    } catch (const ParserError&) {
        std::string hint = "The condition in an if statement must be a valid expression.\n";
        hint += "       Example: if (x == 5) { ... }";
        throw error(keyword, "Invalid condition in if statement", hint);
    }

    if (!check(TokenType::RPAREN)) {
        std::string hint = "Close the condition with ')' before the if body.\n";
        hint += "       Example: if (condition) { ... }";
        throw error(peek(), "Expect ')' after if condition", hint);
    }

    consume(TokenType::RPAREN, "Expect ')' after if condition");

    StmtPtr then_branch = statement();
    std::optional<StmtPtr> else_branch;

    if (match(TokenType::ELSE)) {
        else_branch = statement();
    }

    return std::make_shared<IfStmt>(condition, then_branch, else_branch, keyword);
}

StmtPtr Parser::while_statement() {
    Token keyword = previous();

    loop_depth++;
    if (loop_depth > static_cast<int>(MAX_LOOP_DEPTH)) {
        loop_depth--;
        std::ostringstream hint;
        hint << "Loop nesting is too deep (maximum " << MAX_LOOP_DEPTH << " levels).\n";
        hint << "       Consider extracting inner loops into separate functions.";
        throw error(keyword, "Loop nesting depth exceeds maximum", hint.str());
    }

    try {
        if (!check(TokenType::LPAREN)) {
            std::string hint = "While loops require a condition in parentheses.\n";
            hint += "       Example: while (count < 10) { ... }";
            throw error(peek(), "Expect '(' after 'while'", hint);
        }

        consume(TokenType::LPAREN, "Expect '(' after 'while'");

        ExprPtr condition;
        try {
            condition = expression();
        } catch (const ParserError&) {
            std::string hint = "The condition in a while loop must be a valid expression.\n";
            hint += "       Example: while (x > 0) { ... }";
            throw error(keyword, "Invalid condition in while loop", hint);
        }

        if (!check(TokenType::RPAREN)) {
            std::string hint = "Close the condition with ')' before the loop body.\n";
            hint += "       Example: while (condition) { ... }";
            throw error(peek(), "Expect ')' after while condition", hint);
        }

        consume(TokenType::RPAREN, "Expect ')' after while condition");
        StmtPtr body = statement();

        loop_depth--;
        return std::make_shared<WhileStmt>(condition, body, keyword);
    } catch (...) {
        loop_depth--;
        throw;
    }
}

StmtPtr Parser::for_statement() {
    Token for_keyword = previous();

    loop_depth++;
    if (loop_depth > static_cast<int>(MAX_LOOP_DEPTH)) {
        loop_depth--;
        std::ostringstream hint;
        hint << "Loop nesting is too deep (maximum " << MAX_LOOP_DEPTH << " levels).\n";
        hint << "       Consider extracting inner loops into separate functions.";
        throw error(for_keyword, "Loop nesting depth exceeds maximum", hint.str());
    }

    try {
        if (!check(TokenType::LPAREN)) {
            std::string hint = "For loops require three clauses in parentheses.\n";
            hint += "       Example: for (var i = 0; i < 10; i = i + 1) { ... }";
            throw error(peek(), "Expect '(' after 'for'", hint);
        }

        consume(TokenType::LPAREN, "Expect '(' after 'for'");

        StmtPtr initializer;
        if (match(TokenType::SEMICOLON)) {
            initializer = nullptr;
        } else if (match(TokenType::VAR)) {
            initializer = var_declaration();
        } else {
            initializer = expression_statement();
        }

        ExprPtr condition;
        if (!check(TokenType::SEMICOLON)) {
            try {
                condition = expression();
            } catch (const ParserError&) {
                std::string hint = "The condition clause must be a valid expression.\n";
                hint += "       Example: for (var i = 0; i < 10; i = i + 1) { ... }";
                throw error(for_keyword, "Invalid condition in for loop", hint);
            }
        }

        if (!check(TokenType::SEMICOLON)) {
            std::string hint = "For loop clauses must be separated by semicolons.\n";
            hint += "       Example: for (init; condition; increment) { ... }";
            throw error(peek(), "Expect ';' after loop condition", hint);
        }

        consume(TokenType::SEMICOLON, "Expect ';' after loop condition");

        ExprPtr increment;
        if (!check(TokenType::RPAREN)) {
            try {
                increment = expression();
            } catch (const ParserError&) {
                std::string hint = "The increment clause must be a valid expression.\n";
                hint += "       Example: for (var i = 0; i < 10; i = i + 1) { ... }";
                throw error(for_keyword, "Invalid increment in for loop", hint);
            }
        }

        if (!check(TokenType::RPAREN)) {
            std::string hint = "Close the for loop clauses with ')' before the body.\n";
            hint += "       Example: for (init; cond; incr) { ... }";
            throw error(peek(), "Expect ')' after for clauses", hint);
        }

        consume(TokenType::RPAREN, "Expect ')' after for clauses");

        StmtPtr body = statement();

        if (increment) {
            std::vector<StmtPtr> block_stmts;
            block_stmts.push_back(body);
            block_stmts.push_back(std::make_shared<ExprStmt>(increment));
            body = std::make_shared<Block>(block_stmts, for_keyword);
        }

        if (!condition) {
            condition = std::make_shared<Literal>(true, for_keyword);
        }

        body = std::make_shared<WhileStmt>(condition, body, for_keyword);

        if (initializer) {
            std::vector<StmtPtr> block_stmts;
            block_stmts.push_back(initializer);
            block_stmts.push_back(body);
            body = std::make_shared<Block>(block_stmts, for_keyword);
        }

        loop_depth--;
        return body;
    } catch (...) {
        loop_depth--;
        throw;
    }
}

StmtPtr Parser::return_statement() {
    Token keyword = previous();

    if (function_depth == 0) {
        std::string hint = "Return statements can only be used inside functions.\n";
        hint += "       Move this return statement inside a function body.";
        throw error(keyword, "Cannot use 'return' outside of a function", hint);
    }

    std::optional<ExprPtr> value;
    if (!check(TokenType::SEMICOLON)) {
        try {
            value = expression();
        } catch (const ParserError&) {
            std::string hint = "The return value must be a valid expression.\n";
            hint += "       Example: return 42; or return x + y;";
            throw error(keyword, "Invalid return value expression", hint);
        }
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Return statements must end with a semicolon.\n";
        hint += "       Example: return value;";
        throw error(peek(), "Expect ';' after return value", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after return value");
    return std::make_shared<ReturnStmt>(keyword, value);
}

StmtPtr Parser::break_statement() {
    Token keyword = previous();

    if (loop_depth == 0) {
        std::string hint = "Break statements can only be used inside loops or switch statements.\n";
        hint += "       Move this break statement inside a loop or switch body.";
        throw error(keyword, "Cannot use 'break' outside of a loop or switch", hint);
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Break statements must end with a semicolon.\n";
        hint += "       Example: break;";
        throw error(peek(), "Expect ';' after 'break'", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after 'break'");
    return std::make_shared<BreakStmt>(keyword);
}

StmtPtr Parser::switch_statement() {
    Token keyword = previous();

    if (!check(TokenType::LPAREN)) {
        std::string hint = "Switch statements require a condition in parentheses.\n";
        hint += "       Example: switch (value) { case 1: ... }";
        throw error(peek(), "Expect '(' after 'switch'", hint);
    }

    consume(TokenType::LPAREN, "Expect '(' after 'switch'");

    ExprPtr condition;
    try {
        condition = expression();
    } catch (const ParserError&) {
        std::string hint = "The condition in a switch must be a valid expression.\n";
        hint += "       Example: switch (x) { ... }";
        throw error(keyword, "Invalid condition in switch", hint);
    }

    if (!check(TokenType::RPAREN)) {
        std::string hint = "Close the condition with ')' before the switch body.\n";
        hint += "       Example: switch (condition) { ... }";
        throw error(peek(), "Expect ')' after switch condition", hint);
    }

    consume(TokenType::RPAREN, "Expect ')' after switch condition");

    if (!check(TokenType::LBRACE)) {
        std::string hint = "Switch body must be enclosed in curly braces.\n";
        hint += "       Example: switch (x) { case 1: ... }";
        throw error(peek(), "Expect '{' before switch body", hint);
    }

    consume(TokenType::LBRACE, "Expect '{' before switch body");

    std::vector<CaseClause> cases;
    std::optional<std::vector<StmtPtr>> default_case;
    bool seen_default = false;

    loop_depth++;

    try {
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            if (match(TokenType::CASE)) {
                if (seen_default) {
                    std::string hint = "Case clauses cannot appear after default clause.\n";
                    hint += "       Move all case clauses before the default clause.";
                    throw error(previous(), "Case after default", hint);
                }

                ExprPtr case_value;
                try {
                    case_value = expression();
                } catch (const ParserError&) {
                    std::string hint = "Case value must be a valid expression.\n";
                    hint += "       Example: case 1: ... or case \"hello\": ...";
                    throw error(previous(), "Invalid case value", hint);
                }

                if (!check(TokenType::COLON)) {
                    std::string hint = "Case value must be followed by ':'.\n";
                    hint += "       Example: case 1: statements...";
                    throw error(peek(), "Expect ':' after case value", hint);
                }

                consume(TokenType::COLON, "Expect ':' after case value");

                std::vector<StmtPtr> statements;
                while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) &&
                       !check(TokenType::RBRACE) && !is_at_end()) {
                    StmtPtr stmt = declaration();
                    if (stmt) {
                        statements.push_back(stmt);
                    }
                }

                cases.push_back(CaseClause(case_value, statements));

            } else if (match(TokenType::DEFAULT)) {
                if (seen_default) {
                    std::string hint = "Only one default clause is allowed per switch.\n";
                    hint += "       Remove the duplicate default clause.";
                    throw error(previous(), "Duplicate default clause", hint);
                }

                seen_default = true;

                if (!check(TokenType::COLON)) {
                    std::string hint = "Default must be followed by ':'.\n";
                    hint += "       Example: default: statements...";
                    throw error(peek(), "Expect ':' after 'default'", hint);
                }

                consume(TokenType::COLON, "Expect ':' after 'default'");

                std::vector<StmtPtr> statements;
                while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) &&
                       !check(TokenType::RBRACE) && !is_at_end()) {
                    StmtPtr stmt = declaration();
                    if (stmt) {
                        statements.push_back(stmt);
                    }
                }

                default_case = statements;

            } else {
                std::string hint = "Switch body must contain case or default clauses.\n";
                hint += "       Example: switch (x) { case 1: ... default: ... }";
                throw error(peek(), "Expect 'case' or 'default' in switch body", hint);
            }
        }

        if (!check(TokenType::RBRACE)) {
            std::string hint = "Switch statements must be closed with '}'.\n";
            hint += "       Check that all opening '{' have matching closing '}'.";
            throw error(peek(), "Expect '}' after switch body", hint);
        }

        consume(TokenType::RBRACE, "Expect '}' after switch body");

        loop_depth--;
        return std::make_shared<SwitchStmt>(keyword, condition, cases, default_case);
    } catch (...) {
        loop_depth--;
        throw;
    }
}

std::vector<StmtPtr> Parser::block() {
    std::vector<StmtPtr> statements;

    block_depth++;
    if (block_depth > static_cast<int>(MAX_BLOCK_DEPTH)) {
        block_depth--;
        std::ostringstream hint;
        hint << "Block nesting is too deep (maximum " << MAX_BLOCK_DEPTH << " levels).\n";
        hint << "       Consider refactoring deeply nested code.";
        throw error(peek(), "Block nesting depth exceeds maximum", hint.str());
    }

    try {
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            StmtPtr stmt = declaration();
            if (stmt) {
                statements.push_back(stmt);
            }
        }

        if (!check(TokenType::RBRACE)) {
            std::string hint = "Blocks must be closed with '}'.\n";
            hint += "       Check that all opening '{' have matching closing '}'.";
            throw error(peek(), "Expect '}' after block", hint);
        }

        consume(TokenType::RBRACE, "Expect '}' after block");
        block_depth--;
        return statements;
    } catch (...) {
        block_depth--;
        throw;
    }
}

StmtPtr Parser::expression_statement() {
    ExprPtr expr;
    try {
        expr = expression();
    } catch (const ParserError&) {
        std::string hint = "Statement must be a valid expression.\n";
        hint += "       Check for syntax errors in the expression.";
        throw error(peek(), "Invalid expression statement", hint);
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Statements must end with a semicolon.\n";
        hint += "       Add ';' at the end of the statement.";
        throw error(peek(), "Expect ';' after expression", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after expression");
    return std::make_shared<ExprStmt>(expr);
}

ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::assignment() {
    ExprPtr expr = or_expr();

    if (match(TokenType::EQUAL)) {
        Token equals = previous();
        ExprPtr value = assignment();

        if (auto* var = dynamic_cast<Variable*>(expr.get())) {
            return std::make_shared<Assign>(var->name, value);
        } else if (auto* idx = dynamic_cast<IndexGet*>(expr.get())) {
            return std::make_shared<IndexSet>(idx->array, idx->index, value, idx->bracket);
        }

        std::string hint = "Invalid assignment target. Only variables and array elements can be assigned to.\n";
        hint += "       Example: variableName = value; or arr[0] = value;\n";
        hint += "       Cannot assign to: literals, expressions, function calls";
        throw error(equals, "Invalid assignment target", hint);
    }

    return expr;
}

ExprPtr Parser::or_expr() {
    ExprPtr expr = and_expr();

    while (match(TokenType::OR)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = and_expr();
        } catch (const ParserError&) {
            std::string hint = "The 'or' operator requires valid expressions on both sides.\n";
            hint += "       Example: condition1 or condition2";
            throw error(op, "Invalid right operand for 'or'", hint);
        }
        expr = std::make_shared<Logical>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::and_expr() {
    ExprPtr expr = equality();

    while (match(TokenType::AND)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = equality();
        } catch (const ParserError&) {
            std::string hint = "The 'and' operator requires valid expressions on both sides.\n";
            hint += "       Example: condition1 and condition2";
            throw error(op, "Invalid right operand for 'and'", hint);
        }
        expr = std::make_shared<Logical>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();

    while (match(TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = comparison();
        } catch (const ParserError&) {
            std::ostringstream hint;
            hint << "The '" << op.lexeme << "' operator requires valid expressions on both sides.\n";
            hint << "       Example: value1 " << op.lexeme << " value2";
            throw error(op, "Invalid right operand for '" + op.lexeme + "'", hint.str());
        }
        expr = std::make_shared<Binary>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = term();

    while (match(TokenType::GREATER, TokenType::GREATER_EQUAL, 
                 TokenType::LESS, TokenType::LESS_EQUAL)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = term();
        } catch (const ParserError&) {
            std::ostringstream hint;
            hint << "The '" << op.lexeme << "' operator requires valid expressions on both sides.\n";
            hint << "       Example: value1 " << op.lexeme << " value2";
            throw error(op, "Invalid right operand for '" + op.lexeme + "'", hint.str());
        }
        expr = std::make_shared<Binary>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();

    while (match(TokenType::MINUS, TokenType::PLUS)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = factor();
        } catch (const ParserError&) {
            std::ostringstream hint;
            hint << "The '" << op.lexeme << "' operator requires valid expressions on both sides.\n";
            hint << "       Example: value1 " << op.lexeme << " value2";
            throw error(op, "Invalid right operand for '" + op.lexeme + "'", hint.str());
        }
        expr = std::make_shared<Binary>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();

    while (match(TokenType::SLASH, TokenType::STAR, TokenType::PERCENT)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = unary();
        } catch (const ParserError&) {
            std::ostringstream hint;
            hint << "The '" << op.lexeme << "' operator requires valid expressions on both sides.\n";
            hint << "       Example: value1 " << op.lexeme << " value2";
            throw error(op, "Invalid right operand for '" + op.lexeme + "'", hint.str());
        }
        expr = std::make_shared<Binary>(expr, op, right);
    }

    return expr;
}

ExprPtr Parser::unary() {
    if (match(TokenType::BANG, TokenType::MINUS)) {
        Token op = previous();
        ExprPtr right;
        try {
            right = unary();
        } catch (const ParserError&) {
            std::ostringstream hint;
            hint << "The '" << op.lexeme << "' operator requires a valid expression.\n";
            hint << "       Example: " << op.lexeme << "value";
            throw error(op, "Invalid operand for '" + op.lexeme + "'", hint.str());
        }
        return std::make_shared<Unary>(op, right);
    }

    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            expr = finish_call(expr);
        } else if (match(TokenType::LBRACKET)) {
            expr = finish_index(expr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::finish_call(ExprPtr callee) {
    std::vector<ExprPtr> arguments;

    if (!check(TokenType::RPAREN)) {
        while (true) {
            if (arguments.size() >= MAX_ARGUMENTS) {
                std::ostringstream hint;
                hint << "Function calls support up to " << MAX_ARGUMENTS << " arguments.\n";
                hint << "       Consider restructuring to use fewer arguments.";
                throw error(peek(), "Cannot have more than " + std::to_string(MAX_ARGUMENTS) + " arguments", hint.str());
            }

            try {
                arguments.push_back(expression());
            } catch (const ParserError&) {
                std::string hint = "Function arguments must be valid expressions.\n";
                hint += "       Example: functionName(arg1, arg2, arg3)";
                throw error(previous(), "Invalid argument expression", hint);
            }

            if (!match(TokenType::COMMA)) {
                break;
            }

            if (check(TokenType::RPAREN)) {
                std::string hint = "Remove the trailing comma before ')'.\n";
                hint += "       Example: func(a, b) not func(a, b,)";
                throw error(peek(), "Trailing comma in argument list", hint);
            }
        }
    }

    if (!check(TokenType::RPAREN)) {
        std::string hint = "Function calls must be closed with ')'.\n";
        hint += "       Example: functionName(arg1, arg2)";
        throw error(peek(), "Expect ')' after arguments", hint);
    }

    Token paren = consume(TokenType::RPAREN, "Expect ')' after arguments");
    return std::make_shared<Call>(callee, paren, arguments);
}

ExprPtr Parser::finish_index(ExprPtr array) {
    Token bracket = previous();

    ExprPtr index;
    try {
        index = expression();
    } catch (const ParserError&) {
        std::string hint = "Array index must be a valid expression.\n";
        hint += "       Example: arr[0] or arr[i + 1]";
        throw error(bracket, "Invalid array index expression", hint);
    }

    if (!check(TokenType::RBRACKET)) {
        std::string hint = "Array indexing must be closed with ']'.\n";
        hint += "       Example: arr[index]";
        throw error(peek(), "Expect ']' after array index", hint);
    }

    Token closing_bracket = consume(TokenType::RBRACKET, "Expect ']' after array index");
    return std::make_shared<IndexGet>(array, index, closing_bracket);
}

ExprPtr Parser::array_literal() {
    Token bracket = previous();
    std::vector<ExprPtr> elements;

    if (!check(TokenType::RBRACKET)) {
        while (true) {
            if (elements.size() >= 1000) {
                std::string hint = "Array literals support up to 1000 elements.\n";
                hint += "       Consider using a different data structure or initialization method.";
                throw error(peek(), "Array literal too large", hint);
            }

            try {
                elements.push_back(expression());
            } catch (const ParserError&) {
                std::string hint = "Array elements must be valid expressions.\n";
                hint += "       Example: [1, 2, 3] or [x, y + 1, func()]";
                throw error(bracket, "Invalid array element expression", hint);
            }

            if (!match(TokenType::COMMA)) {
                break;
            }

            if (check(TokenType::RBRACKET)) {
                std::string hint = "Remove the trailing comma before ']'.\n";
                hint += "       Example: [1, 2, 3] not [1, 2, 3,]";
                throw error(peek(), "Trailing comma in array literal", hint);
            }
        }
    }

    if (!check(TokenType::RBRACKET)) {
        std::string hint = "Array literals must be closed with ']'.\n";
        hint += "       Example: [1, 2, 3]";
        throw error(peek(), "Expect ']' after array elements", hint);
    }

    Token closing_bracket = consume(TokenType::RBRACKET, "Expect ']' after array elements");
    return std::make_shared<ArrayLiteral>(elements, closing_bracket);
}

ExprPtr Parser::dict_literal() {
    Token brace = previous();
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;

    if (!check(TokenType::RBRACE)) {
        while (true) {
            if (pairs.size() >= 1000) {
                std::string hint = "Dictionary literals support up to 1000 key-value pairs.\n";
                hint += "       Consider using a different data structure or initialization method.";
                throw error(peek(), "Dictionary literal too large", hint);
            }

            ExprPtr key;
            try {
                key = expression();
            } catch (const ParserError&) {
                std::string hint = "Dictionary keys must be valid expressions.\n";
                hint += "       Example: {\"name\": \"John\", \"age\": 30}";
                throw error(brace, "Invalid dictionary key expression", hint);
            }

            if (!check(TokenType::COLON)) {
                std::string hint = "Dictionary key-value pairs must be separated by ':'.\n";
                hint += "       Example: {key: value}";
                throw error(peek(), "Expect ':' after dictionary key", hint);
            }

            consume(TokenType::COLON, "Expect ':' after dictionary key");

            ExprPtr value;
            try {
                value = expression();
            } catch (const ParserError&) {
                std::string hint = "Dictionary values must be valid expressions.\n";
                hint += "       Example: {\"name\": \"John\", \"age\": 30}";
                throw error(brace, "Invalid dictionary value expression", hint);
            }

            pairs.push_back({key, value});

            if (!match(TokenType::COMMA)) {
                break;
            }

            if (check(TokenType::RBRACE)) {
                std::string hint = "Remove the trailing comma before '}'.\n";
                hint += "       Example: {\"a\": 1, \"b\": 2} not {\"a\": 1, \"b\": 2,}";
                throw error(peek(), "Trailing comma in dictionary literal", hint);
            }
        }
    }

    if (!check(TokenType::RBRACE)) {
        std::string hint = "Dictionary literals must be closed with '}'.\n";
        hint += "       Example: {\"key\": \"value\"}";
        throw error(peek(), "Expect '}' after dictionary elements", hint);
    }

    Token closing_brace = consume(TokenType::RBRACE, "Expect '}' after dictionary elements");
    return std::make_shared<DictLiteral>(pairs, closing_brace);
}

/**
 * @brief Parse primary expressions (terminals and grouped expressions)
 * 
 * Grammar production:
 * primary → NUMBER | STRING | "true" | "false" | "nil"
 *         | IDENTIFIER | "(" expression ")" 
 *         | "[" array_literal "]" | "{" dict_literal "}"
 *         | builtin_function_name
 * 
 * Error handling strategy:
 * - Provides context-specific hints for common mistakes
 * - Distinguishes between missing expressions and invalid tokens
 * - Suggests valid alternatives based on context
 * 
 * @return Parsed primary expression AST node
 * @throws ParserError on invalid token or malformed expression
 */
ExprPtr Parser::primary() {
    if (match(TokenType::FALSE)) {
        return std::make_shared<Literal>(false, previous());
    }
    if (match(TokenType::TRUE)) {
        return std::make_shared<Literal>(true, previous());
    }
    if (match(TokenType::NIL)) {
        return std::make_shared<Literal>(std::monostate{}, previous());
    }

    if (match(TokenType::NUMBER)) {
        Token token = previous();
        if (!std::holds_alternative<double>(token.literal)) {
            throw error(token, "Internal error: NUMBER token without numeric value");
        }
        return std::make_shared<Literal>(token.literal, token);
    }

    if (match(TokenType::STRING)) {
        Token token = previous();
        if (!std::holds_alternative<std::string>(token.literal)) {
            throw error(token, "Internal error: STRING token without string value");
        }
        return std::make_shared<Literal>(token.literal, token);
    }

    if (match(TokenType::IDENTIFIER)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::LEN)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::HAS)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::KEYS)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::VALUES)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::INPUT)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::INPUT_NUM)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::READ_FILE)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::WRITE_FILE)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::APPEND_FILE)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::FILE_EXISTS)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::MALLOC)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::CALLOC)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::REALLOC)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::FREE)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::ADDR_OF)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::DEREF)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::LLVM_INLINE)) {
        return std::make_shared<Variable>(previous());
    }

    if (match(TokenType::LBRACKET)) {
        return array_literal();
    }

    if (match(TokenType::LBRACE)) {
        return dict_literal();
    }

    if (match(TokenType::LPAREN)) {
        ExprPtr expr;
        try {
            expr = expression();
        } catch (const ParserError&) {
            std::string hint = "Grouped expressions must contain valid expressions.\n";
            hint += "       Example: (value + 5)";
            throw error(previous(), "Invalid expression in grouping", hint);
        }

        if (!check(TokenType::RPAREN)) {
            std::string hint = "Grouped expressions must be closed with ')'.\n";
            hint += "       Check that all opening '(' have matching closing ')'.";
            throw error(peek(), "Expect ')' after expression", hint);
        }

        consume(TokenType::RPAREN, "Expect ')' after expression");
        return std::make_shared<Grouping>(expr);
    }

    Token current = peek();
    std::optional<std::string> hint;

    if (current.type == TokenType::SEMICOLON) {
        hint = "Unexpected semicolon. Did you forget an expression before ';'?";
    } else if (current.type == TokenType::RBRACE) {
        hint = "Unexpected '}'. Check for matching '{' or missing expression.";
    } else if (current.type == TokenType::RPAREN) {
        hint = "Unexpected ')'. Check for matching '(' or missing expression.";
    } else if (current.type == TokenType::PLUS || current.type == TokenType::STAR ||
               current.type == TokenType::SLASH || current.type == TokenType::PERCENT) {
        std::ostringstream h;
        h << "'" << current.lexeme << "' requires a left operand.\n";
        h << "       Example: value " << current.lexeme << " 5";
        hint = h.str();
    } else if (current.type == TokenType::END_OF_FILE) {
        hint = "Unexpected end of file. Check for unclosed blocks or incomplete expressions.";
    } else {
        hint = "This token cannot start an expression.\n";
        hint.value() += "       Valid expression starters: numbers, strings, identifiers, '(', '[', '{', true, false, nil";
    }

    throw error(peek(), "Expect expression", hint);
}

StmtPtr Parser::unsafe_statement() {
    Token keyword = previous();

    if (!check(TokenType::LBRACE)) {
        std::string hint = "Unsafe blocks must be followed by '{'.\n";
        hint += "       Example: unsafe { ... }";
        throw error(peek(), "Expect '{' after 'unsafe'", hint);
    }

    consume(TokenType::LBRACE, "Expect '{' after 'unsafe'");

    bool prev_unsafe_state = in_unsafe_block;
    in_unsafe_block = true;
    std::vector<StmtPtr> statements;

    while (!check(TokenType::RBRACE) && !is_at_end()) {
        StmtPtr stmt = declaration();
        if (stmt) {
            statements.push_back(stmt);
        }
    }

    if (!check(TokenType::RBRACE)) {
        std::string hint = "Unsafe blocks must be closed with '}'.\n";
        hint += "       Check that all opening '{' have matching closing '}'.";
        throw error(peek(), "Expect '}' after unsafe block", hint);
    }

    consume(TokenType::RBRACE, "Expect '}' after unsafe block");
    in_unsafe_block = prev_unsafe_state;

    return std::make_shared<UnsafeBlock>(keyword, statements);
}

StmtPtr Parser::llvm_inline_statement() {
    Token keyword = previous();

    if (!in_unsafe_block) {
        std::string hint = "llvm_inline() can only be used inside unsafe blocks.\n";
        hint += "       Wrap your code in: unsafe { llvm_inline(...); }";
        throw error(keyword, "llvm_inline() requires unsafe context", hint);
    }

    if (!check(TokenType::LPAREN)) {
        std::string hint = "llvm_inline requires parentheses.\n";
        hint += "       Example: llvm_inline(\"LLVM IR code\");";
        throw error(peek(), "Expect '(' after 'llvm_inline'", hint);
    }

    consume(TokenType::LPAREN, "Expect '(' after 'llvm_inline'");

    if (!check(TokenType::STRING)) {
        std::string hint = "llvm_inline requires a string literal containing LLVM IR code.\n";
        hint += "       Example: llvm_inline(\"%result = add i32 5, 10\");";
        throw error(peek(), "Expect string literal with LLVM IR code", hint);
    }

    Token llvm_code_token = advance();
    std::string llvm_code;
    if (std::holds_alternative<std::string>(llvm_code_token.literal)) {
        llvm_code = std::get<std::string>(llvm_code_token.literal);
    }

    if (!check(TokenType::RPAREN)) {
        std::string hint = "llvm_inline call must be closed with ')'.\n";
        hint += "       Check that all opening '(' have matching closing ')'.";
        throw error(peek(), "Expect ')' after LLVM IR code", hint);
    }

    consume(TokenType::RPAREN, "Expect ')' after LLVM IR code");

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Statements must end with semicolon.\n";
        hint += "       Add ';' at the end of the statement.";
        throw error(peek(), "Expect ';' after llvm_inline() call", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after statement");

    std::unordered_map<std::string, std::string> variables_map;

    return std::make_shared<LLVMInlineStmt>(keyword, llvm_code, variables_map);
}

StmtPtr Parser::import_statement() {
    Token keyword = previous();

    if (!check(TokenType::STRING)) {
        std::string hint = "import requires a string literal with the file path.\n";
        hint += "       Example: import \"module.box\";";
        throw error(peek(), "Expect string literal with file path after 'import'", hint);
    }

    Token path_token = advance();
    std::string file_path;
    if (std::holds_alternative<std::string>(path_token.literal)) {
        file_path = std::get<std::string>(path_token.literal);
    } else {
        std::string hint = "import path must be a string.\n";
        hint += "       Example: import \"utils.box\";";
        throw error(path_token, "Invalid import path", hint);
    }

    if (file_path.empty()) {
        std::string hint = "Import path cannot be empty.\n";
        hint += "       Provide a valid file path like \"module.box\"";
        throw error(path_token, "Empty import path", hint);
    }

    if (!check(TokenType::SEMICOLON)) {
        std::string hint = "Import statements must end with a semicolon.\n";
        hint += "       Example: import \"module.box\";";
        throw error(peek(), "Expect ';' after import path", hint);
    }

    consume(TokenType::SEMICOLON, "Expect ';' after import path");
    return std::make_shared<ImportStmt>(keyword, file_path, path_token);
}

void Parser::synchronize() {
    advance();

    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) {
            return;
        }

        switch (peek().type) {
            case TokenType::VAR:
            case TokenType::FUN:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::PRINT:
            case TokenType::RETURN:
            case TokenType::SWITCH:
            case TokenType::BREAK:
                return;
            default:
                break;
        }

        advance();
    }
}

bool Parser::match(const std::vector<TokenType>& types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return false;
    }
    return peek().type == type;
}

Token Parser::advance() {
    if (!is_at_end()) {
        current++;
    }
    return previous();
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::END_OF_FILE;
}

Token Parser::peek() const {
    return tokens[current];
}

Token Parser::previous() const {
    return tokens[current - 1];
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }

    throw error(peek(), message);
}

ParserError Parser::error(const Token& token, const std::string& message,
                         const std::optional<std::string>& hint) {
    return ParserError(token, message, hint, source);
}

}
