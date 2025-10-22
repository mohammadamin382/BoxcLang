#include "environment.hpp"
#include "codegen_error.hpp"

namespace box {

Environment::Environment(std::shared_ptr<Environment> enclosing)
    : enclosing(enclosing) {}

void Environment::define(const std::string& name, const BoxValue& value) {
    if (values.find(name) != values.end()) {
        throw std::runtime_error("Variable '" + name + "' already defined in current scope");
    }
    values.emplace(name, value);
}

std::optional<BoxValue> Environment::get(const std::string& name) const {
    auto it = values.find(name);
    if (it != values.end()) {
        return it->second;
    }
    if (enclosing != nullptr) {
        return enclosing->get(name);
    }
    return std::nullopt;
}

bool Environment::assign(const std::string& name, const BoxValue& value) {
    auto it = values.find(name);
    if (it != values.end()) {
        if (!it->second.is_mutable) {
            throw std::runtime_error("Cannot assign to immutable variable '" + name + "'");
        }
        it->second = value;
        return true;
    }
    if (enclosing != nullptr) {
        return enclosing->assign(name, value);
    }
    return false;
}

bool Environment::exists_in_current_scope(const std::string& name) const {
    return values.find(name) != values.end();
}

}
