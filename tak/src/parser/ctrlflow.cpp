//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_cont(lexer& lxr) {
    parser_assert(lxr.current() == TOKEN_KW_CONT, "expected \"cont\" keyword.");

    auto* node = new ast_cont();
    node->pos = lxr.current().src_pos;

    lxr.advance(1);
    return node;
}


ast_node*
parse_brk(lexer& lxr) {
    parser_assert(lxr.current() == TOKEN_KW_BRK, "expected \"brk\" keyword.");

    auto* node = new ast_brk();
    node->pos = lxr.current().src_pos;

    lxr.advance(1);
    return node;
}


ast_node*
parse_branch(parser &parser, lexer &lxr) {

    parser_assert(lxr.current() == TOKEN_KW_IF, "Expected \"if\" keyword.");

    bool  state = false;
    auto* node  = new ast_branch();
    node->pos = lxr.current().src_pos;

    defer_if(!state, [&] {
        delete node;
    });


    do {
        parser.push_scope();
        lxr.advance(1);

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        auto* if_stmt      = new ast_if();
        if_stmt->pos       = lxr.current().src_pos;
        if_stmt->parent    = node;
        if_stmt->condition = parse_expression(parser, lxr, true);


        //
        // Parse branch condition
        //

        node->conditions.emplace_back(if_stmt);
        if(if_stmt->condition == nullptr) {
            return nullptr;
        }

        if(!VALID_SUBEXPRESSION(if_stmt->condition->type) && if_stmt->condition->type != NODE_VARDECL) {
            lxr.raise_error("Expression cannot be used within if statement condition.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() == TOKEN_LBRACE) {
            lxr.advance(1);
            while(lxr.current() != TOKEN_RBRACE) {
                if_stmt->body.emplace_back(parse_expression(parser, lxr, false));
                if(if_stmt->body.back() == nullptr) return nullptr;
                if_stmt->body.back()->parent = if_stmt;
            }
            lxr.advance(1);
        } else {
            if_stmt->body.emplace_back(parse_expression(parser, lxr, false));
            if(if_stmt->body.back() == nullptr) return nullptr;
            if_stmt->body.back()->parent = if_stmt;
        }

        parser.pop_scope();
    } while(lxr.current() == TOKEN_KW_ELIF);


    //
    // Check if there's a final "else" branch
    //

    if(lxr.current() == TOKEN_KW_ELSE) {

        auto* else_stmt   = new ast_else();
        else_stmt->pos    = lxr.current().src_pos;
        else_stmt->parent = node;
        node->_else       = else_stmt;

        lxr.advance(1);
        parser.push_scope();

        if(lxr.current() == TOKEN_LBRACE) {
            lxr.advance(1);
            while(lxr.current() != TOKEN_RBRACE) {
                else_stmt->body.emplace_back(parse_expression(parser, lxr, false));
                if(else_stmt->body.back() == nullptr) return nullptr;
                else_stmt->body.back()->parent = else_stmt;
            }
            lxr.advance(1);
        } else {
            else_stmt->body.emplace_back(parse_expression(parser, lxr, false));
            if(else_stmt->body.back() == nullptr) return nullptr;
            else_stmt->body.back()->parent = else_stmt;
        }

        parser.pop_scope();
    }

    state = true;
    return node;
}


ast_case*
parse_case(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_CASE || lxr.current() == TOKEN_KW_FALLTHROUGH, "Unexpected keyword.");
    parser.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state       = false;
    auto* node        = new ast_case();
    node->fallthrough = lxr.current() == TOKEN_KW_FALLTHROUGH;
    node->pos         = curr_pos;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    lxr.advance(1);
    node->value = dynamic_cast<ast_singleton_literal*>(parse_expression(parser, lxr, true));

    if(node->value == nullptr
        || node->value->literal_type == TOKEN_STRING_LITERAL
        || node->value->literal_type == TOKEN_FLOAT_LITERAL
    ) {
        lxr.raise_error("Case value must be a constant, integer literal.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (beginning of case body).");
        return nullptr;
    }


    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse_expression(parser, lxr, false));
        if(node->body.back() == nullptr) return nullptr;
        node->body.back()->parent = node;
    }

    node->value->parent = node;
    lxr.advance(1);
    state = true;
    return node;
}


