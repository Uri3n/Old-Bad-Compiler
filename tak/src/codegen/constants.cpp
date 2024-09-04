//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


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
