//
// Created by Diago on 2024-07-02.
//

#ifndef LEXER_HPP
#define LEXER_HPP
#include <optional>
#include <token.hpp>
#include <vector>
#include <algorithm>
#include <utils.hpp>
#include <string>
#include <cctype>
#include <cassert>
#include <defer.hpp>
#include <fstream>
#include <io.hpp>
#include <unordered_map>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<char>> read_file(const std::string& file_name);

class Lexer {
public:

    std::vector<char>   src_;
    size_t              src_index_ = 0;
    uint32_t            curr_line_ = 1;
    Token               current_;
    std::string         source_file_name_;

    void   advance(uint32_t amnt);
    Token& current();
    Token  peek(uint32_t amnt);

    char   peek_char();
    char   current_char();
    void   advance_char(uint32_t amnt);
    void   advance_line();

    void   _raise_error_impl(const std::string& message, size_t file_position, uint32_t line);
    void   raise_error(const std::string& message);
    void   raise_error(const std::string& message, size_t file_position, uint32_t line);
    void   raise_error(const std::string& message, size_t file_position);

    bool init(const std::string& source_file_name);
    bool init(const std::vector<char>& source);

    ~Lexer() = default;
    Lexer()  = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*token_func)(Lexer& lxr);

void lexer_token_skip(Lexer& lxr);
void lexer_token_newline(Lexer& lxr);
void lexer_token_semicolon(Lexer& lxr);
void lexer_token_lparen(Lexer& lxr);
void lexer_token_rparen(Lexer& lxr);
void lexer_token_lbrace(Lexer& lxr);
void lexer_token_rbrace(Lexer& lxr);
void lexer_token_comma(Lexer& lxr);
void lexer_token_hyphen(Lexer& lxr);
void lexer_token_plus(Lexer& lxr);
void lexer_token_asterisk(Lexer& lxr);
void lexer_token_fwdslash(Lexer& lxr);
void lexer_token_percent(Lexer& lxr);
void lexer_token_equals(Lexer& lxr);
void lexer_token_lessthan(Lexer& lxr);
void lexer_token_greaterthan(Lexer& lxr);
void lexer_token_ampersand(Lexer& lxr);
void lexer_token_verticalline(Lexer& lxr);
void lexer_token_exclamation(Lexer& lxr);
void lexer_token_tilde(Lexer& lxr);
void lexer_token_uparrow(Lexer& lxr);
void lexer_token_quote(Lexer& lxr);
void lexer_token_singlequote(Lexer& lxr);
void lexer_token_lsquarebracket(Lexer& lxr);
void lexer_token_rsquarebracket(Lexer& lxr);
void lexer_token_questionmark(Lexer& lxr);
void lexer_token_colon(Lexer& lxr);
void lexer_token_dot(Lexer& lxr);
void lexer_token_pound(Lexer& lxr);
void lexer_token_at(Lexer& lxr);
void lexer_token_backtick(Lexer& lxr);
void lexer_token_null(Lexer& lxr);
void lexer_token_backslash(Lexer& lxr);
void lexer_infer_ambiguous_token(Lexer& lxr, const std::unordered_map<char, token_func>& illegals);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Token token_hex_literal(Lexer& lxr);
Token token_numeric_literal(Lexer& lxr);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //LEXER_HPP
