#include "lexer.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace box {

Lexer::Lexer(const std::string& source)
    : source(source)
    , start(0)
    , current(0)
    , line(1)
    , column(1)
    , start_column(1) {
    initialize_keywords();
    initialize_escape_sequences();
    split_lines();
}

void Lexer::initialize_keywords() {
    keywords = {
        {"addr_of", TokenType::ADDR_OF},
        {"and", TokenType::AND},
        {"append_file", TokenType::APPEND_FILE},
        {"break", TokenType::BREAK},
        {"calloc", TokenType::CALLOC},
        {"case", TokenType::CASE},
        {"default", TokenType::DEFAULT},
        {"deref", TokenType::DEREF},
        {"else", TokenType::ELSE},
        {"false", TokenType::FALSE},
        {"file_exists", TokenType::FILE_EXISTS},
        {"for", TokenType::FOR},
        {"free", TokenType::FREE},
        {"import", TokenType::IMPORT},
        {"fun", TokenType::FUN},
        {"has", TokenType::HAS},
        {"if", TokenType::IF},
        {"input", TokenType::INPUT},
        {"input_num", TokenType::INPUT_NUM},
        {"keys", TokenType::KEYS},
        {"len", TokenType::LEN},
        {"llvm_inline", TokenType::LLVM_INLINE},
        {"malloc", TokenType::MALLOC},
        {"nil", TokenType::NIL},
        {"or", TokenType::OR},
        {"print", TokenType::PRINT},
        {"read_file", TokenType::READ_FILE},
        {"realloc", TokenType::REALLOC},
        {"return", TokenType::RETURN},
        {"switch", TokenType::SWITCH},
        {"true", TokenType::TRUE},
        {"unsafe", TokenType::UNSAFE},
        {"values", TokenType::VALUES},
        {"var", TokenType::VAR},
        {"while", TokenType::WHILE},
        {"write_file", TokenType::WRITE_FILE}
    };
}

void Lexer::initialize_escape_sequences() {
    escape_sequences = {
        {'n', '\n'},
        {'t', '\t'},
        {'r', '\r'},
        {'\\', '\\'},
        {'"', '"'},
        {'0', '\0'},
        {'a', '\a'},
        {'b', '\b'},
        {'f', '\f'},
        {'v', '\v'}
    };
}

void Lexer::split_lines() {
    std::string line_text;
    for (char c : source) {
        if (c == '\n') {
            lines.push_back(line_text);
            line_text.clear();
        } else {
            line_text += c;
        }
    }
    if (!line_text.empty() || source.empty() || source.back() == '\n') {
        lines.push_back(line_text);
    }
}

std::string Lexer::get_source_line(int line_num) const {
    if (line_num >= 1 && line_num <= static_cast<int>(lines.size())) {
        return lines[line_num - 1];
    }
    return "";
}

std::vector<Token> Lexer::scan_tokens() {
    while (!is_at_end()) {
        start = current;
        start_column = column;
        try {
            scan_token();
        } catch (const LexerError& e) {
            errors.push_back(e);
        }
    }

    if (!errors.empty()) {
        std::ostringstream error_messages;
        for (const auto& e : errors) {
            error_messages << e.what();
        }
        
        std::ostringstream summary;
        summary << "\n" << std::string(70, '#') << "\n";
        summary << "COMPILATION FAILED: Found " << errors.size() << " lexical error(s)\n";
        summary << std::string(70, '#') << "\n";
        
        throw std::runtime_error(summary.str() + error_messages.str());
    }

    tokens.push_back(Token(TokenType::END_OF_FILE, "", line, column));
    return tokens;
}

