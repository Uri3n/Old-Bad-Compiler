//
// Created by Diago on 2024-07-03.
//

#include <lexer.hpp>


tak::Token
tak::token_hex_literal(Lexer& lxr) {

    assert(lxr.current_char() == '0');
    assert(lxr.peek_char() == 'x');

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start = index;

    lxr.advance_char(2);
    while(true) {
        if(lxr.current_char() == '\0') break;
        if(!isxdigit(static_cast<uint8_t>(lxr.current_char()))) break;

        lxr.advance_char(1);
    }

    const std::string_view token_raw = {&src[start], index - start};
    if(token_raw.back() == '\0' || !isxdigit(static_cast<uint8_t>(token_raw.back()))) {
        return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
    }

    return Token{TOKEN_HEX_LITERAL, KIND_LITERAL, start, token_raw};
}


tak::Token
tak::token_numeric_literal(Lexer& lxr) {

    assert( isdigit(static_cast<uint8_t>(lxr.current_char())) );

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start   = lxr.src_index_;
    bool passed_dot      = false;
    bool within_exponent = false;


    while(true) {
        if(lxr.current_char() == '\0') {
            break;
        }

        if(lxr.current_char() == '.') {
            if(passed_dot || within_exponent) {
                return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src[start], index - start}};
            }

            passed_dot = true;
        }

        else if(lxr.current_char() == 'e') {
            if(!passed_dot || within_exponent) {
                return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src[start], index - start}};
            }

            within_exponent = true;

            // Parse exponent
            if(lxr.peek_char() == '-' || lxr.peek_char() == '+') {
                lxr.advance_char(1);
                if(!isdigit(static_cast<uint8_t>(lxr.peek_char()))) break;
            }
        }

        else if(!isdigit(static_cast<uint8_t>(lxr.current_char()))) {
            break;
        }

        lxr.advance_char(1);
    }


    const std::string_view token_raw = {&src[start], index - start};
    if(!isdigit(static_cast<uint8_t>(token_raw.back()))) {
        return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
    }

    const auto is_float = std::find_if(token_raw.begin(), token_raw.end(), [&](const char c) { return !isdigit(static_cast<uint8_t>(c)); });
    if(is_float == token_raw.end()) {
        return Token{TOKEN_INTEGER_LITERAL, KIND_LITERAL, start, token_raw};
    }

    return Token{TOKEN_FLOAT_LITERAL, KIND_LITERAL, start, token_raw};
}


void
tak::lexer_infer_ambiguous_token(Lexer& lxr, const std::unordered_map<char, token_func>& illegals) {

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start = index;


    static std::unordered_map<std::string, token_t> keywords =
    {
        {"ret",      TOKEN_KW_RET},
        {"brk",      TOKEN_KW_BRK},
        {"for",      TOKEN_KW_FOR},
        {"while",    TOKEN_KW_WHILE},
        {"do",       TOKEN_KW_DO},
        {"if",       TOKEN_KW_IF},
        {"elif",     TOKEN_KW_ELIF},
        {"else",     TOKEN_KW_ELSE},
        {"cont",     TOKEN_KW_CONT},
        {"struct",   TOKEN_KW_STRUCT},
        {"enum",     TOKEN_KW_ENUM},
        {"switch",   TOKEN_KW_SWITCH},
        {"case",     TOKEN_KW_CASE},
        {"default",  TOKEN_KW_DEFAULT},
        {"blk",      TOKEN_KW_BLK},
        {"cast",     TOKEN_KW_CAST},
        {"defer",    TOKEN_KW_DEFER},
        {"defer_if", TOKEN_KW_DEFER_IF},
        {"sizeof",   TOKEN_KW_SIZEOF},
        {"nullptr",  TOKEN_KW_NULLPTR},
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
        {"void", TOKEN_KW_VOID},
    };


    if(index >= src.size()) {
        _current = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }

    if(isdigit(static_cast<uint8_t>(lxr.current_char()))) {
        if(lxr.current_char() == '0' && lxr.peek_char() == 'x') {
            _current = token_hex_literal(lxr);
        } else {
            _current = token_numeric_literal(lxr);
        }

        return;
    }


    //
    // If not a numeric literal it's probably a keyword or identifier
    //

    while(!illegals.contains(lxr.current_char()) && lxr.current_char() != '\0') {
        if(lxr.is_current_utf8_begin()) {
            lxr.skip_utf8_sequence();
        } else {
            lxr.advance_char(1);
        }
    }

    const std::string_view token_raw = {&src[start], index - start};


    //
    // Edge case where the last character still might not be allowed.
    //

    if(illegals.contains(token_raw.back())) {
        _current = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
        return;
    }


    //
    // For boolean literals we have to check them separately.
    //

    if(token_raw == "true" || token_raw == "false") {
        _current = Token{TOKEN_BOOLEAN_LITERAL, KIND_LITERAL, start, token_raw};
        return;
    }


    //
    // Check if the token is a keyword or type identifier
    //

    const auto temp = std::string(token_raw.begin(), token_raw.end());

    if(keywords.contains(temp)) {
        _current = Token{keywords[temp], KIND_KEYWORD, start, token_raw};
    } else if(type_identifiers.contains(temp)) {
        _current = Token{type_identifiers[temp], KIND_TYPE_IDENTIFIER, start, token_raw};
    }  else {
        _current = Token{TOKEN_IDENTIFIER, KIND_UNSPECIFIC, start, token_raw};
    }
}