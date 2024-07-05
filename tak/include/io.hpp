//
// Created by Diago on 2024-07-02.
//

#ifndef IO_HPP
#define IO_HPP
#include <string>
#include <format>
#include <iostream>


template<typename ... Args>
static std::string fmt(const std::format_string<Args...> fmt, Args... args) {

    std::string output;

    try {
        output = std::vformat(fmt.get(), std::make_format_args(args...));
    } catch(const std::format_error& e) {
        output = std::string("FORMAT ERROR: ") + e.what();
    } catch(...) {
        output = "UNKNOWN FORMATTING ERROR";
    }

    return output;
}


template<typename ... Args>
static void print(const std::format_string<Args...> fmt, Args... args) {

    std::string output;

    try {
        output = std::vformat(fmt.get(), std::make_format_args(args...));
    } catch(const std::format_error& e) {
        output = std::string("FORMAT ERROR: ") + e.what();
    } catch(...) {
        output = "UNKNOWN FORMATTING ERROR";
    }

    std::cout << output << std::endl;
}

#endif //IO_HPP
