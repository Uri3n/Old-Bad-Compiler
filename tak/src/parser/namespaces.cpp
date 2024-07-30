//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


bool
Parser::enter_namespace(const std::string& name) {
    for(const auto& _namespace : namespace_stack_)
        if(_namespace == name) return false;

    namespace_stack_.emplace_back(name);
    return true;
}

void
Parser::leave_namespace() {
    if(namespace_stack_.empty())
        return;

    namespace_stack_.pop_back();
}

bool
Parser::namespace_exists(const std::string& name) {
    for(const auto& _namespace : namespace_stack_)
        if(_namespace == name) return true;

    return false;
}

std::string
Parser::get_canonical_name(const std::string& name) {

    std::string full;
    std::string begin;

    if(const size_t pos = name.find('\\'); pos != std::string::npos) {
        begin = name.substr(0, pos);
    } else {
        begin = name;
    }

    for(const auto& _namespace : namespace_stack_) {
        if(_namespace == begin)
            break;

        full += _namespace + '\\';
    }

    full += name;
    return full;
}

std::string
Parser::get_canonical_type_name(const std::string& name) {
    if(type_exists(name) || type_alias_exists(name))
        return name;

    return get_canonical_name(name);
}

std::string
Parser::get_canonical_sym_name(const std::string& name) {
    if(scoped_symbol_exists(name))
        return name;

    return get_canonical_name(name);
}

std::string
Parser::namespace_as_string() {
    std::string as_str;

    for(const auto& _namespace : namespace_stack_)
        as_str += _namespace + '\\';

    return as_str;
}


AstNode*
parse_namespace(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_NAMESPACE, "Expected \"namespace\" keyword.");
    lxr.advance(1);


    if(parser.scope_stack_.size() > 1) {
        lxr.raise_error("Namespace declaration at non-global scope.");
        return nullptr;
    }

    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected namespace identifier.");
        return nullptr;
    }

    if(!parser.enter_namespace(std::string(lxr.current().value))) { // Duplicate namespace.
        lxr.raise_error("Nested namespace has the same name as a parent.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (Beginning of namespace block).");
        return nullptr;
    }


    bool  state     = false;
    auto* node      = new AstNamespaceDecl();
    node->pos       = lxr.current().src_pos;
    node->full_path = parser.namespace_as_string();

    defer([&] {
        if(!state) { delete node; }
        parser.leave_namespace();
    });


    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        node->children.emplace_back(parse_expression(parser, lxr, false));
        if(node->children.back() == nullptr)
            return nullptr;

        if(!VALID_AT_TOPLEVEL(node->children.back()->type)) {
            lxr.raise_error("Expression is invalid as a toplevel statement.", curr_pos, line);
            return nullptr;
        }

        node->children.back()->parent = node;
    }


    lxr.advance(1);
    state = true;
    return node;
}
