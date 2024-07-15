//
// Created by Diago on 2024-07-03.
//

#include <lexer.hpp>


void
lexer_token_skip(lexer& lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current.type = TOKEN_NONE;
        lxr.advance_char(1);
    }
}


void
lexer_token_newline(lexer& lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current.type = TOKEN_NONE;
        lxr.advance_line();
        lxr.advance_char(1);
    }
}


void
lexer_token_semicolon(lexer& lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{SEMICOLON, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_lparen(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{LPAREN, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_rparen(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{RPAREN, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_lbrace(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{LBRACE, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_rbrace(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{RBRACE, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_comma(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{COMMA, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_hyphen(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '=':
            _current = token{SUBEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '>':
            _current = token{ARROW, UNSPECIFIC, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '-':
            _current = token{DECREMENT, UNARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{SUB, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_plus(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    switch(lxr.peek_char()) {
        case '=':
            _current = token{PLUSEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '+':
            _current = token{INCREMENT, UNARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{PLUS, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_asterisk(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == '=') {
        _current = token{MULEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{MUL, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_fwdslash(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '/':
            while(lxr.current_char() != '\0' && lxr.current_char() != '\n') {
                lxr.advance_char(1);
            }

            _current.type = TOKEN_NONE;
            break;

        case '*':
            while(true) {
                if(lxr.current_char() == '\0') {
                    break;
                }

                if(lxr.current_char() == '*' && lxr.peek_char() == '/') {
                    lxr.advance_char(2);
                    break;
                }

                if(lxr.current_char() == '\n') {
                    lxr.advance_line();
                }

                lxr.advance_char(1);
            }


            if(lxr.current_char() == '\0') {
                _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
            } else {
                _current.type = TOKEN_NONE;
            }

            break;

        case '=':
            _current = token{DIVEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{DIV, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_percent(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == '=') {
        _current = token{MODEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{MOD, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_equals(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == '=') {
        _current = token{COMP_EQUALS, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{VALUE_ASSIGNMENT, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_lessthan(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '=':
            _current = token{COMP_LTE, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '<':
            _current = token{BITWISE_LSHIFT, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{COMP_LT, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_greaterthan(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '=':
            _current = token{COMP_GTE, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '>':
            _current = token{BITWISE_RSHIFT, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{COMP_GT, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_ampersand(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '&':
            _current = token{CONDITIONAL_AND, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '=':
            _current = token{BITWISE_ANDEQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{BITWISE_AND, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_verticalline(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    switch(lxr.peek_char()) {
        case '|':
            _current = token{CONDITIONAL_OR, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        case '=':
            _current = token{BITWISE_OREQ, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
            lxr.advance_char(2);
            break;

        default:
            _current = token{BITWISE_OR, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
            lxr.advance_char(1);
            break;
    }
}


void
lexer_token_exclamation(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == '=') {
        _current = token{COMP_NOT_EQUALS, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{CONDITIONAL_NOT, UNARY_EXPR_OPERATOR, index, {&src[index], 2}};
    }
}


void
lexer_token_tilde(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{BITWISE_NOT, UNARY_EXPR_OPERATOR, index, {&src[index],1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_uparrow(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == '=') {
        _current = token{BITWISE_XOREQ, BINARY_EXPR_OPERATOR, index, {&src[index],2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{BITWISE_XOR_OR_PTR, BINARY_EXPR_OPERATOR, index, {&src[index],1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_quote(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
        return;
    }


    const char opening_quote = lxr.current_char();
    size_t     string_start  = index;


    lxr.advance_char(1);
    while(true) {

        if(lxr.current_char() == '\0') {
            _current = token{ILLEGAL, UNSPECIFIC, src.size() - 1, "\\0"};
            break;
        }

        if(lxr.current_char() == opening_quote) {
            _current = token{STRING_LITERAL, LITERAL, index, {&src[string_start], (index - string_start) + 1}};
            lxr.advance_char(1);
            break;
        }

        if(lxr.current_char() == '\\' && lxr.peek_char() == opening_quote) {
            lxr.advance_char(2);
        }

        else {
            lxr.advance_char(1);
        }
    }
}


void
lexer_token_singlequote(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;
    const size_t start = index;

    lxr.advance_char(1);

    if(lxr.current_char() == '\\') {          // check for escaped single-quote
        lxr.advance_char(2);
    } else if(lxr.current_char() == '\'') {   // empty literal
        lxr.advance_char(1);
        _current = token{CHARACTER_LITERAL, LITERAL, start, {&src[start], index - start}};
        return;
    } else {
        lxr.advance_char(1);
    }


    switch(lxr.current_char()) {
        case '\0':
            _current = token{ILLEGAL, UNSPECIFIC, src.size() - 1, "\\0"};
            break;

        case '\'':
            lxr.advance_char(1);
            _current = token{CHARACTER_LITERAL, LITERAL, start, {&src[start], index - start}};
            break;

        default:
            _current = token{ILLEGAL, UNSPECIFIC, start, {&src[start], index - start}};
            break;
    }
}


void
lexer_token_lsquarebracket(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{LSQUARE_BRACKET, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_rsquarebracket(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{RSQUARE_BRACKET, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_questionmark(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{QUESTION_MARK, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


void
lexer_token_colon(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    }

    else if(lxr.peek_char() == ':') {
        _current = token{CONST_TYPE_ASSIGNMENT, BINARY_EXPR_OPERATOR, index, {&src[index], 2}};
        lxr.advance_char(2);
    }

    else {
        _current = token{TYPE_ASSIGNMENT, BINARY_EXPR_OPERATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}

void
lexer_token_dot(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{DOT, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}

void
lexer_token_backslash(lexer& lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{NAMESPACE_ACCESS, UNSPECIFIC, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}


//
// We can leave the following tokens as "illegal" for now.
// can change easily if we decide to do anything with these characters.
// for now, they are unused and don't do anything.
//

void
lexer_token_pound(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{ILLEGAL, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}

void
lexer_token_backtick(lexer& lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{ILLEGAL, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}

void lexer_token_at(lexer &lxr) {

    auto &[src, index, curr_line, _current, _] = lxr;

    if(index >= src.size()) {
        _current = token{END_OF_FILE, UNSPECIFIC, src.size() - 1, "\\0"};
    } else {
        _current = token{ILLEGAL, PUNCTUATOR, index, {&src[index], 1}};
        lxr.advance_char(1);
    }
}
