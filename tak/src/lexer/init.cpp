//
// Created by Diago on 2024-08-02.
//

#include <lexer.hpp>


bool
tak::Lexer::init(const std::string& file_name) {

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

    if(!file_size) {
        print("FATAL, source file \"{}\" is empty.", file_name);
    }

    src_.resize(file_size);
    if(!input.read(src_.data(), file_size)) {
        print("FATAL, opened source file \"{}\" but contents could not be read.", file_name);
        return false;
    }

    return true;
}