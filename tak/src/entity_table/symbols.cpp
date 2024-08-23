//
// Created by Diago on 2024-08-11.
//

#include <entity_table.hpp>


uint32_t
tak::EntityTable::create_placeholder_symbol(
    const std::string& name,
    const std::string& file,
    const size_t src_index,
    const uint32_t line_number
) {

    assert(!scope_stack_.empty());
    assert(!scoped_symbol_exists(name));

    ++curr_sym_index_;

    scope_stack_.front()[name]  = curr_sym_index_;
    sym_table_[curr_sym_index_] = new Symbol();
    auto* sym                   = sym_table_[curr_sym_index_];

    sym->name         = name;
    sym->flags        = ENTITY_PLACEHOLDER;
    sym->symbol_index = curr_sym_index_;
    sym->src_pos      = src_index;
    sym->line_number  = line_number;
    sym->file         = file;
    sym->_namespace   = namespace_as_string();

    return curr_sym_index_;
}


tak::Symbol*
tak::EntityTable::create_symbol(
    const std::string& name,
    const std::string& file,
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

    sym_table_[curr_sym_index_] = new Symbol();
    auto* sym = sym_table_[curr_sym_index_];

    if(data) {
        sym->type = *data;
    }

    sym->type.flags   |= sym_flags;
    sym->type.kind     = sym_type;
    sym->line_number   = line_number;
    sym->src_pos       = src_index;
    sym->symbol_index  = curr_sym_index_;
    sym->name          = name;
    sym->file          = file;
    sym->_namespace    = namespace_as_string();

    if(sym_type == TYPE_KIND_PROCEDURE) {
        sym->type.name = std::monostate(); // being lazy and setting this here.
    }

    return sym;
}


tak::Symbol*
tak::EntityTable::create_generic_proc_permutation(const Symbol* original, std::vector<TypeData>&& generic_params) {

    assert(!scope_stack_.empty());
    assert(original != nullptr);

    const std::string name = [&]() -> std::string {
        std::string _name = "[";
        for(const auto& type : generic_params) {
            _name += TypeData::to_string(type) + ',';
        }

        if(!_name.empty() && _name.back() == ',') {
            _name.erase(_name.size()-1);
        }

        _name += ']';
        return original->name + _name;
    }();

    if(scoped_symbol_exists(name)) {
        return lookup_unique_symbol(lookup_scoped_symbol(name));
    }


    ++curr_sym_index_;
    sym_table_[curr_sym_index_] = new Symbol();
    auto* sym                   = sym_table_[curr_sym_index_];

    sym->name         = name;
    sym->type.name    = std::monostate();
    sym->flags        = ENTITY_GENPERM;
    sym->type.kind    = TYPE_KIND_PROCEDURE;
    sym->src_pos      = original->src_pos;
    sym->line_number  = original->line_number;
    sym->file         = original->file;
    sym->_namespace   = original->_namespace;
    sym->type.sym_ref = original->symbol_index;
    sym->symbol_index = curr_sym_index_;

    sym->type.parameters  = std::make_shared<std::vector<TypeData>>();
    *sym->type.parameters = generic_params;

    scope_stack_.front()[name] = curr_sym_index_;
    return sym;
}

void
tak::EntityTable::delete_unique_symbol(const uint32_t symbol_index) {
    assert(sym_table_.contains(symbol_index));
    delete sym_table_[symbol_index];
    sym_table_.erase(symbol_index);
}

tak::Symbol*
tak::EntityTable::lookup_unique_symbol(const uint32_t symbol_index) {
    if(sym_table_.contains(symbol_index)) {
        return sym_table_[symbol_index];
    }

    panic(fmt("Internal parse-error: failed to lookup unique symbol with index {}", symbol_index));
}