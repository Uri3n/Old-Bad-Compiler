//
// Created by Diago on 2024-07-04.
//

#include <parser.hpp>


ast_node*
parse_expression(parser& parser, lexer& lxr) {

    auto&     curr        = lxr.current();
    ast_node* return_node = nullptr;


    if(curr == IDENTIFIER) {
        return parse_identifier(parser, lxr);
    } else if(curr.kind == KEYWORD){
        lxr.raise_error("This token is not allowed at the beginning of an expression.");
        return nullptr;
    }


    if(parser.scope_stack.size() <= 1) {
        lxr.raise_error("Only declarations are allowed at global scope.");
        delete return_node;
        return nullptr;
    }

    return return_node;
}


ast_node*
parse_decl(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected identifier.");

    auto     name         = std::string(lxr.current().value.begin(), lxr.current().value.end());
    size_t   src_pos      = lxr.current().src_pos;
    uint32_t line         = lxr.current().line;
    uint16_t flags        = SYM_FLAGS_NONE;
    uint16_t ptr_depth    = 0;
    uint32_t array_length = 0;
    sym_t    type         = SYM_VARIABLE;
    var_t    var_type     = VAR_NONE;


    lxr.advance(1);
    if(lxr.current() == CONST_TYPE_ASSIGNMENT) {
        flags |= SYM_VAR_IS_CONSTANT;
    }

    else if(lxr.current() != TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }


    lxr.advance(1);
    switch(lxr.current().type) { // TODO: add a check for a struct / user defined type.
        case TOKEN_KW_PROC: type = SYM_PROCEDURE; break;
        case TOKEN_KW_I8:   var_type = I8;        break;
        case TOKEN_KW_U8:   var_type = U8;        break;
        case TOKEN_KW_I16:  var_type = I16;       break;
        case TOKEN_KW_U16:  var_type = U16;       break;
        case TOKEN_KW_I32:  var_type = I32;       break;
        case TOKEN_KW_U32:  var_type = U32;       break;
        case TOKEN_KW_I64:  var_type = I64;       break;
        case TOKEN_KW_U64:  var_type = U64;       break;
        case TOKEN_KW_F32:  var_type = F32;       break;
        case TOKEN_KW_F64:  var_type = F64;       break;
        case TOKEN_KW_BOOL: var_type = BOOLEAN;   break;

        default:
            lxr.raise_error("Illegal token: expected a type identifier.");
            return nullptr;
    }


    //
    // Check for pointer type.
    //

    lxr.advance(1);
    if(lxr.current() == BITWISE_XOR_OR_PTR) {
        if(type == SYM_PROCEDURE) {
            lxr.raise_error("Illegal token: procedure cannot be a pointer type.");
            return nullptr;
        }

        flags |= SYM_IS_POINTER;
        while(lxr.current() == BITWISE_XOR_OR_PTR) {
            ++ptr_depth;
            lxr.advance(1);
        }
    }


    //
    // Check for array type.
    //

    if(lxr.current() == LSQUARE_BRACKET) {
        if(type == SYM_PROCEDURE) {
            lxr.raise_error("Illegal token: procedure cannot be an array type.");
            return nullptr;
        }

        flags |= SYM_VAR_IS_ARRAY;
        lxr.advance(1);

        if(lxr.current() == INTEGER_LITERAL) {

            auto istr = std::string(lxr.current().value.begin(), lxr.current().value.end());

            try {
                array_length = std::stoul(istr);
            } catch(const std::out_of_range& e) {
                lxr.raise_error("Array size is too large.");
            } catch(...) {
                lxr.raise_error("Array size must be a valid integer literal.");
                return nullptr;
            }

            lxr.advance(1);
        }

        if(lxr.current() != RSQUARE_BRACKET) {
            lxr.raise_error("Expected closing bracket here.");
            return nullptr;
        }

        lxr.advance(1);
    }


    //
    // Check if the symbol already exists in the current scope.
    //




    return nullptr;
}

ast_node*
parse_assign(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER,     "called parse_assign without an identifier.");
    PARSER_ASSERT(lxr.peek(1) == VALUE_ASSIGNMENT, "called parse_assign but the next token is not '='.");

    return nullptr;
}


ast_node*
parse_call(parser& parser, lexer& lxr) {
    return nullptr;
}


ast_node*
parse_identifier(parser& parser, lexer& lxr) {

    const token next = lxr.peek(1);

    switch(next.type) {
        case VALUE_ASSIGNMENT:      return parse_assign(parser, lxr);
        case TYPE_ASSIGNMENT:       return parse_decl(parser, lxr);
        case CONST_TYPE_ASSIGNMENT: return parse_decl(parser, lxr);
        case LPAREN:                return parse_call(parser, lxr);

        default:
            lxr.raise_error("Illegal token following identifier.");
            return nullptr;
    }
}


bool
generate_ast_from_source(parser& parser, const std::string& source_file_name) {

    lexer     lxr;
    ast_node* toplevel_decl = nullptr;


    if(!lxr.init(source_file_name)) {
        return false;
    }

    if(parser.scope_stack.empty()) {
        parser.push_scope(); // push global scope
    }


    do {
        toplevel_decl = parse_expression(parser, lxr);
        parser.toplevel_decls.emplace_back(toplevel_decl);
    } while(toplevel_decl != nullptr);

    return lxr.current() == END_OF_FILE;
}
