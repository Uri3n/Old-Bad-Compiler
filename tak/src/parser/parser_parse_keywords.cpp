//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_cont(lexer& lxr) {
    PARSER_ASSERT(lxr.current() == TOKEN_KW_CONT, "expected \"cont\" keyword.");
    lxr.advance(1);
    return new ast_cont();
}

ast_node*
parse_brk(lexer& lxr) {
    PARSER_ASSERT(lxr.current() == TOKEN_KW_BRK, "expected \"brk\" keyword.");
    lxr.advance(1);
    return new ast_brk();
}

ast_node*
parse_branch(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_IF, "Expected \"if\" keyword.");


    bool  state = false;
    auto* node  = new ast_branch();

    defer([&] {
       if(!state) { delete node; }
    });


    do {
        parser.push_scope();
        lxr.advance(1);

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        auto* if_stmt      = new ast_if();
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
            lxr.raise_error("Expression cannot be used within branch statement condition.", curr_pos, line);
            return nullptr;
        }

        if(lxr.current() != TOKEN_LBRACE) {
            lxr.raise_error("Expected branch body.");
            return nullptr;
        }


        //
        // Parse branch body
        //

        lxr.advance(1);
        while(lxr.current() != TOKEN_RBRACE) {
            if_stmt->body.emplace_back(parse_expression(parser, lxr, false));
            if(if_stmt->body.back() == nullptr) return nullptr;
            if_stmt->body.back()->parent = if_stmt;
        }

        lxr.advance(1);
        parser.pop_scope();
    } while(lxr.current() == TOKEN_KW_ELIF);


    //
    // Check if there's a final "else" branch
    //

    if(lxr.current() == TOKEN_KW_ELSE) {

        auto* else_stmt   = new ast_else();
        else_stmt->parent = node;
        node->_else       = else_stmt;

        if(lxr.peek(1) != TOKEN_LBRACE) {
            lxr.raise_error("Expected beginning of \"else\" branch body.");
            return nullptr;
        }

        lxr.advance(2);
        parser.push_scope();

        while(lxr.current() != TOKEN_RBRACE) {
            else_stmt->body.emplace_back(parse_expression(parser, lxr, false));
            if(else_stmt->body.back() == nullptr) return nullptr;
            else_stmt->body.back()->parent = else_stmt;
        }

        lxr.advance(1);
        parser.pop_scope();
    }

    state = true;
    return node;
}


ast_case*
parse_case(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_CASE || lxr.current() == TOKEN_KW_FALLTHROUGH, "Unexpected keyword.");
    parser.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state       = false;
    auto* node        = new ast_case();
    node->fallthrough = lxr.current() == TOKEN_KW_FALLTHROUGH;

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

    PARSER_ASSERT(lxr.current() == TOKEN_KW_DEFAULT, "Expected \"default\" keyword.");
    parser.push_scope();


    auto* node  = new ast_default();
    bool  state = false;

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

    PARSER_ASSERT(lxr.current() == TOKEN_KW_SWITCH, "Expected \"switch\" keyword.");
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state  = false;
    auto* node   = new ast_switch();
    node->target = parse_expression(parser, lxr, true);

    defer([&] {
        if(!state) { delete node; }
    })


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

    PARSER_ASSERT(lxr.current() == TOKEN_KW_RET, "Expected \"ret\" keyword.");
    auto* node = new ast_ret();

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

    PARSER_ASSERT(lxr.current() == TOKEN_KW_WHILE, "Expected \"while\" keyword.");

    lxr.advance(1);
    parser.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new ast_while();

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    node->condition = parse_expression(parser, lxr, true);
    if(node->condition == nullptr)
        return nullptr;

    node->condition->parent = node;
    if(!VALID_SUBEXPRESSION(node->condition->type) && node->condition->type != NODE_VARDECL) {
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

        else
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
parse_structdef(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_STRUCT, "Expected \"struct\" keyword.");

    if(parser.scope_stack.size() > 1) {
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

    if(parser.type_exists(type_name)) {
        lxr.raise_error("Naming conflict: type has already been defined elsewhere.");
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


ast_node*
parse_for(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_FOR, "Expected \"for\" keyword.");

    lxr.advance(1);
    parser.push_scope();


    auto* node  = new ast_for();
    bool  state = false;

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


ast_node*
parse_namespace(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_NAMESPACE, "Expected \"namespace\" keyword.");
    lxr.advance(1);


    if(parser.scope_stack.size() > 1) {
        lxr.raise_error("Namespace declaration at non-global scope.");
        return nullptr;
    }

    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected namespace identifier.");
        return nullptr;
    }

    if(!parser.enter_namespace(std::string(lxr.current().value))) { // Duplicate namespace.
        lxr.raise_error("Nested namespace has the same name as a parent.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_LBRACE) {
        lxr.raise_error("Expected '{' (Beginning of namespace block).");
        return nullptr;
    }


    bool  state     = false;
    auto* node      = new ast_namespacedecl();
    node->full_path = parser.namespace_as_string();

    defer([&] {
        if(!state) { delete node; }
        parser.leave_namespace();
    });


    lxr.advance(2);
    while(lxr.current() != TOKEN_RBRACE) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        node->children.emplace_back(parse_expression(parser, lxr, false));
        if(node->children.back() == nullptr)
            return nullptr;

        if(!VALID_AT_TOPLEVEL(node->children.back()->type)) {
            lxr.raise_error("Expression is invalid as a toplevel statement.", curr_pos, line);
            return nullptr;
        }

        node->children.back()->parent = node;
    }


    lxr.advance(1);
    state = true;
    return node;
}



ast_node*
parse_keyword(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current().kind == KIND_KEYWORD, "Expected keyword.");

    if(lxr.current() == TOKEN_KW_RET)       return parse_ret(parser, lxr);
    if(lxr.current() == TOKEN_KW_IF)        return parse_branch(parser, lxr);
    if(lxr.current() == TOKEN_KW_SWITCH)    return parse_switch(parser, lxr);
    if(lxr.current() == TOKEN_KW_WHILE)     return parse_while(parser, lxr);
    if(lxr.current() == TOKEN_KW_FOR)       return parse_for(parser, lxr);
    if(lxr.current() == TOKEN_KW_STRUCT)    return parse_structdef(parser, lxr);
    if(lxr.current() == TOKEN_KW_NAMESPACE) return parse_namespace(parser, lxr);


    lxr.raise_error("This keyword is not allowed here.");
    return nullptr;
}