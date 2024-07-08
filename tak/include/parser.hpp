//
// Created by Diago on 2024-07-02.
//

#ifndef PARSER_HPP
#define PARSER_HPP
#include <ast_types.hpp>
#include <unordered_map>
#include <lexer.hpp>
#include <io.hpp>
#include <cstdlib>


//
// PARSER_ASSERT:
// should be used in situations where the callee expects certain tokens to
// have already been checked. For instance, if parse_decl is called and the
// the current token is not an identifier, we have made a fatal mistake and the process
// should terminate itself (after logging the message).
//

#define PARSER_ASSERT(condition, msg) if(!(condition)) {                        \
    print("PARSER ASSERT FAILED :: FILE: {}, LINE: {}", __FILE__, __LINE__ );   \
    print(msg);                                                                 \
    exit(1);                                                                    \
}                                                                               \


struct parser {

    uint32_t curr_sym_index                  = INVALID_SYMBOL_INDEX;
    uint16_t inside_parenthesized_expression = 0;


    std::vector<ast_node*>                                 toplevel_decls;
    std::unordered_map<uint32_t, std::unique_ptr<symbol>>  sym_table;
    std::vector<std::unordered_map<std::string, uint32_t>> scope_stack;


    void     push_scope();
    void     pop_scope();
    bool     scoped_symbol_exists(const std::string& name);
    bool     scoped_symbol_exists_at_current_scope(const std::string& name);
    uint32_t lookup_scoped_symbol(const std::string& name);
    symbol*  lookup_unique_symbol(uint32_t symbol_index);
    symbol*  create_symbol(const std::string& name, size_t src_index, uint32_t line_number, sym_t sym_type, uint16_t sym_flags);


    parser() = default;
    ~parser();
};


bool generate_ast_from_source(parser& parser, const std::string& source_file_name);

ast_node* parse_expression(parser& parser, lexer& lxr);
ast_node* parse_identifier(parser& parser, lexer& lxr);
ast_node* parse_unary_expression(parser& parser, lexer& lxr);
ast_node* parse_decl(parser& parser, lexer& lxr);
ast_node* parse_vardecl(variable* var, parser& parser, lexer& lxr);
ast_node* parse_procdecl(procedure* proc, parser& parser, lexer& lxr);
ast_node* parse_assign(parser& parser, lexer& lxr);
ast_node* parse_call(parser& parser, lexer& lxr);
ast_node* parse_keyword(parser& parser, lexer& lxr);
ast_node* parse_singleton_literal(parser& parser, lexer& lxr);
ast_node* parse_braced_expression(parser& parser, lexer& lxr);
ast_node* parse_parenthesized_expression(parser& parser, lexer& lxr);
ast_vardecl* parse_parameterized_vardecl(parser& parser, lexer& lxr);
ast_node* parse_binary_expression(ast_node* left_operand, parser& parser, lexer& lxr);

var_t token_to_var_t(token_t tok_t);

#endif //PARSER_HPP
