//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


std::shared_ptr<tak::WrappedIRValue>
tak::generate_procdecl(const AstProcdecl* node, CodegenContext& ctx) {
    assert(node != nullptr);

    const Symbol* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);
    if(sym->flags & ENTITY_FOREIGN || sym->flags & ENTITY_FOREIGN_C) {
        return WrappedIRValue::get_empty();
    }

    //
    // Get the associated function, load arguments locally.
    //

    llvm::Function*   func  = ctx.mod_.getFunction(sym->name);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx.llvm_ctx_, "entry", func);
    assert(func != nullptr);
    assert(node->parameters.size() == func->arg_size());

    ctx.builder_.SetInsertPoint(entry);
    ctx.enter_proc(func, sym);
    ctx.push_defers();

    for(size_t i = 0; i < node->parameters.size(); i++) {
        const auto* arg_sym = ctx.tbl_.lookup_unique_symbol(node->parameters[i]->identifier->symbol_index);
        const auto  local   = ctx.set_local(std::to_string(arg_sym->symbol_index), WrappedIRValue::create());

        local->tak_type     = arg_sym->type;
        local->value        = ctx.builder_.CreateAlloca(generate_type(ctx, arg_sym->type), nullptr, arg_sym->name);
        local->loadable     = true;

        ctx.builder_.CreateStore(func->getArg(i), local->value);
    }

    //
    // Generate procedure body.
    //

    for(const auto& child : node->children) {
        if(NODE_NEEDS_GENERATING(child->type)) generate(child, ctx);
        if(ctx.casting_context_exists())       ctx.delete_casting_context();
    }

    //
    // Add a default return value if the user did not return themselves.
    //

    if(!ctx.curr_block_has_terminator()) {
        unpack_defers(ctx);
        if(sym->type.return_type == nullptr) {
            ctx.builder_.CreateRetVoid();
        }
        else if(sym->type.return_type->is_aggregate()) {
            ctx.builder_.CreateRet(llvm::ConstantAggregateZero::get(generate_type(ctx, *sym->type.return_type)));
        }
        else if(sym->type.return_type->flags & TYPE_POINTER){
            ctx.builder_.CreateRet(llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_ctx_, 0)));
        }
        else {
            ctx.builder_.CreateRet(llvm::Constant::getNullValue(generate_type(ctx, *sym->type.return_type)));
        }
    }

    const bool verify_result = llvm::verifyFunction(*func, &llvm::errs());
    if(verify_result == true) {
        panic(fmt("LLVM failed to verify function \"{}\"", func->getName().str())); // should never happen
    }

    ctx.leave_curr_proc();
    ctx.pop_defers();
    return WrappedIRValue::get_empty();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_global_struct(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx) {
    assert(global != nullptr);
    assert(node   != nullptr);
    assert(sym    != nullptr);

    auto* llvm_struct_t  = generate_type(ctx, sym->type);
    const auto* utype    = ctx.tbl_.lookup_type(std::get<std::string>(sym->type.name));
    const auto  wrapped  = WrappedIRValue::create(global, sym->type);

    if(!node->init_value) {
        global->setInitializer(llvm::ConstantAggregateZero::get(llvm_struct_t));
        return wrapped;
    }

    assert(llvm::isa<llvm::StructType>(llvm_struct_t));
    assert((*node->init_value)->type == NODE_BRACED_EXPRESSION);

    global->setInitializer(generate_constant_struct(
        ctx,
        dynamic_cast<AstBracedExpression*>(*node->init_value),
        utype,
        llvm::cast<llvm::StructType>(llvm_struct_t)
    ));

    return wrapped;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_global_array(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx) {
    assert(global != nullptr);
    assert(node   != nullptr);
    assert(sym    != nullptr);

    const auto contained_t  = TypeData::get_lowest_array_type(sym->type);
    const auto llvm_arr_t   = generate_type(ctx, sym->type);
    const auto wrapped      = WrappedIRValue::create(global, sym->type);
    bool       del_cast_ctx = false;

    defer_if(del_cast_ctx, [&] {
       ctx.delete_casting_context();
    });


    if(!node->init_value) {
        global->setInitializer(llvm::ConstantAggregateZero::get(llvm_arr_t));
        return wrapped;
    }

    assert(contained_t.has_value());
    assert((*node->init_value)->type == NODE_BRACED_EXPRESSION);
    assert(llvm::isa<llvm::ArrayType>(llvm_arr_t));

    if(!(contained_t->flags & TYPE_POINTER)) {
        del_cast_ctx = true;
        ctx.set_casting_context(generate_type(ctx, *contained_t), *contained_t);
    }

    global->setInitializer(generate_constant_array(
        ctx,
        dynamic_cast<AstBracedExpression*>(*node->init_value),
        llvm::cast<llvm::ArrayType>(llvm_arr_t)
    ));

    return wrapped;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_global_primitive(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx) {
    assert(global != nullptr);
    assert(node   != nullptr);
    assert(sym    != nullptr);

    const TypeData& tak_t  = sym->type;
    const auto* prim_t     = std::get_if<primitive_t>(&tak_t.name);
    llvm::Type* llvm_t     = generate_type(ctx, tak_t);
    bool delete_cast_ctx   = false;

    defer_if(delete_cast_ctx, [&] {
       ctx.delete_casting_context();
    });


    if(!node->init_value.has_value()) {
        global->setInitializer(llvm::Constant::getNullValue(llvm_t));
        return WrappedIRValue::create(global, tak_t);
    }

    if(!(tak_t.flags & TYPE_POINTER)) {
        assert(prim_t != nullptr);
        delete_cast_ctx = true;
        ctx.set_casting_context(llvm_t, tak_t);
    }

    assert((*node->init_value)->type == NODE_SINGLETON_LITERAL);
    const auto initializer = generate_singleton_literal(
        dynamic_cast<AstSingletonLiteral*>(*node->init_value),
        ctx
    );

    assert(initializer != nullptr);
    assert(llvm::isa<llvm::Constant>(initializer->value));

    global->setInitializer(llvm::cast<llvm::Constant>(initializer->value));
    return initializer;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_vardecl_global(const AstVardecl* node, CodegenContext& ctx) {
    assert(node != nullptr);

    auto*         var = ctx.mod_.getGlobalVariable(ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index)->name, true);
    const Symbol* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);

    assert(var != nullptr);
    assert(sym != nullptr);

    if(sym->type.flags & TYPE_ARRAY)       return generate_global_array(node, sym, var, ctx);
    if(sym->type.flags & TYPE_POINTER)     return generate_global_primitive(node, sym, var, ctx);
    if(sym->type.kind == TYPE_KIND_STRUCT) return generate_global_struct(node, sym, var, ctx);

    return generate_global_primitive(node, sym, var, ctx);
}


void
tak::generate_local_struct_init(
    llvm::Value* ptr,                          // Struct memory location
    llvm::Type* llvm_t,                        // LLVM struct type
    const UserType* utype,                     // Tak user type
    const AstBracedExpression* bracedexpr,     // Braced initializer expression
    std::vector<llvm::Value*>& GEP_indices,    // GEP indices, initial should be { 0 }
    CodegenContext& ctx
) {
    assert(ptr      != nullptr);
    assert(llvm_t     != nullptr);
    assert(bracedexpr != nullptr);
    assert(utype      != nullptr);
    assert(!GEP_indices.empty());
    assert(ctx.inside_procedure());
    assert(bracedexpr->members.size() == utype->members.size());

    for(size_t i = 0; i < bracedexpr->members.size(); i++) {
        GEP_indices.emplace_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), i));
        if(bracedexpr->members[i]->type == NODE_BRACED_EXPRESSION)
        {
            if(utype->members[i].type.flags & TYPE_ARRAY) {
                const auto* to_braced  = dynamic_cast<const AstBracedExpression*>(bracedexpr->members[i]);
                const auto  lowest     = TypeData::get_lowest_array_type(utype->members[i].type);

                assert(to_braced != nullptr);
                assert(lowest.has_value());

                if(lowest->is_primitive()) {
                    ctx.set_casting_context(generate_type(ctx, *lowest), *lowest);
                }

                generate_local_array_init(ptr, llvm_t, to_braced, GEP_indices, ctx);
            }

            else if(utype->members[i].type.kind == TYPE_KIND_STRUCT) {
                const auto* to_braced     = dynamic_cast<const AstBracedExpression*>(bracedexpr->members[i]);
                const auto* nested_utype  = ctx.tbl_.lookup_type(std::get<std::string>(utype->members[i].type.name));

                assert(to_braced != nullptr);
                generate_local_struct_init(ptr, llvm_t, nested_utype, to_braced, GEP_indices, ctx);
            }

            else panic("Unknown type for bracedexpr");
        }

        else // Non-aggregate value.
        {
            if(utype->members[i].type.is_primitive()) {
                ctx.set_casting_context(generate_type(ctx, utype->members[i].type), utype->members[i].type);
            }

            const auto   generated_value = WrappedIRValue::maybe_adjust(bracedexpr->members[i], ctx);
            llvm::Value* calculated      = ctx.builder_.CreateGEP(llvm_t, ptr, GEP_indices);

            assert(generated_value->value != nullptr);
            ctx.builder_.CreateStore(generated_value->value, calculated);
        }

        ctx.delete_casting_context();
        GEP_indices.pop_back();
    }
}


