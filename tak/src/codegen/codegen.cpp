//
// Created by Diago on 2024-08-06.
//
// Just one file for now. Expand later.

#include <codegen.hpp>


template<typename T> requires tak::node_has_children<T>
static auto generate_children(T node, tak::CodegenContext& ctx) -> std::shared_ptr<tak::WrappedIRValue> {
    assert(node != nullptr);
    for(auto* child : node->children) {
        if(NODE_NEEDS_GENERATING(child->type)) tak::generate(child, ctx);
    }

    return tak::WrappedIRValue::get_empty();
}


llvm::Type*
tak::generate_primitive_type(CodegenContext& ctx, const primitive_t prim) {
    switch(prim) {
        case PRIMITIVE_U8:  case PRIMITIVE_I8:  return llvm::Type::getInt8Ty(ctx.llvm_ctx_);
        case PRIMITIVE_U16: case PRIMITIVE_I16: return llvm::Type::getInt16Ty(ctx.llvm_ctx_);
        case PRIMITIVE_U32: case PRIMITIVE_I32: return llvm::Type::getInt32Ty(ctx.llvm_ctx_);
        case PRIMITIVE_U64: case PRIMITIVE_I64: return llvm::Type::getInt64Ty(ctx.llvm_ctx_);
        case PRIMITIVE_F32:                     return llvm::Type::getFloatTy(ctx.llvm_ctx_);
        case PRIMITIVE_F64:                     return llvm::Type::getDoubleTy(ctx.llvm_ctx_);
        case PRIMITIVE_BOOLEAN:                 return llvm::Type::getInt1Ty(ctx.llvm_ctx_);
        case PRIMITIVE_VOID:                    return llvm::Type::getVoidTy(ctx.llvm_ctx_);
        default: break;
    }

    panic("generate_primitive: default reached.");
}


llvm::Type*
tak::generate_proc_signature(CodegenContext& ctx, const TypeData& type) {
    assert(type.kind == TYPE_KIND_PROCEDURE);

    std::vector<llvm::Type*> parameters;
    llvm::Type* return_type = nullptr;
    const bool  is_variadic = type.flags & TYPE_PROC_VARARGS ? true : false;

    if(type.parameters != nullptr) {
        for(const auto& param : *type.parameters) {
            parameters.emplace_back(generate_type(ctx, param));
        }
    }

    if(type.return_type != nullptr) return_type = generate_type(ctx, *type.return_type);
    if(return_type == nullptr)      return_type = llvm::Type::getVoidTy(ctx.llvm_ctx_);
    if(parameters.empty())          return llvm::FunctionType::get(return_type, is_variadic);

    return llvm::FunctionType::get(return_type, parameters, is_variadic);
}


llvm::Type*
tak::generate_type(CodegenContext& ctx, const TypeData& type) {

    llvm::Type* gen_t = nullptr;

    if(type.kind == TYPE_KIND_PRIMITIVE) gen_t = generate_primitive_type(ctx, std::get<primitive_t>(type.name));
    if(type.kind == TYPE_KIND_STRUCT)    gen_t = create_struct_type_if_not_exists(ctx, std::get<std::string>(type.name));
    if(type.kind == TYPE_KIND_PROCEDURE) gen_t = generate_proc_signature(ctx, type);

    assert(gen_t != nullptr);
    if(type.flags & TYPE_POINTER) {
        gen_t = llvm::PointerType::get(ctx.llvm_ctx_, 0); // opaque pointers... so annoying
    }

    for(auto i = type.array_lengths.rbegin(); i != type.array_lengths.rend(); ++i) {
        gen_t = llvm::ArrayType::get(gen_t, *i);
    }

    return gen_t;
}


llvm::StructType*
tak::create_struct_type_if_not_exists(CodegenContext& ctx, const std::string& name) {
    llvm::StructType* _struct = llvm::StructType::getTypeByName(ctx.llvm_ctx_, name);
    return _struct == nullptr ? llvm::StructType::create(ctx.llvm_ctx_, name) : _struct;
}


void
tak::generate_global_placeholders(CodegenContext& ctx) {
    for(const auto &[index, sym] : ctx.tbl_.sym_table_) {
        if(!(sym->flags & ENTITY_GLOBAL) || (sym->type.kind == TYPE_KIND_PROCEDURE && !(sym->flags & TYPE_POINTER))) {
            continue;
        }

        new llvm::GlobalVariable(
            ctx.mod_,
            generate_type(ctx, sym->type),
            sym->type.flags & TYPE_CONSTANT,
            sym->flags & ENTITY_FOREIGN ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage,
            nullptr,
            sym->name
        );
    }
}


void
tak::generate_procedure_signatures(CodegenContext& ctx) {
    for(const auto &[index, sym] : ctx.tbl_.sym_table_) {
        if(sym->type.kind != TYPE_KIND_PROCEDURE || sym->flags & TYPE_POINTER || sym->type.flags & TYPE_ARRAY) {
            continue;
        }

        if(auto* signature = llvm::dyn_cast<llvm::FunctionType>(generate_type(ctx, sym->type))) {
            llvm::Function::Create(
                signature,
                sym->flags & ENTITY_INTERNAL ? llvm::GlobalValue::InternalLinkage : llvm::GlobalValue::ExternalLinkage,
                sym->name,
                ctx.mod_
            );

            continue;
        }

        panic(fmt("Symbol {} is not an instance of a procedure type.", sym->name));
    }
}


void
tak::generate_struct_layouts(CodegenContext& ctx) {
    for(const auto &[name, type] : ctx.tbl_.type_table_) {
        llvm::StructType* _struct = create_struct_type_if_not_exists(ctx, name);
        std::vector<llvm::Type*> elements;

        for(const auto &[name, type] : type->members) {
            const auto& back = elements.emplace_back(generate_type(ctx, type));
            assert(back != nullptr);
        }

        _struct->setBody(elements);
    }
}

