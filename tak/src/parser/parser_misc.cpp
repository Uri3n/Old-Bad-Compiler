//
// Created by Diago on 2024-07-02.
//

#include <parser.hpp>

ast_binexpr::~ast_binexpr() {
    delete left_op;
    delete right_op;
}

ast_branch::~ast_branch() {
    for(ast_node const* node : conditions) {
        delete node;
    }

    delete _else;
}

ast_assign::~ast_assign() {
    delete expression;
    delete identifier;
}

ast_decl::~ast_decl() {
    for(ast_node const* node : children) {
        delete node;
    }

    delete identifier;
}

ast_call::~ast_call() {
    for(ast_node const* node : arguments) {
        delete node;
    }

    delete identifier;
}

ast_for::~ast_for() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete condition;
    delete initialization;
    delete update;
}

ast_while::~ast_while() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete condition;
}

ast_unaryexpr::~ast_unaryexpr() {
    delete operand;
}

ast_multi_literal::~ast_multi_literal() {
    for(ast_node const* node : members) {
        delete node;
    }
}

parser::~parser() {
    for(ast_node const* node : toplevel_decls) {
        delete node;
    }
}
