//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>

/*
    enum foo, i32 {
        VALUE_ONE,
        VALUE_TWO,
        VALUE_THREE
    }
*/

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
            && type.sym_type == SYM_VARIABLE);
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


    bool state       = false;
    auto* node       = new ast_enumdef();
    node->_namespace = new ast_namespacedecl();
    node->alias      = new ast_type_alias();

    node->_namespace->parent = node;
    node->alias->parent      = node;
    node->alias->name        = parser.namespace_as_string() + std::string(lxr.current().value);

    defer_if(!state,[&] {
       delete node;
    });


    //
    // Enter enum as namespace
    //

    if(parser.namespace_exists(std::string(lxr.current().value))
        || parser.type_alias_exists(node->alias->name)
        || parser.type_exists(node->alias->name)
    ) {
        lxr.raise_error("Naming conflict: a namespace, type alias, or struct has the same name as this enum.");
        return nullptr;
    }

    parser.enter_namespace(std::string(lxr.current().value));
    node->_namespace->full_path = parser.namespace_as_string();


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

        auto* sym        = parser.create_symbol(member_name, lxr.current().src_pos, lxr.current().line, SYM_VARIABLE, SYM_FLAGS_NONE, *type);
        auto* decl       = new ast_vardecl();
        decl->identifier = new ast_identifier();

        sym->type.flags |= SYM_IS_CONSTANT;
        sym->type.flags |= SYM_IS_GLOBAL;

        decl->identifier->symbol_index = sym->symbol_index;
        decl->identifier->parent       = decl;

        // TODO: finish
    }

    lxr.advance(1);
    state = true;
    return node;
}


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
    std::vector<member_data> members;

    if(parser.type_exists(type_name) || parser.type_alias_exists(type_name)) {
        lxr.raise_error("Naming conflict: type or type alias has already been defined elsewhere.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Unexpected token.");
    }


    //
    // Parse out each struct member
    //

    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {

        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected identifier.");
            return nullptr;
        }


        auto name     = std::string(lxr.current().value);
        bool is_const = false;

        const size_t src_pos = lxr.current().src_pos;
        const uint32_t line  = lxr.current().line;


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

        if(lxr.current().value == type_name) {
            lxr.raise_error("A struct cannot contain itself.");
            return nullptr;
        }


        if(const auto type = parse_type(parser, lxr)) {
            if(type->sym_type == SYM_PROCEDURE && type->pointer_depth < 1) {
                lxr.raise_error("Procedures cannot be used as struct members.", src_pos, line);
                return nullptr;
            }

            members.emplace_back(member_data(name, *type));
            members.back().type.flags |= is_const ? SYM_IS_CONSTANT | SYM_DEFAULT_INITIALIZED : SYM_DEFAULT_INITIALIZED;

        } else {
            return nullptr;
        }


        if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
            lxr.advance(1);
        }
    }


    //
    // Store type, create AST node.
    //

    if(!parser.create_type(type_name, std::move(members))) {
        return nullptr;
    }

    auto* node = new ast_structdef();
    node->name = type_name;

    lxr.advance(1);
    return node;
}


std::optional<std::vector<uint32_t>>
parse_array_data(lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_LSQUARE_BRACKET, "Expected '['");
    std::vector<uint32_t> lengths;


    while(lxr.current() == TOKEN_LSQUARE_BRACKET) {
        lxr.advance(1);

        uint32_t len = 0; // 0 means size is determined by the compiler.
        if(lxr.current() == TOKEN_INTEGER_LITERAL) {

            const auto istr = std::string(lxr.current().value);

            try {
                len = std::stoul(istr);
            } catch(const std::out_of_range& _) {
                lxr.raise_error("Array size is too large.");
                return std::nullopt;
            } catch(...) {
                lxr.raise_error("Array size must be a valid non-negative integer literal.");
                return std::nullopt;
            }

            if(len == 0) {
                lxr.raise_error("Array length cannot be 0.");
                return std::nullopt;
            }

            lxr.advance(1);
        }

        if(lxr.current() != TOKEN_RSQUARE_BRACKET) {
            lxr.raise_error("Expected closing square bracket.");
            return std::nullopt;
        }

        lengths.emplace_back(len);
        lxr.advance(1);
    }


    return lengths;
}


