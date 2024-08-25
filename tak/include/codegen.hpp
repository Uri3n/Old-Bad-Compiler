//
// Created by Diago on 2024-08-06.
//

#ifndef CODEGEN_HPP
#define CODEGEN_HPP
#include <io.hpp>
#include <parser.hpp>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    struct WrappedIRValue {
        llvm::Value* value = nullptr;
        TypeData tak_type;

        static std::shared_ptr<WrappedIRValue> create(
            llvm::Value* value = nullptr,
            const std::optional<TypeData>& tak_type = std::nullopt
        );

        ~WrappedIRValue() = default;
        WrappedIRValue()  = default;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct IRCastingContext {
        llvm::Type* llvm_t = nullptr;
        TypeData    tak_t;

        ~IRCastingContext() = default;
        IRCastingContext(llvm::Type* const llvm_t, const TypeData &tak_t)
            : llvm_t(llvm_t), tak_t(tak_t) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class CodegenContext {
        struct {
            llvm::BasicBlock* cond  = nullptr;
            llvm::BasicBlock* after = nullptr;
            llvm::BasicBlock* merge = nullptr;
        } curr_loop_;

        struct {
            std::unordered_map<std::string, std::shared_ptr<WrappedIRValue>> named_values;
            llvm::Function* func = nullptr;
        } curr_proc_;

    public:

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        EntityTable&      tbl_;         // Reference to the Tak entity table for this source file.
        llvm::LLVMContext llvm_ctx_;    // LLVM context.
        llvm::Module      mod_;         // LLVM module, same name as source file.
        llvm::IRBuilder<> builder_;     // LLVM IRBuilder, helper class for generating IR.

        std::optional<IRCastingContext>             casting_context_;   // casting context
        std::vector<std::vector<llvm::BasicBlock*>> deferred_blocks_;   // calls for defer statements

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<WrappedIRValue> set_local(const std::string& name, const std::shared_ptr<WrappedIRValue>& ptr);
        std::shared_ptr<WrappedIRValue> get_local(const std::string& name);
        llvm::Function* curr_func();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool casting_context_exists();
        void set_casting_context(llvm::Type* llvm_t, const TypeData& tak_t);
        void delete_casting_context();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool inside_procedure();
        bool inside_loop();
        void enter_proc(llvm::Function* func);
        void enter_loop(llvm::BasicBlock* cond, llvm::BasicBlock* after, llvm::BasicBlock* merge);
        void leave_curr_proc();
        void leave_curr_loop();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ~CodegenContext() = default;
        explicit CodegenContext(EntityTable& tbl, const std::string& module_name)
            : tbl_(tbl), llvm_ctx_(), mod_(module_name, llvm_ctx_), builder_(llvm_ctx_) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    llvm::Type* generate_proc_signature(CodegenContext& ctx, const TypeData& type);
    llvm::Type* generate_primitive_type(CodegenContext& ctx, primitive_t prim);
    llvm::Type* generate_type(CodegenContext& ctx, const TypeData& type);
    llvm::StructType* create_struct_type_if_not_exists(CodegenContext& ctx, const std::string& name);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void generate_struct_layouts(CodegenContext& ctx);
    void generate_procedure_signatures(CodegenContext& ctx);
    void generate_global_placeholders(CodegenContext& ctx);

    void generate_local_struct_init(
        llvm::AllocaInst* alloc,
        llvm::Type* llvm_t,
        const UserType* utype,
        const AstBracedExpression* bracedexpr,
        std::vector<llvm::Value*>& GEP_indices,
        CodegenContext& ctx
    );

    void generate_local_array_init(
        llvm::AllocaInst* alloc,
        llvm::Type* llvm_t,
        const AstBracedExpression* bracedexpr,
        std::vector<llvm::Value*>& GEP_indices,
        CodegenContext& ctx
    );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    llvm::Constant* generate_global_string_constant(CodegenContext& ctx, const AstSingletonLiteral* node);
    llvm::Constant* generate_constant_array(CodegenContext& ctx, const AstBracedExpression* node, llvm::ArrayType* llvm_t);
    llvm::Constant* generate_constant_struct(CodegenContext& ctx, const AstBracedExpression* node, const UserType* utype, llvm::StructType* llvm_t);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<WrappedIRValue> generate_singleton_literal(AstSingletonLiteral* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_struct(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_identifier(const AstIdentifier* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_array(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_primitive(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl_global(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl_local(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_procdecl(const AstProcdecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate(AstNode* node, CodegenContext& ctx);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif //CODEGEN_HPP
