//
// Created by Diago on 2024-07-21.
//

#include <parser.hpp>


static bool
is_type_invalid_as_member(const std::string& struct_name, const tak::TypeData& type) {

    const auto* name_ptr = std::get_if<std::string>(&type.name);
    if(name_ptr != nullptr && *name_ptr == struct_name) {
        return true;
    }

    for(const uint32_t length : type.array_lengths) {
        if(length == 0) return true; // inferred size
    }

    return type.kind == tak::TYPE_KIND_PROCEDURE && type.pointer_depth < 1;
}

static bool
member_already_exists(const std::vector<tak::MemberData>* members, const std::string& member_name) {
    for(const auto& member : *members) {
        if(member.name == member_name) return true;
    }

    return false;
}


tak::AstNode*
tak::parse_structdef(Parser& parser, Lexer& lxr) {

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
    UserType*  replace   = nullptr;

    if(parser.type_exists(type_name)) {
        replace = parser.lookup_type(type_name);
        assert(replace != nullptr);
        if(replace->is_placeholder) {
            replace->is_placeholder = false;
        } else {
            lxr.raise_error("Naming conflict: this type already exists.");
            return nullptr;
        }
    }

    if(parser.type_alias_exists(type_name)) {
        lxr.raise_error("Naming conflict: a type alias already has this name.");
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

    if(replace == nullptr && !parser.create_type(type_name, std::vector<MemberData>())) {
        return nullptr;
    }


    //
    // Parse out each struct member.
    //

    lxr.advance(2);
    std::vector<MemberData>* members = replace == nullptr ? parser.lookup_type_members(type_name) : &replace->members;

    while(lxr.current() != TOKEN_RBRACE) {

        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected identifier.");
            return nullptr;
        }

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        auto name     = std::string(lxr.current().value);
        bool is_const = false;


        lxr.advance(1);
        if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
            is_const = true;
        } else if(lxr.current() != TOKEN_TYPE_ASSIGNMENT) {
            lxr.raise_error("Expected type assignment.");
            return nullptr;
        }

        lxr.advance(1);
        if(lxr.current().kind != KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
            lxr.raise_error("Expected type identifier.");
            return nullptr;
        }


        if(const auto type = parse_type(parser, lxr)) {
            if(is_type_invalid_as_member(type_name, *type)) {
                lxr.raise_error("Invalid type for a struct member.", curr_pos, line);
                return nullptr;
            }

            if(member_already_exists(members, name)) {
                lxr.raise_error("Member with this name already exists.", curr_pos, line);
                return nullptr;
            }

            members->emplace_back(MemberData(name, *type));
            members->back().type.flags |= is_const ? TYPE_CONSTANT | TYPE_DEFAULT_INIT : TYPE_DEFAULT_INIT;
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

    auto* node = new AstStructdef();
    node->pos  = lxr.current().src_pos;
    node->name = type_name;

    lxr.advance(1);
    return node;
}