void
tak::generate_local_array_init(
    llvm::Value* ptr,                           // Array memory location.
    llvm::Type* llvm_t,                         // Array type. e.g. i32 x 2
    const AstBracedExpression* bracedexpr,      // Braced initializer
    std::vector<llvm::Value*>& GEP_indices,     // GEP indices, initial should be { 0 }
    CodegenContext& ctx
) {
    assert(ptr      != nullptr);
    assert(llvm_t     != nullptr);
    assert(bracedexpr != nullptr);
    assert(!GEP_indices.empty());
    assert(ctx.inside_procedure());

    for(size_t i = 0; i < bracedexpr->members.size(); i++) {
        GEP_indices.emplace_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), i));
        if(bracedexpr->members[i]->type == NODE_BRACED_EXPRESSION)
        {
            generate_local_array_init( // recurse downwards
                ptr,
                llvm_t,
                dynamic_cast<const AstBracedExpression*>(bracedexpr->members[i]),
                GEP_indices,
                ctx
            );

            GEP_indices.pop_back();
            continue;
        }

        const auto   generated_value = WrappedIRValue::maybe_adjust(bracedexpr->members[i], ctx);
        llvm::Value* calculated      = ctx.builder_.CreateGEP(llvm_t, ptr, GEP_indices);

        assert(generated_value->value != nullptr);
        ctx.builder_.CreateStore(generated_value->value, calculated);
        GEP_indices.pop_back();
    }
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_vardecl_local(const AstVardecl* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto  saved_ip  = ctx.builder_.saveIP();
    const auto* sym       = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);
    auto&       entry_blk = ctx.curr_proc_.func->getEntryBlock();
    auto*       llvm_t    = generate_type(ctx, sym->type);


    ctx.builder_.SetInsertPoint(&entry_blk, entry_blk.getFirstInsertionPt());
    llvm::AllocaInst* alloc   = ctx.builder_.CreateAlloca(llvm_t, nullptr, sym->name);
    const auto        wrapped = WrappedIRValue::create(alloc, sym->type);

    ctx.builder_.restoreIP(saved_ip);
    ctx.set_local(std::to_string(sym->symbol_index), WrappedIRValue::create(alloc, sym->type));

    if(!node->init_value) {
        if(sym->type.is_aggregate()) {
            ctx.builder_.CreateStore(llvm::ConstantAggregateZero::get(llvm_t), alloc);
        }
        else if(sym->type.is_non_aggregate_pointer()) {
            ctx.builder_.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_ctx_, 0)), alloc);
        }
        else {
            ctx.builder_.CreateStore(llvm::Constant::getNullValue(llvm_t), alloc);
        }

        return wrapped;
    }


    if(sym->type.flags & TYPE_ARRAY) {
        const TypeData            lowest      = TypeData::get_lowest_array_type(sym->type).value();
        llvm::ConstantInt*        const_zero  = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), 0);
        std::vector<llvm::Value*> GEP_indices = { const_zero };

        if(lowest.is_primitive()) {
            ctx.set_casting_context(generate_type(ctx, lowest), lowest);
        }

        assert(llvm::isa<llvm::ArrayType>(llvm_t));
        assert((*node->init_value)->type == NODE_BRACED_EXPRESSION);

        generate_local_array_init(
            alloc,
            llvm_t,
            dynamic_cast<AstBracedExpression*>(*node->init_value),
            GEP_indices,
            ctx
        );
    }

    else if(sym->type.kind == TYPE_KIND_STRUCT
        && !(sym->type.flags & TYPE_POINTER)
        && (*node->init_value)->type == NODE_BRACED_EXPRESSION
    ) {
        const UserType*           utype       = ctx.tbl_.lookup_type(std::get<std::string>(sym->type.name));
        llvm::ConstantInt*        const_zero  = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), 0);
        std::vector<llvm::Value*> GEP_indices = { const_zero };

        assert(llvm::isa<llvm::StructType>(llvm_t));
        generate_local_struct_init(
            alloc,
            llvm_t,
            utype,
            dynamic_cast<AstBracedExpression*>(*node->init_value),
            GEP_indices,
            ctx
        );
    }

    else {
        if(sym->type.is_primitive()) {
            ctx.set_casting_context(llvm_t, sym->type);
        }
        const auto init = WrappedIRValue::maybe_adjust(*node->init_value, ctx);
        ctx.builder_.CreateStore(init->value, alloc);
    }

    ctx.delete_casting_context();
    return wrapped;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_vardecl(const AstVardecl* node, CodegenContext& ctx) {
    assert(node != nullptr);
    const auto* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);

    if(sym->flags & ENTITY_GLOBAL) {
        return generate_vardecl_global(node, ctx);
    }

    return generate_vardecl_local(node, ctx);
}
