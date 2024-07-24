//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>

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
