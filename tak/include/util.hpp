//
// Created by Diago on 2024-07-23.
//

#ifndef UTILS_HPP
#define UTILS_HPP
#include <panic.hpp>
#include <vector>
#include <optional>
#include <cassert>
#include <typedata.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    std::vector<std::string>   split_string(std::string str, char delim);
    std::optional<char>        get_escaped_char_via_real(char real);
    std::optional<std::string> remove_escaped_chars(const std::string_view& str);
    std::optional<std::string> get_actual_string(const std::string_view& str);
    std::optional<char>        get_actual_char(const std::string_view& str);
    primitive_t                token_to_var_t(token_t tok); // not in the token namespace bc of circular include.
}

#endif //UTILS_HPP
