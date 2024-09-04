//
// Created by Diago on 2024-07-03.
//

#include <lexer.hpp>


tak::Token
tak::Lexer::token_hex_literal() {
    assert(current_char() == '0');
    assert(peek_char() == 'x');

    const size_t start = src_index_;

    advance_char(2);
    while(true) {
        if(current_char() == '\0') break;
        if(!isxdigit(static_cast<uint8_t>(current_char()))) break;

        advance_char(1);
    }

    const std::string_view token_raw = {&src_[start], src_index_ - start};
    if(token_raw.back() == '\0' || !isxdigit(static_cast<uint8_t>(token_raw.back()))) {
        return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
    }

    return Token{TOKEN_HEX_LITERAL, KIND_LITERAL, start, token_raw};
}

tak::Token
tak::Lexer::token_numeric_literal() {
    assert( isdigit(static_cast<uint8_t>(current_char())) );

    const size_t start   = src_index_;
    bool passed_dot      = false;
    bool within_exponent = false;

    while(true) {
        if(current_char() == '\0') {
            break;
        }

        if(current_char() == '.') {
            if(peek_char(1) == '.' && peek_char(2) == '.') { // three dot token, break here.
                break;
            }

            if(passed_dot || within_exponent) {              // Only one dot allowed. Not allowed inside of exponent.
                return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src_[start], src_index_ - start}};
            }

            passed_dot = true;
        }

        else if(current_char() == 'e') {
            if(within_exponent) {
                return Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src_[start], src_index_ - start}};
            }

            within_exponent = true;

            // Parse exponent
            if(peek_char() == '-' || peek_char() == '+') {
                advance_char(1);
                if(!isdigit(static_cast<uint8_t>(peek_char()))) break;
            }
        }

        else if(!isdigit(static_cast<uint8_t>(current_char()))) {
            break;
        }

        advance_char(1);
    }


    const std::string_view token_raw = {&src_[start], src_index_ - start};
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
tak::Lexer::infer_ambiguous_token() {

    const size_t start = src_index_;
    static std::unordered_map<std::string, token_t> keywords =
    {
        {"ret",      TOKEN_KW_RET},
        {"brk",      TOKEN_KW_BRK},
        {"for",      TOKEN_KW_FOR},
        {"while",    TOKEN_KW_WHILE},
        {"do",       TOKEN_KW_DO},
        {"if",       TOKEN_KW_IF},
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


    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    if(isdigit(static_cast<uint8_t>(current_char()))) {
        if(current_char() == '0' && peek_char() == 'x') {
            current_ = token_hex_literal();
        } else {
            current_ = token_numeric_literal();
        }

        return;
    }

    //
    // If not a numeric literal it's probably a keyword or identifier
    //

    while(!is_char_reserved(current_char()) && current_char() != '\0') {
        if(is_current_utf8_begin()) {
            skip_utf8_sequence();
        } else {
            advance_char(1);
        }
    }

    const std::string_view token_raw = {&src_[start], src_index_ - start};

    //
    // Edge case where the last character still might not be allowed.
    //

    if(is_char_reserved(token_raw.back())) {
        current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, token_raw};
        return;
    }

    //
    // For boolean literals we have to check them separately.
    //

    if(token_raw == "true" || token_raw == "false") {
        current_ = Token{TOKEN_BOOLEAN_LITERAL, KIND_LITERAL, start, token_raw};
        return;
    }

    //
    // Check if the token is a keyword or type identifier
    //

    const auto temp = std::string(token_raw.begin(), token_raw.end());

    if(keywords.contains(temp)) {
        current_ = Token{keywords[temp], KIND_KEYWORD, start, token_raw};
    } else if(type_identifiers.contains(temp)) {
        current_ = Token{type_identifiers[temp], KIND_TYPE_IDENTIFIER, start, token_raw};
    }  else {
        current_ = Token{TOKEN_IDENTIFIER, KIND_UNSPECIFIC, start, token_raw};
    }
}

