//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


bool
parser::enter_namespace(const std::string& name) {
    for(const auto& _namespace : namespace_stack)
        if(_namespace == name) return false;

    namespace_stack.emplace_back(name);
    return true;
}

void
parser::leave_namespace() {
    if(namespace_stack.empty())
        return;

    namespace_stack.pop_back();
}

bool
parser::namespace_exists(const std::string& name) {
    for(const auto& _namespace : namespace_stack)
        if(_namespace == name) return true;

    return false;
}

std::string
parser::get_canonical_name(const std::string& name) {

    std::string full = name;
    std::string begin;

    if(const size_t pos = name.find('\\'); pos != std::string::npos) {
        begin = name.substr(0, pos);
    } else {
        begin = name;
    }

    for(const auto& _namespace : namespace_stack) {
        if(_namespace == begin)
            break;

        full.insert(0, _namespace + '\\');
    }

    return full;
}

std::string
parser::get_canonical_type_name(const std::string& name) {
    if(type_exists(name))
        return name;

    return get_canonical_name(name);
}

std::string
parser::get_canonical_sym_name(const std::string& name) {
    if(scoped_symbol_exists(name))
        return name;

    return get_canonical_name(name);
}

std::string
parser::namespace_as_string() {
    std::string as_str;

    for(const auto& _namespace : namespace_stack)
        as_str += _namespace + '\\';

    return as_str;
}
