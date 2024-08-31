//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


tak::AstNode*
tak::parse_parenthesized_expression(Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_LPAREN);

    ++parser.inside_parenthesized_expression_;
    lxr.advance(1);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    auto* expr              = parse(parser, lxr, true);

    if(expr == nullptr) return nullptr;
    if(!NODE_VALID_SUBEXPRESSION(expr->type)) {
        lxr.raise_error("This expression cannot be used within parentheses.", curr_pos, line);
        delete expr;
        return nullptr;
    }

    return expr;
}


tak::AstNode*
tak::parse_cast(Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_KW_CAST);

    if(lxr.peek(1) != TOKEN_LPAREN) {
        lxr.raise_error("Expected '('.");
        return nullptr;
    }

    auto* node  = new AstCast(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    bool  state = false;

    defer_if(!state, [&] {
       delete node;
    });


    //
    // Parse cast target
    //

    lxr.advance(2);
    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    node->target            = parse(parser, lxr, true);

    if(node->target == nullptr)
        return nullptr;

    node->target->parent = node;
    if(!NODE_VALID_SUBEXPRESSION(node->target->type)) {
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
    if(!TOKEN_IDENT_START(lxr.current().type) && lxr.current().kind != KIND_TYPE_IDENTIFIER) {
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


tak::AstNode*
tak::parse_singleton_literal([[maybe_unused]] Parser& parser, Lexer& lxr) {

    assert(lxr.current().kind == KIND_LITERAL);

    bool  state        = false;
    auto* node         = new AstSingletonLiteral(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->literal_type = lxr.current().type;

    defer_if(!state, [&] {
        delete node;
    });


    //
    // Resolve escape sequences if they exist
    //

    if(node->literal_type == TOKEN_STRING_LITERAL || node->literal_type == TOKEN_CHARACTER_LITERAL) {
        const auto real = remove_escaped_chars(lxr.current().value);
        if(!real) {
            lxr.raise_error("String contains one or more invalid escaped characters.");
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
    state = true;
    return node;
}


tak::AstNode*
tak::parse_nullptr(Lexer& lxr) {

    assert(lxr.current() == TOKEN_KW_NULLPTR);

    auto* node         = new AstSingletonLiteral(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->literal_type = TOKEN_KW_NULLPTR;
    node->value        = std::string(lxr.current().value);

    lxr.advance(1);
    return node;
}


tak::AstNode*
tak::parse_member_access(AstNode* target, Lexer& lxr) {

    assert(lxr.current() == TOKEN_DOT);
    assert(target != nullptr);


    bool           state    = false;
    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node           = new AstMemberAccess(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->target         = target;
    node->target->parent = node;

    defer_if(!state, [&] {
        delete node;
    });


    while(lxr.current() == TOKEN_DOT && lxr.peek(1) == TOKEN_IDENTIFIER) {
        node->path += '.' + std::string(lxr.peek(1).value);
        lxr.advance(2);
    }

    if(node->path.empty()) {
        lxr.raise_error("Expected member access identifier after '.'", curr_pos, line);
        return nullptr;
    }

    state = true;
    return node;
}


tak::AstNode*
tak::parse_sizeof(Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_KW_SIZEOF);

    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state = false;
    auto* node  = new AstSizeof(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
       delete node;
    });


    lxr.advance(1);
    if(TOKEN_IDENT_START(lxr.current().type)) {

        std::string name_if_type;

        const Token    tmp_token  = lxr.current(); // save.
        const size_t   tmp_pos    = lxr.src_index_;
        const uint32_t tmp_line   = lxr.curr_line_;

        if(const auto name = get_namespaced_identifier(lxr)) {
            name_if_type = parser.tbl_.get_canonical_type_name(*name);
        } else {
            return nullptr;
        }

        lxr.current_   = tmp_token; // restore.
        lxr.src_index_ = tmp_pos;
        lxr.curr_line_ = tmp_line;

        if(parser.tbl_.type_exists(name_if_type) || parser.tbl_.type_alias_exists(name_if_type)) {
            if(const auto data = parse_type(parser,lxr)) {
                node->target = *data;
            } else {
                return nullptr;
            }

            state = true;
            return node;
        }
    }

    if(lxr.current().kind == KIND_TYPE_IDENTIFIER){
        if(const auto data = parse_type(parser,lxr)) {
            node->target = *data;
        } else {
            return nullptr;
        }
    }

    else {
        auto* target = parse(parser, lxr, true);
        if(target == nullptr) {
            return nullptr;
        }

        if(!NODE_VALID_SUBEXPRESSION(target->type)) {
            lxr.raise_error("Invalid subexpression used within sizeof identifier.", curr_pos, line);
            return nullptr;
        }

        target->parent = node;
        node->target   = target;
    }

    state = true;
    return node;
}


tak::AstNode*
tak::parse_braced_expression(Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_LBRACE);

    bool  state = false;
    auto* node  = new AstBracedExpression(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
        delete node;
    });


    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        node->members.emplace_back(parse(parser, lxr, true));
        if(node->members.back() == nullptr) {
            return nullptr;
        }

        if(!NODE_VALID_SUBEXPRESSION(node->members.back()->type)) {
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


tak::AstNode*
tak::parse_unary_expression(Parser& parser, Lexer& lxr) {

    assert(TOKEN_VALID_UNARY_OPERATOR(lxr.current()));

    const size_t   src_pos = lxr.current().src_pos;
    const uint32_t line    = lxr.current().line;

    auto* node      = new AstUnaryexpr(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->_operator = lxr.current().type;

    lxr.advance(1);
    node->operand = parse(parser, lxr, true, true);
    if(node->operand == nullptr) {
        delete node;
        return nullptr;
    }

    const auto right_t = node->operand->type;
    if(!NODE_VALID_SUBEXPRESSION(right_t)) {
        lxr.raise_error("Unexpected expression following unary operator.", src_pos, line);
        delete node;
        return nullptr;
    }

    node->operand->parent = node;
    return node;
}


tak::AstNode*
tak::parse_call(AstNode* operand, Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_LPAREN);

    //
    // Generate AST node.
    //

    const size_t   begin_pos  = lxr.current().src_pos;
    const uint32_t begin_line = lxr.current().line;

    bool  state           = false;
    auto* node            = new AstCall(begin_pos, begin_line, lxr.source_file_name_);
    node->target          = operand;
    node->target->parent  = node;

    defer_if(!state, [&] {
        delete node;
    });


    //
    // Parse out value parameters.
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

        auto* expr = parse(parser, lxr, true);
        if(expr == nullptr) {
            return nullptr;
        }

        const auto _type = expr->type;
        if(!NODE_VALID_SUBEXPRESSION(_type)) {
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


tak::AstNode*
tak::parse_binary_expression(AstNode* left_operand, Parser& parser, Lexer& lxr) {

    assert(lxr.current().kind == KIND_BINARY_EXPR_OPERATOR);
    assert(left_operand != nullptr);


    bool  state    = false;
    auto* binexpr  = new AstBinexpr(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
        delete binexpr;
    });

    binexpr->_operator       = lxr.current().type;
    binexpr->left_op         = left_operand;
    binexpr->left_op->parent = binexpr;

    const size_t   src_pos = lxr.current().src_pos;
    const uint32_t line    = lxr.current().line;


    lxr.advance(1);
    binexpr->right_op = parse(parser, lxr, true, true);
    if(binexpr->right_op == nullptr) {
        return nullptr;
    }

    binexpr->right_op->parent = binexpr;
    const auto right_t        = binexpr->right_op->type;

    if(!NODE_VALID_SUBEXPRESSION(right_t)) {
        lxr.raise_error("Unexpected expression following binary operator.", src_pos, line);
        return nullptr;
    }


    while(lxr.current().kind == KIND_BINARY_EXPR_OPERATOR && Token::precedence_of(lxr.current().type) <= Token::precedence_of(binexpr->_operator)) {
        binexpr->right_op = parse_binary_expression(binexpr->right_op, parser, lxr);
        if(binexpr->right_op == nullptr) {
            return nullptr;
        }
    }

    state = true;
    return binexpr;
}


tak::AstNode*
tak::parse_subscript(AstNode* operand, Parser& parser, Lexer& lxr) {

    assert(lxr.current() == TOKEN_LSQUARE_BRACKET);
    assert(operand != nullptr);
    lxr.advance(1);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node            = new AstSubscript(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    node->operand         = operand;
    node->operand->parent = node;
    node->value           = parse(parser, lxr, true);

    bool state = false;
    defer_if(!state, [&] {
        delete node;
    });


    if(node->value == nullptr)
        return nullptr;

    if(!NODE_VALID_SUBEXPRESSION(node->value->type) || lxr.current() != TOKEN_RSQUARE_BRACKET) {
        lxr.raise_error("Invalid expression within subscript operator.", curr_pos, line);
        return nullptr;
    }

    lxr.advance(1);
    state = true;
    return node;
}
