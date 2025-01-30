//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


std::shared_ptr<tak::WrappedIRValue>
tak::generate_branch(const AstBranch *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    const auto        condition = WrappedIRValue::maybe_adjust(node->_if->condition, ctx);
    llvm::BasicBlock* merge_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "branch.merge", ctx.curr_proc_.func);
    llvm::BasicBlock* if_blk    = llvm::BasicBlock::Create(ctx.llvm_ctx_, "branch.if", ctx.curr_proc_.func);
    llvm::BasicBlock* else_blk  = node->_else ? llvm::BasicBlock::Create(ctx.llvm_ctx_, "branch.else", ctx.curr_proc_.func) : nullptr;

    ctx.builder_.CreateCondBr(
        generate_to_i1(condition, ctx),
        if_blk,
        else_blk == nullptr ? merge_blk : else_blk
    );


    ctx.push_defers();
    ctx.builder_.SetInsertPoint(if_blk);
    for(AstNode* child_node : node->_if->body) {
        if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
        ctx.delete_casting_context();
    }

    if(!ctx.curr_block_has_terminator()) {
        unpack_defers(ctx);
        ctx.builder_.CreateBr(merge_blk);
    }

    ctx.pop_defers();
    if(else_blk != nullptr) {
        ctx.push_defers();
        ctx.builder_.SetInsertPoint(else_blk);
        for(AstNode* child_node : (*node->_else)->body) {
            if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
            ctx.delete_casting_context();
        }

        if(!ctx.curr_block_has_terminator()) {
            unpack_defers(ctx);
            ctx.builder_.CreateBr(merge_blk);
        }

        ctx.pop_defers();
    }

    ctx.builder_.SetInsertPoint(merge_blk);
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_blk(const AstBlock *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    ctx.push_defers();
    for(AstNode* child : node->children) {
        if(NODE_NEEDS_GENERATING(child->type)) generate(child, ctx);
    }
    if(!ctx.curr_block_has_terminator()) {
        unpack_defers(ctx);
    }

    ctx.pop_defers();
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_switch(const AstSwitch *node, CodegenContext &ctx) {
    assert(false && "don't create switches, they're broken");

    // assert(node != nullptr);
    // assert(!ctx.casting_context_exists());
    // assert(ctx.inside_procedure());
    //
    // const auto        wrapped  = WrappedIRValue::maybe_adjust(node->target, ctx);
    // llvm::BasicBlock* merge    = llvm::BasicBlock::Create(ctx.llvm_ctx_, "switch.merge", ctx.curr_proc_.func);
    // llvm::BasicBlock* _default = llvm::BasicBlock::Create(ctx.llvm_ctx_, "switch.default", ctx.curr_proc_.func);
    // llvm::SwitchInst* _switch  = ctx.builder_.CreateSwitch(wrapped->value, _default, node->cases.size());
    //
    //
    // // Iterate over cases, generate each one.
    // for(size_t i = 0; i < node->cases.size(); i++)
    // {
    //     if(wrapped->tak_type.is_primitive()) {
    //         ctx.set_casting_context(generate_type(ctx, wrapped->tak_type), wrapped->tak_type);
    //     }
    //
    //     llvm::BasicBlock* case_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, fmt("switch.case[{}]", i), ctx.curr_proc_.func);
    //     llvm::Value*      case_val = WrappedIRValue::maybe_adjust(node->cases[i]->value, ctx)->value;
    //
    //     if(i > 0 && node->cases[i - 1]->fallthrough && !ctx.curr_block_has_terminator()) {
    //         ctx.builder_.CreateBr(case_blk);
    //     }
    //
    //     assert(llvm::isa<llvm::Constant>(case_val));
    //     assert(llvm::isa<llvm::ConstantInt>(case_val));
    //     _switch->addCase(llvm::cast<llvm::ConstantInt>(case_val), case_blk);
    //
    //     ctx.delete_casting_context();
    //     ctx.push_defers();
    //     ctx.builder_.SetInsertPoint(case_blk);
    //
    //     for(AstNode* child_node : node->cases[i]->body) {
    //         if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
    //         ctx.delete_casting_context();
    //     }
    //
    //     if(!ctx.curr_block_has_terminator()) {
    //         unpack_defers(ctx);
    //         if(!node->cases[i]->fallthrough) {
    //             ctx.builder_.CreateBr(merge);
    //         }
    //     }
    //     ctx.pop_defers();
    // }
    //
    //
    // // Handle default case.
    // if(!node->cases.empty() && node->cases.back()->fallthrough && !ctx.curr_block_has_terminator()) {
    //     ctx.builder_.CreateBr(_default);
    // }
    //
    // ctx.builder_.SetInsertPoint(_default);
    // ctx.push_defers();
    //
    // for(AstNode* child_node : node->_default->body) {
    //     if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
    //     ctx.delete_casting_context();
    // }
    //
    // if(!ctx.curr_block_has_terminator()) {
    //     unpack_defers(ctx);
    //     ctx.builder_.CreateBr(merge);
    // }
    //
    // ctx.pop_defers();

    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_for(const AstFor *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    if(node->init) {
        generate(*node->init, ctx);
    }

    llvm::BasicBlock* merge_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "for.merge", ctx.curr_proc_.func);
    llvm::BasicBlock* cond_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "for.condition", ctx.curr_proc_.func);
    llvm::BasicBlock* after_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "for.after", ctx.curr_proc_.func);
    llvm::BasicBlock* body_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "for.body", ctx.curr_proc_.func);
    auto [old_after, old_merge] = ctx.leave_curr_loop();


    ctx.enter_loop(after_blk, merge_blk);       // Enter new loop context.
    ctx.push_defers(true);                      // Push a new "loop base" defer element onto the stack.
    ctx.builder_.CreateBr(cond_blk);            // Immediately branch to the condition block.
    ctx.builder_.SetInsertPoint(cond_blk);

    if(node->condition) {
        const auto condition = WrappedIRValue::maybe_adjust(*node->condition, ctx);
        ctx.delete_casting_context();
        ctx.builder_.CreateCondBr(generate_to_i1(condition, ctx), body_blk, merge_blk);
    } else {
        ctx.builder_.CreateBr(body_blk);
    }


    ctx.builder_.SetInsertPoint(after_blk);     // Generate after/update clause (i.e. "++i").
    if(node->update) {                          // Unconditionally branches to condition block.
        generate(*node->update, ctx);
        ctx.delete_casting_context();
    }
    ctx.builder_.CreateBr(cond_blk);


    ctx.builder_.SetInsertPoint(body_blk);      // Generate loop body.
    for(AstNode* child_node : node->body) {
        if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
        ctx.delete_casting_context();
    }


    if(!ctx.curr_block_has_terminator()) {
        unpack_defers(ctx);                     // Unpack any deferred calls at the end of the loop.
        ctx.builder_.CreateBr(after_blk);       // End of loop body -> update block
    }

    ctx.pop_defers();
    ctx.builder_.SetInsertPoint(merge_blk);     // Make the IRBuilder leave the loop.
    ctx.leave_curr_loop();                      // Leave loop context.
    ctx.enter_loop(old_after, old_merge);       // Restore the old one (if it exists).

    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_while(const AstWhile *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    llvm::BasicBlock* merge_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "while.merge", ctx.curr_proc_.func);
    llvm::BasicBlock* cond_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "while.condition", ctx.curr_proc_.func);
    llvm::BasicBlock* body_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "while.body", ctx.curr_proc_.func);
    auto [old_after, old_merge] = ctx.leave_curr_loop();


    ctx.builder_.CreateBr(cond_blk);         // Start out at the condition block and evaluate it once
    ctx.builder_.SetInsertPoint(cond_blk);   // ensure builder insert point is set.
    ctx.builder_.CreateCondBr(               // Convert the condition to an i1 (boolean) if it isn't one.
        generate_to_i1(
            WrappedIRValue::maybe_adjust(node->condition, ctx),
            ctx
        ),
        body_blk,
        merge_blk
    );

    ctx.builder_.SetInsertPoint(body_blk);   // Go to the loop body.
    ctx.enter_loop(cond_blk, merge_blk);     // Set a new loop context for brk/cont statements.
    ctx.delete_casting_context();            // Ensure there is no casting context.
    ctx.push_defers(true);                   // Push a new "loop base" defer element onto the stack.

    for(AstNode* child_node : node->body) {
        if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
        ctx.delete_casting_context();
    }

    if(!ctx.curr_block_has_terminator()) {   // Ensure deferred calls are executed if there is no terminator.
        unpack_defers(ctx);                  // Unpack method = REGULAR
        ctx.builder_.CreateBr(cond_blk);     // Restore control flow to the spot after the loop.
    }

    ctx.pop_defers();                        // Pop defer stack.
    ctx.leave_curr_loop();                   // Delete current loop context.
    ctx.enter_loop(old_after, old_merge);    // Restore old loop context.
    ctx.builder_.SetInsertPoint(merge_blk);  // Set insert point to the location after the loop.

    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_dowhile(const AstDoWhile *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    llvm::BasicBlock* merge_blk = llvm::BasicBlock::Create(ctx.llvm_ctx_, "dowhile.merge", ctx.curr_proc_.func);
    llvm::BasicBlock* cond_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "dowhile.condition", ctx.curr_proc_.func);
    llvm::BasicBlock* body_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "dowhile.body", ctx.curr_proc_.func);
    auto [old_after, old_merge] = ctx.leave_curr_loop();


    ctx.enter_loop(cond_blk, merge_blk);      // Set a new loop context.
    ctx.push_defers(true);                    // Push new "loop base" defer spot onto the stack.
    ctx.builder_.CreateBr(body_blk);          // Branch to the loop body immediately.
    ctx.builder_.SetInsertPoint(body_blk);    // Set builder to loop body.

    for(AstNode* child_node : node->body) {
        if(NODE_NEEDS_GENERATING(child_node->type)) generate(child_node, ctx);
        ctx.delete_casting_context();
    }

    if(!ctx.curr_block_has_terminator()) {    // Unpack deferred calls at the end of the loop body.
        unpack_defers(ctx);
        ctx.builder_.CreateBr(cond_blk);
    }
    ctx.pop_defers();

    ctx.builder_.SetInsertPoint(cond_blk);    // Generate loop condition, ensure it's of type i1.
    ctx.builder_.CreateCondBr(                // Conditionally branch back to the body if true.
        generate_to_i1(
            WrappedIRValue::maybe_adjust(node->condition, ctx),
            ctx
        ),
        body_blk,
        merge_blk
    );

    ctx.builder_.SetInsertPoint(merge_blk);   // Go to spot past the end of the loop.
    ctx.enter_loop(old_after, old_merge);     // Restore old loop context.
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_ret(const AstRet *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    unpack_defers(ctx, DEFERRED_UNPACK_ALL);
    const auto* sym = ctx.curr_proc_.sym;

    if(sym->type.return_type == nullptr) {
        ctx.builder_.CreateRetVoid();
        return WrappedIRValue::get_empty();
    }

    if(sym->type.return_type->is_primitive()) {
        ctx.set_casting_context(generate_type(ctx, *sym->type.return_type), *sym->type.return_type);
    }

    ctx.builder_.CreateRet(WrappedIRValue::maybe_adjust(*node->value, ctx)->value);
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_brk(CodegenContext &ctx) {
    assert(ctx.inside_procedure());
    assert(ctx.inside_loop());
    assert(!ctx.deferred_stmts_.empty());

    unpack_defers(ctx, DEFERRED_UNPACK_UNTIL_LOOP_BASE);
    ctx.builder_.CreateBr(ctx.curr_loop_.merge);
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_cont(CodegenContext &ctx) {
    assert(ctx.inside_procedure());
    assert(ctx.inside_loop());
    assert(!ctx.deferred_stmts_.empty());

    unpack_defers(ctx, DEFERRED_UNPACK_UNTIL_LOOP_BASE);
    ctx.builder_.CreateBr(ctx.curr_loop_.after);
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_defer(AstNode *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    // Doesn't actually generate any code, just saves the node.
    ctx.push_deferred_stmt(node);
    return WrappedIRValue::get_empty();
}