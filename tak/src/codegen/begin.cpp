//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


template<typename T> requires tak::node_has_children<T>
static auto generate_children(T node, tak::CodegenContext& ctx) -> std::shared_ptr<tak::WrappedIRValue> {
    assert(node != nullptr);
    for(auto* child : node->children) {
        if(NODE_NEEDS_GENERATING(child->type)) tak::generate(child, ctx);
    }

    return tak::WrappedIRValue::get_empty();
}

std::shared_ptr<tak::WrappedIRValue>
tak::generate(AstNode* node, CodegenContext& ctx) {
    if(ctx.inside_procedure() && ctx.curr_block_has_terminator()) {
        return WrappedIRValue::get_empty();
    }

    assert(node != nullptr);
    switch(node->type) {
        case NODE_DEFER:             return generate_defer(node, ctx);
        case NODE_DEFER_IF:          return generate_defer(node, ctx);
        case NODE_NAMESPACEDECL:     return generate_children(dynamic_cast<AstNamespaceDecl*>(node), ctx);
        case NODE_PROCDECL:          return generate_procdecl(dynamic_cast<const AstProcdecl*>(node), ctx);
        case NODE_VARDECL:           return generate_vardecl(dynamic_cast<const AstVardecl*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return generate_singleton_literal(dynamic_cast<AstSingletonLiteral*>(node), ctx);
        case NODE_IDENT:             return generate_identifier(dynamic_cast<const AstIdentifier*>(node), ctx);
        case NODE_BINEXPR:           return generate_binexpr(dynamic_cast<const AstBinexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return generate_unaryexpr(dynamic_cast<const AstUnaryexpr*>(node), ctx);
        case NODE_SUBSCRIPT:         return generate_subscript(dynamic_cast<const AstSubscript*>(node), ctx);
        case NODE_MEMBER_ACCESS:     return generate_member_access(dynamic_cast<const AstMemberAccess*>(node), ctx);
        case NODE_CALL:              return generate_call(dynamic_cast<const AstCall*>(node), ctx);
        case NODE_RET:               return generate_ret(dynamic_cast<const AstRet*>(node), ctx);
        case NODE_BRANCH:            return generate_branch(dynamic_cast<const AstBranch*>(node), ctx);
        case NODE_FOR:               return generate_for(dynamic_cast<const AstFor*>(node), ctx);
        case NODE_WHILE:             return generate_while(dynamic_cast<const AstWhile*>(node), ctx);
        case NODE_DOWHILE:           return generate_dowhile(dynamic_cast<const AstDoWhile*>(node), ctx);
        case NODE_BRK:               return generate_brk(ctx);
        case NODE_CONT:              return generate_cont(ctx);
        default : break;
    }

    panic("tak::generate: default assertion reached.");
}
