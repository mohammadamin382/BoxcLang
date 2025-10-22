#include "codegen_error.hpp"
#include <sstream>

namespace box {

CodeGenError::CodeGenError(const std::string& msg,
                          const std::optional<Token>& tok,
                          const std::optional<std::string>& h)
    : std::runtime_error(format_error(msg, tok, h))
    , message(msg)
    , token(tok)
    , hint(h) {}

std::string CodeGenError::format_error(const std::string& msg,
                                       const std::optional<Token>& tok,
                                       const std::optional<std::string>& hint) {
    std::ostringstream oss;
    oss << "\n" << std::string(70, '=') << "\n";
    
    if (tok.has_value()) {
        oss << "CODEGEN ERROR at Line " << tok->line << ", Column " << tok->column << "\n";
    } else {
        oss << "CODEGEN ERROR\n";
    }
    
    oss << std::string(70, '=') << "\n";
    oss << "Error: " << msg << "\n";
    
    if (hint.has_value()) {
        oss << "\nHint: " << hint.value() << "\n";
    }
    
    oss << std::string(70, '=') << "\n";
    
    return oss.str();
}

}
