//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


void
tak::unpack_defer(const AstDefer* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(node->call->type == NODE_CALL);
    generate(node->call, ctx);
    ctx.delete_casting_context();
}

void
tak::unpack_defer_if(const AstDeferIf* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(node->call->type == NODE_CALL);

    const auto        condition = WrappedIRValue::maybe_adjust(node->condition, ctx);
    llvm::BasicBlock* if_blk    = llvm::BasicBlock::Create(ctx.llvm_ctx_, "defer_if.body",  ctx.curr_proc_.func);
    llvm::BasicBlock* merge_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "defer_if.merge", ctx.curr_proc_.func);

    ctx.builder_.CreateCondBr(generate_to_i1(condition, ctx), if_blk, merge_blk);
    ctx.builder_.SetInsertPoint(if_blk);

    generate(node->call, ctx);
    ctx.delete_casting_context();

    ctx.builder_.CreateBr(merge_blk);
    ctx.builder_.SetInsertPoint(merge_blk);
}

void
tak::_unpack_defers_impl(const std::vector<AstNode*> & stmts, CodegenContext& ctx) {
    assert(ctx.inside_procedure());
    for(const AstNode* stmt : stmts) {
        if(const auto* pdefer_if = dynamic_cast<const AstDeferIf*>(stmt)) {
            unpack_defer_if(pdefer_if, ctx);
        }
        else if(const auto* pdefer = dynamic_cast<const AstDefer*>(stmt)) {
            unpack_defer(pdefer, ctx);
        }
        else {
            panic("pop_defers: invalid AST node stored on defer stack.");
        }
    }
}

void
tak::unpack_defers_regular(CodegenContext& ctx) {
    assert(!ctx.deferred_stmts_.empty());
    _unpack_defers_impl(ctx.deferred_stmts_.back().stmts, ctx);
}

void
tak::unpack_defers_until_loop_base(CodegenContext& ctx) {
    assert(!ctx.deferred_stmts_.empty());
    for(auto it = ctx.deferred_stmts_.rbegin(); it != ctx.deferred_stmts_.rend(); ++it) {
        _unpack_defers_impl(it->stmts, ctx);
        if(it->is_loop_base) return;
    }

    panic("defer unpack method is \"until loop base\", "
          "but there is no loop base on the stack.");
}

void
tak::unpack_defers_all(CodegenContext& ctx) {
    assert(!ctx.deferred_stmts_.empty());
    for(auto it = ctx.deferred_stmts_.rbegin(); it != ctx.deferred_stmts_.rend(); ++it) {
        _unpack_defers_impl(it->stmts, ctx);
    }
}

void
tak::unpack_defers(CodegenContext& ctx, const defer_unpack_method_t method, const bool pop_after) {
    assert(ctx.inside_procedure());

    defer_if(pop_after, [&] {
       ctx.pop_defers();
    });

    if(ctx.curr_block_has_terminator()) {
        return;
    }

    switch(method) {
        case DEFERRED_UNPACK_REGULAR:
            unpack_defers_regular(ctx);
            break;
        case DEFERRED_UNPACK_UNTIL_LOOP_BASE:
            unpack_defers_until_loop_base(ctx);
            break;
        case DEFERRED_UNPACK_ALL:
            unpack_defers_all(ctx);
            break;
        default:
            panic("tak::unpack_defers: invalid unpack method.");
    }
}
