//
// Created by Diago on 2024-07-10.
//

#include <parser.hpp>


bool
parser::type_exists(const std::string& name) {
    return type_table.contains(name);
}

bool
parser::create_type(const std::string& name, std::vector<member_data>&& type_data) {

    if(type_exists(name)) {
        print("Internal parse-error: attempt to create type \"{}\" when it already exists.", name);
        return false;
    }

    type_table[name] = type_data;
    return true;
}

std::vector<member_data>*
parser::lookup_type(const std::string& name) {

    if(!type_exists(name)) {
        print("Internal parse-error: looking up nonexistent type \"{}\".", name);
        return nullptr;
    }

    return &type_table[name];
}

void
parser::dump_types() {

    if(type_table.empty()) {
        print("No user-defined types exist.");
        return;
    }

    print(" -- USER DEFINED TYPES -- ");
    for(const auto &[name, members] : type_table) {
        print("~ {} ~\n  Members:", name);
        for(size_t i = 0; i < members.size(); ++i) {
            print("    {}. {}", std::to_string(i + 1), members[i].name);
            print("{}", format_type_data(members[i].type));
        }
    }
}
