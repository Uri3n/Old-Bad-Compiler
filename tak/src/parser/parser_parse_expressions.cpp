//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_subscript(ast_node* operand, parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == LSQUARE_BRACKET, "Expected '['.");
    PARSER_ASSERT(operand != nullptr, "Null operand.");
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node            = new ast_subscript();
    node->operand         = operand;
    node->operand->parent = node;
    node->value           = parse_expression(parser, lxr, true);

    bool state = false;
    defer([&] {
        if(!state) { delete node; }
    });


    if(node->value == nullptr)
        return nullptr;

    if(!VALID_SUBEXPRESSION(node->value->type) || lxr.current() != RSQUARE_BRACKET) {
        lxr.raise_error("Invalid expression within subscript operator.", curr_pos, line);
        return nullptr;
    }


    lxr.advance(1);
    state = true;
    return node;
}

ast_node*
parse_expression(parser& parser, lexer& lxr, const bool subexpression, const bool parse_single) {

    const auto  curr  = lxr.current();
    ast_node*   expr  = nullptr;
    bool        state = false;
    defer([&]{ if(!state){ delete expr;} });


    if(curr == END_OF_FILE)             return nullptr;
    if(curr == IDENTIFIER)              expr = parse_identifier(parser, lxr);
    else if(curr == LPAREN)             expr = parse_parenthesized_expression(parser, lxr);
    else if(curr == LBRACE)             expr = parse_braced_expression(parser, lxr);
    else if(curr.kind == LITERAL)       expr = parse_singleton_literal(parser, lxr);
    else if(curr.kind == KEYWORD)       expr = parse_keyword(parser, lxr);
    else if(VALID_UNARY_OPERATOR(curr)) expr = parse_unary_expression(parser, lxr);


    else {
        lxr.raise_error("Invalid token at the beginning of an expression.");
        return nullptr;
    }

    if(expr == nullptr)
        return nullptr;


    //
    // Check if we're leaving a parenthesized expression
    //

    if(lxr.current() == RPAREN) {
        if(!parser.inside_parenthesized_expression) {
            lxr.raise_error("Unexpected token.");
            return nullptr;
        }

        --parser.inside_parenthesized_expression;
        lxr.advance(1);
        state = true;
        return expr;
    }


    //
    // For these expressions we should not check the terminal no matter what.
    //

    if(EXPR_NEVER_NEEDS_TERMINAL(expr->type)) {
        state = true;
        return expr;
    }


    //
    // If the next token is an operator we can recurse until the subexpression is parsed.
    //

    while(lxr.current() == LSQUARE_BRACKET && !parse_single)
        expr = parse_subscript(expr, parser, lxr);

    if(lxr.current().kind == BINARY_EXPR_OPERATOR && !parse_single)
        expr = parse_binary_expression(expr, parser, lxr);

    if(subexpression) {
        state = true;
        return expr;
    }


    if(lxr.current() == SEMICOLON || lxr.current() == COMMA) {
        if(parser.inside_parenthesized_expression) {
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
parse_parenthesized_expression(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == LPAREN, "Expected beginning of parenthesized expression.");

    ++parser.inside_parenthesized_expression;
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
parse_singleton_literal(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current().kind == LITERAL, "Expected literal.");

    auto* node         = new ast_singleton_literal();
    node->literal_type = lxr.current().type;


    if(node->literal_type == STRING_LITERAL || node->literal_type == CHARACTER_LITERAL) {
        const auto real = remove_escaped_chars(lxr.current().value);
        if(!real) {
            lxr.raise_error("String contains one or more invalid escaped characters.");
            delete node;
            return nullptr;
        }

        node->value = *real;
    }

    else {
        node->value = std::string(lxr.current().value);
    }

    lxr.advance(1);
    return node;
}


ast_node*
parse_braced_expression(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == LBRACE, "Expected left-brace.");

    bool  state = false;
    auto* node  = new ast_braced_expression();

    defer([&] {
        if(!state) { delete node; }
    });


    lxr.advance(1);
    while(lxr.current() != RBRACE) {

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

        if(lxr.current() == COMMA) {
            lxr.advance(1);
        }
    }


    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_unary_expression(parser& parser, lexer& lxr) {

    PARSER_ASSERT(VALID_UNARY_OPERATOR(lxr.current()), "Expected unary operator.");

    auto* node        = new ast_unaryexpr();
    node->_operator   = lxr.current().type;

    const size_t   src_pos  = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;


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
parse_assign(ast_node* assigned, parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == VALUE_ASSIGNMENT, "Expected '='.");

    const uint32_t line     = lxr.current().line;
    const size_t   src_pos  = lxr.current().src_pos;

    auto* node   = new ast_assign();
    bool  state  = false;

    defer([&]{ if(!state){delete node;} });


    lxr.advance(1);
    node->assigned         = assigned;
    node->assigned->parent = node;
    node->expression       = parse_expression(parser, lxr, true);

    if(node->expression == nullptr) {
        return nullptr;
    }


    const auto subexpr_type = node->expression->type;
    if(!VALID_SUBEXPRESSION(subexpr_type)) {
        lxr.raise_error("Invalid expression being assigned to variable", src_pos, line);
        return nullptr;
    }


    state = true;
    return node;
}


ast_node*
parse_call(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected called identifier.");


    //
    // First we need to validate that the called symbol is a procedure.
    // I COULD also just do this in the checker?? hmm....
    //

    uint32_t sym_index = 0;
    if(sym_index = parser.lookup_scoped_symbol(std::string(lxr.current().value)); sym_index == INVALID_SYMBOL_INDEX) {
        lxr.raise_error("Symbol does not exist in this scope.");
        return nullptr;
    }

    const auto* sym = parser.lookup_unique_symbol(sym_index);
    if(sym == nullptr) {
        return nullptr;
    }

    if(sym->type.sym_type != SYM_PROCEDURE) {
        lxr.raise_error("Attempt to call symbol that is not a procedure.");
        return nullptr;
    }


    //
    // Generate AST node.
    //

    bool  state      = false;
    auto* node       = new ast_call();
    node->identifier = new ast_identifier();

    node->identifier->parent       = node;
    node->identifier->symbol_index = sym_index;

    defer([&]{ if(!state){ delete node; }  });


    //
    // Parse the parameter list.
    //

    if(lxr.peek(1) != LPAREN) {
        lxr.raise_error("Expected parameter list.");
        return nullptr;
    }

    lxr.advance(2);
    if(lxr.current() == RPAREN) {
        lxr.advance(1);
        state = true;
        return node;
    }


    const uint16_t old_paren_index = parser.inside_parenthesized_expression++;
    while(old_paren_index < parser.inside_parenthesized_expression) {

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

        if(old_paren_index >= parser.inside_parenthesized_expression)
            break;

        if(lxr.current() == COMMA || lxr.current() == SEMICOLON) {
            lxr.advance(1);
            if(lxr.current() == RPAREN) {
                --parser.inside_parenthesized_expression;
                lxr.advance(1);
            }
        }
    }


    state = true;
    return node;
}


ast_node*
parse_binary_expression(ast_node* left_operand, parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current().kind == BINARY_EXPR_OPERATOR, "Expected binary operator.");
    PARSER_ASSERT(left_operand != nullptr, "Null left operand passed.");


    if(lxr.current() == VALUE_ASSIGNMENT) {
        return parse_assign(left_operand, parser, lxr);
    }


    bool  state    = false;
    auto* binexpr  = new ast_binexpr();

    defer([&]{ if(!state){ delete binexpr; } });

    binexpr->_operator       = lxr.current().type;
    binexpr->left_op         = left_operand;
    binexpr->left_op->parent = binexpr;


    const size_t   src_pos = lxr.current().src_pos;
    const uint32_t line    = lxr.current().line;

    lxr.advance(1);
    binexpr->right_op = parse_expression(parser, lxr, true);
    if(binexpr->right_op == nullptr) {
        return nullptr;
    }


    binexpr->right_op->parent = binexpr;
    const auto right_t        = binexpr->right_op->type;

    if(!VALID_SUBEXPRESSION(right_t)) {
        lxr.raise_error("Unexpected expression following binary operator.", src_pos, line);
        return nullptr;
    }

    state = true;
    return binexpr;
}


ast_node*
parse_member_access(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected identifier.");
    PARSER_ASSERT(lxr.peek(1) == DOT, "Expected dot as next token.");


    //
    // Scope check.
    //

    uint32_t sym_index = 0;
    if(sym_index = parser.lookup_scoped_symbol(std::string(lxr.current().value)); sym_index == INVALID_SYMBOL_INDEX) {
        lxr.raise_error("Symbol does not exist in this scope.");
        return nullptr;
    }

    const auto* sym = parser.lookup_unique_symbol(sym_index);
    if(sym == nullptr) {
        return nullptr;
    }


    //
    // Check struct type.
    //

    const auto* type_name = std::get_if<std::string>(&sym->type.name);
    if(type_name == nullptr || sym->type.sym_type != SYM_STRUCT) {
        lxr.raise_error("Attempted member access on non-struct type.");
        return nullptr;
    }

    if(!parser.type_exists(*type_name)) {
        lxr.raise_error("Type does not exist.");
        return nullptr;
    }

    const auto* _member_data = parser.lookup_type(*type_name);
    if(_member_data == nullptr) {
        return nullptr;
    }


    //
    // Check if the member exists.
    //

    lxr.advance(2);
    if(lxr.current() != IDENTIFIER) {
        lxr.raise_error("Expected struct member name.");
        return nullptr;
    }


    std::string path;
    auto looking = std::string(lxr.current().value);
    std::function<bool(decltype(_member_data))> get_member_path;


    //
    // Recurse through each sub-member (if they exist) until we find our target
    //

    get_member_path = [&](const decltype(_member_data) members) -> bool {

        if(members == nullptr) {
            lxr.raise_error("Struct does not have any members.");
            return false;
        }

        for(const auto& member : *members) {
            if(member.name != looking)
                continue;

            path += fmt(".{}", looking);
            if(lxr.peek(1) == DOT && lxr.peek(2) == IDENTIFIER) {
                lxr.advance(2);
                looking = std::string(lxr.current().value);

                auto* substruct_name = std::get_if<std::string>(&member.type.name);
                if(substruct_name == nullptr || member.type.sym_type != SYM_STRUCT) {
                    lxr.raise_error("Attempting to access member from non-struct type.");
                    return false;
                }

                return get_member_path(parser.lookup_type(*substruct_name));
            }

            lxr.advance(1);
            return true;
        }

        lxr.raise_error("Struct member does not exist.");
        return false;
    };

    if(!get_member_path(_member_data)) {
        return nullptr;
    }


    //
    // Generate AST node.
    //

    auto* node         = new ast_identifier();
    node->member_name  = path;
    node->symbol_index = sym->symbol_index;

    return node;
}


ast_node*
parse_identifier(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected identifier.");

    const auto next_type = lxr.peek(1).type;
    if(next_type == TYPE_ASSIGNMENT || next_type == CONST_TYPE_ASSIGNMENT) return parse_decl(parser, lxr);
    if(next_type == LPAREN) return parse_call(parser, lxr);
    if(next_type == DOT) return parse_member_access(parser, lxr);


    uint32_t sym_index = 0;
    if(sym_index = parser.lookup_scoped_symbol(std::string(lxr.current().value)); sym_index == INVALID_SYMBOL_INDEX) {
        lxr.raise_error("Symbol does not exist in this scope.");
        return nullptr;
    }


    auto* ident         = new ast_identifier();
    ident->symbol_index = sym_index;

    lxr.advance(1);
    return ident;
}