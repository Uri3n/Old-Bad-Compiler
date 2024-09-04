//
// Created by Diago on 2024-07-02.
//

#ifndef TOKEN_HPP
#define TOKEN_HPP
#include <cstdint>
#include <string_view>
#include <optional>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TOKEN_LIST                   \
    X(NONE, "")                      \
    X(END_OF_FILE, "\\0")            \
    X(ILLEGAL, "")                   \
    X(IDENTIFIER, "")                \
    X(VALUE_ASSIGNMENT, "=")         \
    X(TYPE_ASSIGNMENT, ":")          \
    X(CONST_TYPE_ASSIGNMENT, "::")   \
    X(SEMICOLON, ";")                \
    X(LPAREN, "(")                   \
    X(RPAREN, ")")                   \
    X(LBRACE, "{")                   \
    X(RBRACE, "}")                   \
    X(LSQUARE_BRACKET, "[")          \
    X(RSQUARE_BRACKET, "]")          \
    X(COMMA, ",")                    \
    X(DOLLAR_SIGN, "$")              \
    X(DOT, ".")                      \
    X(THREE_DOTS, "...")             \
    X(QUESTION_MARK, "?")            \
    X(POUND, "#")                    \
    X(AT, "@")                       \
    X(COMP_EQUALS, "==")             \
    X(COMP_NOT_EQUALS, "!=")         \
    X(COMP_LT, "<")                  \
    X(COMP_LTE, "<=")                \
    X(COMP_GT, ">")                  \
    X(COMP_GTE, ">=")                \
    X(NAMESPACE_ACCESS, "\\")        \
    X(CONDITIONAL_AND, "&&")         \
    X(CONDITIONAL_OR, "||")          \
    X(CONDITIONAL_NOT, "!")          \
    X(INTEGER_LITERAL, "")           \
    X(FLOAT_LITERAL, "")             \
    X(STRING_LITERAL, "")            \
    X(CHARACTER_LITERAL, "")         \
    X(BOOLEAN_LITERAL, "")           \
    X(HEX_LITERAL, "")               \
    X(PLUS, "+")                     \
    X(PLUSEQ, "+=")                  \
    X(SUB, "-")                      \
    X(SUBEQ, "-=")                   \
    X(MUL, "*")                      \
    X(MULEQ, "*=")                   \
    X(DIV, "/")                      \
    X(DIVEQ, "/=")                   \
    X(MOD, "%")                      \
    X(MODEQ, "%=")                   \
    X(INCREMENT, "++")               \
    X(DECREMENT, "--")               \
    X(BITWISE_AND, "&")              \
    X(BITWISE_ANDEQ, "&=")           \
    X(BITWISE_NOT, "~")              \
    X(BITWISE_OR, "|")               \
    X(BITWISE_OREQ, "|=")            \
    X(BITWISE_XOR_OR_PTR, "^")       \
    X(BITWISE_XOREQ, "^=")           \
    X(BITWISE_LSHIFT, "<<")          \
    X(BITWISE_LSHIFTEQ, "<<=")       \
    X(BITWISE_RSHIFT, ">>")          \
    X(BITWISE_RSHIFTEQ, ">>=")       \
    X(KW_RET, "ret")                 \
    X(KW_BRK, "brk")                 \
    X(KW_CONT, "cont")               \
    X(KW_FOR, "for")                 \
    X(KW_WHILE, "while")             \
    X(KW_DO, "do")                   \
    X(KW_IF, "if")                   \
    X(KW_ELSE, "else")               \
    X(KW_STRUCT, "struct")           \
    X(KW_ENUM, "enum")               \
    X(KW_SWITCH, "switch")           \
    X(KW_CASE, "case")               \
    X(KW_DEFAULT, "default")         \
    X(KW_FALLTHROUGH, "fallthrough") \
    X(KW_NAMESPACE, "namespace")     \
    X(KW_DEFER, "defer")             \
    X(KW_DEFER_IF, "defer_if")       \
    X(KW_PROC, "proc")               \
    X(KW_BLK, "block")               \
    X(KW_CAST, "cast")               \
    X(KW_SIZEOF, "sizeof")           \
    X(KW_F32, "f32")                 \
    X(KW_F64, "f64")                 \
    X(KW_BOOL, "bool")               \
    X(KW_U8,  "u8")                  \
    X(KW_I8,  "i8")                  \
    X(KW_U16, "u16")                 \
    X(KW_I16, "i16")                 \
    X(KW_U32, "u32")                 \
    X(KW_I32, "i32")                 \
    X(KW_U64, "u64")                 \
    X(KW_I64, "i64")                 \
    X(KW_VOID,"void")                \
    X(KW_NULLPTR, "nullptr")         \
    X(ARROW,  "->")                  \

#define TOKEN_KIND_LIST              \
    X(UNSPECIFIC)                    \
    X(PUNCTUATOR)                    \
    X(BINARY_EXPR_OPERATOR)          \
    X(UNARY_EXPR_OPERATOR)           \
    X(LITERAL)                       \
    X(KEYWORD)                       \
    X(TYPE_IDENTIFIER)               \

#define TOKEN_VALID_UNARY_OPERATOR(token)          \
   (token.kind == KIND_UNARY_EXPR_OPERATOR         \
    || token == TOKEN_PLUS                         \
    || token == TOKEN_SUB                          \
    || token == TOKEN_BITWISE_XOR_OR_PTR           \
    || token == TOKEN_BITWISE_AND                  \
)                                                  \

