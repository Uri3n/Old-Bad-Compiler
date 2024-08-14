//
// Created by Diago on 2024-08-11.
//

#include <entity_table.hpp>


void
tak::EntityTable::push_scope() {
    scope_stack_.emplace_back(std::unordered_map<std::string, uint32_t>());
}

void
tak::EntityTable::pop_scope() {
    if(scope_stack_.empty()) {
        return;
    }

    scope_stack_.pop_back();
}

uint32_t
tak::EntityTable::lookup_scoped_symbol(const std::string& name) {
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

bool
tak::EntityTable::scoped_symbol_exists(const std::string &name) {
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
tak::EntityTable::scoped_symbol_exists_at_current_scope(const std::string& name) {
    if(scope_stack_.empty()) {
        return false;
    }

    return scope_stack_.back().contains(name);
}