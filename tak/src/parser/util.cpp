//
// Created by Diago on 2024-07-07.
//

#include <parser.hpp>


std::string
var_t_to_string(const var_t type) { // TODO: maybe convert this enum to an X macro too?

    switch(type) {
        case VAR_NONE:                return "None (void)";
        case VAR_U8:                  return "U8";
        case VAR_I8:                  return "I8";
        case VAR_U16:                 return "U16";
        case VAR_I16:                 return "I16";
        case VAR_U32:                 return "U32";
        case VAR_I32:                 return "I32";
        case VAR_U64:                 return "U64";
        case VAR_I64:                 return "I64";
        case VAR_F32:                 return "F32";
        case VAR_F64:                 return "F64";
        case VAR_BOOLEAN:             return "Boolean";
        case VAR_USER_DEFINED_STRUCT: return "Struct";
        case VAR_USER_DEFINED_ENUM:   return "Enumeration";
        case VAR_VOID:                return "Void";
        default:                      return "Unknown";
    }
}

var_t
token_to_var_t(const token_t tok_t) {

    switch(tok_t) {
        case TOKEN_KW_I8:   return VAR_I8;
        case TOKEN_KW_U8:   return VAR_U8;
        case TOKEN_KW_I16:  return VAR_I16;
        case TOKEN_KW_U16:  return VAR_U16;
        case TOKEN_KW_I32:  return VAR_I32;
        case TOKEN_KW_U32:  return VAR_U32;
        case TOKEN_KW_I64:  return VAR_I64;
        case TOKEN_KW_U64:  return VAR_U64;
        case TOKEN_KW_F32:  return VAR_F32;
        case TOKEN_KW_F64:  return VAR_F64;
        case TOKEN_KW_BOOL: return VAR_BOOLEAN;
        default:            return VAR_NONE;
    }
}

uint16_t
precedence_of(const token_t _operator) {

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
            print("FATAL, predence_of: default case reached. Panicking.");
            exit(1);
    }
}

