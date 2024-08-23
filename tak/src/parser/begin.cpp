//
// Created by Diago on 2024-08-16.
//

#include <parser.hpp>


tak::AstNode*
tak::parse(Parser& parser, Lexer& lxr, const bool nocheck_term, const bool parse_single) {

    const auto  curr  = lxr.current();
    AstNode*    expr  = nullptr;
    bool        state = false;

    defer_if(!state, [&] {
        delete expr;
    });


    if(curr == TOKEN_END_OF_FILE)             return nullptr;
    if(curr == TOKEN_AT)                      expr = parse_compiler_directive(parser, lxr);
    else if(curr == TOKEN_LPAREN)             expr = parse_parenthesized_expression(parser, lxr);
    else if(curr == TOKEN_LBRACE)             expr = parse_braced_expression(parser, lxr);
    else if(curr.kind == KIND_LITERAL)        expr = parse_singleton_literal(parser, lxr);
    else if(curr.kind == KIND_KEYWORD)        expr = parse_keyword(parser, lxr);
    else if(TOKEN_IDENT_START(curr.type))     expr = parse_identifier(parser, lxr);
    else if(TOKEN_VALID_UNARY_OPERATOR(curr)) expr = parse_unary_expression(parser, lxr);

    else {
        lxr.raise_error("Unexpected token.");
        return nullptr;
    }

    if(expr == nullptr) {
        return nullptr;
    }


    //
    // For these expressions we should not check for a terminal no matter what.
    //

    if(NODE_EXPR_NEVER_NEEDS_TERMINAL(expr->type)) {
        state = true;
        return expr;
    }


    //
    // Check postfix
    //

    while(true) {
        if(expr == nullptr) {
            return nullptr;
        }

        if(lxr.current() == TOKEN_LSQUARE_BRACKET) {
            expr = parse_subscript(expr, parser, lxr);
            continue;
        }

        if(lxr.current() == TOKEN_LPAREN) {
            expr = parse_call(expr, parser, lxr);
            continue;
        }

        if(lxr.current() == TOKEN_DOT) {
            expr = parse_member_access(expr, lxr);
            continue;
        }

        if(lxr.current().kind == KIND_BINARY_EXPR_OPERATOR && !parse_single) {
            expr = parse_binary_expression(expr, parser, lxr);
            continue;
        }

        break;
    }


    //
    // Check if we're leaving a parenthesized expression
    //

    if(lxr.current() == TOKEN_RPAREN) {
        if(!parser.inside_parenthesized_expression_) {
            lxr.raise_error("Unexpected token.");
            return nullptr;
        }

        if(!parse_single) {
            --parser.inside_parenthesized_expression_;
            lxr.advance(1);
        }
    }

    if(nocheck_term || parse_single) {
        state = true;
        return expr;
    }


    //
    // Check for terminal character (if not inside of a subexpression)
    //

    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        if(parser.inside_parenthesized_expression_) {
            lxr.raise_error("Unexpected token inside of parenthesized expression.");
            return nullptr;
         }

        lxr.advance(1);
        state = true;
        return expr;
    }

    lxr.raise_error("Unexpected token following expression.");
    return nullptr;
}


tak::AstNode*
tak::parse_keyword(Parser &parser, Lexer &lxr) {

    assert(lxr.current().kind == KIND_KEYWORD);

    switch(lxr.current().type) {
        case TOKEN_KW_RET:        return parse_ret(parser, lxr);
        case TOKEN_KW_IF:         return parse_branch(parser, lxr);
        case TOKEN_KW_SWITCH:     return parse_switch(parser, lxr);
        case TOKEN_KW_WHILE:      return parse_while(parser, lxr);
        case TOKEN_KW_FOR:        return parse_for(parser, lxr);
        case TOKEN_KW_STRUCT:     return parse_structdef(parser, lxr);
        case TOKEN_KW_CONT:       return parse_cont(lxr);
        case TOKEN_KW_BRK:        return parse_brk(lxr);
        case TOKEN_KW_NAMESPACE:  return parse_namespace(parser, lxr);
        case TOKEN_KW_DO:         return parse_dowhile(parser, lxr);
        case TOKEN_KW_BLK:        return parse_block(parser, lxr);
        case TOKEN_KW_CAST:       return parse_cast(parser, lxr);
        case TOKEN_KW_ENUM:       return parse_enumdef(parser, lxr);
        case TOKEN_KW_DEFER:      return parse_defer(parser, lxr);
        case TOKEN_KW_DEFER_IF:   return parse_defer_if(parser, lxr);
        case TOKEN_KW_SIZEOF:     return parse_sizeof(parser, lxr);
        case TOKEN_KW_NULLPTR:    return parse_nullptr(lxr);
        default: break;
    }

    lxr.raise_error("This keyword is not allowed here.");
    return nullptr;
}