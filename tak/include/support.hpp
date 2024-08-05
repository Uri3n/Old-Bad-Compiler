//
// Created by Diago on 2024-07-23.
//

#ifndef UTILS_HPP
#define UTILS_HPP
#include <token.hpp>
#include <panic.hpp>
#include <vector>
#include <optional>
#include <cassert>
#include <var_types.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    std::vector<std::string> split_string(std::string str, char delim);
    std::optional<char> get_escaped_char_via_real(char real);
    std::optional<std::string> remove_escaped_chars(const std::string_view& str);
    std::optional<std::string> get_actual_string(const std::string_view& str);
    std::optional<char> get_actual_char(const std::string_view& str);
    std::optional<size_t> lexer_token_lit_to_int(const Token& tok);
    std::string token_to_string(token_t type);
    std::string token_type_to_string(token_t type);
    std::string token_kind_to_string(token_kind kind);
    std::string typedata_to_str_msg(const TypeData& type);
    void  lexer_display_token_data(const Token& tok);
    uint16_t var_t_to_size_bytes(var_t type);
    uint16_t precedence_of(token_t _operator);
    var_t token_to_var_t(token_t tok_t);
    std::string var_t_to_string(var_t type);
}

#endif //UTILS_HPP
