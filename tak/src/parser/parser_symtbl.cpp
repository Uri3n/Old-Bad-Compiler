//
// Created by Diago on 2024-07-04.
//

#include <parser.hpp>

void
parser::pop_scope() {
    if(scope_stack.empty()) {
        return;
    }

    scope_stack.pop_back();
}


bool
parser::scoped_symbol_exists(const std::string &name) {
    if(scope_stack.empty()) {
        return false;
    }

    for(int32_t i = static_cast<int32_t>(scope_stack.size()) - 1; i >= 0; i--) {
        if(scope_stack[i].contains(name)) {
            return true;
        }
    }

    return false;
}


bool
parser::scoped_symbol_exists_at_current_scope(const std::string& name) {
    if(scope_stack.empty()) {
        return false;
    }

    return scope_stack.back().contains(name);
}


void
parser::push_scope() {
    scope_stack.emplace_back(std::unordered_map<std::string, uint32_t>());
}


symbol*
parser::create_symbol(
        const std::string& name,
        const size_t       src_index,
        const uint32_t     line_number,
        const sym_t        sym_type,
        const uint16_t     sym_flags,
        const std::optional<type_data>& data
    ) {

    if(scope_stack.empty()) {
        print("parse-error: call to create_symbol with no symbol stack.");
        return nullptr;
    }

    if(scope_stack.back().contains(name)) {
        print("parse-error: could not create symbol {} because it already exists in this scope.", name);
        return nullptr;
    }


    ++curr_sym_index;
    scope_stack.back()[name] = curr_sym_index;

    auto& sym = sym_table[curr_sym_index]; // This should get back a default-constructed type.
    if(data) {
        sym.type = *data;
    }

    sym.type.flags   |= sym_flags;
    sym.type.sym_type = sym_type;
    sym.line_number   = line_number;
    sym.src_pos       = src_index;
    sym.symbol_index  = curr_sym_index;
    sym.name          = name;

    return &sym;
}


uint32_t
parser::lookup_scoped_symbol(const std::string& name) {
    if(scope_stack.empty()) {
        return INVALID_SYMBOL_INDEX;
    }

    for(int32_t i = static_cast<int32_t>(scope_stack.size()) - 1; i >= 0; i--) {
        if(scope_stack[i].contains(name)) {
            return scope_stack[i][name];
        }
    }

    return INVALID_SYMBOL_INDEX;
}


symbol*
parser::lookup_unique_symbol(const uint32_t symbol_index) {
    if(sym_table.contains(symbol_index)) {
        return &sym_table[symbol_index];
    }

    print("Internal parse-error: failed to lookup unique symbol with index {}", symbol_index);
    return nullptr;
}


std::string
format_type_data(const type_data& type) {

    static constexpr std::string_view fmt_type =
        "\n - Symbol Type:   {}"
        "\n - Flags:         {}"
        "\n - Pointer Depth: {}"
        "\n - Array Length:  {}"
        "\n - Type Name:     {}"
        "\n - Return Type:   {}"
        "\n - Parameters:    {}";


    std::string flags;
    if(type.flags & SYM_IS_CONSTANT) {
        flags += "CONSTANT | ";
    } if(type.flags & SYM_IS_FOREIGN) {
        flags += "FOREIGN | ";
    } if(type.flags & SYM_IS_POINTER) {
        flags += "POINTER | ";
    }  if(type.flags & SYM_IS_GLOBAL) {
        flags += "GLOBAL | ";
    } if(type.flags & SYM_IS_ARRAY) {
        flags += "ARRAY | ";
    } if(type.flags & SYM_IS_PROCARG) {
        flags += "PROCEDURE_ARGUMENT | ";
    } if(type.flags & SYM_DEFAULT_INITIALIZED) {
        flags += "DEFAULT INITIALIZED";
    }

    if(flags.size() >= 2 && flags[flags.size()-2] == '|') {
        flags.erase(flags.size()-2);
    }


    std::string sym_t_str;
    if(type.sym_type == SYM_PROCEDURE) {
        sym_t_str = "Procedure";
    } else if(type.sym_type == SYM_VARIABLE) {
        sym_t_str = "Variable";
    } else if(type.sym_type == SYM_STRUCT) {
        sym_t_str = "Struct";
    }


    std::string type_name_str;
    if(auto is_var = std::get_if<var_t>(&type.name)) {
        type_name_str = var_t_to_string(*is_var);
    } else if(auto is_struct = std::get_if<std::string>(&type.name)) {
        type_name_str = fmt("{} (User Defined Struct)", *is_struct);
    } else {
        type_name_str = "Procedure";
    }


    std::string return_type_data;
    std::string param_data;

    if(type.return_type != nullptr) {
        return_type_data = "\n~~ Return Type:\n";
        return_type_data += format_type_data(*type.return_type);
    } else {
        return_type_data = "N/A";
    }

    if(type.parameters != nullptr) {
        param_data = "\n~~ Parameters:\n";
        for(const auto& param : *type.parameters) {
            param_data += format_type_data(param);
        }
    }


    return fmt(fmt_type,
        sym_t_str,
        flags,
        type.pointer_depth,
        type.array_length,
        type_name_str,
        return_type_data,
        param_data
    );
}

void
parser::dump_symbols() {

    static constexpr std::string_view fmt_sym =
        "~ {} ~"
        "\n - Symbol Index:  {}"
        "\n - Line Number:   {}"
        "\n - File Position: {}"
        "{}"; //< type data


    for(const auto &[index, sym] : sym_table) {

        print(fmt_sym,
            sym.name,
            sym.symbol_index,
            sym.line_number,
            sym.src_pos,
            format_type_data(sym.type)
        );

    }
}

