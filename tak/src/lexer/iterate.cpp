//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>

void
tak::Lexer::advance_char(const uint32_t amnt) {
    if(current_.type != TOKEN_END_OF_FILE && src_index_ < src_.size()) {
        src_index_ += amnt;
    }
}

void
tak::Lexer::advance_line() {
    ++curr_line_;
}

char
tak::Lexer::peek_char() {
    if(src_index_ + 1 >= src_.size()) {
        return '\0';
    }

    return src_[src_index_ + 1];
}

bool
tak::Lexer::is_current_utf8_begin() {
    return static_cast<uint8_t>(current_char()) >= 0x80;
}

void
tak::Lexer::skip_utf8_sequence() {
    assert(is_current_utf8_begin());

    const uint8_t c        = static_cast<uint8_t>(current_char());
    const size_t curr_pos  = src_index_;
    bool invalid           = false;

    if      ((c & 0xE0) == 0xC0) advance_char(2);
    else if ((c & 0xF0) == 0xE0) advance_char(3);
    else if ((c & 0xF8) == 0xF0) advance_char(4);
    else invalid = true;

    if(invalid || src_index_ - 1 >= src_.size()) {
        print("{}", fmt("Invalid UTF-8 character sequence was found in file {} at byte position {}.", source_file_name_, curr_pos));
        exit(1);
    }
}

char
tak::Lexer::current_char() {
    if(src_index_ >= src_.size()) {
        return '\0';
    }

    return src_[src_index_];
}
