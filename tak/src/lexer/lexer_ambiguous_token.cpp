//
// Created by Diago on 2024-07-03.
//

#include <lexer.hpp>


bool
is_token_valid_numeric_literal(const std::string_view& token_value) {

    const char* dot_ptr = nullptr;

    for(const char& c : token_value) {
        if(!isdigit(c) && c != '.') {
            return false;
        }

        if(c == '.') {
            if(dot_ptr == nullptr) {
                dot_ptr = &c;
            } else {
                return false;
            }
        }
    }

    if(dot_ptr != nullptr) {
        if(dot_ptr == &token_value[0] || dot_ptr == &token_value[token_value.size() - 1]) {
            return false;
        }
    }

    return true;
}


token
token_numeric_literal(lexer& lxr, std::unordered_map<char, token_func>& illegals) {

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start = lxr.src_index;


    while(true) {
        if(lxr.current_char() == '\0') {
            break;
        }

        if(illegals.contains(lxr.current_char()) && lxr.current_char() != '.') {
            break;
        }

        lxr.advance_char(1);
    }

    const std::string_view token_raw = {&src[start], index - start};


    //
    // in case of EOF or invalid literal
    //

    if(illegals.contains(token_raw.back()) || !is_token_valid_numeric_literal(token_raw)) {
        return token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
    }


    //
    // Check if the token is an integer literal (no dot) or float
    //

    auto is_float = std::find_if(token_raw.begin(), token_raw.end(), [](char c) { return c == '.'; });

    if(is_float != token_raw.end()) {
        return token{TOKEN_FLOAT_LITERAL, KIND_LITERAL, start, token_raw};
    }

    return token{TOKEN_INTEGER_LITERAL, KIND_LITERAL, start, token_raw};
}


void
lexer_infer_ambiguous_token(lexer& lxr, std::unordered_map<char, token_func>& illegals) {

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start = index;


    static std::unordered_map<std::string, token_t> keywords =
    {
        {"ret",     TOKEN_KW_RET},
        {"brk",     TOKEN_KW_BRK},
        {"for",     TOKEN_KW_FOR},
        {"while",   TOKEN_KW_WHILE},
        {"do",      TOKEN_KW_DO},
        {"if",      TOKEN_KW_IF},
        {"elif",    TOKEN_KW_ELIF},
        {"else",    TOKEN_KW_ELSE},
        {"cont",    TOKEN_KW_CONT},
        {"struct",  TOKEN_KW_STRUCT},
        {"enum",    TOKEN_KW_ENUM},
        {"switch",  TOKEN_KW_SWITCH},
        {"case",    TOKEN_KW_CASE},
        {"default", TOKEN_KW_DEFAULT},
        {"fallthrough", TOKEN_KW_FALLTHROUGH},
        {"namespace", TOKEN_KW_NAMESPACE},
    };

    static std::unordered_map<std::string, token_t> type_identifiers =
    {
        {"u8",   TOKEN_KW_U8},
        {"i8",   TOKEN_KW_I8},
        {"u16",  TOKEN_KW_U16},
        {"i16",  TOKEN_KW_I16},
        {"u32",  TOKEN_KW_U32},
        {"i32",  TOKEN_KW_I32},
        {"u64",  TOKEN_KW_U64},
        {"i64",  TOKEN_KW_I64},
        {"f32",  TOKEN_KW_F32},
        {"f64",  TOKEN_KW_F64},
        {"proc", TOKEN_KW_PROC},
        {"bool", TOKEN_KW_BOOL},
        {"void", TOKEN_KW_VOID}
    };


    if(index >= src.size()) {
        _current = token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    if(isdigit(lxr.current_char())) {
        _current = token_numeric_literal(lxr, illegals);
        return;
    }


    //
    // If not a numeric literal it's probably a keyword or identifier
    //

    while(!illegals.contains(lxr.current_char()) && lxr.current_char() != '\0') {
        lxr.advance_char(1);
    }

    const std::string_view token_raw = {&src[start], index - start};


    //
    // Edge case where the last character still might not be allowed.
    //

    if(illegals.contains(token_raw.back())) {
        _current = token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
        return;
    }


    //
    // For boolean literals we have to check them separately.
    //

    if(token_raw == "true" || token_raw == "false") {
        _current = token{TOKEN_BOOLEAN_LITERAL, KIND_LITERAL, start, token_raw};
        return;
    }


    //
    // Check if the token is a keyword or type identifier
    //

    const auto temp = std::string(token_raw.begin(), token_raw.end());

    if(keywords.contains(temp)) {
        _current = token{keywords[temp], KIND_KEYWORD, start, token_raw};
    }

    else if(type_identifiers.contains(temp)) {
        _current = token{type_identifiers[temp], KIND_TYPE_IDENTIFIER, start, token_raw};
    }

    else {
        _current = token{TOKEN_IDENTIFIER, KIND_UNSPECIFIC, start, token_raw};
    }
}