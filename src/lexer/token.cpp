#include "token.hpp"
#include <sstream>

namespace box {

std::string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NIL: return "NIL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::VAR: return "VAR";
        case TokenType::PRINT: return "PRINT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::FUN: return "FUN";
        case TokenType::RETURN: return "RETURN";
        case TokenType::LEN: return "LEN";
        case TokenType::HAS: return "HAS";
        case TokenType::KEYS: return "KEYS";
        case TokenType::VALUES: return "VALUES";
        case TokenType::SWITCH: return "SWITCH";
        case TokenType::CASE: return "CASE";
        case TokenType::DEFAULT: return "DEFAULT";
        case TokenType::BREAK: return "BREAK";
        case TokenType::INPUT: return "INPUT";
        case TokenType::INPUT_NUM: return "INPUT_NUM";
        case TokenType::READ_FILE: return "READ_FILE";
        case TokenType::WRITE_FILE: return "WRITE_FILE";
        case TokenType::APPEND_FILE: return "APPEND_FILE";
        case TokenType::FILE_EXISTS: return "FILE_EXISTS";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::PTR: return "PTR";
        case TokenType::MALLOC: return "MALLOC";
        case TokenType::FREE: return "FREE";
        case TokenType::CALLOC: return "CALLOC";
        case TokenType::REALLOC: return "REALLOC";
        case TokenType::ADDR_OF: return "ADDR_OF";
        case TokenType::DEREF: return "DEREF";
        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::ARROW: return "ARROW";
        case TokenType::UNSAFE: return "UNSAFE";
        case TokenType::LLVM_INLINE: return "LLVM_INLINE";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN";
    }
}

std::string Token::to_string() const {
    std::ostringstream oss;
    oss << "Token(" << token_type_to_string(type) << ", '" << lexeme << "', ";
    
    if (std::holds_alternative<double>(literal)) {
        oss << std::get<double>(literal);
    } else if (std::holds_alternative<std::string>(literal)) {
        oss << "\"" << std::get<std::string>(literal) << "\"";
    } else if (std::holds_alternative<bool>(literal)) {
        oss << (std::get<bool>(literal) ? "true" : "false");
    } else {
        oss << "None";
    }
    
    oss << ", " << line << ":" << column << ")";
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << token.to_string();
    return os;
}

}