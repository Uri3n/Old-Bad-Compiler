//
// Created by Diago on 2024-07-20.
//

#include <parser.hpp>


ast_node*
parse_type_alias(parser& parser, lexer& lxr) {

    parser_assert(lxr.current().value == "alias", "Expected \"@alias\" directive.");

    if(parser.scope_stack_.size() > 1) {
        lxr.raise_error("Type alias definition at non-global scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected alias name.");
        return nullptr;
    }


    auto* node   = new ast_type_alias();
    bool  state  = false;
    node->line   = lxr.current().line;

    defer_if(!state, [&] {
       delete node;
    });


    node->name = parser.namespace_as_string() + std::string(lxr.current().value);
    if(parser.type_alias_exists(node->name) || parser.type_exists(node->name)) {
        lxr.raise_error("Type or type alias already exists within this namespace.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_VALUE_ASSIGNMENT) {
        lxr.raise_error("Expected '=' after type alias name.");
        return nullptr;
    }

    lxr.advance(2);
    if(lxr.current() != TOKEN_IDENTIFIER && lxr.current().kind != KIND_TYPE_IDENTIFIER) {
        lxr.raise_error("Expected type identifier.");
        return nullptr;
    }


    if(const auto type = parse_type(parser,lxr)) { // create the type alias.
        parser.create_type_alias(node->name, *type);
        state = true;
        return node;
    }

    return nullptr;
}


ast_node*
parse_compiler_directive(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_AT, "Expected '@'.");
    lxr.advance(1);

    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected directive name.");
    }

    if(lxr.current().value == "alias") return parse_type_alias(parser, lxr);

    //
    // Nothing else here for now.
    //

    lxr.raise_error("Invalid compiler directive.");
    return nullptr;
}