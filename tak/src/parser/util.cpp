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
