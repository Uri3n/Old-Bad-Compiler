//
// Created by Diago on 2024-07-02.
//

#ifndef LEXER_HPP
#define LEXER_HPP
#include <optional>
#include <token.hpp>
#include <vector>
#include <algorithm>
#include <support.hpp>
#include <string>
#include <cctype>
#include <cassert>
#include <defer.hpp>
#include <fstream>
#include <io.hpp>
#include <unordered_map>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    class Lexer {
    public:

        typedef void (*token_func)(Lexer& lxr);

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

        bool   is_current_utf8_begin();
        void   skip_utf8_sequence();

        void   _raise_error_impl(const std::string& message, size_t file_position, uint32_t line);
        void   raise_error(const std::string& message);
        void   raise_error(const std::string& message, size_t file_position, uint32_t line);
        void   raise_error(const std::string& message, size_t file_position);

        bool   init(const std::string& file_name);

        static void token_skip(Lexer& lxr);
        static void token_newline(Lexer& lxr);
        static void token_semicolon(Lexer& lxr);
        static void token_lparen(Lexer& lxr);
        static void token_rparen(Lexer& lxr);
        static void token_lbrace(Lexer& lxr);
        static void token_rbrace(Lexer& lxr);
        static void token_comma(Lexer& lxr);
        static void token_hyphen(Lexer& lxr);
        static void token_plus(Lexer& lxr);
        static void token_asterisk(Lexer& lxr);
        static void token_fwdslash(Lexer& lxr);
        static void token_percent(Lexer& lxr);
        static void token_equals(Lexer& lxr);
        static void token_lessthan(Lexer& lxr);
        static void token_greaterthan(Lexer& lxr);
        static void token_ampersand(Lexer& lxr);
        static void token_verticalline(Lexer& lxr);
        static void token_exclamation(Lexer& lxr);
        static void token_tilde(Lexer& lxr);
        static void token_uparrow(Lexer& lxr);
        static void token_quote(Lexer& lxr);
        static void token_singlequote(Lexer& lxr);
        static void token_lsquarebracket(Lexer& lxr);
        static void token_rsquarebracket(Lexer& lxr);
        static void token_questionmark(Lexer& lxr);
        static void token_colon(Lexer& lxr);
        static void token_dot(Lexer& lxr);
        static void token_pound(Lexer& lxr);
        static void token_at(Lexer& lxr);
        static void token_null(Lexer& lxr);
        static void token_backslash(Lexer& lxr);
        static void infer_ambiguous_token(Lexer& lxr, const std::unordered_map<char, token_func>& illegals);

        static Token token_hex_literal(Lexer& lxr);
        static Token token_numeric_literal(Lexer& lxr);

        ~Lexer() = default;
        Lexer()  = default;
    };

}      //namespace tak
#endif //LEXER_HPP
