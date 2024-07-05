//
// Created by Diago on 2024-07-04.
//

#include <lexer.hpp>

void
lexer::_raise_error_impl(const std::string& message, const size_t file_position, const uint32_t line) {

    size_t line_start = file_position;
    size_t line_end   = file_position;

    if(file_position >= src.size()) {
        print("Internal parse-error: illegal token file position is out of bounds.");
        return;
    }


    while(line_start != 0 && src[line_start] != '\n') {
        --line_start;
    }

    while(line_end < src.size() && src[line_end] != '\n') {
        ++line_end;
    }


    uint32_t offset    = file_position - (line_start + 1);
    auto    full_line  = std::string(&src[line_start], line_end - line_start);

    if(full_line.empty() || offset >= full_line.size()) {
        print("Internal parse-error: bad error report.");
    }

    if(full_line.front() == '\n') {
        full_line.erase(0,1);
    }


    //
    // Let's display the error message with a little arrow.
    //

    std::string whitespace;
    whitespace.resize(offset);
    std::fill(whitespace.begin(), whitespace.end(), ' ');

    print("ERROR compiling source file \"{}\" on line {}:", source_file_name, line);
    print("{}", full_line);
    print("{}^", whitespace);
    print("{}{}", whitespace, message);
}


void
lexer::raise_error(const std::string& message) {
    _raise_error_impl(message, _current.src_pos, _current.line);
}

void
lexer::raise_error(const std::string& message, const size_t file_position, const uint32_t line) {
    _raise_error_impl(message, file_position, line);
}
