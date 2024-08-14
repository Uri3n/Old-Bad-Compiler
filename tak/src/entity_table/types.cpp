//
// Created by Diago on 2024-08-11.
//

#include <entity_table.hpp>


bool
tak::EntityTable::type_exists(const std::string& name) {
    return type_table_.contains(name);
}

bool
tak::EntityTable::create_type(const std::string& name, std::vector<MemberData>&& type_data) {
    assert(!type_exists(name));

    type_table_[name] = new UserType();
    auto* user_t      = type_table_[name];
    user_t->members   = type_data;
    user_t->flags     = ENTITY_FLAGS_NONE;

    return true;
}

bool
tak::EntityTable::create_placeholder_type(const std::string& name, const size_t pos, const uint32_t line, const std::string& file) {
    assert(!type_exists(name));

    type_table_[name] = new UserType();
    auto* user_t      = type_table_[name];

    user_t->flags |= ENTITY_PLACEHOLDER;
    user_t->pos   = pos;
    user_t->line  = line;
    user_t->file  = file;
    return true;
}

void
tak::EntityTable::delete_type_alias(const std::string& name) {
    assert(type_alias_exists(name));
    type_aliases_.erase(name);
}

void
tak::EntityTable::delete_type(const std::string& name) {
    assert(type_exists(name));
    delete type_table_[name];
    type_table_.erase(name);
}

std::vector<tak::MemberData>*
tak::EntityTable::lookup_type_members(const std::string& name) {
    assert(type_exists(name));
    return &type_table_[name]->members;
}

tak::UserType*
tak::EntityTable::lookup_type(const std::string& name) {
    assert(type_exists(name));
    return type_table_[name];
}

bool
tak::EntityTable::create_type_alias(const std::string& name, const TypeData& data) {
    assert(!type_aliases_.contains(name));
    type_aliases_[name] = data;
    return true;
}

bool
tak::EntityTable::type_alias_exists(const std::string& name) {
    return type_aliases_.contains(name);
}

tak::TypeData
tak::EntityTable::lookup_type_alias(const std::string& name) {
    assert(type_aliases_.contains(name));
    return type_aliases_[name];
}