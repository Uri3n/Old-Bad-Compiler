//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


tak::AstNode*
tak::parse_cont(Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_CONT);

    auto* node = new AstCont(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    lxr.advance(1);
    return node;
}


tak::AstNode*
tak::parse_brk(Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_BRK);

    auto* node = new AstBrk(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    lxr.advance(1);
    return node;
}


tak::AstNode*
tak::parse_branch(Parser &parser, Lexer &lxr) {
    assert(lxr.current() == TOKEN_KW_IF);

    bool  state = false;
    auto* node  = new AstBranch(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
        delete node;
    });


    parser.tbl_.push_scope();
    lxr.advance(1);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* if_stmt      = new AstIf(curr_pos, line, lxr.source_file_name_);
    if_stmt->parent    = node;
    if_stmt->condition = parse(parser, lxr, true);


    //
    // Parse branch condition
    //

    node->_if = if_stmt;
    if(if_stmt->condition == nullptr) {
        return nullptr;
    }

    if(!NODE_VALID_SUBEXPRESSION(if_stmt->condition->type) && if_stmt->condition->type != NODE_VARDECL) {
        lxr.raise_error("Expression cannot be used within if statement condition.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() == TOKEN_LBRACE) {
        lxr.advance(1);
        while(lxr.current() != TOKEN_RBRACE) {
            if_stmt->body.emplace_back(parse(parser, lxr, false));
            if(if_stmt->body.back() == nullptr) return nullptr;
            if_stmt->body.back()->parent = if_stmt;
        }
        lxr.advance(1);
    } else {
        if_stmt->body.emplace_back(parse(parser, lxr, false));
        if(if_stmt->body.back() == nullptr) return nullptr;
        if_stmt->body.back()->parent = if_stmt;
    }


    //
    // Additional  "else" block
    //

    parser.tbl_.pop_scope();
    if(lxr.current() == TOKEN_KW_ELSE) {

        auto* else_stmt   = new AstElse(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
        else_stmt->parent = node;
        node->_else       = else_stmt;

        lxr.advance(1);
        parser.tbl_.push_scope();

        if(lxr.current() == TOKEN_LBRACE) {
            lxr.advance(1);
            while(lxr.current() != TOKEN_RBRACE) {
                else_stmt->body.emplace_back(parse(parser, lxr, false));
                if(else_stmt->body.back() == nullptr) return nullptr;
                else_stmt->body.back()->parent = else_stmt;
            }
            lxr.advance(1);
        } else {
            else_stmt->body.emplace_back(parse(parser, lxr, false));
            if(else_stmt->body.back() == nullptr) return nullptr;
            else_stmt->body.back()->parent = else_stmt;
        }

        parser.tbl_.pop_scope();
    }

    state = true;
    return node;
}


tak::AstCase*
tak::parse_case(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_CASE || lxr.current() == TOKEN_KW_FALLTHROUGH);
    parser.tbl_.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state       = false;
    auto* node        = new AstCase(curr_pos, line, lxr.source_file_name_);
    node->fallthrough = lxr.current() == TOKEN_KW_FALLTHROUGH;

    defer([&] {
        if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    lxr.advance(1);
    node->value = dynamic_cast<AstSingletonLiteral*>(parse(parser, lxr, true));

    if(node->value == nullptr
        || node->value->literal_type == TOKEN_STRING_LITERAL
        || node->value->literal_type == TOKEN_FLOAT_LITERAL
        || node->value->literal_type == TOKEN_KW_NULLPTR
    ) {
        lxr.raise_error("Invalid case value.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        lxr.advance(1);
        state = true;
        return node;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (beginning of case body).");
        return nullptr;
    }

    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse(parser, lxr, false));
        if(node->body.back() == nullptr) return nullptr;
        node->body.back()->parent = node;
    }

    lxr.advance(1);
    node->value->parent = node;
    state = true;
    return node;
}


tak::AstDefault*
tak::parse_default(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_DEFAULT);
    parser.tbl_.push_scope();


    auto* node  = new AstDefault(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    bool  state = false;

    defer([&] {
        if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    lxr.advance(1);
    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        lxr.advance(1);
        state = true;
        return node;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"default\" (case body is missing).");
        return nullptr;
    }

    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        const auto& child = node->body.emplace_back(parse(parser, lxr, false));
        if(child == nullptr) {
            return nullptr;
        }

        child->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}


tak::AstNode*
tak::parse_switch(Parser &parser, Lexer &lxr) {
    assert(lxr.current() == TOKEN_KW_SWITCH);
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state  = false;
    auto* node   = new AstSwitch(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->target = parse(parser, lxr, true);

    defer_if(!state, [&] {
        delete node;
    });


    if(node->target == nullptr) {
        return nullptr;
    }
    if(!NODE_VALID_SUBEXPRESSION(node->target->type)) {
        lxr.raise_error("Invalid subexpression being used as a switch target.", curr_pos, line);
        return nullptr;
    }
    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected beginning of switch body.");
        return nullptr;
    }


    lxr.advance(1);
    bool reached_default = false;
    while(lxr.current() != TOKEN_RBRACE)
    {
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


tak::AstNode*
tak::parse_ret(Parser &parser, Lexer &lxr) {
    assert(lxr.current() == TOKEN_KW_RET);

    auto* node = new AstRet(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    lxr.advance(1);
    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        return node;
    }

    node->value = parse(parser, lxr, true);
    if(node->value == nullptr) {
        delete node;
        return nullptr;
    }

    const auto _type = (*node->value)->type;
    if(!NODE_VALID_SUBEXPRESSION(_type)) {
        lxr.raise_error("Invalid expression after return statement.");
        delete node;
        return nullptr;
    }

    return node;
}


tak::AstNode*
tak::parse_while(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_WHILE);

    lxr.advance(1);
    parser.tbl_.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new AstWhile(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer([&] {
        if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    node->condition = parse(parser, lxr, true);
    if(node->condition == nullptr)
        return nullptr;

    node->condition->parent = node;
    if(!NODE_VALID_SUBEXPRESSION(node->condition->type)) {
        lxr.raise_error("Invalid \"while\" condition.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (start of loop body)");
        return nullptr;
    }


    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse(parser, lxr, false));
        if(node->body.back() == nullptr) {
            return nullptr;
        }

        node->body.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}


tak::AstNode*
tak::parse_block(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_BLK);

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"blk\" keyword (start of scope block).");
        return nullptr;
    }

    auto* node  = new AstBlock(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    bool  state = false;

    defer([&] {
       if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    parser.tbl_.push_scope();
    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {
        node->children.emplace_back(parse(parser, lxr, false));
        if(node->children.back() == nullptr)
            return nullptr;

        node->children.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}


tak::AstNode*
tak::parse_defer_if(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_DEFER_IF);
    lxr.advance(1);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state     = false;
    auto* node      = new AstDeferIf(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->condition = parse(parser, lxr, true);

    defer_if(!state, [&] {
        delete node;
    });


    if(node->condition == nullptr) {
        return nullptr;
    }

    if(!NODE_VALID_SUBEXPRESSION(node->condition->type)) {
        lxr.raise_error("Invalid subexpression used as defer_if condition.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != TOKEN_COMMA && lxr.current() != TOKEN_SEMICOLON) {
        lxr.raise_error("Expected end of expression.");
        return nullptr;
    }

    lxr.advance(1);
    node->call = parse(parser, lxr, true);
    if(node->call == nullptr) {
        return nullptr;
    }

    if(node->call->type != NODE_CALL) {
        lxr.raise_error("defer_if statement does not have a valid procedure call following its condition.", curr_pos, line);
        return nullptr;
    }


    node->condition->parent = node;
    node->call->parent      = node;

    state = true;
    return node;
}


tak::AstNode*
tak::parse_defer(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_DEFER);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new AstDefer(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
        delete node;
    });


    lxr.advance(1);
    node->call = parse(parser, lxr, true);
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


tak::AstNode*
tak::parse_dowhile(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_DO);

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' after \"do\" (start of do-while body).");
        return nullptr;
    }

    bool  state = false;
    auto* node  = new AstDoWhile(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer([&] {
        if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    lxr.advance(2);
    parser.tbl_.push_scope();

    while(lxr.current() != TOKEN_RBRACE) {
        node->body.emplace_back(parse(parser,lxr,false));
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
    node->condition       = parse(parser,lxr,true);

    if(node->condition == nullptr)
        return nullptr;

    if(!NODE_VALID_SUBEXPRESSION(node->condition->type)) {
        lxr.raise_error("Invalid expression used as while condition.", curr_pos, line);
        return nullptr;
    }

    node->condition->parent = node;
    state = true;
    return node;
}


tak::AstNode*
tak::parse_for(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_KW_FOR);

    lxr.advance(1);
    parser.tbl_.push_scope();

    auto* node  = new AstFor(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    bool  state = false;

    size_t   curr_pos = 0;
    uint32_t line     = 0;

    defer([&] {
        if(!state) { delete node; }
        parser.tbl_.pop_scope();
    });


    //
    // Initialization
    //

    if(lxr.current() == TOKEN_COMMA  || lxr.current() == TOKEN_SEMICOLON)
        lxr.advance(1);

    else {
        curr_pos = lxr.current().src_pos;
        line     = lxr.current().line;

        node->init = parse(parser, lxr, true);
        if(*node->init == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->init)->type;
        if(!NODE_VALID_SUBEXPRESSION(init_t) && init_t != NODE_VARDECL) {
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

        node->condition = parse(parser, lxr, true);
        if(*node->condition == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->condition)->type;
        if(!NODE_VALID_SUBEXPRESSION(init_t)) {
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

        node->update = parse(parser, lxr, true);
        if(*node->update == nullptr) {
            return nullptr;
        }

        const auto init_t = (*node->update)->type;
        if(!NODE_VALID_SUBEXPRESSION(init_t)) {
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
        node->body.emplace_back(parse(parser, lxr, false));
        if(node->body.back() == nullptr) return nullptr;
        node->body.back()->parent = node;
    }

    lxr.advance(1);
    state = true;
    return node;
}
