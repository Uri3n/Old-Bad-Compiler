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

    auto     name     = std::string(lxr.current().value.begin(), lxr.current().value.end());
    size_t   src_pos  = lxr.current().src_pos;
    uint32_t line     = lxr.current().line;
    uint16_t flags    = SYM_FLAGS_NONE;
    sym_t    type     = SYM_VARIABLE;
    var_t    var_type = VAR_NONE;


    lxr.advance(1);
    if(lxr.current() == CONST_TYPE_ASSIGNMENT) {
        flags |= SYM_VAR_IS_CONSTANT;
    }

    else if(lxr.current() != TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }


    switch(lxr.current().type) {
        case TOKEN_KW_PROC:
            type = SYM_PROCEDURE;
            break;

        case TOKEN_KW_I8:
            var_type = I8;
            break;

        case TOKEN_KW_U8:
            var_type = U8;
            break;

        case TOKEN_KW_I16:
            var_type = I16;
            break;

        case TOKEN_KW_U16:
            var_type = U16;
            break;

        case TOKEN_KW_I32:
            var_type = I16;
            break;

        case TOKEN_KW_U32:
            var_type = U32;
            break;

        case TOKEN_KW_I64:

        case TOKEN_KW_U64:

        default:
            lxr.raise_error("Expected a type identifier.");
    }

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
            lxr.raise_error("Unexpected token following identifier.");
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
