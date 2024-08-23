//
// Created by Diago on 2024-07-23.
//

#include <util.hpp>


std::optional<char>
tak::get_escaped_char_via_real(const char real) {
    switch(real) {
        case 'n':  return '\n';
        case 'b':  return '\b';
        case 'a':  return '\a';
        case 'r':  return '\r';
        case '\'': return '\'';
        case '\\': return '\\';
        case '`':  return '`';
        case '"':  return '"';
        case 't':  return '\t';
        case '0':  return '\0';
        default:   return std::nullopt;
    }
}

std::optional<std::string>
tak::remove_escaped_chars(const std::string_view& str) {
    assert(!str.empty());
    std::string buffer;
    size_t      index  = 0;
    const bool  is_raw = str.front() == '`';

    while(index < str.size()) {

        const uint8_t raw = static_cast<uint8_t>(str[index]);
        if(raw >= 0x80) {
            const size_t utf8_begin = index;
            bool invalid            = false;

            if      ((raw & 0xE0) == 0xC0) index += 2;
            else if ((raw & 0xF0) == 0xE0) index += 3;
            else if ((raw & 0xF8) == 0xF0) index += 4;
            else invalid = true;

            if(invalid || index - 1 >= str.size()) {
                panic("Invalid UTF-8 sequence.");
            }

            buffer += str.substr(utf8_begin, index - utf8_begin);
        }
        else if(str[index] == '\\') {
            if(is_raw && str[index + 1] != '`') { // @Unsafe MAYBE unsafe, not bounds checking
                buffer += str[index];
                ++index;
                continue;
            }

            if(const auto escaped = get_escaped_char_via_real(str[index + 1])) {
                buffer += *escaped;
                index += 2;
            } else {
                return std::nullopt;
            }
        }
        else {
            buffer += str[index];
            ++index;
        }
    }

    return buffer;
}

std::optional<std::string>
tak::get_actual_string(const std::string_view& str) {

    if(auto actual = remove_escaped_chars(str)) {
        if(actual->size() < 3)
            return std::nullopt;

        actual->erase(0,1);
        actual->pop_back();
        return *actual;
    }

    return std::nullopt;
}

std::optional<char>
tak::get_actual_char(const std::string_view& str) {

    if(const auto actual = remove_escaped_chars(str)) {
        if(actual->size() < 3)
            return std::nullopt;

        return (*actual)[1];
    }

    return std::nullopt;
}

std::vector<std::string>
tak::split_string(std::string str, const char delim) {

    if(str.empty()) {
        panic("split_string: empty input.");
    }

    if(str.front() == delim) {
        str.erase(0, 1);
    }
    
    if(!str.empty() && str.back() != delim) {
        str += delim;
    }

    std::vector<std::string> chunks;
    size_t start = 0;
    size_t dot   = str.find(delim);

    while(dot != std::string::npos) {
        if(dot > start) {
            chunks.emplace_back(str.substr(start, dot - start));
        }

        start = dot + 1;
        dot = str.find(delim, start);
    }

    return chunks;
}
