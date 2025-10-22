#include "codegen.hpp"
#include <llvm/IR/Constants.h>

namespace box {

BoxValue CodeGenerator::visit_call(Call* expr) {
    if (auto* variable = dynamic_cast<Variable*>(expr->callee.get())) {
        std::string builtin_name = variable->name.lexeme;

        if (builtin_name == "malloc") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'malloc' function requires exactly one argument (size in bytes).\n";
                hint += "       Example: var ptr = malloc(40);";
                throw CodeGenError("malloc() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_malloc(expr->arguments[0]);
        }

        else if (builtin_name == "calloc") {
            if (expr->arguments.size() != 2) {
                std::string hint = "The 'calloc' function requires exactly two arguments (count and size).\n";
                hint += "       Example: var ptr = calloc(10, 8);";
                throw CodeGenError("calloc() expects 2 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_calloc(expr->arguments[0], expr->arguments[1]);
        }

        else if (builtin_name == "realloc") {
            if (expr->arguments.size() != 2) {
                std::string hint = "The 'realloc' function requires exactly two arguments (pointer and new size).\n";
                hint += "       Example: var new_ptr = realloc(old_ptr, 80);";
                throw CodeGenError("realloc() expects 2 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_realloc(expr->arguments[0], expr->arguments[1]);
        }

        else if (builtin_name == "free") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'free' function requires exactly one argument (pointer to free).\n";
                hint += "       Example: free(ptr);";
                throw CodeGenError("free() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_free(expr->arguments[0]);
        }

        else if (builtin_name == "addr_of") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'addr_of' function requires exactly one argument (variable).\n";
                hint += "       Example: var ptr = addr_of(myvar);";
                throw CodeGenError("addr_of() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_addr_of(expr->arguments[0]);
        }

        else if (builtin_name == "deref") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'deref' function requires exactly one argument (pointer).\n";
                hint += "       Example: var value = deref(ptr);";
                throw CodeGenError("deref() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_deref(expr->arguments[0]);
        }

        else if (builtin_name == "input") {
            if (expr->arguments.size() != 0) {
                std::string hint = "The 'input' function takes no arguments.\n";
                hint += "       Example: var name = input();";
                throw CodeGenError("input() expects 0 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_input();
        }

        else if (builtin_name == "input_num") {
            if (expr->arguments.size() != 0) {
                std::string hint = "The 'input_num' function takes no arguments.\n";
                hint += "       Example: var age = input_num();";
                throw CodeGenError("input_num() expects 0 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            return builtin_input_num();
        }

        else if (builtin_name == "read_file") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'read_file' function requires exactly one argument (file path).\n";
                hint += "       Example: var content = read_file(\"data.txt\");";
                throw CodeGenError("read_file() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            BoxValue path_arg = visit_expr(expr->arguments[0]);
            if (path_arg.box_type != BoxType::STRING) {
                std::string hint = "File path must be a string.\n";
                hint += "       Argument type: " + path_arg.box_type;
                throw CodeGenError("read_file() requires a string argument", expr->paren, hint);
            }
            return builtin_read_file(path_arg);
        }

        else if (builtin_name == "write_file") {
            if (expr->arguments.size() != 2) {
                std::string hint = "The 'write_file' function requires exactly two arguments (path, content).\n";
                hint += "       Example: write_file(\"output.txt\", \"Hello\");";
                throw CodeGenError("write_file() expects 2 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            BoxValue path_arg = visit_expr(expr->arguments[0]);
            BoxValue content_arg = visit_expr(expr->arguments[1]);
            
            if (path_arg.box_type != BoxType::STRING) {
                std::string hint = "File path must be a string.\n";
                hint += "       Path type: " + path_arg.box_type;
                throw CodeGenError("write_file() requires string path", expr->paren, hint);
            }
            
            if (content_arg.box_type != BoxType::STRING) {
                std::string hint = "File content must be a string.\n";
                hint += "       Content type: " + content_arg.box_type;
                throw CodeGenError("write_file() requires string content", expr->paren, hint);
            }
            return builtin_write_file(path_arg, content_arg);
        }

        else if (builtin_name == "append_file") {
            if (expr->arguments.size() != 2) {
                std::string hint = "The 'append_file' function requires exactly two arguments (path, content).\n";
                hint += "       Example: append_file(\"log.txt\", \"New entry\");";
                throw CodeGenError("append_file() expects 2 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            BoxValue path_arg = visit_expr(expr->arguments[0]);
            BoxValue content_arg = visit_expr(expr->arguments[1]);
            
            if (path_arg.box_type != BoxType::STRING) {
                std::string hint = "File path must be a string.\n";
                hint += "       Path type: " + path_arg.box_type;
                throw CodeGenError("append_file() requires string path", expr->paren, hint);
            }
            
            if (content_arg.box_type != BoxType::STRING) {
                std::string hint = "File content must be a string.\n";
                hint += "       Content type: " + content_arg.box_type;
                throw CodeGenError("append_file() requires string content", expr->paren, hint);
            }
            return builtin_append_file(path_arg, content_arg);
        }

        else if (builtin_name == "file_exists") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'file_exists' function requires exactly one argument (file path).\n";
                hint += "       Example: if (file_exists(\"data.txt\")) { ... }";
                throw CodeGenError("file_exists() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }
            BoxValue path_arg = visit_expr(expr->arguments[0]);
            if (path_arg.box_type != BoxType::STRING) {
                std::string hint = "File path must be a string.\n";
                hint += "       Argument type: " + path_arg.box_type;
                throw CodeGenError("file_exists() requires a string argument", expr->paren, hint);
            }
            return builtin_file_exists(path_arg);
        }

        else if (builtin_name == "len") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'len' function requires exactly one argument.\n";
                hint += "       Example: len(array)";
                throw CodeGenError("len() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }

            BoxValue arg = visit_expr(expr->arguments[0]);
            if (arg.box_type == BoxType::ARRAY || arg.box_type == BoxType::DICT) {
                // For LLVM 17+, use the explicit struct type
                llvm::Type* struct_type = (arg.box_type == BoxType::ARRAY) ? array_struct_type : dict_struct_type;
                llvm::Value* length_ptr = builder->CreateStructGEP(struct_type, arg.ir_value, 0, "length_ptr");
                llvm::Value* length = builder->CreateLoad(i64_type, length_ptr, "length");
                llvm::Value* length_double = builder->CreateSIToFP(length, double_type, "length_as_double");
                return BoxValue(length_double, BoxType::NUMBER);
            } else {
                std::string hint = "The 'len' function works with arrays and dictionaries.\n";
                hint += "       Argument type: " + arg.box_type;
                throw CodeGenError("len() requires an array or dict argument", expr->paren, hint);
            }
        }

        else if (builtin_name == "has") {
            if (expr->arguments.size() != 2) {
                std::string hint = "The 'has' function requires exactly two arguments.\n";
                hint += "       Example: has(dict, key)";
                throw CodeGenError("has() expects 2 arguments but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }

            BoxValue dict_arg = visit_expr(expr->arguments[0]);
            BoxValue key_arg = visit_expr(expr->arguments[1]);

            if (dict_arg.box_type != BoxType::DICT) {
                std::string hint = "The 'has' function requires a dictionary as first argument.\n";
                hint += "       Argument type: " + dict_arg.box_type;
                throw CodeGenError("has() requires a dict as first argument", expr->paren, hint);
            }

            if (key_arg.box_type != BoxType::NUMBER) {
                std::string hint = "Dictionary keys must be numbers.\n";
                hint += "       Key type: " + key_arg.box_type;
                throw CodeGenError("has() requires a number key", expr->paren, hint);
            }

            return dict_has(dict_arg, key_arg);
        }

        else if (builtin_name == "keys") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'keys' function requires exactly one argument.\n";
                hint += "       Example: keys(dict)";
                throw CodeGenError("keys() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }

            BoxValue arg = visit_expr(expr->arguments[0]);
            if (arg.box_type != BoxType::DICT) {
                std::string hint = "The 'keys' function only works with dictionaries.\n";
                hint += "       Argument type: " + arg.box_type;
                throw CodeGenError("keys() requires a dict argument", expr->paren, hint);
            }

            return dict_keys(arg);
        }

        else if (builtin_name == "values") {
            if (expr->arguments.size() != 1) {
                std::string hint = "The 'values' function requires exactly one argument.\n";
                hint += "       Example: values(dict)";
                throw CodeGenError("values() expects 1 argument but got " + std::to_string(expr->arguments.size()), 
                                 expr->paren, hint);
            }

            BoxValue arg = visit_expr(expr->arguments[0]);
            if (arg.box_type != BoxType::DICT) {
                std::string hint = "The 'values' function only works with dictionaries.\n";
                hint += "       Argument type: " + arg.box_type;
                throw CodeGenError("values() requires a dict argument", expr->paren, hint);
            }

            return dict_values(arg);
        }
    }

    if (!dynamic_cast<Variable*>(expr->callee.get())) {
        std::string hint = "Only named functions can be called.\n";
        hint += "       Example: functionName(arg1, arg2)";
        throw CodeGenError("Can only call named functions", expr->paren, hint);
    }

    std::string func_name = dynamic_cast<Variable*>(expr->callee.get())->name.lexeme;

    if (functions.find(func_name) == functions.end()) {
        std::string hint = "Make sure function '" + func_name + "' is declared before calling it.\n";
        hint += "       Example: fun " + func_name + "() { ... }";
        throw CodeGenError("Undefined function '" + func_name + "'", 
                         dynamic_cast<Variable*>(expr->callee.get())->name, hint);
    }

    auto& func_pair = functions[func_name];
    llvm::Function* func = func_pair.first;
    std::vector<std::string>& param_names = func_pair.second;

    if (expr->arguments.size() != param_names.size()) {
        std::string hint = "Function '" + func_name + "' expects " + std::to_string(param_names.size()) + " argument(s).\n";
        hint += "       You provided " + std::to_string(expr->arguments.size()) + " argument(s).";
        throw CodeGenError(
            "Function '" + func_name + "' expects " + std::to_string(param_names.size()) + 
            " arguments but got " + std::to_string(expr->arguments.size()),
            expr->paren,
            hint
        );
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < expr->arguments.size(); i++) {
        BoxValue arg_val = visit_expr(expr->arguments[i]);
        if (arg_val.box_type == BoxType::NUMBER) {
            args.push_back(arg_val.ir_value);
        } else {
            args.push_back(to_number(arg_val));
        }
    }

    llvm::Value* result = builder->CreateCall(func, args, "call");
    return BoxValue(result, BoxType::NUMBER);
}

}

