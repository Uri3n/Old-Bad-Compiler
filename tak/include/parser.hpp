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

class Parser {
public:

    uint32_t curr_sym_index_ = INVALID_SYMBOL_INDEX;
    uint16_t inside_parenthesized_expression_ = 0;

    std::vector<std::string> namespace_stack_;
    std::vector<AstNode*>    toplevel_decls_;

    std::vector<std::unordered_map<std::string, uint32_t>>   scope_stack_;
    std::unordered_map<uint32_t, Symbol>                     sym_table_;
    std::unordered_map<std::string, std::vector<MemberData>> type_table_;
    std::unordered_map<std::string, TypeData>                type_aliases_;


    void push_scope();
    void pop_scope();

    bool scoped_symbol_exists(const std::string& name);
    bool scoped_symbol_exists_at_current_scope(const std::string& name);

    Symbol*  lookup_unique_symbol(uint32_t symbol_index);
    Symbol*  create_symbol(
        const std::string& name,
        size_t src_index,
        uint32_t line_number,
        type_kind_t sym_type,
        uint64_t sym_flags,
        const std::optional<TypeData>& data = std::nullopt
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

    bool create_type(const std::string& name, std::vector<MemberData>&& type_data);
    bool type_exists(const std::string& name);
    std::vector<MemberData>* lookup_type(const std::string& name);
    uint32_t lookup_scoped_symbol(const std::string& name);

    TypeData  lookup_type_alias(const std::string& name);
    bool      create_type_alias(const std::string& name, const TypeData& data);
    bool      type_alias_exists(const std::string& name);

    Parser() = default;
    ~Parser();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void display_node_data(AstNode* node, uint32_t depth, Parser& parser);
std::string format_type_data(const TypeData& type, uint16_t num_tabs = 0);
std::optional<TypeData> parse_type(Parser& parser, Lexer& lxr);
std::optional<std::vector<uint32_t>> parse_array_data(Lexer& lxr);
std::optional<std::string> get_namespaced_identifier(Lexer& lxr);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AstNode* parse_type_alias(Parser& parser, Lexer& lxr);
AstNode* parse_compiler_directive(Parser& parser, Lexer& lxr);
AstNode* parse_defer(Parser& parser, Lexer& lxr);
AstNode* parse_defer_if(Parser& parser, Lexer& lxr);
AstNode* parse_ret(Parser& parser, Lexer& lxr);
AstNode* parse_cont(Lexer& lxr);
AstNode* parse_brk(Lexer& lxr);
AstNode* parse_sizeof(Parser& parser, Lexer& lxr);
AstNode* parse_for(Parser& parser, Lexer& lxr);
AstNode* parse_dowhile(Parser& parser, Lexer& lxr);
AstNode* parse_block(Parser& parser, Lexer& lxr);
AstNode* parse_while(Parser& parser, Lexer& lxr);
AstNode* parse_branch(Parser& parser, Lexer& lxr);
AstCase* parse_case(Parser& parser, Lexer& lxr);
AstDefault* parse_default(Parser& parser, Lexer& lxr);
AstNode* parse_switch(Parser& parser, Lexer& lxr);
AstNode* parse_structdef(Parser& parser, Lexer& lxr);
AstNode* parse_member_access(AstNode* target, Lexer& lxr);
AstNode* parse_expression(Parser& parser, Lexer& lxr, bool subexpression, bool parse_single = false);
AstNode* parse_identifier(Parser& parser, Lexer& lxr);
AstNode* parse_unary_expression(Parser& parser, Lexer& lxr);
AstNode* parse_decl(Parser& parser, Lexer& lxr);
AstNode* parse_inferred_decl(Symbol* var, Parser& parser, Lexer& lxr);
AstNode* parse_vardecl(Symbol* var, Parser& parser, Lexer& lxr);
AstNode* parse_procdecl(Symbol* proc, Parser& parser, Lexer& lxr);
AstNode* parse_structdecl(Symbol* _struct, Parser& parser, Lexer& lxr);
AstVardecl* parse_parameterized_vardecl(Parser& parser, Lexer& lxr);
AstNode* parse_proc_ptr(Symbol* proc, Parser& parser, Lexer& lxr);
AstNode* parse_keyword(Parser& parser, Lexer& lxr);
AstNode* parse_singleton_literal(Parser& _, Lexer& lxr);
AstNode* parse_braced_expression(Parser& parser, Lexer& lxr);
AstNode* parse_namespace(Parser& parser, Lexer& lxr);
AstNode* parse_enumdef(Parser& parser, Lexer& lxr);
AstNode* parse_nullptr(Lexer& lxr);
AstNode* parse_parenthesized_expression(Parser& parser, Lexer& lxr);
AstNode* parse_subscript(AstNode* operand, Parser& parser, Lexer& lxr);
AstNode* parse_call(AstNode* operand, Parser& parser, Lexer& lxr);
AstNode* parse_binary_expression(AstNode* left_operand, Parser& parser, Lexer& lxr);
AstNode* parse_cast(Parser& parser, Lexer& lxr);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //PARSER_HPP
