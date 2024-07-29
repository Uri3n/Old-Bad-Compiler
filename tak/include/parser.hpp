//
// Created by Diago on 2024-07-02.
//

#ifndef PARSER_HPP
#define PARSER_HPP
#include <ast_types.hpp>
#include <unordered_map>
#include <lexer.hpp>
#include <io.hpp>
#include <panic.hpp>
#include <cstdlib>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define parser_assert(condition, msg) if(!(condition)) {                        \
    print("PARSER ASSERT FAILED :: FILE: {}, LINE: {}", __FILE__, __LINE__ );   \
    print(msg);                                                                 \
    exit(1);                                                                    \
}                                                                               \

#define parser_expect(condition, msg) if((condition)) {                         \
   lxr.raise_error(msg);                                                        \
   return nullptr;                                                              \
}                                                                               \

#define VALID_SUBEXPRESSION(node_type) (node_type == NODE_CALL                  \
    || node_type == NODE_IDENT                                                  \
    || node_type == NODE_BINEXPR                                                \
    || node_type == NODE_SINGLETON_LITERAL                                      \
    || node_type == NODE_UNARYEXPR                                              \
    || node_type == NODE_BRACED_EXPRESSION                                      \
    || node_type == NODE_CAST                                                   \
    || node_type == NODE_SUBSCRIPT                                              \
    || node_type == NODE_CAST                                                   \
    || node_type == NODE_MEMBER_ACCESS                                          \
    || node_type == NODE_SIZEOF                                                 \
)                                                                               \

#define EXPR_NEVER_NEEDS_TERMINAL(node_type) (node_type == NODE_PROCDECL        \
    || node_type == NODE_BRANCH                                                 \
    || node_type == NODE_IF                                                     \
    || node_type == NODE_ELSE                                                   \
    || node_type == NODE_FOR                                                    \
    || node_type == NODE_WHILE                                                  \
    || node_type == NODE_PROCDECL                                               \
    || node_type == NODE_SWITCH                                                 \
    || node_type == NODE_NAMESPACEDECL                                          \
    || node_type == NODE_BLOCK                                                  \
    || node_type == NODE_STRUCT_DEFINITION                                      \
    || node_type == NODE_ENUM_DEFINITION                                        \
)                                                                               \

