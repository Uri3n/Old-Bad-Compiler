//
// Created by Diago on 2024-07-02.
//

#ifndef TOKEN_HPP
#define TOKEN_HPP
#include <cstdint>
#include <string_view>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TOKEN_LIST           \
    X(NONE)                  \
    X(END_OF_FILE)           \
    X(ILLEGAL)               \
    X(IDENTIFIER)            \
    X(VALUE_ASSIGNMENT)      \
    X(TYPE_ASSIGNMENT)       \
    X(CONST_TYPE_ASSIGNMENT) \
    X(SEMICOLON)             \
    X(LPAREN)                \
    X(RPAREN)                \
    X(LBRACE)                \
    X(RBRACE)                \
    X(LSQUARE_BRACKET)       \
    X(RSQUARE_BRACKET)       \
    X(COMMA)                 \
    X(DOT)                   \
    X(QUESTION_MARK)         \
    X(POUND)                 \
    X(AT)                    \
    X(COMP_EQUALS)           \
    X(COMP_NOT_EQUALS)       \
    X(COMP_LT)               \
    X(COMP_LTE)              \
    X(COMP_GT)               \
    X(COMP_GTE)              \
    X(NAMESPACE_ACCESS)      \
    X(CONDITIONAL_AND)       \
    X(CONDITIONAL_OR)        \
    X(CONDITIONAL_NOT)       \
    X(INTEGER_LITERAL)       \
    X(FLOAT_LITERAL)         \
    X(STRING_LITERAL)        \
    X(CHARACTER_LITERAL)     \
    X(BOOLEAN_LITERAL)       \
    X(PLUS)                  \
    X(PLUSEQ)                \
    X(SUB)                   \
    X(SUBEQ)                 \
    X(MUL)                   \
    X(MULEQ)                 \
    X(DIV)                   \
    X(DIVEQ)                 \
    X(MOD)                   \
    X(MODEQ)                 \
    X(INCREMENT)             \
    X(DECREMENT)             \
    X(BITWISE_AND)           \
    X(BITWISE_ANDEQ)         \
    X(BITWISE_NOT)           \
    X(BITWISE_OR)            \
    X(BITWISE_OREQ)          \
    X(BITWISE_XOR_OR_PTR)    \
    X(BITWISE_XOREQ)         \
    X(BITWISE_LSHIFT)        \
    X(BITWISE_LSHIFTEQ)      \
    X(BITWISE_RSHIFT)        \
    X(BITWISE_RSHIFTEQ)      \
    X(KW_RET)                \
    X(KW_BRK)                \
    X(KW_CONT)               \
    X(KW_FOR)                \
    X(KW_WHILE)              \
    X(KW_DO)                 \
    X(KW_IF)                 \
    X(KW_ELSE)               \
    X(KW_STRUCT)             \
    X(KW_ENUM)               \
    X(KW_SWITCH)             \
    X(KW_CASE)               \
    X(KW_DEFAULT)            \
    X(KW_FALLTHROUGH)        \
    X(KW_ELIF)               \
    X(KW_NAMESPACE)          \
    X(KW_DEFER)              \
    X(KW_PROC)               \
    X(KW_BLK)                \
    X(KW_CAST)               \
    X(KW_SIZEOF)             \
    X(KW_F32)                \
    X(KW_F64)                \
    X(KW_BOOL)               \
    X(KW_U8)                 \
    X(KW_I8)                 \
    X(KW_U16)                \
    X(KW_I16)                \
    X(KW_U32)                \
    X(KW_I32)                \
    X(KW_U64)                \
    X(KW_I64)                \
    X(KW_VOID)               \
    X(ARROW)                 \

#define TOKEN_KIND_LIST     \
    X(UNSPECIFIC)           \
    X(PUNCTUATOR)           \
    X(BINARY_EXPR_OPERATOR) \
    X(UNARY_EXPR_OPERATOR)  \
    X(LITERAL)              \
    X(KEYWORD)              \
    X(TYPE_IDENTIFIER)      \

#define VALID_UNARY_OPERATOR(token) (token.kind == KIND_UNARY_EXPR_OPERATOR   \
    || token == TOKEN_PLUS                                                    \
    || token == TOKEN_SUB                                                     \
    || token == TOKEN_BITWISE_XOR_OR_PTR                                      \
    || token == TOKEN_BITWISE_AND                                             \
)                                                                             \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define X(NAME) TOKEN_##NAME,
enum token_t : uint32_t {
    TOKEN_LIST
};
#undef X

#define X(NAME) KIND_##NAME,
enum token_kind : uint8_t {
    TOKEN_KIND_LIST
};
#undef X

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct token {
    token_t    type    = TOKEN_NONE;
    token_kind kind    = KIND_UNSPECIFIC;
    size_t     src_pos = 0;

    std::string_view value;
    uint32_t         line = 0;

    bool operator==(const token_t other) const  {return other == type;}
    bool operator==(const token& other)  const  {return other.type == this->type;}

    bool operator!=(const token_t other) const  {return other != type;}
    bool operator!=(const token& other)  const  {return other.type != this->type;}
};

#endif //TOKEN_HPP
