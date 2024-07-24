//
// Created by Diago on 2024-07-22.
//

#ifndef CHECKER_HPP
#define CHECKER_HPP
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
bool is_type_coercion_permissible(const type_data& left, const type_data& right);
bool is_type_cast_eligible(const type_data& type);
bool is_type_reassignable(const type_data& type);

std::optional<type_data> visit_node(ast_node* node, checker_context& ctx);
std::optional<type_data> visit_binexpr(const ast_binexpr* node, checker_context& ctx);
std::optional<type_data> visit_unaryexpr(const ast_unaryexpr* node, checker_context& ctx);
std::optional<type_data> visit_identifier(const ast_identifier* node, const checker_context& ctx);
std::optional<type_data> visit_singleton_literal(ast_singleton_literal* node);
std::optional<type_data> get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, parser& parser);

#endif //CHECKER_HPP
