//
// Created by Diago on 2024-07-07.
//

#include <parser.hpp>

var_t
token_to_var_t(const token_t tok_t) {

    switch(tok_t) {
        case TOKEN_KW_I8:   return I8;
        case TOKEN_KW_U8:   return U8;
        case TOKEN_KW_I16:  return I16;
        case TOKEN_KW_U16:  return U16;
        case TOKEN_KW_I32:  return I32;
        case TOKEN_KW_U32:  return U32;
        case TOKEN_KW_I64:  return I64;
        case TOKEN_KW_U64:  return U64;
        case TOKEN_KW_F32:  return F32;
        case TOKEN_KW_F64:  return F64;
        case TOKEN_KW_BOOL: return BOOLEAN;
        default:            return VAR_NONE; // TODO: add a check for a struct / user defined type.
    }
}