ast_default*
parse_default(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_DEFAULT, "Expected \"default\" keyword.");
    parser.push_scope();


    auto* node  = new ast_default();
    bool  state = false;
    node->pos   = lxr.current().src_pos;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"default\" (case body is missing).");
        return nullptr;
    }

    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {
        const auto& child = node->body.emplace_back(parse_expression(parser, lxr, false));
        if(child == nullptr) {
            return nullptr;
        }

        child->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_switch(parser &parser, lexer &lxr) {

    parser_assert(lxr.current() == TOKEN_KW_SWITCH, "Expected \"switch\" keyword.");
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state  = false;
    auto* node   = new ast_switch();
    node->target = parse_expression(parser, lxr, true);
    node->pos    = curr_pos;

    defer_if(!state, [&] {
        delete node;
    });


    if(node->target == nullptr)
        return nullptr;

    if(!VALID_SUBEXPRESSION(node->target->type)) {
        lxr.raise_error("Invalid subexpression being used as a switch target.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected beginning of switch body.");
        return nullptr;
    }


    lxr.advance(1);
    bool reached_default = false;
    while(lxr.current() != TOKEN_RBRACE) {

        if(lxr.current() == TOKEN_KW_CASE || lxr.current() == TOKEN_KW_FALLTHROUGH) {
            if(reached_default) {
                lxr.raise_error("Case definition after \"default\".");
                return nullptr;
            }

            const size_t   case_pos  = lxr.current().src_pos;
            const uint32_t case_line = lxr.current().line;
            auto*          new_case  = parse_case(parser, lxr);

            if(new_case == nullptr) {
                return nullptr;
            }

            for(const auto& _case : node->cases) {
                if(_case->value->literal_type == new_case->value->literal_type // Duplicate case value.
                    && _case->value->value == new_case->value->value
                ) {
                    lxr.raise_error("Case pertains to the same value as a previous one.", case_pos, case_line);
                    return nullptr;
                }
            }

            new_case->parent = node;
            node->cases.emplace_back(new_case);
        }

        else if(lxr.current() == TOKEN_KW_DEFAULT) {
            if(reached_default) {
                lxr.raise_error("Multiple definitions of default case.");
                return nullptr;
            }

            node->_default = parse_default(parser, lxr);
            if(node->_default == nullptr)
                return nullptr;

            node->_default->parent = node;
            reached_default = true;
        }

        else {
            lxr.raise_error("Unexpected token in switch body.");
            return nullptr;
        }
    }


    if(!reached_default) {
        lxr.raise_error("Unexpected end of switch body: all switches must contain a default case.");
        return nullptr;
    }

    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_ret(parser &parser, lexer &lxr) {

    parser_assert(lxr.current() == TOKEN_KW_RET, "Expected \"ret\" keyword.");

    auto* node = new ast_ret();
    node->pos  = lxr.current().src_pos;

    lxr.advance(1);
    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        return node;
    }


    node->value = parse_expression(parser, lxr, true);
    if(node->value == nullptr) {
        delete node;
        return nullptr;
    }

    const auto _type = (*node->value)->type;
    if(!VALID_SUBEXPRESSION(_type)) {
        lxr.raise_error("Invalid expression after return statement.");
        delete node;
        return nullptr;
    }

    return node;
}


ast_node*
parse_while(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_WHILE, "Expected \"while\" keyword.");

    lxr.advance(1);
    parser.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new ast_while();
    node->pos   = curr_pos;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    node->condition = parse_expression(parser, lxr, true);
    if(node->condition == nullptr)
        return nullptr;

    node->condition->parent = node;
    if(!VALID_SUBEXPRESSION(node->condition->type)) {
        lxr.raise_error("Invalid \"while\" condition.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (start of loop body)");
        return nullptr;
    }


    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {

        if(lxr.current() == TOKEN_KW_CONT) {
            if(lxr.peek(1) != TOKEN_SEMICOLON && lxr.peek(1) != TOKEN_COMMA) {
                lxr.raise_error("Unxpected end of expression after \"cont\" keyword.");
                return nullptr;
            }

            node->body.emplace_back(parse_cont(lxr));
            lxr.advance(1);
        }

        else if(lxr.current() == TOKEN_KW_BRK) {
            if(lxr.peek(1) != TOKEN_SEMICOLON && lxr.peek(1) != TOKEN_COMMA) {
                lxr.raise_error("Unxpected end of expression after \"brk\" keyword.");
                return nullptr;
            }

            node->body.emplace_back(parse_brk(lxr));
            lxr.advance(1);
        }

        else {
            node->body.emplace_back(parse_expression(parser, lxr, false));
        }

        if(node->body.back() == nullptr) {
            return nullptr;
        }

        node->body.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}

ast_node*
parse_block(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_BLK, "Expected \"blk\" keyword.");

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"blk\" keyword (start of scope block).");
        return nullptr;
    }


    auto* node  = new ast_block();
    bool  state = false;
    node->pos   = lxr.current().src_pos;

    defer([&] {
       if(!state) { delete node; }
        parser.pop_scope();
    });


    parser.push_scope();
    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse_expression(parser, lxr, false));
        if(node->body.back() == nullptr)
            return nullptr;

        node->body.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_defer(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_DEFER, "Expected \"defer\" keyword.");


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new ast_defer();
    node->pos   = curr_pos;

    defer_if(!state, [&] {
        delete node;
    });


    lxr.advance(1);
    node->call = parse_expression(parser, lxr, true);
    if(node->call == nullptr)
        return nullptr;

    if(node->call->type != NODE_CALL) {
        lxr.raise_error("Expression following \"defer\" statement must be a procedure call.", curr_pos, line);
        return nullptr;
    }

    node->call->parent = node;
    state = true;
    return node;
}


