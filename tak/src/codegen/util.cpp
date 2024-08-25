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

bool
tak::CodegenContext::inside_procedure() {
    return curr_proc_.func != nullptr;
}

bool
tak::CodegenContext::inside_loop() {
    return curr_loop_.after != nullptr
        && curr_loop_.cond  != nullptr
        && curr_loop_.merge != nullptr;
}

void
tak::CodegenContext::delete_casting_context() {
    this->casting_context_ = std::nullopt;
}

void
tak::CodegenContext::enter_proc(llvm::Function* func) {
    assert(curr_proc_.func == nullptr);
    curr_proc_.func = func;
}

void
tak::CodegenContext::enter_loop(llvm::BasicBlock* cond, llvm::BasicBlock* after, llvm::BasicBlock* merge) {
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
    return curr_proc_.named_values[name];
}

std::shared_ptr<tak::WrappedIRValue>
tak::CodegenContext::get_local(const std::string& name) {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    assert(curr_proc_.named_values.contains(name) && "named local does not exist.");
    return curr_proc_.named_values[name];
}

llvm::Function*
tak::CodegenContext::curr_func() {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    return curr_proc_.func;
}


