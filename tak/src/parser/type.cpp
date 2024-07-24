//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


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
        data.sym_type = TYPE_KIND_PROCEDURE;
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
            //std::cout << canonical_name << std::endl;
            //std::cout << *name << '\n';
            //std::cout << parser.namespace_as_string() << '\n';
            lxr.raise_error("Invalid type specifier.");
            return std::nullopt;
        }
        else {
            data.sym_type = TYPE_KIND_STRUCT;
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
        data.sym_type = TYPE_KIND_VARIABLE;
    }


    //
    // Get pointer depth, array size (if exists)
    //

    lxr.advance(1);
    if(lxr.current() == TOKEN_BITWISE_XOR_OR_PTR) {
        data.flags |= TYPE_IS_POINTER;
        while(lxr.current() == TOKEN_BITWISE_XOR_OR_PTR) {
            data.pointer_depth++;
            lxr.advance(1);
        }
    }

    if(lxr.current() == TOKEN_LSQUARE_BRACKET) {
        data.flags |= TYPE_IS_ARRAY;
        if(const auto arr_data = parse_array_data(lxr)) {
            data.array_lengths = *arr_data;
        } else {
            return std::nullopt;
        }
    }


    //
    // If the type isn't a proc, we can simply return here.
    //

    if(data.sym_type != TYPE_KIND_PROCEDURE) {
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
            param_data->flags |= TYPE_IS_PROCARG;
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
