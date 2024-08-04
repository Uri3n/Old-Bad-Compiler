//
// Created by Diago on 2024-07-30.
//

#include <parser.hpp>


tak::AstBinexpr::~AstBinexpr() {
    delete left_op;
    delete right_op;
}

tak::AstMemberAccess::~AstMemberAccess() {
    delete target;
}

tak::AstBranch::~AstBranch() {
    for(AstNode const* node : conditions) {
        delete node;
    }

    if(_else.has_value()) {
        delete *_else;
    }
}

tak::AstIf::~AstIf() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

tak::AstElse::~AstElse() {
    for(AstNode const* node : body) {
        delete node;
    }
}

tak::AstProcdecl::~AstProcdecl() {
    for(AstNode const* node : body) {
        delete node;
    }

    for(AstNode const* node : parameters) {
        delete node;
    }

    delete identifier;
}

tak::AstVardecl::~AstVardecl() {
    if(init_value.has_value()) {
        delete *init_value;
    }

    delete identifier;
}

tak::AstCall::~AstCall() {
    for(AstNode const* node : arguments) {
        delete node;
    }

    delete target;
}

tak::AstEnumdef::~AstEnumdef() {
    delete _namespace;
    delete alias;
}

tak::AstCast::~AstCast() {
    delete target;
}

tak::AstBlock::~AstBlock() {
    for(AstNode const* node : children) {
        delete node;
    }
}

tak::AstSubscript::~AstSubscript() {
    delete operand;
    delete value;
}

tak::AstFor::~AstFor() {
    for(AstNode const* node : body) {
        delete node;
    }

    if(condition) delete *condition;
    if(init)      delete *init;
    if(update)    delete *update;
}

tak::AstWhile::~AstWhile() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

tak::AstDoWhile::~AstDoWhile() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete condition;
}

tak::AstUnaryexpr::~AstUnaryexpr() {
    delete operand;
}

tak::AstBracedExpression::~AstBracedExpression() {
    for(AstNode const* node : members) {
        delete node;
    }
}

tak::AstRet::~AstRet() {
    if(value.has_value()) {
        delete *value;
    }
}

tak::AstCase::~AstCase() {
    for(AstNode const* node : body) {
        delete node;
    }

    delete value;
}

tak::AstDefault::~AstDefault() {
    for(AstNode const* node : body) {
        delete node;
    }
}

tak::AstSwitch::~AstSwitch() {
    for(AstNode const* node : cases) {
        delete node;
    }

    delete _default;
    delete target;
}

tak::AstComposeDecl::~AstComposeDecl() {

}

tak::AstNamespaceDecl::~AstNamespaceDecl() {
    for(AstNode const* node : children) {
        delete node;
    }
}

tak::AstSizeof::~AstSizeof() {
    if(const auto* is_ident = std::get_if<AstNode*>(&this->target)) {
        delete *is_ident;
    }
}

tak::AstDefer::~AstDefer() {
    delete call;
}

tak::AstDeferIf::~AstDeferIf() {
    delete call;
    delete condition;
}

tak::Parser::~Parser() {
    for(AstNode const* node : toplevel_decls_) {
        delete node;
    }
}