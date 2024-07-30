//
// Created by Diago on 2024-07-30.
//

#include <parser.hpp>


AstBinexpr::~AstBinexpr() {
    delete left_op;
    delete right_op;
}

AstMemberAccess::~AstMemberAccess() {
    delete target;
}

AstBranch::~AstBranch() {
    for(AstNode const* node : conditions) {
        delete node;
    }

    if(_else.has_value()) {
        delete *_else;
    }
}

AstIf::~AstIf() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

AstElse::~AstElse() {
    for(AstNode const* node : body) {
        delete node;
    }
}

AstProcdecl::~AstProcdecl() {
    for(AstNode const* node : body) {
        delete node;
    }

    for(AstNode const* node : parameters) {
        delete node;
    }

    delete identifier;
}

AstVardecl::~AstVardecl() {
    if(init_value.has_value()) {
        delete *init_value;
    }

    delete identifier;
}

AstCall::~AstCall() {
    for(AstNode const* node : arguments) {
        delete node;
    }

    delete target;
}

AstEnumdef::~AstEnumdef() {
    delete _namespace;
    delete alias;
}

AstCast::~AstCast() {
    delete target;
}

AstBlock::~AstBlock() {
    for(AstNode const* node : body) {
        delete node;
    }
}

AstSubscript::~AstSubscript() {
    delete operand;
    delete value;
}

AstFor::~AstFor() {
    for(AstNode const* node : body) {
        delete node;
    }

    if(condition) delete *condition;
    if(init)      delete *init;
    if(update)    delete *update;
}

AstWhile::~AstWhile() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

AstDoWhile::~AstDoWhile() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

AstUnaryexpr::~AstUnaryexpr() {
    delete operand;
}

AstBracedExpression::~AstBracedExpression() {
    for(AstNode const* node : members) {
        delete node;
    }
}

AstRet::~AstRet() {
    if(value.has_value()) {
        delete *value;
    }
}

AstCase::~AstCase() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete value;
}

AstDefault::~AstDefault() {
    for(AstNode const* node : body) {
        delete node;
    }
}

AstSwitch::~AstSwitch() {
    for(AstNode const* node : cases) {
        delete node;
    }

    delete _default;
    delete target;
}

AstNamespaceDecl::~AstNamespaceDecl() {
    for(AstNode const* node : children) {
        delete node;
    }
}

AstSizeof::~AstSizeof() {
    if(const auto* is_ident = std::get_if<AstNode*>(&this->target)) {
        delete *is_ident;
    }
}

AstDefer::~AstDefer() {
    delete call;
}

AstDeferIf::~AstDeferIf() {
    delete call;
    delete condition;
}

Parser::~Parser() {
    for(AstNode const* node : toplevel_decls_) {
        delete node;
    }
}