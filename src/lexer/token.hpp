#ifndef BOX_TOKEN_HPP
#define BOX_TOKEN_HPP

#include <string>
#include <optional>
#include <variant>
#include <ostream>

namespace box {

enum class TokenType {
    NUMBER,
    STRING,
    TRUE,
    FALSE,
    NIL,
    IDENTIFIER,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,

    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,

    AND,
    OR,

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    SEMICOLON,
    COLON,

    VAR,
    PRINT,
    IF,
    ELSE,
    WHILE,
    FOR,
    FUN,
    RETURN,
    LEN,
    HAS,
    KEYS,
    VALUES,
    SWITCH,
    CASE,
    DEFAULT,
    BREAK,
    INPUT,
    INPUT_NUM,
    READ_FILE,
    WRITE_FILE,
    APPEND_FILE,
    FILE_EXISTS,
    IMPORT,

    PTR,
    MALLOC,
    FREE,
    CALLOC,
    REALLOC,
    ADDR_OF,
    DEREF,
    AMPERSAND,
    ARROW,

    UNSAFE,
    LLVM_INLINE,

    END_OF_FILE
};

std::string token_type_to_string(TokenType type);

using LiteralValue = std::variant<std::monostate, double, std::string, bool>;

struct Token {
    TokenType type;
    std::string lexeme;
    LiteralValue literal;
    int line;
    int column;

    Token(TokenType type, const std::string& lexeme, LiteralValue literal, int line, int column)
        : type(type), lexeme(lexeme), literal(literal), line(line), column(column) {}

    Token(TokenType type, const std::string& lexeme, int line, int column)
        : type(type), lexeme(lexeme), literal(std::monostate{}), line(line), column(column) {}

    std::string to_string() const;
};

std::ostream& operator<<(std::ostream& os, const Token& token);

}

#endif