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

std::optional<tak::IRCastingContext>
tak::CodegenContext::swap_casting_context(llvm::Type* llvm_t, const TypeData& tak_t) {
    const auto saved = casting_context_;
    casting_context_ = IRCastingContext(llvm_t, tak_t);
    return saved;
}

std::optional<tak::IRCastingContext>
tak::CodegenContext::set_casting_context(llvm::Type* llvm_t, const TypeData& tak_t) {
    const auto saved       = this->casting_context_;
    this->casting_context_ = IRCastingContext(llvm_t, tak_t);
    return saved;
}

std::optional<tak::IRCastingContext>
tak::CodegenContext::delete_casting_context() {
    const auto saved       = this->casting_context_;
    this->casting_context_ = std::nullopt;
    return saved;
}

bool tak::CodegenContext::casting_context_exists() {
    return this->casting_context_.has_value();
}

bool tak::CodegenContext::inside_procedure() {
    return curr_proc_.func != nullptr;
}

bool tak::CodegenContext::inside_loop() {
    return curr_loop_.after != nullptr
        && curr_loop_.cond  != nullptr
        && curr_loop_.merge != nullptr;
}

void tak::CodegenContext::enter_proc(llvm::Function* func) {
    assert(curr_proc_.func == nullptr);
    curr_proc_.func = func;
}

void tak::CodegenContext::enter_loop(llvm::BasicBlock* cond, llvm::BasicBlock* after, llvm::BasicBlock* merge) {
    assert(curr_loop_.cond  == nullptr);
    assert(curr_loop_.merge == nullptr);
    assert(curr_loop_.after == nullptr);

    curr_loop_.cond  = cond;
    curr_loop_.after = after;
    curr_loop_.merge = merge;
}

std::shared_ptr<tak::WrappedIRValue>
tak::CodegenContext::set_local(const std::string& name, const std::shared_ptr<WrappedIRValue>& ptr) {
    assert(inside_procedure() && "must be inside a procedure body to call this.");

    curr_proc_.named_values[name] = ptr;
    curr_proc_.named_values[name]->loadable = true;
    return curr_proc_.named_values[name];
}

std::shared_ptr<tak::WrappedIRValue>
tak::CodegenContext::get_local(const std::string& name) {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    assert(curr_proc_.named_values.contains(name) && "named local does not exist.");
    return WrappedIRValue::create(
        curr_proc_.named_values[name]->value,
        curr_proc_.named_values[name]->tak_type,
        curr_proc_.named_values[name]->loadable
    );
}

llvm::Function*
tak::CodegenContext::curr_func() {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    return curr_proc_.func;
}


