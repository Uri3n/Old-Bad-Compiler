//
// Created by Diago on 2024-07-04.
//

#include <parser.hpp>

void
parser::pop_scope() {
    if(scope_stack_.empty()) {
        return;
    }

    scope_stack_.pop_back();
}


bool
parser::scoped_symbol_exists(const std::string &name) {
    if(scope_stack_.empty()) {
        return false;
    }

    for(int32_t i = static_cast<int32_t>(scope_stack_.size()) - 1; i >= 0; i--) {
        if(scope_stack_[i].contains(name)) {
            return true;
        }
    }

    return false;
}


bool
parser::scoped_symbol_exists_at_current_scope(const std::string& name) {
    if(scope_stack_.empty()) {
        return false;
    }

    return scope_stack_.back().contains(name);
}


void
parser::push_scope() {
    scope_stack_.emplace_back(std::unordered_map<std::string, uint32_t>());
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


    if(scope_stack_.empty()) {
        print("parse-error: call to create_symbol with no symbol stack.");
        return nullptr;
    }

    if(scope_stack_.back().contains(name)) {
        print("parse-error: could not create symbol {} because it already exists in this scope.", name);
        return nullptr;
    }


    ++curr_sym_index_;
    scope_stack_.back()[name] = curr_sym_index_;

    auto& sym = sym_table_[curr_sym_index_]; // This should get back a default-constructed type.
    if(data) {
        sym.type = *data;
    }


    sym.type.flags   |= sym_flags;
    sym.type.sym_type = sym_type;
    sym.line_number   = line_number;
    sym.src_pos       = src_index;
    sym.symbol_index  = curr_sym_index_;
    sym.name          = name;

    if(sym_type == SYM_PROCEDURE) {
        sym.type.name = std::monostate(); // being lazy and setting this here.
    }

    return &sym;
}


uint32_t
parser::lookup_scoped_symbol(const std::string& name) {
    if(scope_stack_.empty()) {
        return INVALID_SYMBOL_INDEX;
    }

    for(int32_t i = static_cast<int32_t>(scope_stack_.size()) - 1; i >= 0; i--) {
        if(scope_stack_[i].contains(name)) {
            return scope_stack_[i][name];
        }
    }

    return INVALID_SYMBOL_INDEX;
}


symbol*
parser::lookup_unique_symbol(const uint32_t symbol_index) {
    if(sym_table_.contains(symbol_index)) {
        return &sym_table_[symbol_index];
    }

    print("Internal parse-error: failed to lookup unique symbol with index {}", symbol_index);
    return nullptr;
}




