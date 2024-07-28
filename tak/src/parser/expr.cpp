//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_expression(parser& parser, lexer& lxr, const bool subexpression, const bool parse_single) {

    const auto  curr  = lxr.current();
    ast_node*   expr  = nullptr;
    bool        state = false;

    defer_if(!state, [&] {
        delete expr;
    });


    if(curr == TOKEN_END_OF_FILE)       return nullptr;
    if(curr == TOKEN_IDENTIFIER)        expr = parse_identifier(parser, lxr);
    else if(curr == TOKEN_AT)           expr = parse_compiler_directive(parser, lxr);
    else if(curr == TOKEN_LPAREN)       expr = parse_parenthesized_expression(parser, lxr);
    else if(curr == TOKEN_LBRACE)       expr = parse_braced_expression(parser, lxr);
    else if(curr.kind == KIND_LITERAL)  expr = parse_singleton_literal(parser, lxr);
    else if(curr.kind == KIND_KEYWORD)  expr = parse_keyword(parser, lxr);
    else if(TOKEN_VALID_UNARY_OPERATOR(curr)) expr = parse_unary_expression(parser, lxr);

    else {
        lxr.raise_error("Illegal token."); // @Cleanup: better error message
        return nullptr;
    }

    if(expr == nullptr)
        return nullptr;


    //
    // For these expressions we should not check for a terminal no matter what.
    //

    if(EXPR_NEVER_NEEDS_TERMINAL(expr->type)) {
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

    if(subexpression || parse_single) {
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


ast_node*
parse_keyword(parser &parser, lexer &lxr) {

    parser_assert(lxr.current().kind == KIND_KEYWORD, "Expected keyword.");

    if(lxr.current() == TOKEN_KW_RET)       return parse_ret(parser, lxr);
    if(lxr.current() == TOKEN_KW_IF)        return parse_branch(parser, lxr);
    if(lxr.current() == TOKEN_KW_SWITCH)    return parse_switch(parser, lxr);
    if(lxr.current() == TOKEN_KW_WHILE)     return parse_while(parser, lxr);
    if(lxr.current() == TOKEN_KW_FOR)       return parse_for(parser, lxr);
    if(lxr.current() == TOKEN_KW_STRUCT)    return parse_structdef(parser, lxr);
    if(lxr.current() == TOKEN_KW_NAMESPACE) return parse_namespace(parser, lxr);
    if(lxr.current() == TOKEN_KW_DO)        return parse_dowhile(parser, lxr);
    if(lxr.current() == TOKEN_KW_BLK)       return parse_block(parser, lxr);
    if(lxr.current() == TOKEN_KW_CAST)      return parse_cast(parser, lxr);
    if(lxr.current() == TOKEN_KW_ENUM)      return parse_enumdef(parser, lxr);
    if(lxr.current() == TOKEN_KW_DEFER)     return parse_defer(parser, lxr);
    if(lxr.current() == TOKEN_KW_SIZEOF)    return parse_sizeof(parser, lxr);

    lxr.raise_error("This keyword is not allowed here.");
    return nullptr;
}


ast_node*
parse_parenthesized_expression(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_LPAREN, "Expected beginning of parenthesized expression.");

    ++parser.inside_parenthesized_expression_;
    lxr.advance(1);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    auto* expr              = parse_expression(parser, lxr, true);

    if(expr == nullptr) return nullptr;
    if(!VALID_SUBEXPRESSION(expr->type)) {
        lxr.raise_error("This expression cannot be used within parentheses.", curr_pos, line);
        delete expr;
        return nullptr;
    }

    return expr;
}


ast_node*
parse_cast(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_CAST, "Expected \"cast\" keyword.");

    if(lxr.peek(1) != TOKEN_LPAREN) {
        lxr.raise_error("Expected '('.");
        return nullptr;
    }

    auto* node  = new ast_cast();
    bool  state = false;
    node->pos   = lxr.current().src_pos;

    defer_if(!state, [&] {
       delete node;
    });


    //
    // Parse cast target
    //

    lxr.advance(2);
    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    node->target            = parse_expression(parser, lxr, true);

    if(node->target == nullptr)
        return nullptr;

    node->target->parent = node;
    if(!VALID_SUBEXPRESSION(node->target->type)) {
        lxr.raise_error("Invalid expression used as cast target.", curr_pos, line);
        return nullptr;
    }


    //
    // Parse type
    //

    if(lxr.current() != TOKEN_COMMA && lxr.current() != TOKEN_SEMICOLON) {
        lxr.raise_error("Expected ',' or ';'.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER && lxr.current().kind != KIND_TYPE_IDENTIFIER) {
        lxr.raise_error("Expected type identifier.");
        return nullptr;
    }

    if(const auto type = parse_type(parser, lxr)) {
        node->type = *type;
    } else {
        return nullptr;
    }


    //
    // Finish up, should end with ')' always...
    //

    if(lxr.current() != TOKEN_RPAREN) {
        lxr.raise_error("Expected ')'.");
        return nullptr;
    }

    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_singleton_literal(parser& parser, lexer& lxr) {

    parser_assert(lxr.current().kind == KIND_LITERAL, "Expected literal.");

    auto* node         = new ast_singleton_literal();
    node->literal_type = lxr.current().type;
    node->pos          = lxr.current().src_pos;


    //
    // Resolve escape sequences if they exist
    //

    if(node->literal_type == TOKEN_STRING_LITERAL || node->literal_type == TOKEN_CHARACTER_LITERAL) {
        const auto real = remove_escaped_chars(lxr.current().value);
        if(!real) {
            lxr.raise_error("String contains one or more invalid escaped characters.");
            delete node;
            return nullptr;
        }

        node->value = *real;
    } else {
        node->value = std::string(lxr.current().value);
    }


    //
    // Convert hex literal to base 10 repr, perform bounds check on integer and float literals
    //

    try {
        if(node->literal_type == TOKEN_HEX_LITERAL) {
            const int64_t to_int  = std::stoll(node->value, nullptr, 16);
            node->value           = std::to_string(to_int);
            node->literal_type    = TOKEN_INTEGER_LITERAL;
        }

        if(node->literal_type == TOKEN_FLOAT_LITERAL)   std::stod(node->value);
        if(node->literal_type == TOKEN_INTEGER_LITERAL) std::stoll(node->value);
    }
    catch(const std::out_of_range&) {
        lxr.raise_error("Literal value is too large.");
        return nullptr;
    }
    catch(...) {
        lxr.raise_error("Invalid literal.");
        return nullptr;
    }

    lxr.advance(1);
    return node;
}


ast_node*
parse_sizeof(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_SIZEOF, "Expected \"sizeof\" keyword.");


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new ast_sizeof();
    node->pos   = curr_pos;

    defer_if(!state, [&] {
       delete node;
    });


    if(lxr.peek(1) != TOKEN_LPAREN) {
        lxr.raise_error("Expected '('.");
        return nullptr;
    }


    lxr.advance(2);
    if(lxr.current() == TOKEN_IDENTIFIER) {

        std::string name_if_sym;
        std::string name_if_type;

        const token    tmp_token  = lxr.current(); // save.
        const size_t   tmp_pos    = lxr.src_index_;
        const uint32_t tmp_line   = lxr.curr_line_;


        if(const auto name = get_namespaced_identifier(lxr)) {
            name_if_sym  = parser.get_canonical_sym_name(*name);
            name_if_type = parser.get_canonical_type_name(*name);
        }

        if(parser.type_exists(name_if_type) || parser.type_alias_exists(name_if_type)) {
            lxr.current_   = tmp_token; // restore.
            lxr.src_index_ = tmp_pos;
            lxr.curr_line_ = tmp_line;

            if(const auto data = parse_type(parser,lxr)) {
                node->target = *data;
            } else {
                return nullptr;
            }
        }

        else if(parser.scoped_symbol_exists(name_if_sym)) {
            lxr.advance(1);

            uint32_t sym_index = 0;
            if(sym_index = parser.lookup_scoped_symbol(name_if_sym); sym_index == INVALID_SYMBOL_INDEX) {
                return nullptr;
            }

            const auto* sym = parser.lookup_unique_symbol(sym_index);
            if(sym == nullptr)
                return nullptr;

            ast_identifier* ident = nullptr;
            if(lxr.current() == TOKEN_DOT) {
                ident = dynamic_cast<ast_identifier*>(parse_member_access(sym->symbol_index, parser, lxr));
            } else {
                ident = new ast_identifier();
            }

            if(ident == nullptr)
                return nullptr;

            ident->symbol_index = sym->symbol_index;
            ident->parent       = node;
            ident->pos          = curr_pos;

            node->target = ident;
        }

        else {
            lxr.raise_error("Unrecognized identifier following \"sizeof\".", curr_pos, line);
            return nullptr;
        }
    }
    else if(lxr.current().kind == KIND_TYPE_IDENTIFIER){
        if(const auto data = parse_type(parser,lxr)) {
            node->target = *data;
        } else {
            return nullptr;
        }
    }
    else {
        lxr.raise_error("Unrecognized identifier following \"sizeof\".", curr_pos, line);
        return nullptr;
    }


    if(lxr.current() != TOKEN_RPAREN) {
        lxr.raise_error("Expected ')'.");
        return nullptr;
    }

    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_braced_expression(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_LBRACE, "Expected left-brace.");

    bool  state = false;
    auto* node  = new ast_braced_expression();
    node->pos   = lxr.current().src_pos;

    defer_if(!state, [&] {
        delete node;
    });


    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        node->members.emplace_back(parse_expression(parser, lxr, true));
        if(node->members.back() == nullptr) {
            return nullptr;
        }

        if(!VALID_SUBEXPRESSION(node->members.back()->type)) {
            lxr.raise_error("Invalid subexpression within braced expression.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() == TOKEN_COMMA) {
            lxr.advance(1);
        }
    }


    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_unary_expression(parser& parser, lexer& lxr) {

    parser_assert(TOKEN_VALID_UNARY_OPERATOR(lxr.current()), "Expected unary operator.");

    const size_t   src_pos = lxr.current().src_pos;
    const uint32_t line    = lxr.current().line;

    auto* node      = new ast_unaryexpr();
    node->_operator = lxr.current().type;
    node->pos       = src_pos;


    lxr.advance(1);
    node->operand = parse_expression(parser, lxr, true, true);
    if(node->operand == nullptr) {
        delete node;
        return nullptr;
    }

    const auto right_t = node->operand->type;
    if(!VALID_SUBEXPRESSION(right_t)) {
        lxr.raise_error("Unexpected expression following unary operator.", src_pos, line);
        delete node;
        return nullptr;
    }

    node->operand->parent = node;
    return node;
}


ast_node*
parse_call(ast_node* operand, parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_LPAREN, "Expected '('.");

    //
    // Generate AST node.
    //

    bool  state           = false;
    auto* node            = new ast_call();
    node->target          = operand;
    node->pos             = lxr.current().src_pos;
    node->target->parent  = node;

    defer_if(!state, [&] {
        delete node;
    });


    //
    // Parse the parameter list.
    //

    lxr.advance(1);
    if(lxr.current() == TOKEN_RPAREN) {
        lxr.advance(1);
        state = true;
        return node;
    }


    const uint16_t old_paren_index = parser.inside_parenthesized_expression_++;
    while(old_paren_index < parser.inside_parenthesized_expression_) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;


        auto* expr = parse_expression(parser, lxr, true);
        if(expr == nullptr) {
            return nullptr;
        }

        const auto _type = expr->type;
        if(!VALID_SUBEXPRESSION(_type)) {
            lxr.raise_error("Invalid subexpression within call.", curr_pos, line);
            return nullptr;
        }

        node->arguments.emplace_back(expr);

        if(old_paren_index >= parser.inside_parenthesized_expression_)
            break;

        if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
            lxr.advance(1);
            if(lxr.current() == TOKEN_RPAREN) {
                --parser.inside_parenthesized_expression_;
                lxr.advance(1);
            }
        }
    }


    state = true;
    return node;
}


