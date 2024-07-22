//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>


std::string
lexer_token_type_to_string(const token_t type) {

#define X(NAME) case TOKEN_##NAME: return #NAME;
    switch(type) {
        TOKEN_LIST
        default: return "Unknown."; // failsafe
    }
#undef X
}

std::string
lexer_token_kind_to_string(const token_kind kind) {

#define X(NAME) case KIND_##NAME: return #NAME;
    switch(kind) {
        TOKEN_KIND_LIST
        default: return "Unknown"; // failsafe
    }
#undef X
}

void
lexer_display_token_data(const token& tok) {
    print(
        "Value: {}\nType: {}\nKind: {}\nFile Pos Index: {}\nLine Number: {}\n",
        tok.value,
        lexer_token_type_to_string(tok.type),
        lexer_token_kind_to_string(tok.kind),
        tok.src_pos,
        tok.line
    );
}

void
lexer::advance_char(const uint32_t amnt) {
    if(current_.type != TOKEN_END_OF_FILE && src_index_ < src_.size()) {
        src_index_ += amnt;
    }
}

void
lexer::advance_line() {
    ++curr_line_;
}

char
lexer::peek_char() {
    if(src_index_ + 1 >= src_.size()) {
        return '\0';
    }

    return src_[src_index_ + 1];
}

char
lexer::current_char() {
    if(src_index_ >= src_.size()) {
        return '\0';
    }

    return src_[src_index_];
}

std::optional<char>
get_escaped_char_via_real(const char real) {

    switch(real) {
        case 'n':  return '\n';
        case 'b':  return '\b';
        case 'a':  return '\a';
        case 'r':  return '\r';
        case '\'': return '\'';
        case '\\': return '\\';
        case '"':  return '"';
        case 't':  return '\t';
        case '0':  return '\0';
        default:   return std::nullopt;
    }
}

std::optional<std::string>
remove_escaped_chars(const std::string_view& str) {

    std::string buffer;

    for(size_t i = 0; i < str.size(); i++) {
        if(str[i] == '\\') {
            if(i + 1 >= str.size()) {
                return std::nullopt;
            }

            if(const auto escaped = get_escaped_char_via_real(str[i + 1])) {
                buffer += *escaped;
                i++;
            } else {
                return std::nullopt;
            }
        }

        else {
            buffer += str[i];
        }
    }

    return buffer;
}

std::optional<std::string>
get_actual_string(const std::string_view& str) {

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
get_actual_char(const std::string_view& str) {

    if(const auto actual = remove_escaped_chars(str)) {
        if(actual->size() < 3)
            return std::nullopt;

        return (*actual)[1];
    }

    return std::nullopt;
}

std::optional<size_t>
lexer_token_lit_to_int(const token& tok) {

    size_t val = 0;

    if(tok == TOKEN_INTEGER_LITERAL) {
        try {
            val = std::stoull(std::string(tok.value));
        } catch(...) {
            return std::nullopt;
        }
    } else if(tok == TOKEN_CHARACTER_LITERAL) {
        if(const auto actual = get_actual_char(tok.value)) {
            val = static_cast<size_t>(*actual);
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt; // Cannot be converted to an integer
    }

    return val;
}

bool
lexer::init(const std::string& file_name) {

    std::ifstream input(file_name, std::ios::binary);
    source_file_name_ = file_name;

    defer([&] {
       if(input.is_open()) {
           input.close();
       }
    });


    if(!input.is_open()) {
        print("FATAL, could not open source file \"{}\".", file_name);
        return false;
    }


    input.seekg(0, std::ios::end);
    const std::streamsize file_size = input.tellg();
    input.seekg(0, std::ios::beg);


    src_.resize(file_size);
    if(!input.read(src_.data(), file_size)) {
        print("FATAL, opened source file but contents could not be read.");
        return false;
    }

    return true;
}

bool
lexer::init(const std::vector<char>& src) {
    this->src_ = src;
    return true;
}