ast_node*
parse_dowhile(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_DO, "Expected \"do\" keyword.");

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"do\" (start of do-while body).");
        return nullptr;
    }


    bool  state = false;
    auto* node  = new ast_dowhile();
    node->pos   = lxr.current().src_pos;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    lxr.advance(2);
    parser.push_scope();

    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse_expression(parser,lxr,false));
        if(node->body.back() == nullptr) return nullptr;
        node->body.back()->parent = node;
    }

    if(lxr.peek(1) != TOKEN_KW_WHILE) {
        lxr.raise_error("Expected \"while\" keyword after \"do\" block.");
        return nullptr;
    }


    lxr.advance(2);
    const size_t curr_pos = lxr.current().src_pos;
    const uint32_t line   = lxr.current().line;
    node->condition       = parse_expression(parser,lxr,true);

    if(node->condition == nullptr)
        return nullptr;

    if(!VALID_SUBEXPRESSION(node->condition->type)) {
        lxr.raise_error("Invalid expression used as while condition.", curr_pos, line);
        return nullptr;
    }


    node->condition->parent = node;
    state = true;
    return node;
}


ast_node*
parse_for(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_FOR, "Expected \"for\" keyword.");

    lxr.advance(1);
    parser.push_scope();


    auto* node  = new ast_for();
    bool  state = false;
    node->pos   = lxr.current().src_pos;

    size_t   curr_pos = 0;
    uint32_t line     = 0;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    //
    // Initialization
    //

    if(lxr.current() == TOKEN_COMMA  || lxr.current() == TOKEN_SEMICOLON)
        lxr.advance(1);

    else {
        curr_pos = lxr.current().src_pos;
        line     = lxr.current().line;

        node->init = parse_expression(parser, lxr, true);
        if(*node->init == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->init)->type;
        if(!VALID_SUBEXPRESSION(init_t) && init_t != NODE_VARDECL) {
            lxr.raise_error("Invalid subexpression used as part of for-loop initialization.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() != TOKEN_COMMA && lxr.current() != TOKEN_SEMICOLON) {
            lxr.raise_error("Expected ';' or ','.");
            return nullptr;
        }


        (*node->init)->parent = node;
        lxr.advance(1);
    }


    //
    // Condition ( A lot of duplicate code here... kinda bad )
    //

    if(lxr.current() == TOKEN_COMMA  || lxr.current() == TOKEN_SEMICOLON)
        lxr.advance(1);

    else {
        curr_pos = lxr.current().src_pos;
        line     = lxr.current().line;

        node->condition = parse_expression(parser, lxr, true);
        if(*node->condition == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->condition)->type;
        if(!VALID_SUBEXPRESSION(init_t)) {
            lxr.raise_error("Invalid subexpression used as part of for-loop condition.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() != TOKEN_COMMA && lxr.current() != TOKEN_SEMICOLON) {
            lxr.raise_error("Expected ';' or ','.");
            return nullptr;
        }

        (*node->condition)->parent = node;
        lxr.advance(1);
    }


    //
    // Update
    //

    if(lxr.current() != TOKEN_LBRACE) {
        curr_pos = lxr.current().src_pos;
        line     = lxr.current().line;

        node->update = parse_expression(parser, lxr, true);
        if(*node->update == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->update)->type;
        if(!VALID_SUBEXPRESSION(init_t)) {
            lxr.raise_error("Invalid subexpression used as part of for-loop update.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() != TOKEN_LBRACE) {
            lxr.raise_error("Expected '{' (start of loop body).");
            return nullptr;
        }

        (*node->update)->parent = node;
    }


    //
    // Loop body
    //

    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse_expression(parser, lxr, false));
        if(node->body.back() == nullptr) return nullptr;
        node->body.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}