ast_node*
parse_binary_expression(ast_node* left_operand, parser& parser, lexer& lxr) {

    parser_assert(lxr.current().kind == KIND_BINARY_EXPR_OPERATOR, "Expected binary operator.");
    parser_assert(left_operand != nullptr, "Null left operand passed.");


    bool  state    = false;
    auto* binexpr  = new ast_binexpr();
    binexpr->pos   = lxr.current().src_pos;

    defer_if(!state, [&] {
        delete binexpr;
    });

    binexpr->_operator       = lxr.current().type;
    binexpr->left_op         = left_operand;
    binexpr->left_op->parent = binexpr;

    const size_t   src_pos = lxr.current().src_pos;
    const uint32_t line    = lxr.current().line;


    lxr.advance(1);
    binexpr->right_op = parse_expression(parser, lxr, true, true);
    if(binexpr->right_op == nullptr) {
        return nullptr;
    }


    binexpr->right_op->parent = binexpr;
    const auto right_t        = binexpr->right_op->type;

    if(!VALID_SUBEXPRESSION(right_t)) {
        lxr.raise_error("Unexpected expression following binary operator.", src_pos, line);
        return nullptr;
    }


    while(lxr.current().kind == KIND_BINARY_EXPR_OPERATOR && precedence_of(lxr.current().type) <= precedence_of(binexpr->_operator)) {
        binexpr->right_op = parse_binary_expression(binexpr->right_op, parser, lxr);
        if(binexpr->right_op == nullptr) {
            return nullptr;
        }
    }

    state = true;
    return binexpr;
}


ast_node*
parse_subscript(ast_node* operand, parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_LSQUARE_BRACKET, "Expected '['.");
    parser_assert(operand != nullptr, "Null operand.");
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node            = new ast_subscript();
    node->operand         = operand;
    node->pos             = lxr.current().src_pos;
    node->operand->parent = node;
    node->value           = parse_expression(parser, lxr, true);

    bool state = false;
    defer_if(!state, [&] {
        delete node;
    });


    if(node->value == nullptr)
        return nullptr;

    if(!VALID_SUBEXPRESSION(node->value->type) || lxr.current() != TOKEN_RSQUARE_BRACKET) {
        lxr.raise_error("Invalid expression within subscript operator.", curr_pos, line);
        return nullptr;
    }


    lxr.advance(1);
    state = true;
    return node;
}
