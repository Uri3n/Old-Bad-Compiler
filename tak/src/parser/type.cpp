//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


std::optional<std::vector<uint32_t>>
tak::parse_array_data(Lexer& lxr) {

    assert(lxr.current() == TOKEN_LSQUARE_BRACKET);
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


static bool
parse_userdefined_type(tak::TypeData& data, tak::Parser& parser, tak::Lexer& lxr) {

    assert(TOKEN_IDENT_START(lxr.current().type));

    const size_t   curr_pos  = lxr.current().src_pos;
    const uint32_t curr_line = lxr.current().line;
    const auto     name      = get_namespaced_identifier(lxr);


    //
    // Determine type.
    //

    if(!name) {
        return false;
    }

    const auto canonical_name = parser.tbl_.get_canonical_type_name(*name);
    if(parser.tbl_.type_alias_exists(canonical_name)) {
        data = parser.tbl_.lookup_type_alias(canonical_name);
    }
    else {
        if(!parser.tbl_.type_exists(canonical_name)) {
            assert(parser.tbl_.create_placeholder_type(canonical_name, curr_pos, curr_line, lxr.source_file_name_));
        }
        data.kind = tak::TYPE_KIND_STRUCT;
        data.name = canonical_name;
    }

    if(lxr.peek(1) != tak::TOKEN_DOLLAR_SIGN) {
        return true;
    }


    //
    // Generic parameters.
    //

    data.parameters = std::make_shared<std::vector<tak::TypeData>>();
    bool state      = false;

    defer_if(!state, [&] {
       if(data.parameters != nullptr) {
           data.parameters.reset();
           data.parameters = nullptr;
       }
    });


    if(parser.tbl_.type_alias_exists(canonical_name)) {
        lxr.raise_error("Cannot use generic parameters on type aliases.");
        return false;
    }

    lxr.advance(2);
    if(lxr.current() != tak::TOKEN_LSQUARE_BRACKET) {
        lxr.raise_error("Expected '['.");
        return false;
    }

    lxr.advance(1);
    while(lxr.current() != tak::TOKEN_RSQUARE_BRACKET) {
        if(lxr.current().kind != tak::KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
            lxr.raise_error("Expected type identifier.");
            return false;
        }

        if(const auto generic_t = parse_type(parser, lxr)) {
            data.parameters->emplace_back(*generic_t);
        } else {
            return false;
        }

        if(lxr.current() == tak::TOKEN_COMMA || lxr.current() == tak::TOKEN_SEMICOLON) {
            lxr.advance(1);
        }
    }

    if(parser.tbl_.type_exists(data.to_string())) {
        data.parameters.reset();
        data.parameters = nullptr;
        data.name = data.to_string();
    }

    state = true;
    return true;
}


static bool
parse_primitive_type(tak::TypeData& data, tak::Lexer& lxr) {

    assert(lxr.current().kind == tak::KIND_TYPE_IDENTIFIER);

    tak::primitive_t     _var_t    = tak::PRIMITIVE_NONE;
    const size_t   curr_pos  = lxr.current().src_pos;
    const uint32_t curr_line = lxr.current().line;


    if(lxr.current() == tak::TOKEN_KW_VOID) {
        _var_t = tak::PRIMITIVE_VOID;
        if(lxr.peek(1) != tak::TOKEN_BITWISE_XOR_OR_PTR) {
            lxr.raise_error("Use of \"void\" as non pointer type.");
            return false;
        }
    }
    else {
        _var_t = token_to_var_t(lxr.current().type);
        if(_var_t == tak::PRIMITIVE_NONE) {
            lxr.raise_error("Invalid type specifier.", curr_pos, curr_line);
            return false;
        }
    }

    data.name = _var_t;
    data.kind = tak::TYPE_KIND_PRIMITIVE;
    return true;
}


static bool
parse_type_postfixes(tak::TypeData& data, tak::Lexer& lxr) {

    if(lxr.current() == tak::TOKEN_BITWISE_XOR_OR_PTR) {
        data.flags |= tak::TYPE_POINTER;
        while(lxr.current() == tak::TOKEN_BITWISE_XOR_OR_PTR) {
            data.pointer_depth++;
            lxr.advance(1);
        }
    }

    if(lxr.current() == tak::TOKEN_LSQUARE_BRACKET) {
        data.flags |= tak::TYPE_ARRAY;
        if(const auto arr_data = parse_array_data(lxr)) {
            data.array_lengths = *arr_data;
        } else {
            return false;
        }
    }

    return true;
}


static bool
parse_proctype_postfixes(tak::TypeData& data, tak::Parser& parser, tak::Lexer& lxr) {

    assert(lxr.current() == tak::TOKEN_LPAREN);

    //
    // Disallow non-pointer procedure types
    //

    if(!(data.flags & tak::TYPE_POINTER)) {
        lxr.raise_error("Cannot create a procedure here.");
        return false;
    }

    //
    // Get parameter list
    //

    lxr.advance(1);
    data.parameters = std::make_shared<std::vector<tak::TypeData>>();

    while(lxr.current() != tak::TOKEN_RPAREN)
    {
        if(lxr.current() == tak::TOKEN_THREE_DOTS) {
            data.flags |= tak::TYPE_PROC_VARARGS;
            if(lxr.peek(1) != tak::TOKEN_RPAREN) {
                lxr.raise_error("Expected ')' after variadic argument specifier.");
                return false;
            }

            lxr.advance(1);
            break;
        }

        if(lxr.current().kind != tak::KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
            lxr.raise_error("Expected type identifier.");
            return false;
        }

        if(auto param_data = parse_type(parser, lxr)) {
            param_data->flags |= tak::TYPE_PROCARG;
            data.parameters->emplace_back(*param_data);
        } else {
            return false;
        }

        if(lxr.current() == tak::TOKEN_COMMA) {
            lxr.advance(1);
        }
    }

    //
    // Get return type. If VOID, we can just leave data.return_type as nullptr and return.
    //

    if(lxr.peek(1) != tak::TOKEN_ARROW || (lxr.peek(2).kind != tak::KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.peek(2).type))) {
        lxr.raise_error("Expected procedure return type after parameter list. Example: -> i32");
        return false;
    }

    lxr.advance(2);
    if(lxr.current() == tak::TOKEN_KW_VOID) {
        lxr.advance(1);
        return true;
    }

    data.return_type = std::make_shared<tak::TypeData>();
    if(const auto ret_type = parse_type(parser, lxr)) {
        if(ret_type->flags & tak::TYPE_ARRAY) {
            lxr.raise_error("Return type cannot be a static array.");
            return false;
        }
        *data.return_type = *ret_type; // expensive copy
    } else {
        return false;
    }

    return true;
}


