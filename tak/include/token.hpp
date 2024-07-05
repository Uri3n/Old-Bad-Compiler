//
// Created by Diago on 2024-07-02.
//

#ifndef TOKEN_HPP
#define TOKEN_HPP
#include <cstdint>
#include <string_view>


enum token_t : uint32_t {
    TOKEN_NONE,
    END_OF_FILE,
    ILLEGAL,
    IDENTIFIER,
    VALUE_ASSIGNMENT,      // =
    TYPE_ASSIGNMENT,       // :
    CONST_TYPE_ASSIGNMENT, // ::
    SEMICOLON,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LSQUARE_BRACKET,
    RSQUARE_BRACKET,
    COMMA,
    DOT,
    QUESTION_MARK,
    POUND,
    AT,
    COMP_EQUALS,
    COMP_NOT_EQUALS,
    COMP_LT,
    COMP_LTE,
    COMP_GT,
    COMP_GTE,
    CONDITIONAL_AND,
    CONDITIONAL_OR,
    CONDITIONAL_NOT,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOLEAN_LITERAL,
    PLUS,
    PLUSEQ,
    SUB,
    SUBEQ,
    MUL,
    MULEQ,
    DIV,
    DIVEQ,
    MOD,
    MODEQ,
    BITWISE_AND,
    BITWISE_ANDEQ,
    BITWISE_NOT,
    BITWISE_OR,
    BITWISE_OREQ,
    BITWISE_XOR_OR_PTR,
    BITWISE_XOREQ,
    BITWISE_LSHIFT,
    BITWISE_RSHIFT,
    KW_RET,
    KW_BRK,
    KW_CONT,
    KW_FOR,
    KW_WHILE,
    KW_DO,
    KW_IF,
    KW_ELSE,
    KW_ELIF,
    TOKEN_KW_PROC,
    TOKEN_KW_STR,
    TOKEN_KW_FLT,
    TOKEN_KW_BOOL,
    TOKEN_KW_U8,
    TOKEN_KW_I8,
    TOKEN_KW_U16,
    TOKEN_KW_I16,
    TOKEN_KW_U32,
    TOKEN_KW_I32,
    TOKEN_KW_U64,
    TOKEN_KW_I64,
    ARROW
};

enum token_kind : uint8_t {
    UNSPECIFIC,
    PUNCTUATOR,
    BINARY_EXPR_OPERATOR,
    UNARY_EXPR_OPERATOR,
    LITERAL,
    KEYWORD,
    TYPE_IDENTIFIER // This is still technically a keyword, But it's more specific.
};

struct token {
    token_t    type    = TOKEN_NONE;
    token_kind kind    = UNSPECIFIC;
    size_t     src_pos = 0;

    std::string_view value;
    uint32_t         line = 0;

    bool operator==(const token_t other) const  {return other == type;}
    bool operator==(const token& other)  const  {return other.type == this->type;}

    bool operator!=(const token_t other) const  {return other != type;}
    bool operator!=(const token& other)  const  {return other.type != this->type;}
};

#endif //TOKEN_HPP
