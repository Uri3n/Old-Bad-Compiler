//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


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
        cmp = ctx.builder_.CreateICmpULT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpSLT(LHS->value, RHS->value);
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
        cmp = ctx.builder_.CreateICmpULE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpSLE(LHS->value, RHS->value);
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
        cmp = ctx.builder_.CreateICmpUGT(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpSGT(LHS->value, RHS->value);
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
        cmp = ctx.builder_.CreateICmpUGE(LHS->value, RHS->value);
    }
    else if(LHS->tak_type.is_signed_primitive()) {
        cmp = ctx.builder_.CreateICmpSGE(LHS->value, RHS->value);
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
tak::generate_cast(const AstCast* node, CodegenContext &ctx) {
    assert(node != nullptr);
    assert(ctx.inside_procedure());

    const auto   og_ctx  = ctx.delete_casting_context();
    const auto   wrapped = WrappedIRValue::maybe_adjust(node->target, ctx);
    llvm::Type*  cast_t  = generate_type(ctx, node->type);
    llvm::Value* castval = nullptr;


    // conversion to float
    if(node->type.is_floating_point())
    {
        if(wrapped->tak_type.is_integer()) {
            castval = wrapped->tak_type.is_signed_primitive()
                ? ctx.builder_.CreateSIToFP(wrapped->value, cast_t)
                : ctx.builder_.CreateUIToFP(wrapped->value, cast_t);
        }
        else if(wrapped->tak_type.is_f32() && node->type.is_f64()){
            castval = ctx.builder_.CreateFPExt(wrapped->value, cast_t);
        }
        else if(wrapped->tak_type.is_f64() && node->type.is_f32()) {
            castval = ctx.builder_.CreateFPTrunc(wrapped->value, cast_t);
        }
        else {
            castval = wrapped->value;
        }
    }

    // conversion to integer
    else if(node->type.is_integer())
    {
        if(wrapped->tak_type.is_floating_point()) {
            castval = node->type.is_signed_primitive()
                ? ctx.builder_.CreateFPToSI(wrapped->value, cast_t)
                : ctx.builder_.CreateFPToUI(wrapped->value, cast_t);
        }
        else if(wrapped->tak_type.is_integer()) {
            castval = wrapped->tak_type.is_signed_primitive()
                ? ctx.builder_.CreateSExtOrTrunc(wrapped->value, cast_t)
                : ctx.builder_.CreateZExtOrTrunc(wrapped->value, cast_t);
        }
        else if(wrapped->tak_type.flags & TYPE_POINTER) {
            castval = ctx.builder_.CreatePtrToInt(wrapped->value, cast_t);
        }
        else {
            panic("default assertion");
        }
    }

    // conversion to pointer
    else if(node->type.flags & TYPE_POINTER)
    {
        if(wrapped->tak_type.is_integer()) {
            ctx.builder_.CreateIntToPtr(wrapped->value, cast_t);
        }
        else if(wrapped->tak_type.flags & TYPE_POINTER) {
            castval = wrapped->value;
        }
        else {
            panic("default assertion");
        }
    }

    if(og_ctx) ctx.casting_context_ = og_ctx;
    return WrappedIRValue::create(castval, node->type);
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

    if(callee->tak_type.return_type == nullptr) {
        return WrappedIRValue::get_empty(); // Should only happen for void returns.
    }


    std::string  alloc_name = "";
    const auto*  func_val   = llvm::dyn_cast<llvm::Function>(callee->value);
    auto&        entry_blk  = ctx.curr_proc_.func->getEntryBlock();
    const auto   saved_ip   = ctx.builder_.saveIP();

    if(func_val != nullptr) {
        alloc_name = func_val->getName().str() + ".returnalloc";
    }

    ctx.builder_.SetInsertPoint(&entry_blk, entry_blk.getFirstInsertionPt());
    llvm::Value* alloc = ctx.local_exists(alloc_name)
        ? ctx.get_local(alloc_name)->value
        : ctx.builder_.CreateAlloca(generate_type(ctx, *callee->tak_type.return_type), nullptr, alloc_name);

    ctx.builder_.restoreIP(saved_ip);
    ctx.builder_.CreateStore(call, alloc);

    const auto wrapped = WrappedIRValue::create(alloc, *callee->tak_type.return_type, true);
    if(func_val != nullptr && !ctx.local_exists(alloc_name)) {
        ctx.set_local(alloc_name, wrapped);
    }

    return wrapped;
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