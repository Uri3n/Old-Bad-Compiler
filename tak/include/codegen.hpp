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
        TypeData tak_type;
        llvm::Value* value = nullptr;
        bool loadable      = false;

        static std::shared_ptr<WrappedIRValue> get_empty();
        static std::shared_ptr<WrappedIRValue> maybe_adjust(
            std::shared_ptr<WrappedIRValue> wrapped,
            class CodegenContext& ctx
        );

        static std::shared_ptr<WrappedIRValue> maybe_adjust(
            llvm::Value* value,
            const std::optional<TypeData>& tak_type,
            CodegenContext& ctx
        );

        static std::shared_ptr<WrappedIRValue> maybe_adjust(
            AstNode* node,
            CodegenContext& ctx,
            bool loadable = false
        );

        static std::shared_ptr<WrappedIRValue> create(
            llvm::Value* value = nullptr,
            const std::optional<TypeData>& tak_type = std::nullopt,
            bool loadable = false
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

    enum defer_unpack_method_t : uint8_t {
        DEFERRED_UNPACK_REGULAR,
        DEFERRED_UNPACK_UNTIL_LOOP_BASE,
        DEFERRED_UNPACK_ALL
    };

    struct DeferredStackElement {
        std::vector<AstNode*> stmts;
        bool is_loop_base = false;

        ~DeferredStackElement() = default;
        explicit DeferredStackElement(const bool is_loop_base)
            : is_loop_base(is_loop_base) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class CodegenContext {
    public:
        struct {
            llvm::BasicBlock* after = nullptr;
            llvm::BasicBlock* merge = nullptr;
        } curr_loop_;

        struct {
            std::unordered_map<std::string, std::shared_ptr<WrappedIRValue>> named_values;
            llvm::Function* func = nullptr;
            const Symbol* sym    = nullptr;
        } curr_proc_;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        EntityTable&      tbl_;         // Reference to the Tak entity table for this source file.
        llvm::LLVMContext llvm_ctx_;    // LLVM context.
        llvm::Module      mod_;         // LLVM module, same name as source file.
        llvm::IRBuilder<> builder_;     // LLVM IRBuilder, helper class for generating IR.

        std::optional<IRCastingContext>    casting_context_;  // casting context
        std::vector<DeferredStackElement>  deferred_stmts_;   // calls for defer statements

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<WrappedIRValue> set_local(const std::string& name, const std::shared_ptr<WrappedIRValue>& ptr);
        std::shared_ptr<WrappedIRValue> get_local(const std::string& name);
        bool                            local_exists(const std::string& name);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::optional<IRCastingContext> swap_casting_context(llvm::Type* llvm_t, const TypeData& tak_t);
        std::optional<IRCastingContext> delete_casting_context();
        std::optional<IRCastingContext> set_casting_context(llvm::Type* llvm_t, const TypeData& tak_t);
        bool casting_context_exists();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool inside_procedure();
        bool curr_block_has_terminator();
        bool inside_loop();
        void pop_defers();
        void push_defers(bool is_loop_base = false);
        void push_deferred_stmt(AstNode* node);
        void enter_proc(llvm::Function* func, const Symbol* sym);
        void enter_loop(llvm::BasicBlock* after, llvm::BasicBlock* merge);
        void leave_curr_proc();
        std::array<llvm::BasicBlock*, 2> leave_curr_loop();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        CodegenContext(const CodegenContext&)            = delete;
        CodegenContext& operator=(const CodegenContext&) = delete;

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
    void _unpack_defers_impl(const std::vector<AstNode*>& stmts, CodegenContext& ctx);
    void unpack_defer(const AstDefer* node, CodegenContext& ctx);
    void unpack_defer_if(const AstDeferIf* node, CodegenContext& ctx);
    void unpack_defers_regular(CodegenContext& ctx);
    void unpack_defers_until_loop_base(CodegenContext& ctx);
    void unpack_defers_all(CodegenContext& ctx);

    void unpack_defers(
        CodegenContext& ctx,
        defer_unpack_method_t method = DEFERRED_UNPACK_REGULAR,
        bool pop_after               = false
    );

    void generate_local_struct_init(
        llvm::Value* ptr,
        llvm::Type* llvm_t,
        const UserType* utype,
        const AstBracedExpression* bracedexpr,
        std::vector<llvm::Value*>& GEP_indices,
        CodegenContext& ctx
    );

    void generate_local_array_init(
        llvm::Value* ptr,
        llvm::Type* llvm_t,
        const AstBracedExpression* bracedexpr,
        std::vector<llvm::Value*>& GEP_indices,
        CodegenContext& ctx
    );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    llvm::Constant* generate_global_string_constant(CodegenContext& ctx, const AstSingletonLiteral* node);
    llvm::Constant* generate_constant_array(CodegenContext& ctx, const AstBracedExpression* node, llvm::ArrayType* llvm_t);
    llvm::Constant* generate_constant_struct(CodegenContext& ctx, const AstBracedExpression* node, const UserType* utype, llvm::StructType* llvm_t);
    llvm::Value*    generate_to_i1(std::shared_ptr<WrappedIRValue> to_convert, CodegenContext& ctx);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<WrappedIRValue> generate_singleton_literal(AstSingletonLiteral* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_struct(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_identifier(const AstIdentifier* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_conditional_not(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_bitwise_not(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_unary_minus(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_decrement(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_increment(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_add(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_addeq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_sub(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_subeq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_div(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_diveq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_mul(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_muleq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_mod(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_modeq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_assign_bracedexpr(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_assign(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_bitwise_or(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_bitwise_oreq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_bitwise_and(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_bitwise_andeq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_xor(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_xoreq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_lshift(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_lshifteq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_rshift(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_rshifteq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_eq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_neq(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_lt(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_lte(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_gt(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_gte(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_conditional_and(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_conditional_or(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_sizeof(const AstSizeof* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_cast(const AstCast* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_switch(const AstSwitch* node, CodegenContext& ctx);

    std::shared_ptr<WrappedIRValue> generate_for(const AstFor* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_while(const AstWhile* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_dowhile(const AstDoWhile* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_brk(CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_cont(CodegenContext& ctx);

    std::shared_ptr<WrappedIRValue> generate_branch(const AstBranch* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_blk(const AstBlock* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_member_access(const AstMemberAccess* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_subscript(const AstSubscript* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_ret(const AstRet* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_call(const AstCall* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_binexpr(const AstBinexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_address_of(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_dereference(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_unaryexpr(const AstUnaryexpr* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_array(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_global_primitive(const AstVardecl* node, const Symbol* sym, llvm::GlobalVariable* global, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl_global(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl_local(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_vardecl(const AstVardecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_procdecl(const AstProcdecl* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate_defer(AstNode* node, CodegenContext& ctx);
    std::shared_ptr<WrappedIRValue> generate(AstNode* node, CodegenContext& ctx);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif //CODEGEN_HPP