std::optional<type_data>
parse_type(parser& parser, lexer& lxr) {

    parser_assert(lxr.current().kind == KIND_TYPE_IDENTIFIER || lxr.current() == TOKEN_IDENTIFIER, "Expected type.");
    type_data data;


    //
    // Determine if variable, struct, or proc.
    //

    if(lxr.current() == TOKEN_KW_PROC) {
        data.sym_type = SYM_PROCEDURE;
        data.name     = std::monostate();
    }

    else if(lxr.current() == TOKEN_IDENTIFIER) {

        const auto name = get_namespaced_identifier(lxr);
        if(!name) {
            return std::nullopt;
        }

        const auto canonical_name = parser.get_canonical_type_name(*name);
        if(parser.type_alias_exists(canonical_name)) {
            data = parser.lookup_type_alias(canonical_name);
        }
        else if(!parser.type_exists(canonical_name)) {
            lxr.raise_error("Invalid type specifier.");
            return std::nullopt;
        }
        else {
            data.sym_type = SYM_STRUCT;
            data.name     = canonical_name;
        }
    }

    else {
        var_t _var_t = VAR_NONE;

        if(lxr.current() == TOKEN_KW_VOID) {
            _var_t = VAR_VOID;
            if(lxr.peek(1) != TOKEN_BITWISE_XOR_OR_PTR) {
                lxr.raise_error("Use of \"void\" as non pointer type.");
                return std::nullopt;
            }
        } else {
            _var_t = token_to_var_t(lxr.current().type);
            if(_var_t == VAR_NONE) {
                lxr.raise_error("Invalid type specifier.");
                return std::nullopt;
            }
        }

        data.name     = _var_t;
        data.sym_type = SYM_VARIABLE;
    }


    //
    // Get pointer depth, array size (if exists)
    //

    lxr.advance(1);
    if(lxr.current() == TOKEN_BITWISE_XOR_OR_PTR) {
        data.flags |= SYM_IS_POINTER;
        while(lxr.current() == TOKEN_BITWISE_XOR_OR_PTR) {
            data.pointer_depth++;
            lxr.advance(1);
        }
    }

    if(lxr.current() == TOKEN_LSQUARE_BRACKET) {
        data.flags |= SYM_IS_ARRAY;
        if(const auto arr_data = parse_array_data(lxr)) {
            data.array_lengths = *arr_data;
        } else {
            return std::nullopt;
        }
    }


    //
    // If the type isn't a proc, we can simply return here.
    //

    if(data.sym_type != SYM_PROCEDURE) {
        return data;
    }

    if(lxr.current() != TOKEN_LPAREN) {
        lxr.raise_error("Expected beginning of parameter type list.");
        return std::nullopt;
    }


    //
    // Parse procedure parameter list.
    //

    lxr.advance(1);
    data.parameters = std::make_shared<std::vector<type_data>>();

    while(lxr.current() != TOKEN_RPAREN) {

        if(lxr.current().kind != KIND_TYPE_IDENTIFIER && lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected type identifier.");
            return std::nullopt;
        }

        if(auto param_data = parse_type(parser, lxr)) {
            param_data->flags |= SYM_IS_PROCARG;
            data.parameters->emplace_back(*param_data);
        } else {
            return std::nullopt;
        }

        if(lxr.current() == TOKEN_COMMA) {
            lxr.advance(1);
        }
    }


    //
    // Get return type. If VOID, we can just leave data.return_type as nullptr and return.
    //

    if(lxr.peek(1) != TOKEN_ARROW || (lxr.peek(2).kind != KIND_TYPE_IDENTIFIER && lxr.peek(2) != TOKEN_IDENTIFIER)) {
        lxr.raise_error("Expected procedure return type after parameter list. Example: -> i32");
        return std::nullopt;
    }

    lxr.advance(2);
    if(lxr.current() == TOKEN_KW_VOID) {
        lxr.advance(1);
        return data;
    }


    data.return_type = std::make_shared<type_data>();
    if(const auto ret_type = parse_type(parser, lxr)) {
        *data.return_type = *ret_type; // expensive copy
    } else {
        return std::nullopt;
    }

    return data;
}
