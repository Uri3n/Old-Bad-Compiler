//
// Created by Diago on 2024-07-21.
//

#include <parser.hpp>


static bool
type_is_valid_as_enumeration(const type_data& type) {
    if(auto* _var_t = std::get_if<var_t>(&type.name)) {
        return (*_var_t == VAR_I8
            || *_var_t == VAR_U8
            || *_var_t == VAR_I16
            || *_var_t == VAR_U16
            || *_var_t == VAR_I32
            || *_var_t == VAR_U32
            || *_var_t == VAR_I64
            || *_var_t == VAR_U64)
            && (type.array_lengths.empty()
            && type.pointer_depth == 0
            && type.kind == TYPE_KIND_VARIABLE);
    }

    return false;
}


ast_node*
parse_enumdef(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_ENUM, "Expected \"enum\" keyword.");

    if(parser.scope_stack_.size() > 1) {
        lxr.raise_error("Enum definition at non-global scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected enum name.");
    }


    //
    // Create AST node: enum should contain its namespace and alias
    //

    bool state       = false;
    auto* node       = new ast_enumdef();
    node->_namespace = new ast_namespacedecl();
    node->alias      = new ast_type_alias();

    node->_namespace->parent = node;
    node->alias->parent      = node;
    node->alias->name        = parser.namespace_as_string() + std::string(lxr.current().value);

    node->pos               = lxr.current().src_pos;
    node->_namespace->pos   = lxr.current().src_pos;
    node->alias->pos        = lxr.current().src_pos;


    //
    // Enter enum as namespace
    //

    if(parser.namespace_exists(std::string(lxr.current().value))
        || parser.type_alias_exists(node->alias->name)
        || parser.type_exists(node->alias->name)
    ) {
        lxr.raise_error("Naming conflict: a namespace, type alias, or struct has the same name as this enum.");
        delete node;
        return nullptr;
    }

    parser.enter_namespace(std::string(lxr.current().value));
    node->_namespace->full_path = parser.namespace_as_string();

    defer([&] {
        if(!state) { delete node; }
        parser.leave_namespace();
    });


    if(lxr.peek(1) != TOKEN_COMMA && lxr.peek(1) != TOKEN_SEMICOLON) {
        lxr.raise_error("Unexpected token after enum name.");
        return nullptr;
    }

    lxr.advance(2);
    if(lxr.current() != TOKEN_IDENTIFIER && lxr.current().kind != KIND_TYPE_IDENTIFIER) {
        lxr.raise_error("Expected enum type identifier.");
        return nullptr;
    }


    //
    // Use enum name as a type alias
    //

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    auto           type     = parse_type(parser, lxr);

    if(!type) {
        return nullptr;
    }

    if(!type_is_valid_as_enumeration(*type)) {
        lxr.raise_error("Specified type is not valid for an enum.", curr_pos, line);
        return nullptr;
    }

    parser.create_type_alias(node->alias->name, *type);


    //
    // Parse enum members
    //

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{'.");
        return nullptr;
    }


    lxr.advance(1);
    size_t enum_index = 0;

    while(lxr.current() != TOKEN_RBRACE){

        //
        // Get member name
        //

        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected identifier.");
        }

        auto member_name = parser.namespace_as_string() + std::string(lxr.current().value);
        if(parser.scoped_symbol_exists_at_current_scope(member_name)) {
            lxr.raise_error("Redeclaration of enum member.");
            return nullptr;
        }

        if(parser.namespace_exists(std::string(lxr.current().value))) {
            lxr.raise_error("Enum member has the same name as a namespace it is declared in.");
            return nullptr;
        }


        //
        // Create a symbol for the enum member.
        //

        auto* sym        = parser.create_symbol(member_name, lxr.current().src_pos, lxr.current().line, TYPE_KIND_VARIABLE, TYPE_FLAGS_NONE, *type);
        auto* decl       = new ast_vardecl();
        decl->identifier = new ast_identifier();
        decl->pos        = lxr.current().src_pos;

        sym->type.flags |= TYPE_IS_CONSTANT;
        sym->type.flags |= TYPE_IS_GLOBAL;

        decl->identifier->symbol_index = sym->symbol_index;
        decl->identifier->parent       = decl;
        decl->identifier->pos          = lxr.current().src_pos;

        node->_namespace->children.emplace_back(decl);


        //
        // Determine correct value for enum member.
        //

        lxr.advance(1);
        decl->init_value  = new ast_singleton_literal();
        auto* lit         = dynamic_cast<ast_singleton_literal*>(*decl->init_value);
        lit->parent       = decl;
        lit->pos          = lxr.current().src_pos;

        if(lxr.current() == TOKEN_VALUE_ASSIGNMENT) {

            lxr.advance(1);
            if(lxr.current() != TOKEN_INTEGER_LITERAL && lxr.current() != TOKEN_CHARACTER_LITERAL) {
                lxr.raise_error("Illegal token.");
                return nullptr;
            }

            lit->value        = std::string(lxr.current().value);
            lit->literal_type = lxr.current().type;

            if(const auto to_int = lexer_token_lit_to_int(lxr.current())) {
                enum_index = *to_int;
            } else {
                lxr.raise_error("Invalid numeric literal.");
                return nullptr;
            }

            lxr.advance(1);

        } else {
            lit->value        = std::to_string(enum_index);
            lit->literal_type = TOKEN_INTEGER_LITERAL;
        }


        //
        // Check for terminal, move on to the next value.
        //

        if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON)
            lxr.advance(1);

        ++enum_index;
    }

    lxr.advance(1);
    state = true;
    return node;
}