#ifndef BOX_CODEGEN_ERROR_HPP
#define BOX_CODEGEN_ERROR_HPP

#include "../lexer/token.hpp"
#include <stdexcept>
#include <string>
#include <optional>

namespace box {

class CodeGenError : public std::runtime_error {
public:
    std::string message;
    std::optional<Token> token;
    std::optional<std::string> hint;

    explicit CodeGenError(const std::string& msg,
                         const std::optional<Token>& tok = std::nullopt,
                         const std::optional<std::string>& h = std::nullopt);

private:
    static std::string format_error(const std::string& msg,
                                    const std::optional<Token>& tok,
                                    const std::optional<std::string>& hint);
};

}

#endif
