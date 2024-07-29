//
// Created by Diago on 2024-07-23.
//

#ifndef UTILS_HPP
#define UTILS_HPP
#include <token.hpp>
#include <panic.hpp>
#include <vector>
#include <optional>
#include <var_types.hpp>

std::vector<std::string> split_struct_member_path(std::string path);
std::optional<char> get_escaped_char_via_real(char real);
std::optional<std::string> remove_escaped_chars(const std::string_view& str);
std::optional<std::string> get_actual_string(const std::string_view& str);
std::optional<char> get_actual_char(const std::string_view& str);
std::optional<size_t> lexer_token_lit_to_int(const token& tok);
std::string lexer_token_to_string(token_t type);
std::string lexer_token_type_to_string(token_t type);
std::string lexer_token_kind_to_string(token_kind kind);
std::string typedata_to_str_msg(const type_data& type);
void  lexer_display_token_data(const token& tok);
uint16_t var_t_to_size_bytes(var_t type);

#endif //UTILS_HPP
