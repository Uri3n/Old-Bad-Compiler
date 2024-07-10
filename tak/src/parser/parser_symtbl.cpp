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
        const uint16_t     sym_flags
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

    if(sym_type == SYM_VARIABLE) {
        sym_table[curr_sym_index] = std::make_unique<variable>();
    } else {
        sym_table[curr_sym_index] = std::make_unique<procedure>();
    }


    auto& sym          = sym_table[curr_sym_index];
    sym->flags         = sym_flags;
    sym->sym_type      = sym_type;
    sym->line_number   = line_number;
    sym->src_pos       = src_index;
    sym->symbol_index  = curr_sym_index;
    sym->name          = name;

    return sym.get();
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
        auto& ref = sym_table[symbol_index];
        return ref.get();
    }

    print("Internal parse-error: failed to lookup unique symbol with index {}", symbol_index);
    return nullptr;
}

void
parser::dump_symbols() {

    static constexpr std::string_view fmt_sym =
        "~ {} ~"
        "\n - Symbol Index:  {}"
        "\n - Line Number:   {}"
        "\n - File Position: {}"
        "\n - Symbol Type:   {}"
        "\n - Flags:         {}"
        "\n - Pointer Depth: {}"
        "\n - Array Length:  {}";

    static constexpr std::string_view fmt_proc =
        " - Parameter List: {}"
        "\n - Return Type:    {}";

    static constexpr std::string_view fmt_var =
        " - Variable Type: {}";


    for(const auto &[index, sym] : sym_table) {

        std::string flags;
        if(sym->flags & SYM_IS_CONSTANT) {
            flags += "CONSTANT | ";
        } if(sym->flags & SYM_IS_FOREIGN) {
            flags += "FOREIGN | ";
        } if(sym->flags & SYM_IS_POINTER) {
            flags += "POINTER | ";
        }  if(sym->flags & SYM_IS_GLOBAL) {
            flags += "GLOBAL | ";
        } if(sym->flags & SYM_IS_ARRAY) {
            flags += "ARRAY | ";
        } if(sym->flags & SYM_IS_PROCARG) {
            flags += "PROCEDURE_ARGUMENT | ";
        } if(sym->flags & SYM_DEFAULT_INITIALIZED) {
            flags += "DEFAULT INITIALIZED";
        }

        if(flags.size() >= 2 && flags[flags.size()-2] == '|') {
            flags.erase(flags.size()-2);
        }


        print(fmt_sym,
            sym->name,
            sym->symbol_index,
            sym->line_number,
            sym->src_pos,
            sym->sym_type == SYM_PROCEDURE ? "Procedure" : "Variable",
            flags.empty() ? std::string("None") : flags,
            sym->pointer_depth,
            sym->array_length
        );


        if(const auto* var_ptr = dynamic_cast<variable*>(sym.get())) {
            print(fmt_var, var_t_to_string(var_ptr->variable_type));
        }

        else if(const auto* proc_ptr = dynamic_cast<procedure*>(sym.get())) {
            std::string s_paramlist;

            for(const auto& param_t : proc_ptr->parameter_list) {
                s_paramlist += var_t_to_string(param_t) + ',';
            }

            if(!s_paramlist.empty() && s_paramlist.back() == ',') {
                s_paramlist.pop_back();
            }

            print(fmt_proc,
                s_paramlist.empty() ? std::string("None") : s_paramlist,
                var_t_to_string(proc_ptr->return_type)
            );
        }

        else {
            print(" -- WARNING: SYMBOL BEING STORED AS BASE CLASS --"); // This should never happen...
        }

        std::cout << std::endl;
    }
}

