//
// Created by Diago on 2024-08-02.
//

#include <lexer.hpp>


bool
tak::Lexer::init(const std::string& file_name) {

    this->src_index_        = 0;
    this->curr_line_        = 1;
    this->current_          = Token();
    this->source_file_name_ = std::string();


    if(!std::filesystem::exists(file_name)) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("FATAL, source file \"{}\" does not exist.", file_name);
        return false;
    }

    if(!is_regular_file(std::filesystem::path(file_name))) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("FATAL, \"{}\" is not a regular file.", file_name);
        return false;
    }


    source_file_name_ = std::filesystem::absolute(file_name);
    std::ifstream input(source_file_name_, std::ios::binary);

    defer_if(input.is_open(), [&] {
        input.close();
    });


    if(!input.is_open()) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("FATAL, could not open source file \"{}\".", file_name);
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamsize file_size = input.tellg();
    input.seekg(0, std::ios::beg);

    if(!file_size) {
        print("FATAL, source file \"{}\" is empty.", file_name);
        return false;
    }


    src_.clear();
    src_.resize(file_size);

    if(!input.read(src_.data(), file_size)) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("FATAL, opened source file \"{}\" but contents could not be read.", file_name);
        return false;
    }

    // UTF-8 Byte-Order-Mark check (optional in spec)
    if(src_.size() > 3
        && static_cast<uint8_t>(src_[0]) == 0xEF
        && static_cast<uint8_t>(src_[1]) == 0xBB
        && static_cast<uint8_t>(src_[2]) == 0xBF
    ) {
        src_index_ = 3;
    }

    return true;
}