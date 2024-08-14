//
// Created by Diago on 2024-07-02.
//

#ifndef PARSER_HPP
#define PARSER_HPP
#include <ast_types.hpp>
#include <entity_table.hpp>
#include <lexer.hpp>
#include <io.hpp>
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define parser_assert(condition, msg) if(!(condition)) {        \
    tak::print("PARSER ASSERTION \"{}\" FAILED.", #condition);  \
    panic(msg);                                                 \
}                                                               \

#define NODE_VALID_SUBEXPRESSION(node_type)        \
 (node_type == tak::NODE_CALL                      \
    || node_type == tak::NODE_IDENT                \
    || node_type == tak::NODE_BINEXPR              \
    || node_type == tak::NODE_SINGLETON_LITERAL    \
    || node_type == tak::NODE_UNARYEXPR            \
    || node_type == tak::NODE_BRACED_EXPRESSION    \
    || node_type == tak::NODE_CAST                 \
    || node_type == tak::NODE_SUBSCRIPT            \
    || node_type == tak::NODE_CAST                 \
    || node_type == tak::NODE_MEMBER_ACCESS        \
    || node_type == tak::NODE_SIZEOF               \
)                                                  \

#define NODE_VALID_AT_TOPLEVEL(node_type)          \
   (node_type == tak::NODE_VARDECL                 \
    || node_type == tak::NODE_STRUCT_DEFINITION    \
    || node_type == tak::NODE_NAMESPACEDECL        \
    || node_type == tak::NODE_PROCDECL             \
    || node_type == tak::NODE_INCLUDE_STMT         \
    || node_type == tak::NODE_ENUM_DEFINITION      \
    || node_type == tak::NODE_TYPE_ALIAS           \
)                                                  \

#define NODE_EXPR_NEVER_NEEDS_TERMINAL(node_type)  \
   (node_type == tak::NODE_PROCDECL                \
    || node_type == tak::NODE_BRANCH               \
    || node_type == tak::NODE_IF                   \
    || node_type == tak::NODE_ELSE                 \
    || node_type == tak::NODE_FOR                  \
    || node_type == tak::NODE_WHILE                \
    || node_type == tak::NODE_PROCDECL             \
    || node_type == tak::NODE_SWITCH               \
    || node_type == tak::NODE_NAMESPACEDECL        \
    || node_type == tak::NODE_BLOCK                \
    || node_type == tak::NODE_STRUCT_DEFINITION    \
    || node_type == tak::NODE_ENUM_DEFINITION      \
)                                                  \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    enum include_state_t : uint8_t {
        INCLUDE_STATE_PENDING = 0,
        INCLUDE_STATE_DONE    = 1,
    };

    struct IncludedFile {
        std::string     name;
        include_state_t state;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class Parser {
    public:
        uint16_t                  inside_parenthesized_expression_ = 0;
        std::vector<IncludedFile> included_files_;
        std::vector<AstNode*>     toplevel_decls_;
        EntityTable               tbl_;

        void dump_symbols();
        void dump_nodes();
        void dump_types();

        Parser(const Parser&)            = delete;
        Parser& operator=(const Parser&) = delete;

        Parser() = default;
        ~Parser();
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void display_node_data(AstNode* node, uint32_t depth, Parser& parser);
    bool parse_proc_signature_and_body(Symbol* proc,  AstProcdecl* node, Parser& parser, Lexer& lxr);
    std::string format_type_data(const TypeData& type, uint16_t num_tabs = 0);
    std::optional<TypeData> parse_type(Parser& parser, Lexer& lxr);
    std::optional<std::vector<uint32_t>> parse_array_data(Lexer& lxr);
    std::optional<std::string> get_namespaced_identifier(Lexer& lxr);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    AstNode* parse_type_alias(Parser& parser, Lexer& lxr);
    AstNode* parse_callconv(Parser& parser, Lexer& lxr);
    AstNode* parse_include(Parser& parser, Lexer& lxr);
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
    AstNode* parse_usertype_decl(Symbol* sym, Parser& parser, Lexer& lxr);
    AstVardecl* parse_parameterized_vardecl(Parser& parser, Lexer& lxr);
    AstNode* parse_proc_ptr(Symbol* proc, Parser& parser, Lexer& lxr);
    AstNode* parse_keyword(Parser& parser, Lexer& lxr);
    AstNode* parse_singleton_literal(Parser& parser, Lexer& lxr);
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
}
#endif //PARSER_HPP
