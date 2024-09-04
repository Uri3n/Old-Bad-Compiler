//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::create(
    llvm::Value* value,
    const std::optional<TypeData>& tak_type,
    const bool loadable
) {
    auto val      = std::make_shared<WrappedIRValue>();
    val->value    = value;
    val->tak_type = tak_type.value_or(TypeData());
    val->loadable = loadable;

    return val;
}

std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::get_empty() {
    static const auto empty = create();
    return empty;
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