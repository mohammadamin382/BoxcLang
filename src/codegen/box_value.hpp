#ifndef BOX_CODEGEN_BOX_VALUE_HPP
#define BOX_CODEGEN_BOX_VALUE_HPP

#include <llvm/IR/Value.h>
#include <string>
#include <optional>

namespace box {

class BoxType {
public:
    static constexpr const char* NUMBER = "number";
    static constexpr const char* STRING = "string";
    static constexpr const char* BOOL = "bool";
    static constexpr const char* NIL = "nil";
    static constexpr const char* ARRAY = "array";
    static constexpr const char* DICT = "dict";
    static constexpr const char* FUNCTION = "function";
    static constexpr const char* POINTER = "pointer";
};

class BoxValue {
public:
    llvm::Value* ir_value;
    std::string box_type;
    bool is_mutable;
    std::optional<std::string> element_type;
    std::optional<std::string> value_type;

    BoxValue() : ir_value(nullptr), box_type(""), is_mutable(false) {}
    
    BoxValue(llvm::Value* value, const std::string& type, bool mutable_val = true,
             const std::optional<std::string>& elem_type = std::nullopt,
             const std::optional<std::string>& val_type = std::nullopt);

    std::string to_string() const;
};

}

#endif
