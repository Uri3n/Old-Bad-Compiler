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

    if(_else.has_value()) {
        delete *_else;
    }
}

ast_if::~ast_if() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete condition;
}

ast_else::~ast_else() {
    for(ast_node const* node : body) {
        delete node;
    }
}

ast_procdecl::~ast_procdecl() {
    for(ast_node const* node : body) {
        delete node;
    }

    for(ast_node const* node : parameters) {
        delete node;
    }

    delete identifier;
}

ast_vardecl::~ast_vardecl() {
    if(init_value.has_value()) {
        delete *init_value;
    }

    delete identifier;
}

ast_call::~ast_call() {
    for(ast_node const* node : arguments) {
        delete node;
    }

    delete identifier;
}

ast_enumdef::~ast_enumdef() {
    delete _namespace;
    delete alias;
}

ast_cast::~ast_cast() {
    delete target;
}

ast_block::~ast_block() {
    for(ast_node const* node : body) {
        delete node;
    }
}

ast_subscript::~ast_subscript() {
    delete operand;
    delete value;
}

ast_for::~ast_for() {
    for(ast_node const* node : body) {
        delete node;
    }

    if(condition) delete *condition;
    if(init)      delete *init;
    if(update)    delete *update;
}

ast_while::~ast_while() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete condition;
}

ast_dowhile::~ast_dowhile() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete condition;
}

ast_unaryexpr::~ast_unaryexpr() {
    delete operand;
}

ast_braced_expression::~ast_braced_expression() {
    for(ast_node const* node : members) {
        delete node;
    }
}

ast_ret::~ast_ret() {
    if(value.has_value()) {
        delete *value;
    }
}

ast_case::~ast_case() {
    for(ast_node const* node : body) {
        delete node;
    }

    delete value;
}

ast_default::~ast_default() {
    for(ast_node const* node : body) {
        delete node;
    }
}

ast_switch::~ast_switch() {
    for(ast_node const* node : cases) {
        delete node;
    }

    delete _default;
    delete target;
}

ast_namespacedecl::~ast_namespacedecl() {
    for(ast_node const* node : children) {
        delete node;
    }
}

parser::~parser() {
    for(ast_node const* node : toplevel_decls_) {
        delete node;
    }
}