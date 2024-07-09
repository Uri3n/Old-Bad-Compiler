//
// Created by Diago on 2024-07-02.
//

#ifndef LEXER_HPP
#define LEXER_HPP
#include <optional>
#include <token.hpp>
#include <vector>
#include <algorithm>
#include <string>
#include <defer.hpp>
#include <fstream>
#include <io.hpp>
#include <unordered_map>


std::optional<std::vector<char>> read_file(const std::string& file_name);

struct lexer {

    std::vector<char>   src;

    size_t              src_index   = 0;
    uint32_t            curr_line   = 1;
    token               _current;
    std::string         source_file_name;

    void   advance(uint32_t amnt);
    token& current();
    token  peek(uint32_t amnt);

    char   peek_char();
    char   current_char();
    void   advance_char(uint32_t amnt);
    void   advance_line();

    void   _raise_error_impl(const std::string& message, size_t file_position, uint32_t line);
    void   raise_error(const std::string& message);
    void   raise_error(const std::string& message, size_t file_position, uint32_t line);

    bool init(const std::string& source_file_name);
    bool init(const std::vector<char>& source);

    ~lexer() = default;
    lexer()  = default;
};


typedef void (*token_func)(lexer& lxr);

void lexer_token_skip(lexer& lxr);
void lexer_token_newline(lexer& lxr);
void lexer_token_semicolon(lexer& lxr);
void lexer_token_lparen(lexer& lxr);
void lexer_token_rparen(lexer& lxr);
void lexer_token_lbrace(lexer& lxr);
void lexer_token_rbrace(lexer& lxr);
void lexer_token_comma(lexer& lxr);
void lexer_token_hyphen(lexer& lxr);
void lexer_token_plus(lexer& lxr);
void lexer_token_asterisk(lexer& lxr);
void lexer_token_fwdslash(lexer& lxr);
void lexer_token_percent(lexer& lxr);
void lexer_token_equals(lexer& lxr);
void lexer_token_lessthan(lexer& lxr);
void lexer_token_greaterthan(lexer& lxr);
void lexer_token_ampersand(lexer& lxr);
void lexer_token_verticalline(lexer& lxr);
void lexer_token_exclamation(lexer& lxr);
void lexer_token_tilde(lexer& lxr);
void lexer_token_uparrow(lexer& lxr);
void lexer_token_quote(lexer& lxr);
void lexer_token_singlequote(lexer& lxr);
void lexer_token_lsquarebracket(lexer& lxr);
void lexer_token_rsquarebracket(lexer& lxr);
void lexer_token_questionmark(lexer& lxr);
void lexer_token_colon(lexer& lxr);
void lexer_token_dot(lexer& lxr);
void lexer_token_pound(lexer& lxr);
void lexer_token_at(lexer& lxr);
void lexer_token_backtick(lexer& lxr);
void lexer_infer_ambiguous_token(lexer& lxr, std::unordered_map<char, token_func>& illegals);


void lexer_display_token_data(const token& tok);
std::string lexer_token_kind_to_string(token_kind kind);
std::string lexer_token_type_to_string(token_t type);

std::optional<std::string> remove_escaped_chars(const std::string_view& str);
std::optional<char>        get_escaped_char_via_real(char real);
bool                       is_token_valid_numeric_literal(const std::string_view& token_value);
token                      token_numeric_literal(lexer& lxr, std::unordered_map<char, token_func>& illegals);

#endif //LEXER_HPP
