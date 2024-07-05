//
// Created by Diago on 2024-07-02.
//

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP
#include <cstdint>
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
    STRING,
    FLOAT,
    BOOLEAN,
};

enum ast_node_t : uint8_t {
    NODE_NONE,
    NODE_DECL,
    NODE_BINEXPR,
    NODE_ASSIGN,
    NODE_IDENT,
    NODE_BRANCH,
    NODE_IF,
    NODE_ELSE,
    NODE_FOR,
    NODE_WHILE,
    NODE_CALL,
    NODE_STRING_LITERAL,
    NODE_INTEGER_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_BOOLEAN_LITERAL,
    NODE_ARRAY_LITERAL,
    BREAK
};

enum sym_flags : uint16_t {
    SYM_FLAGS_NONE      = 0b0000000000000000,
    SYM_VAR_IS_CONSTANT = 0b0000000000000001,
    SYM_PROC_IS_FOREIGN = 0b0000000000000010,
    SYM_IS_POINTER      = 0b0000000000000100,
    SYM_IS_GLOBAL       = 0b0000000000001000
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct symbol {
    sym_t    sym_type     = SYM_NONE;
    uint32_t symbol_index = INVALID_SYMBOL_INDEX;
    uint16_t flags        = SYM_FLAGS_NONE;
    size_t   line_number  = 0;
    size_t   src_pos      = 0;

    std::string name;

    virtual ~symbol() = default;
    symbol()          = default;
};

struct variable final : symbol {
    var_t variable_type = VAR_NONE;

    ~variable() override = default;
    variable()           = default;
};

struct procedure final : symbol {
    std::vector<var_t> parameter_list;

    ~procedure() override = default;
    procedure()           = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ast_node {
    ast_node_t type  = NODE_NONE;
    ast_node* parent = nullptr;

    virtual ~ast_node() = default;
};

struct ast_binexpr final : ast_node {
    token_t _operator  = TOKEN_NONE;
    ast_node* left_op  = nullptr;
    ast_node* right_op = nullptr;

    ~ast_binexpr() override;
    ast_binexpr() = default;
};

struct ast_branch final : ast_node {
    std::vector<ast_node*> conditions;      // consecutive if/else statements
    ast_node*              _else = nullptr; // can be null!

    ~ast_branch() override;
    ast_branch() = default;
};

struct ast_identifier final : ast_node {
    std::string name;
    uint32_t    symbol_index = INVALID_SYMBOL_INDEX;

    ~ast_identifier() override = default;
    ast_identifier()           = default;
};

struct ast_assign final : ast_node {
    ast_identifier* identifier = nullptr;
    ast_node*       expression = nullptr;

    ~ast_assign() override;
    ast_assign() = default;
};

struct ast_decl final : ast_node {
    ast_identifier*              identifier = nullptr;
    std::vector<ast_identifier*> children;

    ~ast_decl() override;
    ast_decl() = default;
};

struct ast_call final : ast_node {
    ast_identifier*        identifier = nullptr;
    std::vector<ast_node*> arguments;

    ~ast_call() override;
    ast_call() = default;
};

struct ast_for final : ast_node {
    std::vector<ast_node*> body;
    ast_node*              initialization = nullptr;
    ast_node*              condition      = nullptr;
    ast_node*              update         = nullptr;

    ~ast_for() override;
    ast_for() = default;
};

struct ast_unaryexpr final : ast_node {
    token_t   _operator = TOKEN_NONE;
    ast_node* operand   = nullptr;

    ~ast_unaryexpr() override;
    ast_unaryexpr() = default;
};

struct ast_while final : ast_node {
    ast_node*              condition = nullptr;
    std::vector<ast_node*> body;

    ~ast_while() override;
    ast_while() = default;
};

struct ast_singleton_literal final : ast_node {
    std::string value;

    ~ast_singleton_literal() override = default;
    ast_singleton_literal()           = default;
};

struct ast_multi_literal final : ast_node {
    std::vector<ast_node*> members;

    ~ast_multi_literal() override;
    ast_multi_literal() = default;
};

#endif //AST_TYPES_HPP
