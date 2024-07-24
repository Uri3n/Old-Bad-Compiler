//
// Created by Diago on 2024-07-22.
//

#ifndef PANIC_HPP
#define PANIC_HPP
#include <cstdlib>
#include <io.hpp>

#define panic(message)                               \
    print("PANIC :: {}", message);                   \
    print("FILE: {}\nLINE: {}", __FILE__, __LINE__); \
    exit(1);                                         \

#endif //PANIC_HPP
