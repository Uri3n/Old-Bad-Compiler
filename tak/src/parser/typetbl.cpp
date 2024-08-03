//
// Created by Diago on 2024-07-10.
//

#include <parser.hpp>


bool
tak::Parser::type_exists(const std::string& name) {
    return type_table_.contains(name);
}

bool
tak::Parser::create_type(const std::string& name, std::vector<MemberData>&& type_data) {
    assert(!type_exists(name));

    auto& user_t          = type_table_[name];
    user_t.members        = type_data;
    user_t.is_placeholder = false;

    return true;
}

bool
tak::Parser::create_placeholder_type(const std::string& name, const size_t pos, const uint32_t line) {
    assert(!type_exists(name));

    auto& user_t           = type_table_[name];
    user_t.is_placeholder  = true;
    user_t.pos_first_used  = pos;
    user_t.line_first_used = line;
    return true;
}

std::vector<tak::MemberData>*
tak::Parser::lookup_type_members(const std::string& name) {
    assert(type_exists(name));
    return &type_table_[name].members;
}

tak::UserType*
tak::Parser::lookup_type(const std::string& name) {
    assert(type_exists(name));
    return &type_table_[name];
}

bool
tak::Parser::create_type_alias(const std::string& name, const TypeData& data) {
    assert(!type_aliases_.contains(name));
    type_aliases_[name] = data;
    return true;
}

bool
tak::Parser::type_alias_exists(const std::string& name) {
    return type_aliases_.contains(name);
}

tak::TypeData
tak::Parser::lookup_type_alias(const std::string& name) {
    assert(type_aliases_.contains(name));
    return type_aliases_[name];
}
