//
// Created by Diago on 2024-07-10.
//

#include <parser.hpp>


bool
Parser::type_exists(const std::string& name) {
    return type_table_.contains(name);
}

bool
Parser::create_type(const std::string& name, std::vector<MemberData>&& type_data) {
    if(type_exists(name)) {
        panic(fmt("Internal parse-error: attempt to create type \"{}\" when it already exists.", name));
    }

    type_table_[name] = type_data;
    return true;
}

std::vector<MemberData>*
Parser::lookup_type(const std::string& name) {
    if(!type_exists(name)) {
        panic(fmt("Internal parse-error: looking up nonexistent type \"{}\".", name));
    }

    return &type_table_[name];
}

bool
Parser::create_type_alias(const std::string& name, const TypeData& data) {
    if(type_aliases_.contains(name)) {
        panic(fmt("Internal parse-error: attempt to create type alias \"{}\" when it already exists.", name));
    }

    type_aliases_[name] = data;
    return true;
}

bool
Parser::type_alias_exists(const std::string& name) {
    return type_aliases_.contains(name);
}

TypeData
Parser::lookup_type_alias(const std::string& name) {
    if(!type_aliases_.contains(name)) {
        panic(fmt("Internal parse-error: attempted lookup for non-existent type alias \"{}\".", name));
    }

    return type_aliases_[name];
}
