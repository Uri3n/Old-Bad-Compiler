//
// Created by Diago on 2024-07-10.
//

#include <parser.hpp>


bool
parser::type_exists(const std::string& name) {
    return type_table_.contains(name);
}

bool
parser::create_type(const std::string& name, std::vector<member_data>&& type_data) {
    if(type_exists(name)) {
        print("Internal parse-error: attempt to create type \"{}\" when it already exists.", name);
        return false;
    }

    type_table_[name] = type_data;
    return true;
}

std::vector<member_data>*
parser::lookup_type(const std::string& name) {
    if(!type_exists(name)) {
        print("Internal parse-error: looking up nonexistent type \"{}\".", name);
        return nullptr;
    }

    return &type_table_[name];
}

bool
parser::create_type_alias(const std::string& name, const type_data& data) {
    if(type_aliases_.contains(name)) {
        print("Internal parse-error: attempt to create type alias \"{}\" when it already exists.", name);
        return false;
    }

    type_aliases_[name] = data;
    return true;
}

bool
parser::type_alias_exists(const std::string& name) {
    return type_aliases_.contains(name);
}

type_data
parser::lookup_type_alias(const std::string& name) {
    if(!type_aliases_.contains(name)) {
        print("Internal parse-error: attempted lookup for non-existent type alias \"{}\".", name);
    }

    return type_aliases_[name];
}
