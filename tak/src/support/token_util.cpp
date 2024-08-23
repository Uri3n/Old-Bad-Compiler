//
// Created by Diago on 2024-08-17.
//

#include <token.hpp>
#include <panic.hpp>
#include <util.hpp>


std::string
tak::Token::to_string(const token_t tok) {

#define X(NAME, STR) case TOKEN_##NAME: return STR;
    switch(tok) {
        TOKEN_LIST;
        default: return "Unknown."; // failsafe
    }
#undef X
}

std::string
tak::Token::type_to_string(const token_t tok) {

#define X(NAME, UNUSED_STR) case TOKEN_##NAME: return #NAME;
    switch(tok) {
        TOKEN_LIST
        default: return "Unknown."; // failsafe
    }
#undef X
}

std::string
tak::Token::kind_to_string(const token_kind kind) {

#define X(NAME) case KIND_##NAME: return #NAME;
    switch(kind) {
        TOKEN_KIND_LIST
        default: return "Unknown"; // failsafe
    }
#undef X
}

tak::primitive_t
tak::token_to_var_t(const token_t tok) {
    switch(tok) {
        case TOKEN_KW_I8:   return PRIMITIVE_I8;
        case TOKEN_KW_U8:   return PRIMITIVE_U8;
        case TOKEN_KW_I16:  return PRIMITIVE_I16;
        case TOKEN_KW_U16:  return PRIMITIVE_U16;
        case TOKEN_KW_I32:  return PRIMITIVE_I32;
        case TOKEN_KW_U32:  return PRIMITIVE_U32;
        case TOKEN_KW_I64:  return PRIMITIVE_I64;
        case TOKEN_KW_U64:  return PRIMITIVE_U64;
        case TOKEN_KW_F32:  return PRIMITIVE_F32;
        case TOKEN_KW_F64:  return PRIMITIVE_F64;
        case TOKEN_KW_BOOL: return PRIMITIVE_BOOLEAN;
        default:            return PRIMITIVE_NONE;
    }
}

uint16_t
tak::Token::precedence_of(const token_t _operator) {
    switch(_operator) {
        case TOKEN_CONDITIONAL_AND:    return 13;
        case TOKEN_CONDITIONAL_OR:     return 12;
        case TOKEN_MUL:
        case TOKEN_DIV:
        case TOKEN_MOD:                return 8;
        case TOKEN_PLUS:
        case TOKEN_SUB:                return 7;
        case TOKEN_BITWISE_LSHIFT:
        case TOKEN_BITWISE_RSHIFT:     return 6;
        case TOKEN_COMP_GTE:
        case TOKEN_COMP_GT:
        case TOKEN_COMP_LTE:
        case TOKEN_COMP_LT:            return 5;
        case TOKEN_COMP_EQUALS:
        case TOKEN_COMP_NOT_EQUALS:    return 4;
        case TOKEN_BITWISE_AND:        return 3;
        case TOKEN_BITWISE_XOR_OR_PTR: return 2;
        case TOKEN_BITWISE_OR:         return 1;
        case TOKEN_VALUE_ASSIGNMENT:
        case TOKEN_PLUSEQ:
        case TOKEN_SUBEQ:
        case TOKEN_MULEQ:
        case TOKEN_DIVEQ:
        case TOKEN_MODEQ:
        case TOKEN_BITWISE_LSHIFTEQ:
        case TOKEN_BITWISE_RSHIFTEQ:
        case TOKEN_BITWISE_ANDEQ:
        case TOKEN_BITWISE_OREQ:
        case TOKEN_BITWISE_XOREQ:      return 0;

        default:
            panic("predence_of: default case reached");
    }
}

std::optional<size_t>
tak::Token::lit_to_int(const Token& tok) {

    size_t val = 0;

    if(tok == TOKEN_INTEGER_LITERAL) {
        try {
            val = static_cast<size_t>(std::stoll(std::string(tok.value)));
        } catch(...) {
            return std::nullopt;
        }
    } else if(tok == TOKEN_CHARACTER_LITERAL) {
        if(const auto actual = get_actual_char(tok.value)) {
            val = static_cast<size_t>(*actual);
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt; // Cannot be converted to an integer
    }

    return val;
}

void tak::Token::dump(const Token& tok) {
    print(
        "Value: {}\nType: {}\nKind: {}\nFile Pos Index: {}\nLine Number: {}\n",
        tok.value,
        to_string(tok.type),
        kind_to_string(tok.kind),
        tok.src_pos,
        tok.line
    );
}