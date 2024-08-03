//
// Created by Diago on 2024-07-04.
//

#include <parser.hpp>


void
tak::Parser::pop_scope() {
    if(scope_stack_.empty()) {
        return;
    }

    scope_stack_.pop_back();
}

bool
tak::Parser::scoped_symbol_exists(const std::string &name) {
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
tak::Parser::scoped_symbol_exists_at_current_scope(const std::string& name) {
    if(scope_stack_.empty()) {
        return false;
    }

    return scope_stack_.back().contains(name);
}

void
tak::Parser::push_scope() {
    scope_stack_.emplace_back(std::unordered_map<std::string, uint32_t>());
}

uint32_t
tak::Parser::create_placeholder_symbol(const std::string& name, const size_t src_index, const uint32_t line_number) {

    assert(!scope_stack_.empty());
    assert(!scoped_symbol_exists(name));

    ++curr_sym_index_;
    scope_stack_.front()[name] = curr_sym_index_;

    auto& sym        = sym_table_[curr_sym_index_];
    sym.name         = name;
    sym.placeholder  = true;
    sym.symbol_index = curr_sym_index_;
    sym.src_pos      = src_index;
    sym.line_number  = line_number;

    return curr_sym_index_;
}

tak::Symbol*
tak::Parser::create_symbol(
    const std::string& name,
    const size_t src_index,
    const uint32_t line_number,
    const type_kind_t sym_type,
    const uint64_t sym_flags,
    const std::optional<TypeData>& data
) {

    assert(!scope_stack_.empty());
    assert(!scope_stack_.back().contains(name));

    ++curr_sym_index_;
    scope_stack_.back()[name] = curr_sym_index_;

    auto& sym = sym_table_[curr_sym_index_]; // This should get back a default-constructed type.
    if(data) {
        sym.type = *data;
    }

    sym.type.flags   |= sym_flags;
    sym.type.kind     = sym_type;
    sym.line_number   = line_number;
    sym.src_pos       = src_index;
    sym.symbol_index  = curr_sym_index_;
    sym.name          = name;

    if(sym_type == TYPE_KIND_PROCEDURE) {
        sym.type.name = std::monostate(); // being lazy and setting this here.
    }

    return &sym;
}


uint32_t
tak::Parser::lookup_scoped_symbol(const std::string& name) {
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


tak::Symbol*
tak::Parser::lookup_unique_symbol(const uint32_t symbol_index) {
    if(sym_table_.contains(symbol_index)) {
        return &sym_table_[symbol_index];
    }

    panic(fmt("Internal parse-error: failed to lookup unique symbol with index {}", symbol_index));
}




