//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>



void
lexer::advance(const uint32_t amnt) {

    if(current_ == TOKEN_END_OF_FILE || current_ == TOKEN_ILLEGAL) {
        return;
    }


    static std::unordered_map<char, token_func> token_map =
    {
        {' ',  lexer_token_skip},
        {'\r', lexer_token_skip},
        {'\b', lexer_token_skip},
        {'\t', lexer_token_skip},
        {'\n', lexer_token_newline},
        {';', lexer_token_semicolon},
        {'(', lexer_token_lparen},
        {')', lexer_token_rparen},
        {'{', lexer_token_lbrace},
        {'}', lexer_token_rbrace},
        {',', lexer_token_comma},
        {'-', lexer_token_hyphen},
        {'+', lexer_token_plus},
        {'*', lexer_token_asterisk},
        {'/', lexer_token_fwdslash},
        {'%', lexer_token_percent},
        {'=', lexer_token_equals},
        {'<', lexer_token_lessthan},
        {'>', lexer_token_greaterthan},
        {'&', lexer_token_ampersand},
        {'|', lexer_token_verticalline},
        {'!', lexer_token_exclamation},
        {'~', lexer_token_tilde},
        {'^', lexer_token_uparrow},
        {'\'', lexer_token_singlequote},
        {'"', lexer_token_quote},
        {'[', lexer_token_lsquarebracket},
        {']', lexer_token_rsquarebracket},
        {'?', lexer_token_questionmark},
        {':', lexer_token_colon},
        {'#', lexer_token_pound},
        {'@', lexer_token_at},
        {'.', lexer_token_dot},
        {'`', lexer_token_backtick},
        {'\\', lexer_token_backslash},
        {'\0', lexer_token_null},
    };


    for(uint32_t i = 0; i < amnt; i++) {
        do {
            if(token_map.contains(current_char())) {
                token_map[current_char()](*this);
            } else {
                lexer_infer_ambiguous_token(*this, token_map);
            }
        } while(current_ == TOKEN_NONE);

        current_.line = curr_line_;
    }
}


token&
lexer::current() {
    if(current_ == TOKEN_NONE) { // if the lexer was just created current will be TOKEN_NONE initially
        advance(1);
    }

    return current_;
}


token
lexer::peek(const uint32_t amnt) {

    if(current_ == TOKEN_NONE) {
        advance(1);
    }


    const uint32_t line_tmp   = this->curr_line_;
    const size_t   index_tmp  = this->src_index_;
    const token    tok_tmp    = this->current_;

    advance(amnt);
    const token    tok_peeked = this->current_;

    curr_line_ = line_tmp;
    src_index_ = index_tmp;
    current_  = tok_tmp;

    return tok_peeked;
}