void Lexer::scan_token() {
    char c = advance();

    switch (c) {
        case '(': add_token(TokenType::LPAREN); break;
        case ')': add_token(TokenType::RPAREN); break;
        case '{': add_token(TokenType::LBRACE); break;
        case '}': add_token(TokenType::RBRACE); break;
        case '[': add_token(TokenType::LBRACKET); break;
        case ']': add_token(TokenType::RBRACKET); break;
        case ',': add_token(TokenType::COMMA); break;
        case ';': add_token(TokenType::SEMICOLON); break;
        case ':': add_token(TokenType::COLON); break;
        case '+': add_token(TokenType::PLUS); break;
        case '-':
            if (match('>')) {
                add_token(TokenType::ARROW);
            } else {
                add_token(TokenType::MINUS);
            }
            break;
        case '*': add_token(TokenType::STAR); break;
        case '&': add_token(TokenType::AMPERSAND); break;
        case '%': add_token(TokenType::PERCENT); break;
        case '!':
            add_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
            break;
        case '=':
            add_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
            break;
        case '<':
            add_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '>':
            add_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
        case '/':
            if (match('/')) {
                while (peek() != '\n' && !is_at_end()) {
                    advance();
                }
            } else if (match('*')) {
                block_comment();
            } else {
                add_token(TokenType::SLASH);
            }
            break;
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            line++;
            column = 0;
            break;
        case '"':
            string();
            break;
        default:
            if (is_digit(c)) {
                number();
            } else if (is_alpha(c)) {
                identifier();
            } else {
                std::string source_line = get_source_line(line);
                std::optional<std::string> hint;

                if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']') {
                    hint = std::string("The character '") + c + "' might be misplaced. Check for matching pairs.";
                } else if (c == '@' || c == '#' || c == '$') {
                    hint = std::string("'") + c + "' is not a valid Box operator. Did you mean to use a different operator?";
                } else if (c == '|') {
                    hint = "Use 'or' keyword instead of '|' for logical OR operation.";
                } else if (static_cast<unsigned char>(c) < 32 || c == 127) {
                    hint = std::string("Invisible control character detected (ASCII ") + 
                           std::to_string(static_cast<int>(c)) + "). Remove it from source code.";
                } else {
                    hint = "This character is not recognized in Box. Check your syntax.";
                }

                std::ostringstream msg;
                msg << "Unexpected character '" << c << "' (ASCII " << static_cast<int>(c) << ")";
                throw LexerError(msg.str(), line, start_column, hint, source_line);
            }
            break;
    }
}

void Lexer::block_comment() {
    int depth = 1;
    int start_line = line;
    int start_col = start_column;

    while (depth > 0 && !is_at_end()) {
        if (peek() == '/' && peek_next() == '*') {
            advance();
            advance();
            depth++;
        } else if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            depth--;
        } else {
            if (peek() == '\n') {
                line++;
                column = 0;
            }
            advance();
        }
    }

    if (depth > 0) {
        std::string source_line = get_source_line(start_line);
        std::ostringstream hint;
        hint << "Add '*/' to close the comment. Block comments must be properly closed.\n";
        hint << "       Opened at line " << start_line << ", column " << start_col << ".";
        
        std::ostringstream msg;
        msg << "Unterminated block comment (missing " << depth << " closing '*/')";
        throw LexerError(msg.str(), line, column, hint.str(), source_line);
    }
}

