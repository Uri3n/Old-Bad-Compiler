//
// Created by Diago on 2024-07-22.
//

#ifndef CHECKER_HPP
#define CHECKER_HPP
#include <limits>
#include <cstdint>
#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <typeindex>
#include <parser.hpp>
#include <semantic_error_handler.hpp>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    class CheckerContext {
    public:
        EntityTable&         tbl_;
        SemanticErrorHandler errs_;

        explicit CheckerContext(EntityTable& entities) : tbl_(entities) {}
        ~CheckerContext() = default;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::optional<TypeData> evaluate(AstNode* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_arraydecl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
    std::optional<TypeData> evaluate_inferred_decl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
    std::optional<TypeData> evaluate_member_access(const AstMemberAccess* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_vardecl(const AstVardecl* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_procdecl(AstProcdecl* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_call(AstCall* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_switch(const AstSwitch* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_cast(const AstCast* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_subscript(const AstSubscript* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_while(AstNode* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_ret(const AstRet* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_branch(const AstBranch* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_for(const AstFor* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_binexpr(AstBinexpr* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_namespacedecl(const AstNamespaceDecl* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_unaryexpr(AstUnaryexpr* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_identifier(const AstIdentifier* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_singleton_literal(AstSingletonLiteral* node, CheckerContext& _);
    std::optional<TypeData> evaluate_block(const AstBlock* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_sizeof(const AstSizeof* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_defer_if(const AstDeferIf* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_defer(const AstDefer* node, CheckerContext& ctx);
    std::optional<TypeData> evaluate_brk_or_cont(const AstNode* node, CheckerContext& ctx);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::optional<TypeData> get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, EntityTable& tbl);
    std::optional<TypeData> get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx, bool only_literals = false);
    void assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx, bool only_literals = false);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TypeData convert_int_lit_to_type(const AstSingletonLiteral* node);
    TypeData convert_float_lit_to_type(const AstSingletonLiteral* node);
}

#endif //CHECKER_HPP