void
tak::Lexer::token_skip() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_.type = TOKEN_NONE;
        advance_char(1);
    }
}

void
tak::Lexer::token_newline() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_.type = TOKEN_NONE;
        advance_line();
        advance_char(1);
    }
}

void
tak::Lexer::token_semicolon() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_SEMICOLON, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_lparen() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_LPAREN, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_rparen() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_RPAREN, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_dollarsign() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_DOLLAR_SIGN, KIND_UNSPECIFIC, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_lbrace() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_LBRACE, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_rbrace() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_RBRACE, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_comma() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_COMMA, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_hyphen() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    switch(peek_char()) {
        case '=':
            current_ = Token{TOKEN_SUBEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '>':
            current_ = Token{TOKEN_ARROW, KIND_UNSPECIFIC, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '-':
            current_ = Token{TOKEN_DECREMENT, KIND_UNARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        default:
            current_ = Token{TOKEN_SUB, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_plus() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }

    switch(peek_char()) {
        case '=':
            current_ = Token{TOKEN_PLUSEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '+':
            current_ = Token{TOKEN_INCREMENT, KIND_UNARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        default:
            current_ = Token{TOKEN_PLUS, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_asterisk() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == '=') {
        current_ = Token{TOKEN_MULEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_MUL, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_fwdslash() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }


    switch(peek_char()) {
        case '/':
            while(current_char() != '\0' && current_char() != '\n') {
                if(is_current_utf8_begin()) {
                    skip_utf8_sequence();
                } else {
                    advance_char(1);
                }
            }

            current_.type = TOKEN_NONE;
            break;

        case '*':
            while(true) {
                if(current_char() == '\0') {
                    break;
                }

                if(current_char() == '*' && peek_char() == '/') {
                    advance_char(2);
                    break;
                }

                if(current_char() == '\n') {
                    advance_line();
                }

                if(is_current_utf8_begin()) {
                    skip_utf8_sequence();
                } else {
                    advance_char(1);
                }
            }


            if(current_char() == '\0') {
                current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
            } else {
                current_.type = TOKEN_NONE;
            }

            break;

        case '=':
            current_ = Token{TOKEN_DIVEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        default:
            current_ = Token{TOKEN_DIV, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_percent() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == '=') {
        current_ = Token{TOKEN_MODEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_MOD, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_equals() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == '=') {
        current_ = Token{TOKEN_COMP_EQUALS, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_VALUE_ASSIGNMENT, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_lessthan() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    const size_t start = src_index_;
    switch(peek_char()) {
        case '=':
            current_ = Token{TOKEN_COMP_LTE, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '<':
            advance_char(1);
            if(peek_char() == '=') {
                current_ = Token{TOKEN_BITWISE_LSHIFTEQ, KIND_BINARY_EXPR_OPERATOR, start, {&src_[start], 3}};
                advance_char(2);
            } else {
                current_ = Token{TOKEN_BITWISE_LSHIFT, KIND_BINARY_EXPR_OPERATOR, start, {&src_[start], 2}};
                advance_char(1);
            }
            break;

        default:
            current_ = Token{TOKEN_COMP_LT, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_greaterthan() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    const size_t start = src_index_;
    switch(peek_char()) {
        case '=':
            current_ = Token{TOKEN_COMP_GTE, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '>':
            advance_char(1);
            if(peek_char() == '=') {
                current_ = Token{TOKEN_BITWISE_RSHIFTEQ, KIND_BINARY_EXPR_OPERATOR, start, {&src_[start], 3}};
                advance_char(2);
            } else {
                current_ = Token{TOKEN_BITWISE_RSHIFT, KIND_BINARY_EXPR_OPERATOR, start, {&src_[start], 2}};
                advance_char(1);
            }
            break;

        default:
            current_ = Token{TOKEN_COMP_GT, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_ampersand() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    switch(peek_char()) {
        case '&':
            current_ = Token{TOKEN_CONDITIONAL_AND, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '=':
            current_ = Token{TOKEN_BITWISE_ANDEQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        default:
            current_ = Token{TOKEN_BITWISE_AND, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}}; // can also mean address-of!
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_verticalline() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    switch(peek_char()) {
        case '|':
            current_ = Token{TOKEN_CONDITIONAL_OR, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        case '=':
            current_ = Token{TOKEN_BITWISE_OREQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
            advance_char(2);
            break;

        default:
            current_ = Token{TOKEN_BITWISE_OR, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
            advance_char(1);
            break;
    }
}

void
tak::Lexer::token_bang() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == '=') {
        current_ = Token{TOKEN_COMP_NOT_EQUALS, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_CONDITIONAL_NOT, KIND_UNARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_tilde() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_BITWISE_NOT, KIND_UNARY_EXPR_OPERATOR, src_index_, {&src_[src_index_],1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_uparrow() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == '=') {
        current_ = Token{TOKEN_BITWISE_XOREQ, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_],2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_BITWISE_XOR_OR_PTR, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_],1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_quote() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    const char   opening_quote = current_char();
    const size_t string_start  = src_index_;

    advance_char(1);
    while(true) {
        if(current_char() == '\0') {
            current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, string_start, "\\0"};
            break;
        }
        if(current_char() == opening_quote) {
            current_ = Token{TOKEN_STRING_LITERAL, KIND_LITERAL, src_index_, {&src_[string_start], (src_index_ - string_start) + 1}};
            advance_char(1);
            break;
        }
        if(current_char() == '\\' && peek_char() == opening_quote) {
            advance_char(2);
        }

        else if (is_current_utf8_begin()){
            skip_utf8_sequence();
        }
        else {
            advance_char(1);
        }
    }
}

void
tak::Lexer::token_singlequote() {
    const size_t start = src_index_;

    advance_char(1);
    if(current_char() == '\\') {
        advance(1);
        if(is_current_utf8_begin()) {
            current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src_[start], src_index_ - start}};
            return;
        }
        advance_char(1);
    }
    else if(current_char() == '\'') {
        advance_char(1);
        current_ = Token{TOKEN_CHARACTER_LITERAL, KIND_LITERAL, start, {&src_[start], src_index_ - start}};
        return;
    }
    else if(is_current_utf8_begin()) {
        current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src_[start], src_index_ - start}};
        return;
    }
    else {
        advance_char(1);
    }

    switch(current_char()) {
        case '\0':
            current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, "\\0"};
            break;

        case '\'':
            advance_char(1);
            current_ = Token{TOKEN_CHARACTER_LITERAL, KIND_LITERAL, start, {&src_[start], src_index_ - start}};
            break;

        default:
            current_ = Token{TOKEN_ILLEGAL, KIND_UNSPECIFIC, start, {&src_[start], src_index_ - start}};
            break;
    }
}

void
tak::Lexer::token_lsquarebracket() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_LSQUARE_BRACKET, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_rsquarebracket() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_RSQUARE_BRACKET, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_questionmark() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_QUESTION_MARK, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_colon() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    }
    else if(peek_char() == ':') {
        current_ = Token{TOKEN_CONST_TYPE_ASSIGNMENT, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 2}};
        advance_char(2);
    }
    else {
        current_ = Token{TOKEN_TYPE_ASSIGNMENT, KIND_BINARY_EXPR_OPERATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_dot() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else if(peek_char(1) == '.' && peek_char(2) == '.'){
        current_ = Token{TOKEN_THREE_DOTS, KIND_UNSPECIFIC, src_index_, {&src_[src_index_], 3}};
        advance_char(3);
    } else {
        current_ = Token{TOKEN_DOT, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_backslash() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_NAMESPACE_ACCESS, KIND_UNSPECIFIC, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_at() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_AT, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}

void
tak::Lexer::token_null() {
    current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
}

void
tak::Lexer::token_pound() {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
    } else {
        current_ = Token{TOKEN_ILLEGAL, KIND_PUNCTUATOR, src_index_, {&src_[src_index_], 1}};
        advance_char(1);
    }
}