std::shared_ptr<tak::WrappedIRValue>
tak::generate_procdecl(const AstProcdecl* node, CodegenContext& ctx) {

    assert(node != nullptr);
    const Symbol* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);
    if(sym->flags & ENTITY_FOREIGN) {
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
    // Add a default return value if the user forgot to return themselves.
    //

    llvm::BasicBlock& last_blk = func->back();
    if(last_blk.empty() || !llvm::isa<llvm::ReturnInst>(last_blk.back())) {
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

    verifyFunction(*func, &llvm::errs());
    ctx.leave_curr_proc();
    return WrappedIRValue::get_empty();
}


llvm::Constant*
tak::generate_constant_array(CodegenContext& ctx, const AstBracedExpression* node, llvm::ArrayType* llvm_t) {
    assert(node   != nullptr);
    assert(llvm_t != nullptr);

    std::vector<llvm::Constant*> constants;
    for(const auto& member : node->members)
    {
        if(member->type == NODE_BRACED_EXPRESSION) {
            assert(llvm::isa<llvm::ArrayType>(llvm_t->getElementType()));
            constants.emplace_back(generate_constant_array(
                ctx,
                dynamic_cast<AstBracedExpression*>(member),
                llvm::cast<llvm::ArrayType>(llvm_t->getElementType())
            ));
        }
        else if(member->type == NODE_SINGLETON_LITERAL) {
            const auto lit = generate_singleton_literal(dynamic_cast<AstSingletonLiteral*>(member), ctx);
            assert(lit);
            assert(llvm::isa<llvm::Constant>(lit->value));
            constants.emplace_back(llvm::cast<llvm::Constant>(lit->value));
        }
        else {
            panic("generate_constant_array: invalid member constant.");
        }
    }

    return llvm::ConstantArray::get(llvm_t, constants);
}


llvm::Constant*
tak::generate_constant_struct(CodegenContext& ctx, const AstBracedExpression* node, const UserType* utype, llvm::StructType* llvm_t) {
    assert(node  != nullptr);
    assert(utype != nullptr);
    assert(utype->members.size() == node->members.size());

    std::vector<llvm::Constant*> constants;
    for(size_t i = 0; i < node->members.size(); i++) {
        if(utype->members[i].type.flags & TYPE_ARRAY)
        {
            auto  contained_t = TypeData::get_lowest_array_type(utype->members[i].type);
            auto* array_t     = generate_type(ctx, utype->members[i].type);

            assert(contained_t.has_value());
            assert(node->members[i]->type == NODE_BRACED_EXPRESSION);
            assert(llvm::isa<llvm::ArrayType>(array_t));

            ctx.set_casting_context(generate_type(ctx, *contained_t), *contained_t);
            constants.emplace_back(generate_constant_array(
                ctx,
                dynamic_cast<AstBracedExpression*>(node->members[i]),
                llvm::cast<llvm::ArrayType>(array_t)
            ));
        }

        else if(utype->members[i].type.flags & TYPE_POINTER || utype->members[i].type.kind == TYPE_KIND_PRIMITIVE)
        {
            const auto* prim_t    = std::get_if<primitive_t>(&utype->members[i].type.name);
            llvm::Type* prim_ll_t = generate_type(ctx, utype->members[i].type);

            if(!(utype->members[i].type.flags & TYPE_POINTER)) {
                assert(prim_t != nullptr);
                ctx.set_casting_context(prim_ll_t, utype->members[i].type);
            }

            assert(node->members[i]->type == NODE_SINGLETON_LITERAL);
            const auto initializer = generate_singleton_literal(
                dynamic_cast<AstSingletonLiteral*>(node->members[i]),
                ctx
            );

            assert(initializer != nullptr);
            assert(llvm::isa<llvm::Constant>(initializer->value));
            constants.emplace_back(llvm::cast<llvm::Constant>(initializer->value));
        }

        else // kind == TYPE_KIND_STRUCT
        {
            auto*       struct_t     = generate_type(ctx, utype->members[i].type);
            const auto* member_utype = ctx.tbl_.lookup_type(std::get<std::string>(utype->members[i].type.name));

            assert(struct_t && member_utype);
            assert(node->members[i]->type == NODE_BRACED_EXPRESSION);
            assert(llvm::isa<llvm::StructType>(struct_t));

            constants.emplace_back(generate_constant_struct(
                ctx,
                dynamic_cast<AstBracedExpression*>(node->members[i]),
                member_utype,
                llvm::cast<llvm::StructType>(struct_t)
            ));
        }

        ctx.delete_casting_context();
    }

    return llvm::ConstantStruct::get(llvm_t, constants);
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


llvm::Constant*
tak::generate_global_string_constant(CodegenContext& ctx, const AstSingletonLiteral* node) {

    assert(node != nullptr);
    assert(!node->value.empty());

    auto* arr_t = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx.llvm_ctx_), node->value.size() + 1);
    auto* init  = llvm::ConstantDataArray::getString(ctx.llvm_ctx_, node->value, true);

    auto* global_string = new llvm::GlobalVariable(
        ctx.mod_,
        arr_t,
        true,
        llvm::GlobalValue::PrivateLinkage,
        init
    );

    llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), 0);
    llvm::Constant* GEP  = llvm::ConstantExpr::getGetElementPtr(
        arr_t,
        global_string,
        llvm::ArrayRef{ zero, zero }
    );

    return GEP;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_singleton_literal(AstSingletonLiteral* node, CodegenContext& ctx) {
    assert(node != nullptr);

    // For character and string literals we need to erase the opening and closing quotes.
    if(node->literal_type == TOKEN_STRING_LITERAL || node->literal_type == TOKEN_CHARACTER_LITERAL) {
        assert(node->value.size() >= 2);
        node->value.pop_back();
        node->value.erase(0,1);

        if(node->value.empty()) {
            node->value = '\0';
        }
    }

    // Pointer literals (strings and "nullptr").
    if(node->literal_type == TOKEN_STRING_LITERAL) {
        llvm::Constant* ptr = ctx.builder_.GetInsertBlock() == nullptr
            ? generate_global_string_constant(ctx, node)
            : ctx.builder_.CreateGlobalStringPtr(node->value);

        return WrappedIRValue::create(ptr, TypeData::get_const_string());
    }

    if(node->literal_type == TOKEN_KW_NULLPTR) {
        return WrappedIRValue::create(
            llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_ctx_, 0)),
            TypeData::get_const_voidptr()
        );
    }

    // Any numeric literal (integers, characters, or booleans).
    if(node->literal_type     == TOKEN_INTEGER_LITERAL
        || node->literal_type == TOKEN_CHARACTER_LITERAL
        || node->literal_type == TOKEN_BOOLEAN_LITERAL
    ) {
        const uint64_t lit_val = [&]() -> uint64_t {
            if(node->literal_type == TOKEN_CHARACTER_LITERAL) {
                return static_cast<uint64_t>(node->value[0]);
            }
            else if(node->literal_type == TOKEN_BOOLEAN_LITERAL) {
                return static_cast<uint64_t>(node->value == "true");
            }
            else try {
                return std::stoull(node->value);
            }
            catch(const std::exception& e) {
                panic(fmt("generate_singleton_literal: Unexpected exception: {}", e.what()));
            }
            catch(...) {
                panic("generate_singleton_literal");
            }
        }();

        if(!ctx.casting_context_.has_value()) {
            ctx.set_casting_context(llvm::Type::getInt64Ty(ctx.llvm_ctx_), TypeData::get_const_uint64());
            return WrappedIRValue::create(
                llvm::ConstantInt::get(ctx.casting_context_->llvm_t, lit_val),
                ctx.casting_context_->tak_t
            );
        }

        assert(ctx.casting_context_->llvm_t != nullptr);
        if(ctx.casting_context_->llvm_t->isFloatingPointTy()) {
            return WrappedIRValue::create(
                llvm::ConstantFP::get(ctx.casting_context_->llvm_t, static_cast<double>(lit_val)),
                ctx.casting_context_->tak_t
            );
        }

        return WrappedIRValue::create(
            llvm::ConstantInt::get(ctx.casting_context_->llvm_t, lit_val),
            ctx.casting_context_->tak_t
        );
    }

    // Float literals (f32 and f64)
    if(node->literal_type == TOKEN_FLOAT_LITERAL) {
        const double lit_val = [&]() -> double {
            try {
                return std::stod(node->value);
            }
            catch(const std::exception& e) {
                panic(fmt("generate_singleton_literal: Unexpected exception: {}", e.what()));
            }
            catch(...) {
                panic("generate_singleton_literal");
            }
        }();

        if(!ctx.casting_context_.has_value()) {
            ctx.set_casting_context(llvm::Type::getDoubleTy(ctx.llvm_ctx_), TypeData::get_const_double());
            return WrappedIRValue::create(
                llvm::ConstantFP::get(ctx.casting_context_->llvm_t, lit_val),
                ctx.casting_context_->tak_t
            );
        }

        assert(ctx.casting_context_->llvm_t != nullptr);
        assert(ctx.casting_context_->llvm_t->isFloatingPointTy());

        return WrappedIRValue::create(
            llvm::ConstantFP::get(ctx.casting_context_->llvm_t, lit_val),
            ctx.casting_context_->tak_t
        );
    }

    // @Unreachable
    panic("generate_singleton_literal: default assertion.");
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