std::optional<tak::TypeData>
tak::parse_type(Parser& parser, Lexer& lxr) {

    assert(lxr.current().kind == KIND_TYPE_IDENTIFIER || TOKEN_IDENT_START(lxr.current().type));
    TypeData data; // < returning this

    //
    // Determine if variable, struct, or proc.
    //

    if(lxr.current() == TOKEN_KW_PROC) {
        data.kind = TYPE_KIND_PROCEDURE;
        data.name = std::monostate();
    }
    else if(TOKEN_IDENT_START(lxr.current().type)) {
        if(!parse_userdefined_type(data, parser, lxr)) {
            return std::nullopt;
        }
    }
    else if(lxr.current().kind == KIND_TYPE_IDENTIFIER){
        if(!parse_primitive_type(data, lxr)) {
            return std::nullopt;
        }
    }
    else {
        assert(false);
    }

    //
    // Get pointer depth, array size (if exists)
    //

    lxr.advance(1);
    if(!parse_type_postfixes(data, lxr)) {
        return std::nullopt;
    }

    //
    // If the type isn't a proc, we can simply return here.
    //

    if(data.kind != TYPE_KIND_PROCEDURE) {
        return data;
    }

    if(lxr.current() != TOKEN_LPAREN) {
        lxr.raise_error("Expected beginning of parameter type list.");
        return std::nullopt;
    }

    //
    // Parse procedure parameter list.
    //

    if(!parse_proctype_postfixes(data, parser, lxr)) {
        return std::nullopt;
    }

    return data;
}
