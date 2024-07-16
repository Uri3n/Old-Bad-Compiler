//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>



void
lexer::advance(const uint32_t amnt) {

    if(_current == TOKEN_END_OF_FILE || _current == TOKEN_ILLEGAL) {
        return;
    }


    static std::unordered_map<char, token_func> map =
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
    };


    for(uint32_t i = 0; i < amnt; i++) {
        do {
            if(map.contains(current_char())) {
                map[current_char()](*this);
            } else {
                lexer_infer_ambiguous_token(*this, map);
            }
        } while(_current == TOKEN_NONE);

        _current.line = curr_line;
    }
}


token&
lexer::current() {
    if(_current == TOKEN_NONE) { // if the lexer was just created current will be TOKEN_NONE initially
        advance(1);
    }

    return _current;
}


token
lexer::peek(const uint32_t amnt) {

    if(_current == TOKEN_NONE) {
        advance(1);
    }


    const uint32_t line_tmp   = this->curr_line;
    const size_t   index_tmp  = this->src_index;
    const token    tok_tmp    = this->_current;

    advance(amnt);
    const token    tok_peeked = this->_current;

    curr_line = line_tmp;
    src_index = index_tmp;
    _current  = tok_tmp;

    return tok_peeked;
}