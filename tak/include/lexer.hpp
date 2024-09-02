//
// Created by Diago on 2024-07-02.
//

#ifndef LEXER_HPP
#define LEXER_HPP
#include <optional>
#include <token.hpp>
#include <vector>
#include <algorithm>
#include <util.hpp>
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
        std::vector<char>   src_;
        size_t              src_index_ = 0;
        uint32_t            curr_line_ = 1;
        Token               current_;
        std::string         source_file_name_;

        void   _advance_impl();
        void   advance(uint32_t amnt);
        Token& current();
        Token  peek(uint32_t amnt);

        char peek_char(uint32_t amnt = 1);
        char current_char();
        void advance_char(uint32_t amnt);
        void advance_line();

        bool is_current_utf8_begin();
        void skip_utf8_sequence();

        void _raise_error_impl(const std::string& message, size_t file_position, uint32_t line, bool warning = false);
        void raise_error(const std::string& message);
        void raise_error(const std::string& message, size_t file_position, uint32_t line);
        void raise_error(const std::string& message, size_t file_position);
        void raise_warning(const std::string& message);
        void raise_warning(const std::string& message, size_t file_position, uint32_t line);
        void raise_warning(const std::string& message, size_t file_position);
        bool init(const std::string& file_name);

        void token_skip();
        void token_newline();
        void token_semicolon();
        void token_lparen();
        void token_rparen();
        void token_lbrace();
        void token_rbrace();
        void token_comma();
        void token_hyphen();
        void token_plus();
        void token_asterisk();
        void token_fwdslash();
        void token_percent();
        void token_equals();
        void token_lessthan();
        void token_greaterthan();
        void token_ampersand();
        void token_verticalline();
        void token_bang();
        void token_tilde();
        void token_uparrow();
        void token_quote();
        void token_singlequote();
        void token_lsquarebracket();
        void token_rsquarebracket();
        void token_questionmark();
        void token_colon();
        void token_dot();
        void token_pound();
        void token_at();
        void token_null();
        void token_backslash();
        void token_dollarsign();
        void infer_ambiguous_token();
        Token token_hex_literal();
        Token token_numeric_literal();

        static bool is_char_reserved(char c);
        Lexer(const Lexer&)            = delete;
        Lexer& operator=(const Lexer&) = delete;

        ~Lexer() = default;
        Lexer()  = default;
    };

}      //namespace tak
#endif //LEXER_HPP
