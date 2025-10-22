#ifndef BOX_LEXER_ERROR_HPP
#define BOX_LEXER_ERROR_HPP

#include <stdexcept>
#include <string>
#include <optional>

namespace box {

class LexerError : public std::runtime_error {
public:
    std::string message;
    int line;
    int column;
    std::optional<std::string> hint;
    std::optional<std::string> source_line;

    LexerError(const std::string& message, int line, int column,
               const std::optional<std::string>& hint = std::nullopt,
               const std::optional<std::string>& source_line = std::nullopt);

    const char* what() const noexcept override;

private:
    std::string formatted_message;
    void format_error_message();
};

}

#endif