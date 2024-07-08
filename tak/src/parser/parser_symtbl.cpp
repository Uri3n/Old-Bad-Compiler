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

    return nullptr;
}