#define TOKEN_OP_IS_ARITH_ASSIGN(token_type)       \
   (token_type == tak::TOKEN_PLUSEQ                \
    || token_type == tak::TOKEN_SUBEQ              \
    || token_type == tak::TOKEN_MULEQ              \
    || token_type == tak::TOKEN_DIVEQ              \
    || token_type == tak::TOKEN_MODEQ              \
    || token_type == tak::TOKEN_INCREMENT          \
    || token_type == tak::TOKEN_DECREMENT          \
)                                                  \

#define TOKEN_IDENT_START(token_type)              \
    (token_type == tak::TOKEN_IDENTIFIER           \
     || token_type == tak::TOKEN_NAMESPACE_ACCESS  \
)                                                  \

#define TOKEN_OP_IS_VALID_PTR_ARITH(token_type)    \
   (token_type == tak::TOKEN_PLUS                  \
    || token_type == tak::TOKEN_PLUSEQ             \
    || token_type == tak::TOKEN_SUB                \
    || token_type == tak::TOKEN_SUBEQ              \
    || token_type == tak::TOKEN_INCREMENT          \
    || token_type == tak::TOKEN_DECREMENT          \
)                                                  \

#define TOKEN_OP_IS_ARITHMETIC(token_type)         \
   (token_type == tak::TOKEN_PLUS                  \
    || token_type == tak::TOKEN_PLUSEQ             \
    || token_type == tak::TOKEN_SUB                \
    || token_type == tak::TOKEN_SUBEQ              \
    || token_type == tak::TOKEN_MUL                \
    || token_type == tak::TOKEN_MULEQ              \
    || token_type == tak::TOKEN_DIV                \
    || token_type == tak::TOKEN_DIVEQ              \
    || token_type == tak::TOKEN_MOD                \
    || token_type == tak::TOKEN_MODEQ              \
    || token_type == tak::TOKEN_INCREMENT          \
    || token_type == tak::TOKEN_DECREMENT          \
)                                                  \

#define TOKEN_OP_IS_BW_ASSIGN(token_type)          \
   (token_type == tak::TOKEN_BITWISE_ANDEQ         \
    || token_type == tak::TOKEN_BITWISE_OREQ       \
    || token_type == tak::TOKEN_BITWISE_XOREQ      \
    || token_type == tak::TOKEN_BITWISE_LSHIFTEQ   \
    || token_type == tak::TOKEN_BITWISE_RSHIFTEQ   \
)                                                  \

#define TOKEN_OP_IS_BITWISE(token_type)            \
   (token_type == tak::TOKEN_BITWISE_AND           \
    || token_type == tak::TOKEN_BITWISE_ANDEQ      \
    || token_type == tak::TOKEN_BITWISE_OR         \
    || token_type == tak::TOKEN_BITWISE_NOT        \
    || token_type == tak::TOKEN_BITWISE_OREQ       \
    || token_type == tak::TOKEN_BITWISE_XOR_OR_PTR \
    || token_type == tak::TOKEN_BITWISE_XOREQ      \
    || token_type == tak::TOKEN_BITWISE_LSHIFT     \
    || token_type == tak::TOKEN_BITWISE_LSHIFTEQ   \
    || token_type == tak::TOKEN_BITWISE_RSHIFT     \
    || token_type == tak::TOKEN_BITWISE_RSHIFTEQ   \
)                                                  \

#define TOKEN_OP_IS_COMPARISON(token_type)         \
   (token_type == tak::TOKEN_COMP_EQUALS           \
    || token_type == tak::TOKEN_COMP_NOT_EQUALS    \
    || token_type == tak::TOKEN_COMP_LT            \
    || token_type == tak::TOKEN_COMP_LTE           \
    || token_type == tak::TOKEN_COMP_GT            \
    || token_type == tak::TOKEN_COMP_GTE           \
)                                                  \

#define TOKEN_OP_IS_LOGICAL(token_type)            \
   (token_type == tak::TOKEN_COMP_EQUALS           \
    || token_type == tak::TOKEN_COMP_NOT_EQUALS    \
    || token_type == tak::TOKEN_COMP_LT            \
    || token_type == tak::TOKEN_COMP_LTE           \
    || token_type == tak::TOKEN_COMP_GT            \
    || token_type == tak::TOKEN_COMP_GTE           \
    || token_type == tak::TOKEN_CONDITIONAL_AND    \
    || token_type == tak::TOKEN_CONDITIONAL_OR     \
    || token_type == tak::TOKEN_CONDITIONAL_NOT    \
)                                                  \

namespace tak {
    #define X(NAME, STR_UNUSED) TOKEN_##NAME,
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

    struct Token {
        token_t    type    = TOKEN_NONE;
        token_kind kind    = KIND_UNSPECIFIC;
        size_t     src_pos = 0;

        std::string_view value;
        uint32_t line  = 1;

        static void                  dump(const Token& tok);
        static std::string           to_string(token_t tok);
        static std::string           type_to_string(token_t tok);
        static std::string           kind_to_string(token_kind kind);
        static uint16_t              precedence_of(token_t _operator);
        static std::optional<size_t> lit_to_int(const Token& tok);

        bool operator==(const token_t other) const  {return other == type;}
        bool operator==(const Token& other)  const  {return other.type == this->type;}

        bool operator!=(const token_t other) const  {return other != type;}
        bool operator!=(const Token& other)  const  {return other.type != this->type;}
    };
}
#endif //TOKEN_HPP
