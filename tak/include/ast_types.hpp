//
// Created by Diago on 2024-07-02.
//

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <token.hpp>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum ast_node_t : uint16_t {
    NODE_NONE,
    NODE_VARDECL,
    NODE_PROCDECL,
    NODE_BINEXPR,
    NODE_UNARYEXPR,
    NODE_ASSIGN,
    NODE_IDENT,
    NODE_BRANCH,
    NODE_IF,
    NODE_ELSE,
    NODE_FOR,
    NODE_SWITCH,
    NODE_CASE,
    NODE_DEFAULT,
    NODE_WHILE,
    NODE_DOWHILE,
    NODE_CALL,
    NODE_BRK,
    NODE_CONT,
    NODE_RET,
    NODE_SINGLETON_LITERAL,
    NODE_BRACED_EXPRESSION,
    NODE_STRUCT_DEFINITION,
    NODE_ENUM_DEFINITION,
    NODE_SUBSCRIPT,
    NODE_NAMESPACEDECL
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ast_node {
    ast_node_t               type   = NODE_NONE;
    std::optional<ast_node*> parent = std::nullopt;

    virtual ~ast_node() = default;
    explicit ast_node(const ast_node_t type) : type(type) {}
};

struct ast_singleton_literal final : ast_node {
    std::string value;
    token_t     literal_type = TOKEN_NONE;

    ~ast_singleton_literal() override = default;
    ast_singleton_literal() : ast_node(NODE_SINGLETON_LITERAL) {}
};

struct ast_braced_expression final : ast_node {
    std::vector<ast_node*> members;

    ~ast_braced_expression() override;
    ast_braced_expression() : ast_node(NODE_BRACED_EXPRESSION) {}
};

struct ast_binexpr final : ast_node {
    token_t _operator  = TOKEN_NONE;
    ast_node* left_op  = nullptr;
    ast_node* right_op = nullptr;

    ~ast_binexpr() override;
    ast_binexpr() : ast_node(NODE_BINEXPR) {}
};

struct ast_if final : ast_node {
    std::vector<ast_node*> body;
    ast_node*              condition = nullptr;

    ~ast_if() override;
    ast_if() : ast_node(NODE_IF) {}
};

struct ast_else final : ast_node {
    std::vector<ast_node*> body;

    ~ast_else() override;
    ast_else() : ast_node(NODE_ELSE) {}
};

struct ast_branch final : ast_node {
    std::vector<ast_if*>     conditions;               // consecutive if/else statements
    std::optional<ast_else*> _else = std::nullopt;     // can be null!

    ~ast_branch() override;
    ast_branch() : ast_node(NODE_BRANCH) {}
};

struct ast_case final : ast_node {
    ast_singleton_literal* value = nullptr;
    bool fallthrough             = false;

    std::vector<ast_node*> body;

    ~ast_case() override;
    ast_case() : ast_node(NODE_CASE) {}
};

struct ast_default final : ast_node {                  // default case in switches.
    std::vector<ast_node*> body;

    ~ast_default() override;
    ast_default() : ast_node(NODE_DEFAULT) {}
};

struct ast_switch final : ast_node {
    ast_node*                   target   = nullptr;
    ast_default*                _default = nullptr;
    std::vector<ast_case*>      cases;

    ~ast_switch() override;
    ast_switch() : ast_node(NODE_SWITCH) {}
};

struct ast_identifier final : ast_node {
    uint32_t symbol_index = 0;                         // INVALID_SYMBOL_INDEX
    std::optional<std::string> member_name;            // used if struct member.

    ~ast_identifier() override = default;
    ast_identifier() : ast_node(NODE_IDENT) {}
};

struct ast_assign final : ast_node {
    ast_node* assigned   = nullptr;
    ast_node* expression = nullptr;

    ~ast_assign() override;
    ast_assign() : ast_node(NODE_ASSIGN) {}
};

struct ast_vardecl final : ast_node {
    ast_identifier*          identifier = nullptr;
    std::optional<ast_node*> init_value = std::nullopt; // Can be null!

    ~ast_vardecl() override;
    ast_vardecl() : ast_node(NODE_VARDECL) {}
};

struct ast_procdecl final : ast_node {
    ast_identifier*           identifier = nullptr;
    std::vector<ast_vardecl*> parameters;
    std::vector<ast_node*>    body;

    ~ast_procdecl() override;
    ast_procdecl() : ast_node(NODE_PROCDECL) {}
};

struct ast_structdef final : ast_node {
    std::string name;

    ~ast_structdef() override = default;
    ast_structdef() : ast_node(NODE_STRUCT_DEFINITION) {}
};

struct ast_call final : ast_node {
    ast_identifier*        identifier = nullptr;
    std::vector<ast_node*> arguments;                   // Can be empty, if the procedure is "paramless".

    ~ast_call() override;
    ast_call() : ast_node(NODE_CALL) {}
};

struct ast_for final : ast_node {
    std::vector<ast_node*> body;

    std::optional<ast_node*> init      = std::nullopt;
    std::optional<ast_node*> condition = std::nullopt;
    std::optional<ast_node*> update    = std::nullopt;

    ~ast_for() override;
    ast_for() : ast_node(NODE_FOR) {}
};

struct ast_unaryexpr final : ast_node {
    token_t   _operator = TOKEN_NONE;
    ast_node* operand   = nullptr;

    ~ast_unaryexpr() override;
    ast_unaryexpr() : ast_node(NODE_UNARYEXPR) {}
};

struct ast_while final : ast_node {
    ast_node*              condition = nullptr;
    std::vector<ast_node*> body;

    ~ast_while() override;
    ast_while() : ast_node(NODE_WHILE) {}
};

struct ast_dowhile final : ast_node {
    ast_node*              condition = nullptr;
    std::vector<ast_node*> body;

    ~ast_dowhile() override;
    ast_dowhile() : ast_node(NODE_DOWHILE) {}
};

struct ast_subscript final : ast_node {
    ast_node* operand = nullptr;                   // The thing being subscripted.
    ast_node* value   = nullptr;                   // The index value.

    ~ast_subscript() override;
    ast_subscript() : ast_node(NODE_SUBSCRIPT) {}
};

struct ast_namespacedecl final : ast_node {
    std::string            full_path;
    std::vector<ast_node*> children;

    ~ast_namespacedecl() override;
    ast_namespacedecl() : ast_node(NODE_NAMESPACEDECL) {}
};

struct ast_ret final : ast_node {
    std::optional<ast_node*> value = std::nullopt; // can be null!

    ~ast_ret() override;
    ast_ret() : ast_node(NODE_RET) {}
};

struct ast_cont final : ast_node {
    ~ast_cont() override = default;
    ast_cont() : ast_node(NODE_CONT) {}
};

struct ast_brk final : ast_node {
    ~ast_brk() override = default;
    ast_brk() : ast_node(NODE_BRK) {}
};

#endif //AST_TYPES_HPP
