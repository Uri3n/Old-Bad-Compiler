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


//
// TODO: this doesn't actually work, because substructures can contain the struct and it won't catch it.
//       figure this out later in the postparsing step...
//

static bool
member_already_exists(const tak::UserType* type, const std::string& member_name) {
    const auto mem_exists = std::find_if(type->members.begin(), type->members.end(), [&](const tak::MemberData& member) {
        return member.name == member_name;
    });

    return mem_exists != type->members.end();
}

tak::AstNode*
tak::parse_structdef(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_STRUCT, "Expected \"struct\" keyword.");

    if(parser.tbl_.scope_stack_.size() > 1) {
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

    const size_t   begin_pos  = lxr.current().src_pos;
    const uint32_t begin_line = lxr.current().line;
    const auto     type_name  = parser.tbl_.namespace_as_string() + std::string(lxr.current().value);
    UserType*      replace    = nullptr;

    if(parser.tbl_.type_exists(type_name)) {
        replace = parser.tbl_.lookup_type(type_name);
        assert(replace != nullptr);
        if(replace->flags & ENTITY_PLACEHOLDER) {
            replace->flags &= ~ENTITY_PLACEHOLDER;
        } else {
            lxr.raise_error("Naming conflict: this type already exists.");
            return nullptr;
        }
    }

    if(parser.tbl_.type_alias_exists(type_name)) {
        lxr.raise_error("Naming conflict: a type alias already has this name.");
        return nullptr;
    }


    //
    // We need to create the type and then get a pointer to its member list AFTER and modify it.
    // This is because members may refer back to the struct sometimes. Like if a member was
    // foo^, where foo is the struct.
    //

    if(replace == nullptr && !parser.tbl_.create_type(type_name, std::vector<MemberData>())) {
        return nullptr;
    }


    //
    // Parse generic parameters if they exist.
    //

    UserType* new_type = replace == nullptr ? parser.tbl_.lookup_type(type_name) : replace;
    new_type->file = lxr.source_file_name_;
    new_type->pos  = begin_pos;
    new_type->line = begin_line;

    lxr.advance(1);
    if(lxr.current() == TOKEN_LSQUARE_BRACKET) {
        lxr.advance(1);
        while(lxr.current() != TOKEN_RSQUARE_BRACKET) {
            if(lxr.current() != TOKEN_IDENTIFIER) {
                lxr.raise_error("Expected generic parameter name (e.g. 'T').");
                return nullptr;
            }

            new_type->generic_type_names.emplace_back(std::string(lxr.current().value));
            parser.tbl_.create_placeholder_type(
                std::string(lxr.current().value), lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

            lxr.advance(1);
            if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
                lxr.advance(1);
            }
        }

        lxr.advance(1);
    }


    //
    // Parse out each struct member.
    //

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Unexpected token.");
    }

    lxr.advance(1);
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

        else if(const auto type = parse_type(parser, lxr)) {
            if(is_type_invalid_as_member(type_name, *type)) {
                lxr.raise_error("Invalid type for a struct member.", curr_pos, line);
                return nullptr;
            }

            if(member_already_exists(new_type, name)) {
                lxr.raise_error("Member with this name already exists.", curr_pos, line);
                return nullptr;
            }

            new_type->members.emplace_back(MemberData(name, *type));
            new_type->members.back().type.flags |= is_const ? TYPE_CONSTANT | TYPE_DEFAULT_INIT : TYPE_DEFAULT_INIT;
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

    auto* node = new AstStructdef(begin_pos, begin_line, lxr.source_file_name_);
    node->name = type_name;

    for(const auto& generic : new_type->generic_type_names) {
        parser.tbl_.delete_type(generic);
    }

    lxr.advance(1);
    return node;
}