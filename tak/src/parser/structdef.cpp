//
// Created by Diago on 2024-07-21.
//

#include <parser.hpp>


ast_node*
parse_structdef(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_STRUCT, "Expected \"struct\" keyword.");

    if(parser.scope_stack_.size() > 1) {
        lxr.raise_error("Struct definition at non-global scope.");
        return nullptr;
    }


    //
    // Check for type redeclaration
    //

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected struct name.");
        return nullptr;
    }

    const auto type_name = parser.namespace_as_string() + std::string(lxr.current().value);

    if(parser.type_exists(type_name) || parser.type_alias_exists(type_name)) {
        lxr.raise_error("Naming conflict: type or type alias has already been defined elsewhere.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Unexpected token.");
    }


    //
    // We need to create the type and then get a pointer to its member list AFTER and modify it.
    // This is because members may refer back to the struct sometimes. Like if a member was
    // foo^, where foo is the struct.
    //

    if(!parser.create_type(type_name, std::vector<member_data>())) {
        return nullptr;
    }


    //
    // Parse out each struct member
    //

    lxr.advance(2);
    std::vector<member_data>* members = parser.lookup_type(type_name);

    while(lxr.current() != TOKEN_RBRACE) {

        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected identifier.");
            return nullptr;
        }


        auto name     = std::string(lxr.current().value);
        bool is_const = false;

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;


        lxr.advance(1);
        if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
            is_const = true;
        } else if(lxr.current() != TOKEN_TYPE_ASSIGNMENT) {
            lxr.raise_error("Expected type assignment.");
            return nullptr;
        }

        lxr.advance(1);
        if(lxr.current().kind != KIND_TYPE_IDENTIFIER && lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected type identifier.");
            return nullptr;
        }

        if(const auto type = parse_type(parser, lxr)) {
            if(type->sym_type == TYPE_KIND_PROCEDURE && type->pointer_depth < 1) {
                lxr.raise_error("Procedures cannot be used as struct members.", curr_pos, line);
                return nullptr;
            }

            if(auto* member_name = std::get_if<std::string>(&type->name)) {
                if(*member_name == type_name && type->pointer_depth < 1) {
                    lxr.raise_error("A struct cannot contain itself.", curr_pos, line);
                    return nullptr;
                }
            }

            members->emplace_back(member_data(name, *type));
            members->back().type.flags |= is_const ? TYPE_IS_CONSTANT | TYPE_DEFAULT_INITIALIZED : TYPE_DEFAULT_INITIALIZED;

        } else {
            return nullptr;
        }


        if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
            lxr.advance(1);
        }
    }


    //
    // create AST node.
    //

    auto* node = new ast_structdef();
    node->pos  = lxr.current().src_pos;
    node->name = type_name;

    lxr.advance(1);
    return node;
}