//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_cont(lexer& lxr) {
    PARSER_ASSERT(lxr.current() == KW_CONT, "expected \"cont\" keyword.");
    lxr.advance(1);
    return new ast_cont();
}

ast_node*
parse_brk(lexer& lxr) {
    PARSER_ASSERT(lxr.current() == KW_BRK, "expected \"brk\" keyword.");
    lxr.advance(1);
    return new ast_brk();
}

ast_node*
parse_branch(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current() == KW_IF, "Expected \"if\" keyword.");


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

        if(lxr.current() != LBRACE) {
            lxr.raise_error("Expected branch body.");
            return nullptr;
        }


        //
        // Parse branch body
        //

        lxr.advance(1);
        while(lxr.current() != RBRACE) {
            if_stmt->body.emplace_back(parse_expression(parser, lxr, false));
            if(if_stmt->body.back() == nullptr) return nullptr;
            if_stmt->body.back()->parent = if_stmt;
        }

        lxr.advance(1);
        parser.pop_scope();
    } while(lxr.current() == KW_ELIF);


    //
    // Check if there's a final "else" branch
    //

    if(lxr.current() == KW_ELSE) {

        auto* else_stmt   = new ast_else();
        else_stmt->parent = node;
        node->_else       = else_stmt;

        if(lxr.peek(1) != LBRACE) {
            lxr.raise_error("Expected beginning of \"else\" branch body.");
            return nullptr;
        }

        lxr.advance(2);
        parser.push_scope();

        while(lxr.current() != RBRACE) {
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

    PARSER_ASSERT(lxr.current() == KW_CASE || lxr.current() == KW_FALLTHROUGH, "Unexpected keyword.");
    parser.push_scope();


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state       = false;
    auto* node        = new ast_case();
    node->fallthrough = lxr.current() == KW_FALLTHROUGH;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    lxr.advance(1);
    node->value = dynamic_cast<ast_singleton_literal*>(parse_expression(parser, lxr, true));

    if(node->value == nullptr
        || node->value->literal_type == STRING_LITERAL
        || node->value->literal_type == FLOAT_LITERAL
        ) {
        lxr.raise_error("Case value must be a constant, integer literal.", curr_pos, line);
        return nullptr;
    }

    if(lxr.current() != LBRACE) {
        lxr.raise_error("Expected '{' (beginning of case body).");
        return nullptr;
    }


    lxr.advance(1);
    while(lxr.current() != RBRACE) {
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

    PARSER_ASSERT(lxr.current() == KW_DEFAULT, "Expected \"default\" keyword.");
    parser.push_scope();


    auto* node  = new ast_default();
    bool  state = false;

    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    if(lxr.peek(1) != LBRACE) {
        lxr.raise_error("Expected '{' after \"default\" (case body is missing).");
        return nullptr;
    }

    lxr.advance(2);
    while(lxr.current() != RBRACE) {
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

    PARSER_ASSERT(lxr.current() == KW_SWITCH, "Expected \"switch\" keyword.");
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

    if(lxr.current() != LBRACE) {
        lxr.raise_error("Expected beginning of switch body.");
        return nullptr;
    }


    lxr.advance(1);
    bool reached_default = false;
    while(lxr.current() != RBRACE) {

        if(lxr.current() == KW_CASE || lxr.current() == KW_FALLTHROUGH) {
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

        else if(lxr.current() == KW_DEFAULT) {
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

    PARSER_ASSERT(lxr.current() == KW_RET, "Expected \"ret\" keyword.");
    auto* node = new ast_ret();

    lxr.advance(1);
    if(lxr.current() == SEMICOLON || lxr.current() == COMMA) {
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

    PARSER_ASSERT(lxr.current() == KW_WHILE, "Expected \"while\" keyword.");

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

    if(lxr.current() != LBRACE) {
        lxr.raise_error("Expected '{' (start of loop body)");
        return nullptr;
    }


    lxr.advance(1);
    while(lxr.current() != RBRACE) {

        if(lxr.current() == KW_CONT) {
            if(lxr.peek(1) != SEMICOLON && lxr.peek(1) != COMMA) {
                lxr.raise_error("Unxpected end of expression after \"cont\" keyword.");
                return nullptr;
            }

            node->body.emplace_back(parse_cont(lxr));
            lxr.advance(1);
        }

        else if(lxr.current() == KW_BRK) {
            if(lxr.peek(1) != SEMICOLON && lxr.peek(1) != COMMA) {
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

    PARSER_ASSERT(lxr.current() == KW_STRUCT, "Expected \"struct\" keyword.");

    if(parser.scope_stack.size() <= 1) {
        lxr.raise_error("Struct definition at non-global scope.");
        return nullptr;
    }


    //
    // Check for type redeclaration
    //

    lxr.advance(1);
    if(lxr.current() != IDENTIFIER) {
        lxr.raise_error("Expected struct name.");
        return nullptr;
    }

    if(parser.type_exists(std::string(lxr.current().value))) {
        lxr.raise_error("Naming conflict: type has already been defined elsewhere.");
        return nullptr;
    }


    const auto type_name = std::string(lxr.current().value);
    std::vector<member_data> members;

    if(lxr.peek(1) != LBRACE) {
        lxr.raise_error("Unexpected end of struct definition.");
    }


    //
    // Parse out each struct member
    //

    lxr.advance(2);
    while(lxr.current() != RBRACE) {

        if(lxr.current() != IDENTIFIER) {
            lxr.raise_error("Expected identifier.");
            return nullptr;
        }


        auto name     = std::string(lxr.current().value);
        bool is_const = false;

        const size_t src_pos = lxr.current().src_pos;
        const uint32_t line  = lxr.current().line;


        lxr.advance(1);
        if(lxr.current() == CONST_TYPE_ASSIGNMENT) {
            is_const = true;
        } else if(lxr.current() != TYPE_ASSIGNMENT) {
            lxr.raise_error("Expected type assignment.");
            return nullptr;
        }


        lxr.advance(1);
        if(lxr.current().kind != TYPE_IDENTIFIER && lxr.current() != IDENTIFIER) {
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


        if(lxr.current() == COMMA || lxr.current() == SEMICOLON) {
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
parse_keyword(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current().kind == KEYWORD, "Expected keyword.");

    if(lxr.current() == KW_RET)    return parse_ret(parser, lxr);
    if(lxr.current() == KW_IF)     return parse_branch(parser, lxr);
    if(lxr.current() == KW_SWITCH) return parse_switch(parser, lxr);
    if(lxr.current() == KW_WHILE)  return parse_while(parser, lxr);
    if(lxr.current() == KW_STRUCT) return parse_structdef(parser, lxr);


    lxr.raise_error("This keyword is not allowed here.");
    return nullptr;
}