void Lexer::string() {
    int start_line = line;
    int start_col = start_column;
    std::string value;

    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            line++;
            column = 0;
            value += '\n';
            advance();
        } else if (peek() == '\\') {
            advance();
            if (is_at_end()) {
                std::string source_line = get_source_line(start_line);
                std::ostringstream hint;
                hint << "Add closing '\"' to terminate the string literal.\n";
                hint << "       String started at line " << start_line << ", column " << start_col << ".";
                throw LexerError("Unterminated string: reached end of file while parsing string",
                               line, column, hint.str(), source_line);
            }

            char escape_char = peek();
            int escape_column = column;
            advance();

            if (escape_sequences.count(escape_char)) {
                value += escape_sequences[escape_char];
            } else if (escape_char == 'x') {
                auto hex_value = read_hex_escape(2);
                if (hex_value.has_value()) {
                    if (hex_value.value() > 255) {
                        std::string source_line = get_source_line(line);
                        std::ostringstream msg;
                        msg << "Hexadecimal escape value out of range: \\x" << std::hex << hex_value.value();
                        throw LexerError(msg.str(), line, escape_column,
                                       "Hexadecimal escape sequences must be in range \\x00 to \\xFF.",
                                       source_line);
                    }
                    value += static_cast<char>(hex_value.value());
                } else {
                    std::string source_line = get_source_line(line);
                    throw LexerError("Invalid hexadecimal escape sequence", line, escape_column,
                                   "Hexadecimal escape sequences require exactly 2 hex digits.\n"
                                   "       Example: \\x41 (represents 'A')", source_line);
                }
            } else if (escape_char == 'u') {
                auto hex_value = read_hex_escape(4);
                if (hex_value.has_value()) {
                    if (hex_value.value() > 0x10FFFF) {
                        std::string source_line = get_source_line(line);
                        std::ostringstream hint;
                        hint << "Unicode code point U+" << std::hex << hex_value.value() << " is not valid.";
                        std::ostringstream msg;
                        msg << "Invalid Unicode code point: \\u" << std::hex << hex_value.value();
                        throw LexerError(msg.str(), line, escape_column, hint.str(), source_line);
                    }
                    
                    int codepoint = hex_value.value();
                    if (codepoint <= 0x7F) {
                        value += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        value += static_cast<char>(0xC0 | (codepoint >> 6));
                        value += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0xFFFF) {
                        value += static_cast<char>(0xE0 | (codepoint >> 12));
                        value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        value += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        value += static_cast<char>(0xF0 | (codepoint >> 18));
                        value += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        value += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                } else {
                    std::string source_line = get_source_line(line);
                    throw LexerError("Invalid unicode escape sequence", line, escape_column,
                                   "Unicode escape sequences require exactly 4 hex digits.\n"
                                   "       Example: \\u0041 (represents 'A')", source_line);
                }
            } else {
                std::string source_line = get_source_line(line);
                std::ostringstream hint;
                hint << "Valid escape sequences are: ";
                bool first = true;
                for (const auto& [k, v] : escape_sequences) {
                    if (!first) hint << ", ";
                    hint << "\\" << k;
                    first = false;
                }
                hint << ", \\xHH, \\uHHHH\n";
                hint << "       If you want a literal backslash, use \\\\";
                
                std::ostringstream msg;
                msg << "Invalid escape sequence '\\" << escape_char << "' in string literal";
                throw LexerError(msg.str(), line, escape_column, hint.str(), source_line);
            }
        } else {
            value += peek();
            advance();
        }
    }

    if (is_at_end()) {
        std::string source_line = get_source_line(start_line);
        std::ostringstream hint;
        hint << "Add closing '\"' to terminate the string literal.\n";
        hint << "       String started at line " << start_line << ", column " << start_col << ".\n";
        if (value.find('\n') != std::string::npos) {
            hint << "       Note: Multi-line strings are allowed in Box.";
        }
        throw LexerError("Unterminated string literal: missing closing quote",
                       line, column, hint.str(), source_line);
    }

    advance();
    add_token(TokenType::STRING, value);
}

