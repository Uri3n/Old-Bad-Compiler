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

#define MAX_ERROR_COUNT 40 // Maximum errors permitted before panic occurs


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NODE_NEEDS_VISITING(node_type) (node_type != NODE_BRK        \
        && node_type != NODE_STRUCT_DEFINITION                       \
        && node_type != NODE_ENUM_DEFINITION                         \
        && node_type != NODE_CONT                                    \
        && node_type != NODE_TYPE_ALIAS                              \
)                                                                    \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class checker_context {
public:

    uint32_t error_count_   = 0;
    uint32_t warning_count_ = 0;
    lexer&   lxr_;
    parser&  parser_;

    void raise_error(const std::string& message, size_t position);
    void raise_warning(const std::string& message, size_t position);

    explicit checker_context(lexer& lxr, parser& parser) : lxr_(lxr), parser_(parser) {}
    ~checker_context() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool check_tree(parser& parser, lexer& lxr);
bool types_are_identical(const type_data& first, const type_data& second);
bool check_type_promote_non_concrete(type_data& left, const type_data& right);
bool is_type_coercion_permissible(type_data& left, const type_data& right);
bool is_type_cast_permissible(const type_data& from, const type_data& to);
bool is_type_invalid_in_inferred_context(const type_data& type);
bool is_type_cast_eligible(const type_data& type);
bool is_type_reassignable(const type_data& type);
bool can_operator_be_applied_to(token_t _operator, const type_data& type);
bool is_type_arithmetic_eligible(const type_data& type, token_t _operator);
bool is_type_bwop_eligible(const type_data& type);
bool is_type_lop_eligible(const type_data& type);
bool flip_sign(type_data& type);
bool are_array_types_equivalent(const type_data& first, const type_data& second);
bool array_has_inferred_sizes(const type_data& type);
void check_structassign_bracedexpr(const type_data& type, const ast_braced_expression* expr, checker_context& ctx);

std::optional<type_data> visit_node(ast_node* node, checker_context& ctx);
std::optional<type_data> checker_handle_arraydecl(symbol* sym, const ast_vardecl* decl, checker_context& ctx);
std::optional<type_data> checker_handle_inferred_decl(symbol* sym, const ast_vardecl* decl, checker_context& ctx);
std::optional<type_data> visit_member_access(const ast_member_access* node, checker_context& ctx);
std::optional<type_data> visit_vardecl(const ast_vardecl* node, checker_context& ctx);
std::optional<type_data> visit_procdecl(const ast_procdecl* node, checker_context& ctx);
std::optional<type_data> visit_call(const ast_call* node, checker_context& ctx);
std::optional<type_data> visit_switch(const ast_switch* node, checker_context& ctx);
std::optional<type_data> visit_cast(const ast_cast* node, checker_context& ctx);
std::optional<type_data> visit_subscript(const ast_subscript* node, checker_context& ctx);
std::optional<type_data> visit_while(ast_node* node, checker_context& ctx);
std::optional<type_data> visit_ret(const ast_ret* node, checker_context& ctx);
std::optional<type_data> visit_branch(const ast_branch* node, checker_context& ctx);
std::optional<type_data> visit_for(const ast_for* node, checker_context& ctx);
std::optional<type_data> visit_binexpr(const ast_binexpr* node, checker_context& ctx);
std::optional<type_data> visit_namespacedecl(const ast_namespacedecl* node, checker_context& ctx);
std::optional<type_data> visit_unaryexpr(const ast_unaryexpr* node, checker_context& ctx);
std::optional<type_data> visit_identifier(const ast_identifier* node, checker_context& ctx);
std::optional<type_data> visit_singleton_literal(ast_singleton_literal* node, checker_context& ctx);
std::optional<type_data> visit_block(const ast_block* node, checker_context& ctx);
std::optional<type_data> visit_sizeof(const ast_sizeof* node, checker_context& ctx);
std::optional<type_data> visit_defer(const ast_defer* node, checker_context& ctx);
std::optional<type_data> get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, parser& parser);
std::optional<type_data> get_dereferenced_type(const type_data& type);
std::optional<type_data> get_addressed_type(const type_data& type);
std::optional<type_data> get_bracedexpr_as_array_t(const ast_braced_expression* node, checker_context& ctx);

type_data convert_int_lit_to_type(const ast_singleton_literal* node);
type_data convert_float_lit_to_type(const ast_singleton_literal* node);

#endif //CHECKER_HPP