std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::maybe_adjust(std::shared_ptr<WrappedIRValue> wrapped, CodegenContext& ctx) {
    assert(wrapped->value != nullptr);
    assert(ctx.inside_procedure());

    if(wrapped->tak_type.flags & TYPE_ARRAY) {
        return wrapped;
    }

    if(wrapped->loadable) {
        wrapped->loadable = false;
        wrapped->value    = ctx.builder_.CreateLoad(generate_type(ctx, wrapped->tak_type), wrapped->value);
    }

    if(!wrapped->tak_type.is_primitive()
        || !ctx.casting_context_exists()
        || wrapped->value->getType() == ctx.casting_context_->llvm_t
    ) {
        return wrapped;
    }

    llvm::Value* val    = wrapped->value;
    llvm::Type*  llvm_t = ctx.casting_context_->llvm_t;
    const auto&  tak_t  = ctx.casting_context_->tak_t;

    if(tak_t.is_floating_point()) {
        if(wrapped->tak_type.is_floating_point()) {
            return create(ctx.builder_.CreateFPExt(val, llvm_t), tak_t);
        }
        if(wrapped->tak_type.is_signed_primitive()) {
            return create(ctx.builder_.CreateSIToFP(val, llvm_t), tak_t);
        }
        return create(ctx.builder_.CreateUIToFP(val, llvm_t), tak_t);
    }

    if(tak_t.is_integer()) {
        if(wrapped->tak_type.is_signed_primitive()) {
            return create(ctx.builder_.CreateSExtOrTrunc(val, llvm_t), tak_t);
        }
        if(wrapped->tak_type.is_unsigned_primitive()) {
            return create(ctx.builder_.CreateZExtOrTrunc(val, llvm_t), tak_t);
        }
    }

    panic("default assertion");
}

std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::maybe_adjust(AstNode* node, CodegenContext& ctx, const bool loadable) {
    const auto generated = generate(node, ctx);
    const auto adjusted  = maybe_adjust(generated, ctx);
    adjusted->loadable   = loadable;

    return adjusted;
}

