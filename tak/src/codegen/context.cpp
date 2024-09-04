//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


void
tak::CodegenContext::leave_curr_proc() {
    curr_proc_.named_values.clear();
    curr_proc_.func = nullptr;
    curr_proc_.sym  = nullptr;
}

// basic block return order: after, merge
std::array<llvm::BasicBlock*, 2>
tak::CodegenContext::leave_curr_loop() {
    const std::array saved =
    {
        curr_loop_.after,
        curr_loop_.merge
    };

    curr_loop_.after = nullptr;
    curr_loop_.merge = nullptr;
    return saved;
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
    return curr_proc_.func != nullptr
        && curr_proc_.sym  != nullptr;
}

bool tak::CodegenContext::curr_block_has_terminator() {
    assert(inside_procedure());
    const auto* insert_blk = builder_.GetInsertBlock();
    return insert_blk != nullptr && insert_blk->getTerminator() != nullptr;
}

bool tak::CodegenContext::inside_loop() {
    return curr_loop_.after != nullptr && curr_loop_.merge != nullptr;
}

void tak::CodegenContext::push_defers(const bool is_loop_base) {
    assert(inside_procedure() && "not inside procedure.");
    deferred_stmts_.emplace_back(DeferredStackElement(is_loop_base));
}

void tak::CodegenContext::pop_defers() {
    assert(!deferred_stmts_.empty());
    deferred_stmts_.pop_back();
}

void tak::CodegenContext::push_deferred_stmt(AstNode *node) {
    assert(node->type == NODE_DEFER || node->type == NODE_DEFER_IF);
    assert(!deferred_stmts_.empty());
    assert(inside_procedure());
    deferred_stmts_.back().stmts.emplace_back(node);
}

void tak::CodegenContext::enter_proc(llvm::Function* func, const Symbol* sym) {
    assert(curr_proc_.func == nullptr && "call leave_proc before this.");
    assert(curr_proc_.sym == nullptr  && "call leave_proc before this.");
    curr_proc_.func = func;
    curr_proc_.sym  = sym;
}

void tak::CodegenContext::enter_loop(llvm::BasicBlock* after, llvm::BasicBlock* merge) {
    assert(curr_loop_.merge == nullptr);
    assert(curr_loop_.after == nullptr);
    curr_loop_.after = after;
    curr_loop_.merge = merge;
}

bool tak::CodegenContext::local_exists(const std::string& name) {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    return name.empty() ? false : curr_proc_.named_values.contains(name);
}

std::shared_ptr<tak::WrappedIRValue>
tak::CodegenContext::set_local(const std::string& name, const std::shared_ptr<WrappedIRValue>& ptr) {
    assert(inside_procedure() && "must be inside a procedure body to call this.");
    assert(!name.empty()      && "name must not be empty.");

    curr_proc_.named_values[name]           = std::make_shared<WrappedIRValue>();
    curr_proc_.named_values[name]->value    = ptr->value;
    curr_proc_.named_values[name]->tak_type = ptr->tak_type;
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