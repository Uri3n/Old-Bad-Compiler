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

#define NODE_NEEDS_VISITING(node_type)           \
   (node_type != tak::NODE_STRUCT_DEFINITION     \
    && node_type != tak::NODE_ENUM_DEFINITION    \
    && node_type != tak::NODE_INCLUDE_STMT       \
    && node_type != tak::NODE_TYPE_ALIAS         \
)                                                \

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

    bool types_are_identical(const TypeData& first, const TypeData& second);
    bool type_promote_non_concrete(TypeData& left, const TypeData& right);
    bool is_type_coercion_permissible(TypeData& left, const TypeData& right);
    bool is_type_cast_permissible(const TypeData& from, const TypeData& to);
    bool is_type_invalid_in_inferred_context(const TypeData& type);
    bool is_type_cast_eligible(const TypeData& type);
    bool is_type_reassignable(const TypeData& type);
    bool can_operator_be_applied_to(token_t _operator, const TypeData& type);
    bool is_type_arithmetic_eligible(const TypeData& type, token_t _operator);
    bool is_returntype_lvalue_eligible(const TypeData& type);
    bool is_type_bwop_eligible(const TypeData& type);
    bool is_type_lop_eligible(const TypeData& type);
    bool flip_sign(TypeData& type);
    bool are_array_types_equivalent(const TypeData& first, const TypeData& second);
    bool array_has_inferred_sizes(const TypeData& type);
    void assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx, bool only_literals = false);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::optional<TypeData> visit_node(AstNode* node, CheckerContext& ctx);
    std::optional<TypeData> checker_handle_arraydecl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
    std::optional<TypeData> checker_handle_inferred_decl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
    std::optional<TypeData> visit_member_access(const AstMemberAccess* node, CheckerContext& ctx);
    std::optional<TypeData> visit_vardecl(const AstVardecl* node, CheckerContext& ctx);
    std::optional<TypeData> visit_procdecl(AstProcdecl* node, CheckerContext& ctx);
    std::optional<TypeData> visit_call(AstCall* node, CheckerContext& ctx);
    std::optional<TypeData> visit_switch(const AstSwitch* node, CheckerContext& ctx);
    std::optional<TypeData> visit_cast(const AstCast* node, CheckerContext& ctx);
    std::optional<TypeData> visit_subscript(const AstSubscript* node, CheckerContext& ctx);
    std::optional<TypeData> visit_while(AstNode* node, CheckerContext& ctx);
    std::optional<TypeData> visit_ret(const AstRet* node, CheckerContext& ctx);
    std::optional<TypeData> visit_branch(const AstBranch* node, CheckerContext& ctx);
    std::optional<TypeData> visit_for(const AstFor* node, CheckerContext& ctx);
    std::optional<TypeData> visit_binexpr(AstBinexpr* node, CheckerContext& ctx);
    std::optional<TypeData> visit_namespacedecl(const AstNamespaceDecl* node, CheckerContext& ctx);
    std::optional<TypeData> visit_unaryexpr(AstUnaryexpr* node, CheckerContext& ctx);
    std::optional<TypeData> visit_identifier(const AstIdentifier* node, CheckerContext& ctx);
    std::optional<TypeData> visit_singleton_literal(AstSingletonLiteral* node, CheckerContext& ctx);
    std::optional<TypeData> visit_block(const AstBlock* node, CheckerContext& ctx);
    std::optional<TypeData> visit_sizeof(const AstSizeof* node, CheckerContext& ctx);
    std::optional<TypeData> visit_defer_if(const AstDeferIf* node, CheckerContext& ctx);
    std::optional<TypeData> visit_defer(const AstDefer* node, CheckerContext& ctx);
    std::optional<TypeData> visit_brk_or_cont(const AstNode* node, CheckerContext& ctx);
    std::optional<TypeData> get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, EntityTable& tbl);
    std::optional<TypeData> get_dereferenced_type(const TypeData& type);
    std::optional<TypeData> get_addressed_type(const TypeData& type);
    std::optional<TypeData> get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx, bool only_literals = false);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TypeData to_lvalue(const TypeData& type);
    TypeData to_rvalue(const TypeData& type);
    TypeData convert_int_lit_to_type(const AstSingletonLiteral* node);
    TypeData convert_float_lit_to_type(const AstSingletonLiteral* node);
}
#endif //CHECKER_HPP