std::optional<int> Lexer::read_hex_escape(int length) {
    std::string hex_chars;
    for (int i = 0; i < length; i++) {
        if (is_at_end()) {
            return std::nullopt;
        }
        char c = peek();
        if (!is_digit(c) && !(std::tolower(c) >= 'a' && std::tolower(c) <= 'f')) {
            return std::nullopt;
        }
        hex_chars += advance();
    }

    try {
        return std::stoi(hex_chars, nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Parse numeric literals with IEEE 754 compliance
 * 
 * Architectural rationale:
 * - Handles integer and floating-point literals
 * - Supports scientific notation (e/E)
 * - Validates overflow conditions
 * - Ensures LLVM IR compatibility
 * 
 * Technical constraints:
 * - Must align with LLVM double type semantics
 * - Prevents silent overflow/underflow
 * - Maintains source position tracking for diagnostics
 */
void Lexer::number() {
    int start_col = start_column;

    while (is_digit(peek())) {
        advance();
    }

    bool has_decimal = false;
    if (peek() == '.' && is_digit(peek_next())) {
        has_decimal = true;
        advance();
        while (is_digit(peek())) {
            advance();
        }
    } else if (peek() == '.' && !is_digit(peek_next())) {
        std::string source_line = get_source_line(line);
        throw LexerError("Invalid number literal: decimal point must be followed by digits",
                       line, column,
                       "A decimal point must be followed by at least one digit.\n"
                       "       Example: 3.14 (correct), 3. (incorrect)",
                       source_line);
    }

    bool has_exponent = false;
    if (peek() == 'e' || peek() == 'E') {
        has_exponent = true;
        advance();

        if (peek() == '+' || peek() == '-') {
            advance();
        }

        if (!is_digit(peek())) {
            std::string source_line = get_source_line(line);
            throw LexerError("Invalid number literal: exponent must be followed by digits",
                           line, start_col,
                           "Exponent must be followed by at least one digit.\n"
                           "       Example: 1.5e10 (correct), 1.5e (incorrect)",
                           source_line);
        }

        while (is_digit(peek())) {
            advance();
        }
    }

    std::string literal = source.substr(start, current - start);

    try {
        double value = std::stod(literal);

        if (std::isinf(value) && value > 0) {
            std::string source_line = get_source_line(line);
            std::ostringstream msg;
            msg << "Number literal overflow: '" << literal << "' exceeds maximum representable value";
            throw LexerError(msg.str(), line, start_col,
                           "Number is too large to represent. Use a smaller value.",
                           source_line);
        }
        
        if (std::isinf(value) && value < 0) {
            std::string source_line = get_source_line(line);
            std::ostringstream msg;
            msg << "Number literal underflow: '" << literal << "' exceeds minimum representable value";
            throw LexerError(msg.str(), line, start_col,
                           "Number is too small to represent. Use a larger value.",
                           source_line);
        }

        if (value == 0.0 && !literal.empty() && literal != "0" && literal.find_first_not_of("0.eE+-") != std::string::npos) {
            if (has_decimal || has_exponent) {
            }
        }

        add_token(TokenType::NUMBER, value);
    } catch (const std::out_of_range& e) {
        std::string source_line = get_source_line(line);
        std::ostringstream msg;
        msg << "Number literal out of range: '" << literal << "'";
        throw LexerError(msg.str(), line, start_col,
                       "Number exceeds representable range. Valid range: ±1.7976931348623157e+308",
                       source_line);
    } catch (const std::invalid_argument& e) {
        std::string source_line = get_source_line(line);
        std::ostringstream msg;
        msg << "Malformed number literal: '" << literal << "'";
        throw LexerError(msg.str(), line, start_col,
                       "Check the number format. Valid examples: 42, 3.14, 1.5e10, 2.0e-5",
                       source_line);
    } catch (const std::exception& e) {
        std::string source_line = get_source_line(line);
        std::ostringstream msg;
        msg << "Invalid number literal '" << literal << "': " << e.what();
        throw LexerError(msg.str(), line, start_col,
                       "Check the number format. Valid examples: 42, 3.14, 1.5e10",
                       source_line);
    }
}

void Lexer::identifier() {
    while (is_alnum(peek())) {
        advance();
    }

    std::string text = source.substr(start, current - start);

    if (text.length() > 255) {
        std::string source_line = get_source_line(line);
        std::ostringstream hint;
        hint << "Identifiers must be 255 characters or fewer.\n";
        hint << "       Current length: " << text.length() << " characters.\n";
        hint << "       Consider using a shorter, more descriptive name.";
        
        std::ostringstream msg;
        msg << "Identifier too long: '" << text.substr(0, 50) << "...'";
        throw LexerError(msg.str(), line, start_column, hint.str(), source_line);
    }

    auto it = keywords.find(text);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
    add_token(type);
}

bool Lexer::match(char expected) {
    if (is_at_end() || source[current] != expected) {
        return false;
    }
    current++;
    column++;
    return true;
}

char Lexer::peek() const {
    return is_at_end() ? '\0' : source[current];
}

char Lexer::peek_next() const {
    return (current + 1 >= source.length()) ? '\0' : source[current + 1];
}

char Lexer::advance() {
    if (is_at_end()) {
        return '\0';
    }
    char c = source[current];
    current++;
    column++;
    return c;
}

void Lexer::add_token(TokenType type) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, line, start_column));
}

void Lexer::add_token(TokenType type, LiteralValue literal) {
    std::string text = source.substr(start, current - start);
    tokens.push_back(Token(type, text, literal, line, start_column));
}

bool Lexer::is_at_end() const {
    return current >= source.length();
}

void Lexer::error(const std::string& message, const std::optional<std::string>& hint) {
    std::string source_line = get_source_line(line);
    throw LexerError(message, line, start_column, hint, source_line);
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_alnum(char c) const {
    return is_alpha(c) || is_digit(c);
}

}