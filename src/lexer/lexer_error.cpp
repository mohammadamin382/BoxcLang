#include "lexer_error.hpp"
#include <sstream>
#include <string>

namespace box {

LexerError::LexerError(const std::string& message, int line, int column,
                       const std::optional<std::string>& hint,
                       const std::optional<std::string>& source_line)
    : std::runtime_error(message)
    , message(message)
    , line(line)
    , column(column)
    , hint(hint)
    , source_line(source_line) {
    format_error_message();
}

void LexerError::format_error_message() {
    std::ostringstream oss;
    
    oss << "\n" << std::string(70, '=') << "\n";
    oss << "LEXER ERROR at Line " << line << ", Column " << column << "\n";
    oss << std::string(70, '=') << "\n";
    oss << "Error: " << message << "\n";

    if (source_line.has_value()) {
        char line_num[10];
        snprintf(line_num, sizeof(line_num), "%4d", line);
        oss << "\n" << line_num << " | " << source_line.value() << "\n";
        oss << "     | " << std::string(column - 1, ' ') << "^\n";
    }

    if (hint.has_value()) {
        oss << "\nHint: " << hint.value() << "\n";
    }

    oss << std::string(70, '=') << "\n";
    
    formatted_message = oss.str();
}

const char* LexerError::what() const noexcept {
    return formatted_message.c_str();
}

}
