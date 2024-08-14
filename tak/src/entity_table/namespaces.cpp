//
// Created by Diago on 2024-08-11.
//

#include <entity_table.hpp>


bool
tak::EntityTable::enter_namespace(const std::string& name) {
    for(const auto& _namespace : namespace_stack_)
        if(_namespace == name) return false;

    namespace_stack_.emplace_back(name);
    return true;
}

void
tak::EntityTable::leave_namespace() {
    if(namespace_stack_.empty())
        return;

    namespace_stack_.pop_back();
}

bool
tak::EntityTable::namespace_exists(const std::string& name) {
    for(const auto& _namespace : namespace_stack_)
        if(_namespace == name) return true;

    return false;
}

std::string
tak::EntityTable::get_canonical_name(std::string name, const bool is_symbol) {

    assert(!name.empty());
    if(name.front() == '\\') {
        return name;
    }

    std::string begin;
    std::string last_exists;
    std::string namespaces = "\\";

    if(const size_t pos = name.find('\\'); pos != std::string::npos) {
        begin = name.substr(0, pos);
    } else {
        begin = name;
    }

    for(const auto& _namespace : namespace_stack_) {

        std::string actual = namespaces + name;
        if(is_symbol && scoped_symbol_exists(actual)) {
            last_exists = actual;
        } else if(!is_symbol && (type_exists(actual) || type_alias_exists(actual))) {
            last_exists = actual;
        }

        if(_namespace == begin)
            break;

        namespaces += _namespace + '\\';
    }

    std::string actual = namespaces + name;         // being a lazy bitch here idc
    if(is_symbol && scoped_symbol_exists(actual)) {
        last_exists = actual;
    } else if(!is_symbol && (type_exists(actual) || type_alias_exists(actual))) {
        last_exists = actual;
    }

    if(!last_exists.empty()) {
        return last_exists;
    }

    return namespaces + name;
}

std::string
tak::EntityTable::get_canonical_type_name(const std::string& name) {
    if(type_exists(name) || type_alias_exists(name)) return name;
    return get_canonical_name(name, false);
}

std::string
tak::EntityTable::get_canonical_sym_name(const std::string& name) {
    if(scoped_symbol_exists(name)) return name;
    return get_canonical_name(name, true);
}

std::string
tak::EntityTable::namespace_as_string() {
    std::string as_str = "\\";
    for(const auto& _namespace : namespace_stack_)
        as_str += _namespace + '\\';

    return as_str;
}