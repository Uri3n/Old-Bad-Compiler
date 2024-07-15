//
// Created by Diago on 2024-07-02.
//

#ifndef PARSER_HPP
#define PARSER_HPP
#include <ast_types.hpp>
#include <sym_types.hpp>
#include <unordered_map>
#include <lexer.hpp>
#include <io.hpp>
#include <cstdlib>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PARSER_ASSERT(condition, msg) if(!(condition)) {                        \
    print("PARSER ASSERT FAILED :: FILE: {}, LINE: {}", __FILE__, __LINE__ );   \
    print(msg);                                                                 \
    exit(1);                                                                    \
}                                                                               \

#define VALID_SUBEXPRESSION(node_type) (node_type == NODE_ASSIGN                \
    || node_type == NODE_CALL                                                   \
    || node_type == NODE_IDENT                                                  \
    || node_type == NODE_BINEXPR                                                \
    || node_type == NODE_SINGLETON_LITERAL                                      \
    || node_type == NODE_UNARYEXPR                                              \
    || node_type == NODE_BRACED_EXPRESSION                                      \
)                                                                               \

#define VALID_UNARY_OPERATOR(token) (token.kind == UNARY_EXPR_OPERATOR          \
    || token == PLUS                                                            \
    || token == SUB                                                             \
    || token == BITWISE_XOR_OR_PTR                                              \
    || token == BITWISE_AND                                                     \
)                                                                               \

#define EXPR_NEVER_NEEDS_TERMINAL(node_type) (node_type == NODE_PROCDECL        \
    || node_type == NODE_BRANCH                                                 \
    || node_type == NODE_IF                                                     \
    || node_type == NODE_ELSE                                                   \
    || node_type == NODE_FOR                                                    \
    || node_type == NODE_WHILE                                                  \
    || node_type == NODE_DOWHILE                                                \
    || node_type == NODE_PROCDECL                                               \
    || node_type == NODE_SWITCH                                                 \
)                                                                               \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class parser {
public:

    uint32_t curr_sym_index = INVALID_SYMBOL_INDEX;
    uint16_t inside_parenthesized_expression = 0;

    std::vector<ast_node*> toplevel_decls;
    std::vector<std::unordered_map<std::string, uint32_t>> scope_stack;

    std::unordered_map<uint32_t, symbol> sym_table;
    std::unordered_map<std::string, std::vector<member_data>> type_table;


    void push_scope();
    void pop_scope();

    bool scoped_symbol_exists(const std::string& name);
    bool scoped_symbol_exists_at_current_scope(const std::string& name);

    symbol*  lookup_unique_symbol(uint32_t symbol_index);
    symbol*  create_symbol(
        const std::string& name,
        size_t src_index,
        uint32_t line_number,
        sym_t sym_type,
        uint16_t sym_flags,
        const std::optional<type_data>& data = std::nullopt
    );

    void dump_symbols();
    void dump_nodes();
    void dump_types();

    bool create_type(const std::string& name, std::vector<member_data>&& type_data);
    bool type_exists(const std::string& name);
    std::vector<member_data>* lookup_type(const std::string& name);
    uint32_t lookup_scoped_symbol(const std::string& name);


    parser() = default;
    ~parser();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool generate_ast_from_source(parser& parser, const std::string& source_file_name);
void display_node_data(ast_node* node, uint32_t depth, parser& parser);
var_t token_to_var_t(token_t tok_t);
std::string var_t_to_string(var_t type);
std::string format_type_data(const type_data& type, uint16_t num_tabs = 0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<type_data> parse_type(parser& parser, lexer& lxr);
std::optional<std::vector<uint32_t>> parse_array_data(lexer& lxr);

ast_node* parse_ret(parser& parser, lexer& lxr);
ast_node* parse_cont(lexer& lxr);
ast_node* parse_brk(lexer& lxr);
ast_node* parse_for(parser& parser, lexer& lxr);
ast_node* parse_while(parser& parser, lexer& lxr);
ast_node* parse_branch(parser& parser, lexer& lxr);
ast_case* parse_case(parser& parser, lexer& lxr);
ast_default* parse_default(parser& parser, lexer& lxr);
ast_node* parse_switch(parser& parser, lexer& lxr);
ast_node* parse_structdef(parser& parser, lexer& lxr);
ast_node* parse_member_access(parser& parser, lexer& lxr);
ast_node* parse_expression(parser& parser, lexer& lxr, bool subexpression, bool parse_single = false);
ast_node* parse_identifier(parser& parser, lexer& lxr);
ast_node* parse_unary_expression(parser& parser, lexer& lxr);
ast_node* parse_decl(parser& parser, lexer& lxr);
ast_node* parse_vardecl(symbol* var, parser& parser, lexer& lxr);
ast_node* parse_procdecl(symbol* proc, parser& parser, lexer& lxr);
ast_node* parse_structdecl(symbol* _struct, parser& parser, lexer& lxr);
ast_node* parse_assign(ast_node* assigned, parser& parser, lexer& lxr);
ast_node* parse_call(parser& parser, lexer& lxr);
ast_vardecl* parse_parameterized_vardecl(parser& parser, lexer& lxr);
ast_node* parse_proc_ptr(symbol* proc, parser& parser, lexer& lxr);
ast_node* parse_keyword(parser& parser, lexer& lxr);
ast_node* parse_singleton_literal(parser& parser, lexer& lxr);
ast_node* parse_braced_expression(parser& parser, lexer& lxr);
ast_node* parse_parenthesized_expression(parser& parser, lexer& lxr);
ast_node* parse_subscript(ast_node* operand, parser& parser, lexer& lxr);
ast_node* parse_binary_expression(ast_node* left_operand, parser& parser, lexer& lxr);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //PARSER_HPP
