#ifndef BOX_CODEGEN_ENVIRONMENT_HPP
#define BOX_CODEGEN_ENVIRONMENT_HPP

#include "box_value.hpp"
#include <unordered_map>
#include <memory>
#include <optional>

namespace box {

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> enclosing = nullptr);

    void define(const std::string& name, const BoxValue& value);
    std::optional<BoxValue> get(const std::string& name) const;
    bool assign(const std::string& name, const BoxValue& value);
    bool exists_in_current_scope(const std::string& name) const;

private:
    std::unordered_map<std::string, BoxValue> values;
    std::shared_ptr<Environment> enclosing;
};

}

#endif
