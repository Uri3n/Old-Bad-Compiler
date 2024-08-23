//
// Created by Diago on 2024-08-20.
//

#include <codegen.hpp>

void
tak::CodegenContext::leave_curr_proc() {
    curr_proc_.named_values.clear();
    curr_proc_.func = nullptr;
}

void
tak::CodegenContext::leave_curr_loop() {
    curr_loop_.cond  = nullptr;
    curr_loop_.after = nullptr;
    curr_loop_.merge = nullptr;
}

std::shared_ptr<tak::WrappedIRValue>
tak::WrappedIRValue::create(llvm::Value* value, const std::optional<TypeData>& tak_type) {
    auto val = std::make_shared<WrappedIRValue>();
    val->value = value;
    if(tak_type) {
        val->tak_type = *tak_type;
    }

    return val;
}

void
tak::CodegenContext::set_casting_context(llvm::Type* llvm_t, const TypeData& tak_t) {
    this->casting_context_ = IRCastingContext(llvm_t, tak_t);
}

bool
tak::CodegenContext::casting_context_exists() {
    return this->casting_context_.has_value();
}

void
tak::CodegenContext::delete_casting_context() {
    this->casting_context_ = std::nullopt;
}