#define VALID_AT_TOPLEVEL(node_type) (node_type == NODE_VARDECL                 \
    || node_type == NODE_STRUCT_DEFINITION                                      \
    || node_type == NODE_NAMESPACEDECL                                          \
    || node_type == NODE_PROCDECL                                               \
    || node_type == NODE_ENUM_DEFINITION                                        \
    || node_type == NODE_TYPE_ALIAS                                             \
)                                                                               \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class parser {
public:

    uint32_t curr_sym_index_ = INVALID_SYMBOL_INDEX;
    uint16_t inside_parenthesized_expression_ = 0;

    std::vector<std::string> namespace_stack_;
    std::vector<ast_node*>   toplevel_decls_;

    std::vector<std::unordered_map<std::string, uint32_t>>    scope_stack_;
    std::unordered_map<uint32_t, symbol>                      sym_table_;
    std::unordered_map<std::string, std::vector<member_data>> type_table_;
    std::unordered_map<std::string, type_data>                type_aliases_;


    void push_scope();
    void pop_scope();

    bool scoped_symbol_exists(const std::string& name);
    bool scoped_symbol_exists_at_current_scope(const std::string& name);

    symbol*  lookup_unique_symbol(uint32_t symbol_index);
    symbol*  create_symbol(
        const std::string& name,
        size_t   src_index,
        uint32_t line_number,
        type_kind_t    sym_type,
        uint16_t sym_flags,
        const std::optional<type_data>& data = std::nullopt
    );

    void dump_symbols();
    void dump_nodes();
    void dump_types();

    bool enter_namespace(const std::string& name);
    bool namespace_exists(const std::string& name);
    void leave_namespace();
    std::string namespace_as_string();

    std::string get_canonical_name(const std::string& name);
    std::string get_canonical_type_name(const std::string& name);
    std::string get_canonical_sym_name(const std::string& name);

    bool create_type(const std::string& name, std::vector<member_data>&& type_data);
    bool type_exists(const std::string& name);
    std::vector<member_data>* lookup_type(const std::string& name);
    uint32_t lookup_scoped_symbol(const std::string& name);

    type_data  lookup_type_alias(const std::string& name);
    bool       create_type_alias(const std::string& name, const type_data& data);
    bool       type_alias_exists(const std::string& name);

    parser() = default;
    ~parser();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void display_node_data(ast_node* node, uint32_t depth, parser& parser);
uint16_t precedence_of(token_t _operator);
var_t token_to_var_t(token_t tok_t);
std::string var_t_to_string(var_t type);
std::string format_type_data(const type_data& type, uint16_t num_tabs = 0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<type_data> parse_type(parser& parser, lexer& lxr);
std::optional<std::vector<uint32_t>> parse_array_data(lexer& lxr);
std::optional<std::string> get_namespaced_identifier(lexer& lxr);

ast_node* parse_type_alias(parser& parser, lexer& lxr);
ast_node* parse_compiler_directive(parser& parser, lexer& lxr);
ast_node* parse_defer(parser& parser, lexer& lxr);
ast_node* parse_ret(parser& parser, lexer& lxr);
ast_node* parse_cont(lexer& lxr);
ast_node* parse_brk(lexer& lxr);
ast_node* parse_sizeof(parser& parser, lexer& lxr);
ast_node* parse_for(parser& parser, lexer& lxr);
ast_node* parse_dowhile(parser& parser, lexer& lxr);
ast_node* parse_block(parser& parser, lexer& lxr);
ast_node* parse_while(parser& parser, lexer& lxr);
ast_node* parse_branch(parser& parser, lexer& lxr);
ast_case* parse_case(parser& parser, lexer& lxr);
ast_default* parse_default(parser& parser, lexer& lxr);
ast_node* parse_switch(parser& parser, lexer& lxr);
ast_node* parse_structdef(parser& parser, lexer& lxr);
ast_node* parse_member_access(ast_node* target, lexer& lxr);
ast_node* parse_expression(parser& parser, lexer& lxr, bool subexpression, bool parse_single = false);
ast_node* parse_identifier(parser& parser, lexer& lxr);
ast_node* parse_unary_expression(parser& parser, lexer& lxr);
ast_node* parse_decl(parser& parser, lexer& lxr);
ast_node* parse_vardecl(symbol* var, parser& parser, lexer& lxr);
ast_node* parse_procdecl(symbol* proc, parser& parser, lexer& lxr);
ast_node* parse_structdecl(symbol* _struct, parser& parser, lexer& lxr);
ast_vardecl* parse_parameterized_vardecl(parser& parser, lexer& lxr);
ast_node* parse_proc_ptr(symbol* proc, parser& parser, lexer& lxr);
ast_node* parse_keyword(parser& parser, lexer& lxr);
ast_node* parse_singleton_literal(parser& parser, lexer& lxr);
ast_node* parse_braced_expression(parser& parser, lexer& lxr);
ast_node* parse_namespace(parser& parser, lexer& lxr);
ast_node* parse_enumdef(parser& parser, lexer& lxr);
ast_node* parse_parenthesized_expression(parser& parser, lexer& lxr);
ast_node* parse_subscript(ast_node* operand, parser& parser, lexer& lxr);
ast_node* parse_call(ast_node* operand, parser& parser, lexer& lxr);
ast_node* parse_binary_expression(ast_node* left_operand, parser& parser, lexer& lxr);
ast_node* parse_cast(parser& parser, lexer& lxr);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //PARSER_HPP
