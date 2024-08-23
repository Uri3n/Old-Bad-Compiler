//
// Created by Diago on 2024-08-06.
//
// Just one file for now. Expand later.

#include <codegen.hpp>


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
            std::to_string(sym->symbol_index)
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
                std::to_string(sym->symbol_index),
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
    if(node->children.empty()) {
        return nullptr;
    }

    //
    // Get the associated function, load arguments.
    //

    llvm::Function*   func  = ctx.mod_.getFunction(std::to_string(node->identifier->symbol_index));
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx.llvm_ctx_, "entry", func);

    assert(func != nullptr);
    assert(node->parameters.size() == func->arg_size());

    ctx.builder_.SetInsertPoint(entry);
    ctx.curr_proc_.func = func;

    for(size_t i = 0; i < node->parameters.size(); i++)
    {
        const auto*     sym   = ctx.tbl_.lookup_unique_symbol(node->parameters[i]->identifier->symbol_index);
        WrappedIRValue& local = ctx.curr_proc_.named_values[std::to_string(sym->symbol_index)];

        if(sym->type.flags & TYPE_CONSTANT) {
            local.tak_type = sym->type;
            local.value    = func->getArg(i);
        } else {
            local.tak_type = TypeData::get_addressed_type(sym->type).value(); // Shouldn't be unsafe, right?
            local.value    = ctx.builder_.CreateAlloca(generate_type(ctx, sym->type));
        }
    }

    for(const auto& child : node->children) {
        if(NODE_NEEDS_GENERATING(child->type)) generate(child, ctx);
    }

    return nullptr;
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

    auto*         var = ctx.mod_.getGlobalVariable(std::to_string(node->identifier->symbol_index), true);
    const Symbol* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);

    assert(var != nullptr);
    assert(sym != nullptr);

    if(sym->type.flags & TYPE_ARRAY)       return generate_global_array(node, sym, var, ctx);
    if(sym->type.flags & TYPE_POINTER)     return generate_global_primitive(node, sym, var, ctx);
    if(sym->type.kind == TYPE_KIND_STRUCT) return generate_global_struct(node, sym, var, ctx);

    return generate_global_primitive(node, sym, var, ctx);
}


std::shared_ptr<tak::WrappedIRValue>
tak::generate_vardecl_local(const AstVardecl* node, CodegenContext& ctx) {

    assert(node != nullptr);
    assert(ctx.curr_proc_.func != nullptr);
    return nullptr;
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
    if(node->literal_type == TOKEN_STRING_LITERAL || node->literal_type == TOKEN_CHARACTER_LITERAL) {
        assert(node->value.size() >= 2);
        node->value.pop_back();
        node->value.erase(0,1);

        if(node->value.empty()) {
            node->value = '\0';
        }
    }

    if(node->literal_type == TOKEN_STRING_LITERAL) {
        if(ctx.builder_.GetInsertBlock() == nullptr) {
            return WrappedIRValue::create(
                generate_global_string_constant(ctx, node),
                TypeData::get_const_string()
            );
        }

        return WrappedIRValue::create(
            ctx.builder_.CreateGlobalStringPtr(node->value),
            TypeData::get_const_string()
        );
    }

    if(node->literal_type == TOKEN_KW_NULLPTR) {
        return WrappedIRValue::create(
            llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_ctx_, 0)),
            TypeData::get_const_voidptr()
        );
    }

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
            return WrappedIRValue::create(
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_ctx_), lit_val),
                TypeData::get_const_uint64()
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
            return WrappedIRValue::create(
                llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx.llvm_ctx_), lit_val),
                TypeData::get_const_double()
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
tak::generate(const AstNode* node, CodegenContext& ctx) {

    assert(node != nullptr);
    switch(node->type) {
        case NODE_PROCDECL: return generate_procdecl(dynamic_cast<const AstProcdecl*>(node), ctx);
        case NODE_VARDECL:  return generate_vardecl(dynamic_cast<const AstVardecl*>(node), ctx);
        default: break;
    }

    panic("tak::generate_node: default reached.");
}