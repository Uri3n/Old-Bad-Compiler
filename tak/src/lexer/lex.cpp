//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>


void
tak::Lexer::advance(const uint32_t amnt) {

    if(current_ == TOKEN_END_OF_FILE || current_ == TOKEN_ILLEGAL) {
        return;
    }


    static std::unordered_map<char, token_func> token_map =
    {
        {' ',  token_skip},
        {'\r', token_skip},
        {'\b', token_skip},
        {'\t', token_skip},
        {'\n', token_newline},
        {';',  token_semicolon},
        {'(',  token_lparen},
        {')',  token_rparen},
        {'{',  token_lbrace},
        {'}',  token_rbrace},
        {',',  token_comma},
        {'-',  token_hyphen},
        {'+',  token_plus},
        {'*',  token_asterisk},
        {'/',  token_fwdslash},
        {'%',  token_percent},
        {'=',  token_equals},
        {'<',  token_lessthan},
        {'>',  token_greaterthan},
        {'&',  token_ampersand},
        {'|',  token_verticalline},
        {'!',  token_exclamation},
        {'~',  token_tilde},
        {'^',  token_uparrow},
        {'\'', token_singlequote},
        {'"',  token_quote},
        {'`',  token_quote},
        {'[',  token_lsquarebracket},
        {']',  token_rsquarebracket},
        {'?',  token_questionmark},
        {':',  token_colon},
        {'#',  token_pound},
        {'@',  token_at},
        {'.',  token_dot},
        {'\\', token_backslash},
        {'\0', token_null},
    };


    for(uint32_t i = 0; i < amnt; i++) {
        do {
            if(token_map.contains(current_char())) {
                token_map[current_char()](*this);
            } else {
                infer_ambiguous_token(*this, token_map);
            }
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