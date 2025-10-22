#ifndef BOX_LEXER_HPP
#define BOX_LEXER_HPP

#include "token.hpp"
#include "lexer_error.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace box {

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> scan_tokens();

private:
    std::string source;
    std::vector<std::string> lines;
    std::vector<Token> tokens;
    size_t start;
    size_t current;
    int line;
    int column;
    int start_column;
    std::vector<LexerError> errors;

    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<char, char> escape_sequences;

    void initialize_keywords();
    void initialize_escape_sequences();
    void split_lines();

    std::string get_source_line(int line_num) const;
    void scan_token();
    void block_comment();
    void string();
    std::optional<int> read_hex_escape(int length);
    void number();
    void identifier();

    bool match(char expected);
    char peek() const;
    char peek_next() const;
    char advance();
    void add_token(TokenType type);
    void add_token(TokenType type, LiteralValue literal);
    bool is_at_end() const;
    void error(const std::string& message, const std::optional<std::string>& hint = std::nullopt);

    bool is_digit(char c) const;
    bool is_alpha(char c) const;
    bool is_alnum(char c) const;
};

}

#endif