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

#define INVALID_SYMBOL_INDEX 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum sym_t : uint8_t {
    SYM_NONE,
    SYM_VARIABLE,
    SYM_PROCEDURE
};

enum var_t : uint16_t {
    VAR_NONE,
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    F32,
    F64,
    BOOLEAN,
    USER_DEFINED_STRUCT,
    USER_DEFINED_ENUM,
};

enum ast_node_t : uint8_t {
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
    NODE_CALL,
    NODE_BRK,
    NODE_CONT,
    NODE_RET,
    NODE_SINGLETON_LITERAL,
    NODE_BRACED_EXPRESSION,
    NODE_STRUCT_DEFINITION,
};

enum sym_flags : uint32_t {
    SYM_FLAGS_NONE              = 0UL,
    SYM_VAR_IS_CONSTANT         = 1UL,
    SYM_PROC_IS_FOREIGN         = 1UL << 1,
    SYM_IS_POINTER              = 1UL << 2,
    SYM_IS_GLOBAL               = 1UL << 3,
    SYM_VAR_IS_ARRAY            = 1UL << 4,
    SYM_VAR_IS_PROCARG          = 1UL << 5,
    SYM_VAR_DEFAULT_INITIALIZED = 1UL << 6,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct symbol {
    sym_t    sym_type      = SYM_NONE;
    uint32_t symbol_index  = INVALID_SYMBOL_INDEX;
    uint16_t flags         = SYM_FLAGS_NONE;
    size_t   line_number   = 0;
    size_t   src_pos       = 0;

    std::string name;

    virtual ~symbol() = default;
    symbol()          = default;
};

struct variable final : symbol {
    var_t    variable_type = VAR_NONE;
    uint16_t pointer_depth = 0;
    uint32_t array_length  = 0;

    ~variable() override = default;
    variable()           = default;
};

struct procedure final : symbol {
    std::vector<var_t> parameter_list;         // params are also stored within the decl node. This is here for QOL.
    var_t              return_type = VAR_NONE; // VAR_NONE here indicates a "void" return type.

    ~procedure() override = default;
    procedure()           = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ast_node {
    ast_node_t               type   = NODE_NONE;
    std::optional<ast_node*> parent = std::nullopt;

    virtual ~ast_node() = default;
    explicit ast_node(const ast_node_t type) : type(type) {}
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

struct ast_branch  final : ast_node {
    std::vector<ast_if*>     conditions;           // consecutive if/else statements
    std::optional<ast_else*> _else = std::nullopt; // can be null!

    ~ast_branch() override;
    ast_branch() : ast_node(NODE_BRANCH) {}
};

struct ast_identifier final : ast_node {
    uint32_t    symbol_index = INVALID_SYMBOL_INDEX;

    ~ast_identifier() override = default;
    ast_identifier() : ast_node(NODE_IDENT) {}
};

struct ast_assign final : ast_node {
    ast_identifier* identifier = nullptr;
    ast_node*       expression = nullptr;

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

struct ast_call final : ast_node {
    ast_identifier*        identifier = nullptr;
    std::vector<ast_node*> arguments;

    ~ast_call() override;
    ast_call() : ast_node(NODE_CALL) {}
};

struct ast_for final : ast_node {
    std::vector<ast_node*> body;
    ast_node*              initialization = nullptr;
    ast_node*              condition      = nullptr;
    ast_node*              update         = nullptr;

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

struct ast_singleton_literal final : ast_node {
    std::string value;

    ~ast_singleton_literal() override = default;
    ast_singleton_literal() : ast_node(NODE_SINGLETON_LITERAL) {}
};

struct ast_braced_expression final : ast_node {
    std::vector<ast_node*> members;

    ~ast_braced_expression() override;
    ast_braced_expression() : ast_node(NODE_BRACED_EXPRESSION) {}
};

#endif //AST_TYPES_HPP
