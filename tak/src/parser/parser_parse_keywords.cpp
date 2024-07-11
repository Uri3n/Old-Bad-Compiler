//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_branch(parser &parser, lexer &lxr) {
    return nullptr;
}


ast_node*
parse_ret(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current() == KW_RET, "Expected \"ret\" keyword.");
    auto* node = new ast_ret();

    lxr.advance(1);
    if(lxr.current() == SEMICOLON || lxr.current() == COMMA) {
        return node;
    }


    node->value      = parse_expression(parser, lxr, true);
    const auto _type = (*node->value)->type;

    if(!VALID_SUBEXPRESSION(_type)) {
        lxr.raise_error("Invalid expression after return statement.");
        delete node;
        return nullptr;
    }

    return node;
}

/*
struct foo {
    member1: i32;
    member2: i8^^;
};

*/

ast_node*
parse_structdef(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == KW_STRUCT, "Expected \"struct\" keyword.");

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


    auto type_name = std::string(lxr.current().value);
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

        if(lxr.current().value == name) {
            lxr.raise_error("A struct cannot contain itself.");
            return nullptr;
        }


        if(const auto type = parse_type(parser, lxr)) {
            if(type->sym_type == SYM_PROCEDURE && type->pointer_depth < 1) {
                lxr.raise_error("Procedures cannot be used as struct members.", src_pos, line);
                return nullptr;
            }

            members.emplace_back(member_data(name, *type));
            if(is_const) {
                members.back().type.flags |= SYM_IS_CONSTANT;
            }
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

    return node;
}


ast_node*
parse_keyword(parser &parser, lexer &lxr) {

    PARSER_ASSERT(lxr.current().kind == KEYWORD, "Expected keyword.");

    if(lxr.current() == KW_CONT)   return new ast_cont();
    if(lxr.current() == KW_BRK)    return new ast_brk();
    if(lxr.current() == KW_RET)    return parse_ret(parser, lxr);
    if(lxr.current() == KW_IF)     return parse_branch(parser, lxr);
    if(lxr.current() == KW_STRUCT) return parse_structdef(parser, lxr);

    lxr.raise_error("This keyword is not allowed here.");
    return nullptr;
}