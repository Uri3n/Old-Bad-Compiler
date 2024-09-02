//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>


void
tak::Lexer::_advance_impl() {
    switch(current_char()) {
        case ' ':  token_skip();            break;
        case '\r': token_skip();            break;
        case '\b': token_skip();            break;
        case '\t': token_skip();            break;
        case '\n': token_newline();         break;
        case ';':  token_semicolon();       break;
        case '(':  token_lparen();          break;
        case ')':  token_rparen();          break;
        case '{':  token_lbrace();          break;
        case '}':  token_rbrace();          break;
        case ',':  token_comma();           break;
        case '-':  token_hyphen();          break;
        case '+':  token_plus();            break;
        case '*':  token_asterisk();        break;
        case '/':  token_fwdslash();        break;
        case '%':  token_percent();         break;
        case '=':  token_equals();          break;
        case '<':  token_lessthan();        break;
        case '>':  token_greaterthan();     break;
        case '&':  token_ampersand();       break;
        case '|':  token_verticalline();    break;
        case '!':  token_bang();            break;
        case '~':  token_tilde();           break;
        case '^':  token_uparrow();         break;
        case '\'': token_singlequote();     break;
        case '"':  token_quote();           break;
        case '`':  token_quote();           break;
        case '[':  token_lsquarebracket();  break;
        case ']':  token_rsquarebracket();  break;
        case '?':  token_questionmark();    break;
        case ':':  token_colon();           break;
        case '#':  token_pound();           break;
        case '@':  token_at();              break;
        case '.':  token_dot();             break;
        case '\\': token_backslash();       break;
        case '$':  token_dollarsign();      break;
        case '\0': token_null();            break;
        default :  infer_ambiguous_token(); break;
    }
}

bool
tak::Lexer::is_char_reserved(const char c) {
    switch(c) {
        case ' ':
        case '\r':
        case '\b':
        case '\t':
        case '\n':
        case ';':
        case '(':
        case ')':
        case '{':
        case '}':
        case ',':
        case '-':
        case '+':
        case '*':
        case '/':
        case '%':
        case '=':
        case '<':
        case '>':
        case '&':
        case '|':
        case '!':
        case '~':
        case '^':
        case '\'':
        case '"':
        case '`':
        case '[':
        case ']':
        case '?':
        case ':':
        case '#':
        case '@':
        case '.':
        case '\\':
        case '$':
        case '\0': return true;
        default :  return false;
    }
}

void
tak::Lexer::advance(const uint32_t amnt) {
    if(src_index_ >= src_.size()) {
        current_ = Token{TOKEN_END_OF_FILE, KIND_UNSPECIFIC, src_.size() - 1, "\\0"};
        return;
    }

    for(uint32_t i = 0; i < amnt; i++) {
        do {
            _advance_impl();
        } while(current_ == TOKEN_NONE);
        current_.line = curr_line_;
    }
}

tak::Token&
tak::Lexer::current() {
    if(current_ == TOKEN_NONE) { // if the lexer was just created current will be TOKEN_NONE initially
        advance(1);
    }

    return current_;
}

tak::Token
tak::Lexer::peek(const uint32_t amnt) {
    if(current_ == TOKEN_NONE) {
        advance(1);
    }

    const uint32_t line_tmp   = this->curr_line_;
    const size_t   index_tmp  = this->src_index_;
    const Token    tok_tmp    = this->current_;

    advance(amnt);
    const Token    tok_peeked = this->current_;

    curr_line_ = line_tmp;
    src_index_ = index_tmp;
    current_   = tok_tmp;

    return tok_peeked;
}

void
tak::Lexer::advance_char(const uint32_t amnt) {
    if(current_.type != TOKEN_END_OF_FILE && src_index_ < src_.size()) {
        src_index_ += amnt;
    }
}

void
tak::Lexer::advance_line() {
    ++curr_line_;
}

char
tak::Lexer::peek_char(const uint32_t amnt) {
    if(src_index_ + amnt >= src_.size()) {
        return '\0';
    }

    return src_[src_index_ + amnt];
}

bool
tak::Lexer::is_current_utf8_begin() {
    return static_cast<uint8_t>(current_char()) >= 0x80;
}

void
tak::Lexer::skip_utf8_sequence() {
    assert(is_current_utf8_begin());

    const uint8_t c        = static_cast<uint8_t>(current_char());
    const size_t curr_pos  = src_index_;
    bool invalid           = false;

    if      ((c & 0xE0) == 0xC0) advance_char(2);
    else if ((c & 0xF0) == 0xE0) advance_char(3);
    else if ((c & 0xF8) == 0xF0) advance_char(4);
    else invalid = true;

    if(invalid || src_index_ - 1 >= src_.size()) {
        bold("{}", fmt("Invalid UTF-8 character sequence was found in file {} at byte position {}.", source_file_name_, curr_pos));
        exit(1);
    }
}

char
tak::Lexer::current_char() {
    if(src_index_ >= src_.size()) {
        return '\0';
    }

    return src_[src_index_];
}
