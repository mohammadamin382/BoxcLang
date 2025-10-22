#include "box_value.hpp"
#include <sstream>

namespace box {

BoxValue::BoxValue(llvm::Value* value, const std::string& type, bool mutable_val,
                   const std::optional<std::string>& elem_type,
                   const std::optional<std::string>& val_type)
    : ir_value(value)
    , box_type(type)
    , is_mutable(mutable_val)
    , element_type(elem_type)
    , value_type(val_type) {}

std::string BoxValue::to_string() const {
    std::ostringstream oss;
    if (box_type == BoxType::ARRAY) {
        oss << "BoxValue(array<" << element_type.value_or("?") << ">, " << ir_value << ")";
    } else if (box_type == BoxType::DICT) {
        oss << "BoxValue(dict<" << element_type.value_or("?") 
            << ", " << value_type.value_or("?") << ">, " << ir_value << ")";
    } else {
        oss << "BoxValue(" << box_type << ", " << ir_value << ")";
    }
    return oss.str();
}

}