std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::maybe_adjust(llvm::Value* value, const std::optional<TypeData>& tak_type, CodegenContext& ctx) {
    return maybe_adjust(create(value, tak_type), ctx);
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_identifier(const AstIdentifier* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto* sym  = ctx.tbl_.lookup_unique_symbol(node->symbol_index);
    const auto  val  = [&]() -> std::shared_ptr<WrappedIRValue>
    {
        if(sym->type.kind == TYPE_KIND_PROCEDURE && !(sym->type.flags & TYPE_POINTER)) {
            return WrappedIRValue::create(ctx.mod_.getFunction(sym->name), sym->type, false);
        }

        if(sym->flags & ENTITY_GLOBAL) {
            return WrappedIRValue::create(ctx.mod_.getGlobalVariable(sym->name, true), sym->type, true);
        }

        return ctx.get_local(std::to_string(sym->symbol_index));
    }();

    // @Incorrect ? don't know yet if this makes sense or not...
    if(!ctx.casting_context_exists() && val->tak_type.is_primitive()) {
        ctx.set_casting_context(generate_type(ctx, val->tak_type), val->tak_type);
    }

    assert(val->value != nullptr);
    return val;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_address_of(const AstUnaryexpr* node, CodegenContext& ctx) {
    const auto generated = generate(node->operand, ctx);
    const bool is_valid  =
           llvm::isa<llvm::AllocaInst>(generated->value)
        || llvm::isa<llvm::GetElementPtrInst>(generated->value)
        || llvm::isa<llvm::GlobalVariable>(generated->value)
        || llvm::isa<llvm::Function>(generated->value);

    assert(node->_operator == TOKEN_BITWISE_AND);
    assert(generated->value->getType()->isPointerTy());
    assert(is_valid);

    generated->tak_type = TypeData::get_pointer_to(generated->tak_type).value();
    generated->loadable = false;
    return generated;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_dereference(const AstUnaryexpr* node, CodegenContext& ctx) {
    const auto generated = WrappedIRValue::maybe_adjust(node->operand, ctx);
    const auto contained = TypeData::get_contained(generated->tak_type).value();

    if(generated->tak_type.flags & TYPE_ARRAY) {
        llvm::Value* constzero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_ctx_), 0);
        llvm::Value* GEP       = ctx.builder_.CreateGEP(generate_type(ctx, generated->tak_type), generated->value, {constzero, constzero});
        return WrappedIRValue::create(GEP, contained, true);
    }

    generated->loadable = true;
    generated->tak_type = contained;
    return generated;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_conditional_not(const AstUnaryexpr* node, CodegenContext& ctx) {
    const auto   generated = WrappedIRValue::maybe_adjust(node->operand, ctx);
    llvm::Type*  gen_type  = generated->value->getType();
    llvm::Value* cmp_inst  = nullptr;

    if(gen_type->isFloatTy()) {
        cmp_inst = ctx.builder_.CreateFCmpOEQ(
            generated->value,
            llvm::ConstantFP::get(gen_type, 0.00)
        );
    }

    if(gen_type->isPointerTy()) {
        cmp_inst = ctx.builder_.CreateICmpEQ(
            generated->value,
            llvm::ConstantPointerNull::get(
            llvm::PointerType::get(ctx.llvm_ctx_, 0)
        ));
    }

    else {
        cmp_inst = ctx.builder_.CreateICmpEQ(
            generated->value,
            llvm::ConstantInt::get(gen_type, 0)
        );
    }

    return WrappedIRValue::create(cmp_inst, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_bitwise_not(const AstUnaryexpr* node, CodegenContext& ctx) {
    const auto generated = WrappedIRValue::maybe_adjust(node->operand, ctx);
    generated->value     = ctx.builder_.CreateNot(generated->value);
    generated->loadable  = false;

    return generated;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_unary_minus(const AstUnaryexpr* node, CodegenContext& ctx) {
    const auto generated = WrappedIRValue::maybe_adjust(node->operand, ctx);
    generated->loadable  = false;
    generated->value     = generated->tak_type.is_floating_point()
        ? ctx.builder_.CreateFNeg(generated->value)
        : ctx.builder_.CreateNeg(generated->value);

    generated->tak_type.flip_sign();
    return generated;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_unaryexpr(const AstUnaryexpr* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    switch(node->_operator) {
        case TOKEN_BITWISE_AND:        return generate_address_of(node, ctx);
        case TOKEN_BITWISE_XOR_OR_PTR: return generate_dereference(node, ctx);
        case TOKEN_BITWISE_NOT:        return generate_bitwise_not(node, ctx);
        case TOKEN_CONDITIONAL_NOT:    return generate_conditional_not(node, ctx);
        case TOKEN_SUB:                return generate_unary_minus(node, ctx);
        case TOKEN_DECREMENT:          return generate_decrement(node, ctx);
        case TOKEN_INCREMENT:          return generate_increment(node, ctx);
        case TOKEN_PLUS:               return WrappedIRValue::maybe_adjust(node->operand, ctx);

        default : break;
    }

    panic(fmt("Invalid unary operator: {}", Token::to_string(node->_operator)));
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_add(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_PLUS);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = [&]() -> std::shared_ptr<WrappedIRValue>
    {
        if(LHS->tak_type.flags & TYPE_POINTER) {
            const auto saved      = ctx.swap_casting_context(llvm::Type::getInt64Ty(ctx.llvm_ctx_), TypeData::get_const_uint64());
            const auto _to_return = WrappedIRValue::maybe_adjust(node->right_op, ctx);
            ctx.casting_context_  = saved;
            return _to_return;
        } else {
            return WrappedIRValue::maybe_adjust(node->right_op, ctx);
        }
    }();

    if(LHS->tak_type.flags & TYPE_POINTER) {
        LHS->value = ctx.builder_.CreatePtrAdd(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_floating_point()) {
        LHS->value = ctx.builder_.CreateFAdd(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        LHS->value = ctx.builder_.CreateAdd(LHS->value, RHS->value);
    }
    else {
        panic("default assertion");
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_addeq(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_PLUSEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = [&]() -> std::shared_ptr<WrappedIRValue>
    {
        if(LHS->tak_type.flags & TYPE_POINTER) {
            const auto saved      = ctx.swap_casting_context(llvm::Type::getInt64Ty(ctx.llvm_ctx_), TypeData::get_const_uint64());
            const auto _to_return = WrappedIRValue::maybe_adjust(node->right_op, ctx);
            ctx.casting_context_  = saved;
            return _to_return;
        } else {
            return WrappedIRValue::maybe_adjust(node->right_op, ctx);
        }
    }();

    assert(LHS->loadable);
    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* add  = nullptr;

    if(LHS->tak_type.flags & TYPE_POINTER) {
        add = ctx.builder_.CreatePtrAdd(load, RHS->value);
    }
    else if(LHS->tak_type.is_floating_point()) {
        add = ctx.builder_.CreateFAdd(load, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        add = ctx.builder_.CreateAdd(load, RHS->value);
    }

    assert(add != nullptr);
    ctx.builder_.CreateStore(add, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_div(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_DIV);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    if(LHS->tak_type.is_floating_point()) {
        LHS->value = ctx.builder_.CreateFDiv(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        LHS->value = LHS->tak_type.is_signed_primitive()
            ? ctx.builder_.CreateSDiv(LHS->value, RHS->value)
            : ctx.builder_.CreateUDiv(LHS->value, RHS->value);
    }
    else {
        panic("default assertion");
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_diveq(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_DIVEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->loadable);
    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* div  = nullptr;

    if(LHS->tak_type.is_floating_point()) {
        div = ctx.builder_.CreateFDiv(load, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        div = LHS->tak_type.is_signed_primitive()
            ? ctx.builder_.CreateSDiv(load, RHS->value)
            : ctx.builder_.CreateUDiv(load, RHS->value);
    }

    assert(div != nullptr);
    ctx.builder_.CreateStore(div, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_mul(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_MUL);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    if(LHS->tak_type.is_floating_point()) {
        LHS->value = ctx.builder_.CreateFMul(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        LHS->value = ctx.builder_.CreateMul(LHS->value, RHS->value);
    }
    else {
        panic("default assertion");
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_muleq(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_MULEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->loadable);
    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* mul  = nullptr;

    if(LHS->tak_type.is_floating_point()) {
        mul = ctx.builder_.CreateFMul(load, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        mul = ctx.builder_.CreateMul(load, RHS->value);
    }

    assert(mul != nullptr);
    ctx.builder_.CreateStore(mul, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_mod(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_MOD);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    if(LHS->tak_type.is_floating_point()) {
        LHS->value = ctx.builder_.CreateFRem(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        LHS->value = ctx.builder_.CreateSRem(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_unsigned_primitive()) {
        LHS->value = ctx.builder_.CreateURem(LHS->value, RHS->value);
    }
    else {
        panic("default assertion");
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_modeq(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_MODEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->loadable);
    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* mod  = nullptr;

    if(LHS->tak_type.is_floating_point()) {
        mod = ctx.builder_.CreateFRem(load, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        mod = ctx.builder_.CreateSRem(load, RHS->value);
    }
    else if(LHS->tak_type.is_unsigned_primitive()) {
        mod = ctx.builder_.CreateURem(load, RHS->value);
    }

    assert(mod != nullptr);
    ctx.builder_.CreateStore(mod, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_assign_bracedexpr(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_VALUE_ASSIGNMENT);
    assert(ctx.inside_procedure());
    
    const auto   LHS         = generate(node->left_op, ctx);
    const auto*  struct_name = std::get_if<std::string>(&LHS->tak_type.name);
    llvm::Value* const_zero  = llvm::ConstantInt::get(ctx.builder_.getInt32Ty(), 0);
    llvm::Type*  llvm_t      = generate_type(ctx, LHS->tak_type);

    assert(  LHS->tak_type.kind == TYPE_KIND_STRUCT);
    assert(!(LHS->tak_type.flags & TYPE_POINTER));
    assert(llvm::isa<llvm::StructType>(llvm_t));
    assert(struct_name != nullptr);

    std::vector GEP_indices = { const_zero };
    generate_local_struct_init(
        LHS->value,
        generate_type(ctx, LHS->tak_type),
        ctx.tbl_.lookup_type(*struct_name),
        dynamic_cast<AstBracedExpression*>(node->right_op),
        GEP_indices,
        ctx
    );
    
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_assign(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_VALUE_ASSIGNMENT);
    assert(ctx.inside_procedure());

    if(node->right_op->type == NODE_BRACED_EXPRESSION) { // Only for structs.
        return generate_assign_bracedexpr(node, ctx);
    }

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->loadable);
    ctx.builder_.CreateStore(RHS->value, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_bitwise_or(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_OR);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    LHS->value = ctx.builder_.CreateOr(LHS->value, RHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_bitwise_oreq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_OREQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    assert(LHS->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* _or  = ctx.builder_.CreateOr(load, RHS->value);

    ctx.builder_.CreateStore(_or, LHS->value);
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_bitwise_and(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_AND);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    LHS->value = ctx.builder_.CreateAnd(LHS->value, RHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_bitwise_andeq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_ANDEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    assert(LHS->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* _and = ctx.builder_.CreateAnd(load, RHS->value);

    ctx.builder_.CreateStore(_and, LHS->value);
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_xor(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_XOR_OR_PTR);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    LHS->value = ctx.builder_.CreateXor(LHS->value, RHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_xoreq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_XOREQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    assert(LHS->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* _xor = ctx.builder_.CreateXor(load, RHS->value);

    ctx.builder_.CreateStore(_xor, LHS->value);
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_lshift(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_LSHIFT);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    LHS->value = ctx.builder_.CreateShl(LHS->value, RHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_lshifteq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_LSHIFTEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    assert(LHS->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* shl  = ctx.builder_.CreateShl(load, RHS->value);

    ctx.builder_.CreateStore(shl, LHS->value);
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_rshift(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_RSHIFT);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    LHS->value = LHS->tak_type.is_signed_primitive()
        ? ctx.builder_.CreateAShr(LHS->value, RHS->value)
        : ctx.builder_.CreateLShr(LHS->value, RHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_rshifteq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_BITWISE_RSHIFTEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    assert(LHS->tak_type.is_integer());
    assert(LHS->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* shr  = LHS->tak_type.is_signed_primitive()
        ? ctx.builder_.CreateAShr(load, RHS->value)
        : ctx.builder_.CreateLShr(load, RHS->value);

    ctx.builder_.CreateStore(shr, LHS->value);
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_eq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_EQUALS);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = LHS->tak_type.is_floating_point()
        ? ctx.builder_.CreateFCmpOEQ(LHS->value, RHS->value)
        : ctx.builder_.CreateICmpEQ(LHS->value, RHS->value);

    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_neq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_NOT_EQUALS);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = LHS->tak_type.is_floating_point()
        ? ctx.builder_.CreateFCmpONE(LHS->value, RHS->value)
        : ctx.builder_.CreateICmpNE(LHS->value, RHS->value);

    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_lt(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_LT);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = nullptr;
    if(LHS->tak_type.is_floating_point()) {
        cmp = ctx.builder_.CreateFCmpOLT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.flags & TYPE_POINTER || LHS->tak_type.is_unsigned_primitive()) {
        cmp = ctx.builder_.CreateICmpSLT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpULT(LHS->value, RHS->value);
    }

    assert(cmp != nullptr);
    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_lte(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_LTE);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = nullptr;
    if(LHS->tak_type.is_floating_point()) {
        cmp = ctx.builder_.CreateFCmpOLE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.flags & TYPE_POINTER || LHS->tak_type.is_unsigned_primitive()) {
        cmp = ctx.builder_.CreateICmpSLE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpULE(LHS->value, RHS->value);
    }

    assert(cmp != nullptr);
    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_gt(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_GT);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = nullptr;
    if(LHS->tak_type.is_floating_point()) {
        cmp = ctx.builder_.CreateFCmpOGT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.flags & TYPE_POINTER || LHS->tak_type.is_unsigned_primitive()) {
        cmp = ctx.builder_.CreateICmpSGT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpUGT(LHS->value, RHS->value);
    }

    assert(cmp != nullptr);
    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_gte(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_COMP_GTE);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS   = WrappedIRValue::maybe_adjust(node->right_op, ctx);

    llvm::Value* cmp = nullptr;
    if(LHS->tak_type.is_floating_point()) {
        cmp = ctx.builder_.CreateFCmpOGE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.flags & TYPE_POINTER || LHS->tak_type.is_unsigned_primitive()) {
        cmp = ctx.builder_.CreateICmpSGE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpUGE(LHS->value, RHS->value);
    }

    assert(cmp != nullptr);
    ctx.casting_context_ = saved;
    return WrappedIRValue::create(cmp, TypeData::get_const_bool());
}


llvm::Value*
tak::generate_to_i1(const std::shared_ptr<WrappedIRValue> to_convert, CodegenContext& ctx) {
    assert(to_convert->value != nullptr);
    assert(ctx.inside_procedure());

    llvm::Value* null_val = llvm::Constant::getNullValue(to_convert->value->getType());

    if(to_convert->tak_type.is_boolean()) {
        return to_convert->value;
    }
    if(to_convert->tak_type.is_integer() || to_convert->tak_type.flags & TYPE_POINTER) {
        return ctx.builder_.CreateICmpNE(to_convert->value, null_val);
    }
    if(to_convert->tak_type.is_floating_point()) {
        return ctx.builder_.CreateFCmpONE(to_convert->value, null_val);
    }

    panic("default assertion");
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_conditional_and(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_CONDITIONAL_AND);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    auto* then_blk   = llvm::BasicBlock::Create(ctx.llvm_ctx_, "&&.RHS", ctx.curr_proc_.func);
    auto* merge_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "&&.merge", ctx.curr_proc_.func);
    auto* LHS_blk    = ctx.builder_.GetInsertBlock();


    ctx.builder_.CreateCondBr(generate_to_i1(LHS, ctx), then_blk, merge_blk);
    ctx.builder_.SetInsertPoint(then_blk);

    const auto RHS     = generate_to_i1(WrappedIRValue::maybe_adjust(node->right_op, ctx), ctx);
    auto*      RHS_blk = ctx.builder_.GetInsertBlock();

    ctx.builder_.CreateBr(merge_blk);
    ctx.builder_.SetInsertPoint(merge_blk);

    llvm::PHINode* result = ctx.builder_.CreatePHI(ctx.builder_.getInt1Ty(), 2, "&&.result");
    result->addIncoming(ctx.builder_.getInt1(false), LHS_blk); // left condition is false.
    result->addIncoming(RHS, RHS_blk);                         // returned condition is righthand side.


    ctx.casting_context_ = saved;
    return WrappedIRValue::create(result, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_conditional_or(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_CONDITIONAL_OR);
    assert(ctx.inside_procedure());

    const auto saved = ctx.delete_casting_context();
    const auto LHS   = generate_to_i1(WrappedIRValue::maybe_adjust(node->left_op, ctx), ctx);
    auto* then_blk   = llvm::BasicBlock::Create(ctx.llvm_ctx_, "||.RHS",   ctx.curr_proc_.func);
    auto* merge_blk  = llvm::BasicBlock::Create(ctx.llvm_ctx_, "||.merge", ctx.curr_proc_.func);
    auto* LHS_blk    = ctx.builder_.GetInsertBlock();


    ctx.delete_casting_context();
    ctx.builder_.CreateCondBr(LHS, merge_blk, then_blk);
    ctx.builder_.SetInsertPoint(then_blk);

    const auto RHS     = generate_to_i1(WrappedIRValue::maybe_adjust(node->right_op, ctx), ctx);
    auto*      RHS_blk = ctx.builder_.GetInsertBlock();

    ctx.builder_.CreateBr(merge_blk);
    ctx.builder_.SetInsertPoint(merge_blk);

    llvm::PHINode* result = ctx.builder_.CreatePHI(ctx.builder_.getInt1Ty(), 2, "||.result");
    result->addIncoming(LHS, LHS_blk);
    result->addIncoming(RHS, RHS_blk);


    ctx.casting_context_ = saved;
    return WrappedIRValue::create(result, TypeData::get_const_bool());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_sizeof(const AstSizeof* node, CodegenContext &ctx) {
    assert(node != nullptr);

    const auto& DL   = ctx.mod_.getDataLayout();
    const auto  type = [&]() -> TypeData {
        if(const auto* is_typedata = std::get_if<TypeData>(&node->target)) {
            return *is_typedata;
        }
        if(const auto* is_node = std::get_if<AstNode*>(&node->target)) {
            return WrappedIRValue::maybe_adjust(*is_node, ctx)->tak_type;
        }

        panic("default assertion");
    }();

    assert(!(type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth == 0));
    llvm::Value* const_int = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(ctx.llvm_ctx_),
        DL.getTypeAllocSize(generate_type(ctx,type))
    );

    return WrappedIRValue::create(const_int, TypeData::get_const_int32());
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_cast(const AstCast *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto   og_ctx  = ctx.delete_casting_context();
    const auto   wrapped = WrappedIRValue::maybe_adjust(node->target, ctx);
    llvm::Type*  cast_t  = generate_type(ctx, node->type);
    llvm::Value* castval = nullptr;

    if(node->type.is_floating_point())
    {
        if(wrapped->tak_type.is_integer()) {
            castval = wrapped->tak_type.is_signed_primitive()
                ? ctx.builder_.CreateSIToFP(wrapped->value, cast_t)
                : ctx.builder_.CreateUIToFP(wrapped->value, cast_t);
        }
        
        if(wrapped->tak_type.is_f32() && node->type.is_f64()){
            castval = ctx.builder_.CreateFPExt(wrapped->value, cast_t);
        }
        
        if(wrapped->tak_type.is_f64() && node->type.is_f32()) {
            castval = ctx.builder_.CreateFPTrunc(wrapped->value, cast_t);
        }
        
        else {
            castval = wrapped->value;
        }
    }

    else if(node->type.is_integer()) {
        if(wrapped->tak_type.is_floating_point()) {
            castval = node->type.is_signed_primitive()
                ? ctx.builder_.CreateFPToSI(wrapped->value, cast_t)
                : ctx.builder_.CreateFPToUI(wrapped->value, cast_t);
        }
    }
    
    // TODO: finish
    if(og_ctx) ctx.casting_context_ = og_ctx;
    return WrappedIRValue::create();
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_member_access(const AstMemberAccess *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto  saved      = ctx.delete_casting_context();
    const auto  target     = generate(node->target, ctx);
    const auto  path       = split_string(node->path, '.');
    const auto  contained  = TypeData::get_contained(target->tak_type);
    const auto* utype      = ctx.tbl_.lookup_type(std::get<std::string>(target->tak_type.name));
    auto*       zero       = llvm::ConstantInt::get(ctx.builder_.getInt32Ty(), 0);
    size_t      path_index = 0;


    std::function<std::shared_ptr<WrappedIRValue>(
        llvm::Value*,
        llvm::Type*,
        std::vector<llvm::Value*>&,
        const UserType*)
    > recurse;

    recurse = [&](
        llvm::Value* ptr,                    // pointer to struct base address
        llvm::Type* llvm_t,                  // should be an LLVM struct type.
        std::vector<llvm::Value*>& indices,  // GEP indices. Should always contain 0 at first.
        const UserType* child_utype          // Tak userdefined type for member iteration.
    ) -> std::shared_ptr<WrappedIRValue> {   // returns wrapped IR value where ->value = GEP pointer to element.

        assert(llvm::isa<llvm::StructType>(llvm_t));
        assert(child_utype != nullptr);
        assert(!indices.empty());
        assert(path_index < path.size());

        size_t pos = 0;
        for(; pos < child_utype->members.size(); ++pos) {
            if(child_utype->members[pos].name == path[path_index]) {
                indices.emplace_back( llvm::ConstantInt::get(ctx.builder_.getInt32Ty(), pos) );
                break;
            }
        }

        assert(pos < child_utype->members.size());
        const auto&  member_t = child_utype->members[pos].type;

        if(path_index + 1 >= path.size()) {
            if(!ctx.casting_context_exists() && member_t.is_primitive()) {
                ctx.set_casting_context(generate_type(ctx, member_t), member_t);
            }
            return WrappedIRValue::create(ctx.builder_.CreateGEP(llvm_t, ptr, indices), member_t, true);
        }

        if(member_t.flags & TYPE_POINTER) {
            ptr     = ctx.builder_.CreateLoad(generate_type(ctx, member_t), ctx.builder_.CreateGEP(llvm_t, ptr, indices));
            llvm_t  = generate_type(ctx, TypeData::get_contained(member_t).value());
            indices = { zero };
        }

        ++path_index;
        return recurse( ptr, llvm_t, indices, ctx.tbl_.lookup_type(std::get<std::string>(member_t.name)) );
    };


    if(target->tak_type.flags & TYPE_POINTER) {
        assert(contained.has_value());
    }

    ctx.casting_context_ = saved;
    llvm::Value* ptr = target->tak_type.flags & TYPE_POINTER
        ? ctx.builder_.CreateLoad(generate_type(ctx, target->tak_type), target->value)
        : target->value;

    llvm::Type* ll_struct_t = target->tak_type.flags & TYPE_POINTER
        ? generate_type(ctx, contained.value())
        : generate_type(ctx, target->tak_type);

    std::vector<llvm::Value*> indices = { zero };
    assert(llvm::isa<llvm::StructType>(ll_struct_t));

    return recurse( ptr, ll_struct_t, indices, utype );
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_subscript(const AstSubscript *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto target = WrappedIRValue::maybe_adjust(node->operand, ctx);

    ctx.casting_context_ = IRCastingContext{ ctx.builder_.getInt32Ty(), TypeData::get_const_int32() };
    const auto indice    = WrappedIRValue::maybe_adjust(node->value, ctx);
    const auto contained = TypeData::get_contained(target->tak_type);


    std::vector<llvm::Value*> GEP_indices;
    if(target->tak_type.flags & TYPE_ARRAY) {
        GEP_indices.emplace_back(llvm::ConstantInt::get(ctx.builder_.getInt32Ty(), 0));
    } else {
        assert(target->tak_type.flags & TYPE_POINTER);
    }

    assert(contained.has_value());
    GEP_indices.emplace_back(indice->value);
    auto* GEP = ctx.builder_.CreateGEP(
        target->tak_type.flags & TYPE_ARRAY
            ? generate_type(ctx, target->tak_type)
            : generate_type(ctx, *contained),
        target->value,
        GEP_indices
    );

    if(og_ctx.has_value()) {
        if(og_ctx) ctx.casting_context_ = og_ctx;
    } else if(contained->is_primitive()) {
        ctx.set_casting_context(generate_type(ctx, *contained), *contained);
    }

    return WrappedIRValue::create(GEP, contained, true);
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_ret(const AstRet *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(!ctx.casting_context_exists());
    assert(ctx.inside_procedure());

    const auto* sym   = ctx.curr_proc_.sym;
    const auto  empty = WrappedIRValue::get_empty();

    if(sym->type.return_type == nullptr) {
        ctx.builder_.CreateRetVoid();
        return empty;
    }

    if(sym->type.return_type->is_primitive()) {
        ctx.set_casting_context(generate_type(ctx, *sym->type.return_type), *sym->type.return_type);
    }

    ctx.builder_.CreateRet(WrappedIRValue::maybe_adjust(*node->value, ctx)->value);
    return empty;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_call(const AstCall *node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto   saved    = ctx.delete_casting_context();
    const auto   callee   = WrappedIRValue::maybe_adjust(node->target, ctx);
    TypeData&    callee_t = callee->tak_type;
    const size_t given    = node->arguments.size();
    const size_t takes    = callee->tak_type.parameters == nullptr ? 0 : callee->tak_type.parameters->size();


    if(callee->tak_type.flags & TYPE_POINTER) {
        assert(callee->tak_type.pointer_depth == 1);
        callee_t.pointer_depth = 0;
        callee_t.flags        &= ~TYPE_POINTER;
    }

    llvm::Type* func_t = generate_type(ctx, callee_t);
    std::vector<llvm::Value*> arguments;

    assert(llvm::isa<llvm::FunctionType>(func_t));
    assert(takes <= given);

    for(size_t i = 0; i < given; i++) {
        if(i < takes && callee_t.parameters->at(i).is_primitive()) {
            ctx.set_casting_context(generate_type(ctx, callee_t.parameters->at(i)), callee_t.parameters->at(i));
        }

        arguments.emplace_back(WrappedIRValue::maybe_adjust(node->arguments[i], ctx)->value);
        ctx.delete_casting_context();
    }


    ctx.casting_context_ = saved;
    llvm::CallInst* call = ctx.builder_.CreateCall(llvm::cast<llvm::FunctionType>(func_t), callee->value, arguments);
    if(callee->tak_type.return_type != nullptr)
    {
        auto&        entry_blk = ctx.curr_proc_.func->getEntryBlock();
        const auto   saved_ip  = ctx.builder_.saveIP();
        llvm::Value* alloc     = nullptr;

        ctx.builder_.SetInsertPoint(&entry_blk, entry_blk.getFirstInsertionPt());
        alloc = ctx.builder_.CreateAlloca(generate_type(ctx, *callee->tak_type.return_type), nullptr, "returnalloc");

        ctx.builder_.restoreIP(saved_ip);
        ctx.builder_.CreateStore(call, alloc);

        return WrappedIRValue::create(alloc, *callee->tak_type.return_type, true);
    }

    return WrappedIRValue::get_empty(); // Should only happen for void returns.
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_sub(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_SUB);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = WrappedIRValue::maybe_adjust(node->left_op, ctx);
    const auto RHS    = [&]() -> std::shared_ptr<WrappedIRValue>
    {
        if(LHS->tak_type.flags & TYPE_POINTER) {
            const auto saved      = ctx.swap_casting_context(llvm::Type::getInt64Ty(ctx.llvm_ctx_), TypeData::get_const_uint64());
            const auto _to_return = WrappedIRValue::maybe_adjust(node->right_op, ctx);
            ctx.casting_context_  = saved;
            return _to_return;
        } else {
            return WrappedIRValue::maybe_adjust(node->right_op, ctx);
        }
    }();

    if(LHS->tak_type.flags & TYPE_POINTER) {
        const auto   contained = TypeData::get_contained(LHS->tak_type);
        llvm::Value* negation  = ctx.builder_.CreateNeg(RHS->value);

        assert(contained.has_value());
        LHS->value = ctx.builder_.CreateGEP(generate_type(ctx, *contained), LHS->value, { negation });
    }
    else if(LHS->tak_type.is_floating_point()) {
        LHS->value = ctx.builder_.CreateFSub(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        LHS->value = ctx.builder_.CreateSub(LHS->value, RHS->value);
    }
    else {
        panic("default assertion");
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_subeq(const AstBinexpr *node, CodegenContext &ctx) {
    assert(node->_operator == TOKEN_SUBEQ);
    assert(ctx.inside_procedure());

    const auto og_ctx = ctx.delete_casting_context();
    const auto LHS    = generate(node->left_op, ctx);
    const auto RHS    = [&]() -> std::shared_ptr<WrappedIRValue>
    {
        if(LHS->tak_type.flags & TYPE_POINTER) {
            const auto saved      = ctx.swap_casting_context(llvm::Type::getInt64Ty(ctx.llvm_ctx_), TypeData::get_const_uint64());
            const auto _to_return = WrappedIRValue::maybe_adjust(node->right_op, ctx);
            ctx.casting_context_  = saved;
            return _to_return;
        } else {
            return WrappedIRValue::maybe_adjust(node->right_op, ctx);
        }
    }();

    assert(LHS->loadable);
    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, LHS->tak_type), LHS->value);
    llvm::Value* sub  = nullptr;

    if(LHS->tak_type.flags & TYPE_POINTER) {
        const auto   contained = TypeData::get_contained(LHS->tak_type);
        llvm::Value* negation  = ctx.builder_.CreateNeg(RHS->value);

        assert(contained.has_value());
        sub = ctx.builder_.CreateGEP(generate_type(ctx, *contained), LHS->value, { negation });
    }
    else if(LHS->tak_type.is_floating_point()) {
        sub = ctx.builder_.CreateFSub(load, RHS->value);
    }
    else if(LHS->tak_type.is_primitive()) {
        sub = ctx.builder_.CreateSub(load, RHS->value);
    }

    assert(sub != nullptr);
    ctx.builder_.CreateStore(sub, LHS->value);

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return LHS;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_decrement(const AstUnaryexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_DECREMENT);
    assert(ctx.inside_procedure());

    const auto gen = generate(node->operand, ctx);
    assert(gen->loadable);

    const auto   contained = TypeData::get_contained(gen->tak_type);
    llvm::Value* load      = ctx.builder_.CreateLoad(generate_type(ctx, gen->tak_type), gen->value);
    llvm::Value* neg_one   = llvm::ConstantInt::get(ctx.builder_.getInt64Ty(), -1);
    llvm::Value* add       = nullptr;

    if(gen->tak_type.flags & TYPE_POINTER) {
        assert(contained.has_value());
        add = ctx.builder_.CreateGEP(generate_type(ctx, *contained), load, { neg_one });
    }
    else if(gen->tak_type.is_floating_point()) {
        add = ctx.builder_.CreateFSub(load, llvm::ConstantFP::get(generate_type(ctx, gen->tak_type), 1.00));
    }
    else if(gen->tak_type.is_primitive()) {
        add = ctx.builder_.CreateSub(load, llvm::ConstantInt::get(generate_type(ctx, gen->tak_type), 1));
    }

    assert(add != nullptr);
    ctx.builder_.CreateStore(add, gen->value);
    return gen;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_increment(const AstUnaryexpr* node, CodegenContext& ctx) {
    assert(node->_operator == TOKEN_INCREMENT);
    assert(ctx.inside_procedure());

    const auto gen = generate(node->operand, ctx);
    assert(gen->loadable);

    llvm::Value* load = ctx.builder_.CreateLoad(generate_type(ctx, gen->tak_type), gen->value);
    llvm::Value* add  = nullptr;

    if(gen->tak_type.flags & TYPE_POINTER) {
        add = ctx.builder_.CreatePtrAdd(load, llvm::ConstantInt::get(ctx.builder_.getInt64Ty(), 1));
    }
    else if(gen->tak_type.is_floating_point()) {
        add = ctx.builder_.CreateFAdd(load, llvm::ConstantFP::get(generate_type(ctx, gen->tak_type), 1));
    }
    else if(gen->tak_type.is_primitive()) {
        add = ctx.builder_.CreateAdd(load, llvm::ConstantInt::get(generate_type(ctx, gen->tak_type), 1));
    }

    assert(add != nullptr);
    ctx.builder_.CreateStore(add, gen->value);
    return gen;
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_binexpr(const AstBinexpr* node, CodegenContext& ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    switch(node->_operator) {
        case TOKEN_PLUS:               return generate_add(node, ctx);
        case TOKEN_PLUSEQ:             return generate_addeq(node, ctx);
        case TOKEN_SUB:                return generate_sub(node, ctx);
        case TOKEN_SUBEQ:              return generate_subeq(node, ctx);
        case TOKEN_DIV:                return generate_div(node, ctx);
        case TOKEN_DIVEQ:              return generate_diveq(node, ctx);
        case TOKEN_MUL:                return generate_mul(node, ctx);
        case TOKEN_MULEQ:              return generate_muleq(node, ctx);
        case TOKEN_MOD:                return generate_mod(node, ctx);
        case TOKEN_MODEQ:              return generate_modeq(node, ctx);
        case TOKEN_VALUE_ASSIGNMENT:   return generate_assign(node, ctx);
        case TOKEN_BITWISE_OR:         return generate_bitwise_or(node, ctx);
        case TOKEN_BITWISE_OREQ:       return generate_bitwise_oreq(node, ctx);
        case TOKEN_BITWISE_AND:        return generate_bitwise_and(node, ctx);
        case TOKEN_BITWISE_ANDEQ:      return generate_bitwise_andeq(node, ctx);
        case TOKEN_BITWISE_XOR_OR_PTR: return generate_xor(node, ctx);
        case TOKEN_BITWISE_XOREQ:      return generate_xoreq(node, ctx);
        case TOKEN_BITWISE_LSHIFT:     return generate_lshift(node, ctx);
        case TOKEN_BITWISE_LSHIFTEQ:   return generate_lshifteq(node, ctx);
        case TOKEN_BITWISE_RSHIFT:     return generate_rshift(node, ctx);
        case TOKEN_BITWISE_RSHIFTEQ:   return generate_rshifteq(node, ctx);
        case TOKEN_COMP_NOT_EQUALS:    return generate_neq(node, ctx);
        case TOKEN_COMP_EQUALS:        return generate_eq(node, ctx);
        case TOKEN_COMP_LT:            return generate_lt(node, ctx);
        case TOKEN_COMP_GT:            return generate_gt(node, ctx);
        case TOKEN_COMP_LTE:           return generate_lte(node, ctx);
        case TOKEN_COMP_GTE:           return generate_gte(node, ctx);
        case TOKEN_CONDITIONAL_OR:     return generate_conditional_or(node, ctx);
        case TOKEN_CONDITIONAL_AND:    return generate_conditional_and(node, ctx);
        default : break;
    }

    panic("generate_binexpr: default assert");
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate(AstNode* node, CodegenContext& ctx) {
    assert(node != nullptr);
    switch(node->type) {
        case NODE_PROCDECL:          return generate_procdecl(dynamic_cast<const AstProcdecl*>(node), ctx);
        case NODE_VARDECL:           return generate_vardecl(dynamic_cast<const AstVardecl*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return generate_singleton_literal(dynamic_cast<AstSingletonLiteral*>(node), ctx);
        case NODE_IDENT:             return generate_identifier(dynamic_cast<const AstIdentifier*>(node), ctx);
        case NODE_BINEXPR:           return generate_binexpr(dynamic_cast<const AstBinexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return generate_unaryexpr(dynamic_cast<const AstUnaryexpr*>(node), ctx);
        case NODE_NAMESPACEDECL:     return generate_children(dynamic_cast<AstNamespaceDecl*>(node), ctx);
        case NODE_SUBSCRIPT:         return generate_subscript(dynamic_cast<const AstSubscript*>(node), ctx);
        case NODE_MEMBER_ACCESS:     return generate_member_access(dynamic_cast<const AstMemberAccess*>(node), ctx);
        case NODE_CALL:              return generate_call(dynamic_cast<const AstCall*>(node), ctx);
        case NODE_RET:               return generate_ret(dynamic_cast<const AstRet*>(node), ctx);

        default : break;
    }

    panic("tak::generate_node: default reached.");
}