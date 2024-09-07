//
// Created by Diago on 2024-07-04.
//

#include <lexer.hpp>


void
tak::Lexer::_raise_error_impl(const std::string& message, size_t file_position, const uint32_t line, const bool warning) {

    size_t line_start = file_position;
    size_t line_end   = file_position;


    if(src_.empty())
        return;

    if(file_position >= src_.size())
        file_position = src_.size() - 1;

    while(line_start != 0 && src_[line_start] != '\n')
        --line_start;

    while(line_end < src_.size() && src_[line_end] != '\n')
        ++line_end;


    const uint32_t offset  = file_position - (line_start + 1);
    auto full_line         = std::string(&src_[line_start], line_end - line_start);

    if(full_line.empty() || offset >= full_line.size()) {
        print("Unexpected end of file!"); // @Cleanup: This is lazy and bad.
        exit(1);
    }

    if(full_line.front() == '\n') {
        full_line.erase(0,1);
    }


    //
    // display error message with arrow
    //

    std::string filler;
    std::string whitespace;

    filler.resize(full_line.size());
    whitespace.resize(offset);

    std::fill(filler.begin(), filler.end(), '~');
    std::fill(whitespace.begin(), whitespace.end(), ' ');

    assert(offset < filler.size());
    filler[offset] = '^';

    print<TFG_NONE, TBG_NONE, TSTYLE_BOLD>("in {}{}", source_file_name_, !line ? std::string("") : ':' + std::to_string(line));
    print<TFG_NONE, TBG_NONE, TSTYLE_NONE>("{}", full_line);
    print<TFG_NONE, TBG_NONE, TSTYLE_NONE>("{}", filler);

    if(warning) print<TFG_YELLOW, TBG_NONE, TSTYLE_NONE>("{}{}\n", whitespace, message);
    else        print<TFG_RED, TBG_NONE, TSTYLE_NONE>("{}{}\n", whitespace, message);
}

void tak::Lexer::raise_error(const std::string& message) {
    _raise_error_impl(message, current_.src_pos, current_.line);
}

void tak::Lexer::raise_error(const std::string& message, const size_t file_position, const uint32_t line) {
    _raise_error_impl(message, file_position, line);
}

void tak::Lexer::raise_error(const std::string& message, const size_t file_position) {
    _raise_error_impl(message, file_position, 0);
}

void tak::Lexer::raise_warning(const std::string& message) {
    _raise_error_impl(message, current_.src_pos, current_.line, true);
}

void tak::Lexer::raise_warning(const std::string& message, const size_t file_position, const uint32_t line) {
    _raise_error_impl(message, file_position, line, true);
}

void tak::Lexer::raise_warning(const std::string& message, const size_t file_position) {
    _raise_error_impl(message, file_position, 0);
}
