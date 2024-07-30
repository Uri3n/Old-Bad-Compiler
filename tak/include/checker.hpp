//
// Created by Diago on 2024-07-22.
//

#ifndef CHECKER_HPP
#define CHECKER_HPP
#include <limits>
#include <cstdint>
#include <cassert>
#include <string>
#include <typeindex>
#include <parser.hpp>

#define MAX_ERROR_COUNT 45 // Maximum errors permitted before panic occurs


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NODE_NEEDS_VISITING(node_type) (node_type != NODE_BRK        \
        && node_type != NODE_STRUCT_DEFINITION                       \
        && node_type != NODE_ENUM_DEFINITION                         \
        && node_type != NODE_CONT                                    \
        && node_type != NODE_TYPE_ALIAS                              \
)                                                                    \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CheckerContext {
public:

    uint32_t error_count_   = 0;
    uint32_t warning_count_ = 0;
    Lexer&   lxr_;
    Parser&  parser_;

    void raise_error(const std::string& message, size_t position);
    void raise_warning(const std::string& message, size_t position);

    explicit CheckerContext(Lexer& lxr, Parser& parser) : lxr_(lxr), parser_(parser) {}
    ~CheckerContext() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool visit_tree(Parser& parser, Lexer& lxr);
bool types_are_identical(const TypeData& first, const TypeData& second);
bool type_promote_non_concrete(TypeData& left, const TypeData& right);
bool is_type_coercion_permissible(TypeData& left, const TypeData& right);
bool is_type_cast_permissible(const TypeData& from, const TypeData& to);
bool is_type_invalid_in_inferred_context(const TypeData& type);
bool is_type_cast_eligible(const TypeData& type);
bool is_type_reassignable(const TypeData& type);
bool can_operator_be_applied_to(token_t _operator, const TypeData& type);
bool is_type_arithmetic_eligible(const TypeData& type, token_t _operator);
bool is_type_bwop_eligible(const TypeData& type);
bool is_type_lop_eligible(const TypeData& type);
bool flip_sign(TypeData& type);
bool are_array_types_equivalent(const TypeData& first, const TypeData& second);
bool array_has_inferred_sizes(const TypeData& type);
void assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<TypeData> visit_node(AstNode* node, CheckerContext& ctx);
std::optional<TypeData> checker_handle_arraydecl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
std::optional<TypeData> checker_handle_inferred_decl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx);
std::optional<TypeData> visit_member_access(const AstMemberAccess* node, CheckerContext& ctx);
std::optional<TypeData> visit_vardecl(const AstVardecl* node, CheckerContext& ctx);
std::optional<TypeData> visit_procdecl(const AstProcdecl* node, CheckerContext& ctx);
std::optional<TypeData> visit_call(const AstCall* node, CheckerContext& ctx);
std::optional<TypeData> visit_switch(const AstSwitch* node, CheckerContext& ctx);
std::optional<TypeData> visit_cast(const AstCast* node, CheckerContext& ctx);
std::optional<TypeData> visit_subscript(const AstSubscript* node, CheckerContext& ctx);
std::optional<TypeData> visit_while(AstNode* node, CheckerContext& ctx);
std::optional<TypeData> visit_ret(const AstRet* node, CheckerContext& ctx);
std::optional<TypeData> visit_branch(const AstBranch* node, CheckerContext& ctx);
std::optional<TypeData> visit_for(const AstFor* node, CheckerContext& ctx);
std::optional<TypeData> visit_binexpr(const AstBinexpr* node, CheckerContext& ctx);
std::optional<TypeData> visit_namespacedecl(const AstNamespaceDecl* node, CheckerContext& ctx);
std::optional<TypeData> visit_unaryexpr(const AstUnaryexpr* node, CheckerContext& ctx);
std::optional<TypeData> visit_identifier(const AstIdentifier* node, CheckerContext& ctx);
std::optional<TypeData> visit_singleton_literal(AstSingletonLiteral* node, CheckerContext& ctx);
std::optional<TypeData> visit_block(const AstBlock* node, CheckerContext& ctx);
std::optional<TypeData> visit_sizeof(const AstSizeof* node, CheckerContext& ctx);
std::optional<TypeData> visit_defer_if(const AstDeferIf* node, CheckerContext& ctx);
std::optional<TypeData> visit_defer(const AstDefer* node, CheckerContext& ctx);
std::optional<TypeData> get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, Parser& parser);
std::optional<TypeData> get_dereferenced_type(const TypeData& type);
std::optional<TypeData> get_addressed_type(const TypeData& type);
std::optional<TypeData> get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TypeData convert_int_lit_to_type(const AstSingletonLiteral* node);
TypeData convert_float_lit_to_type(const AstSingletonLiteral* node);

#endif //CHECKER_HPP
