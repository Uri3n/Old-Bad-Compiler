//
// Created by Diago on 2024-07-02.
//

#ifndef DEFER_HPP
#define DEFER_HPP
#include <type_traits>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define defer(callable) auto _ = defer_wrapper(callable);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define defer_if(condition, callable) \
    auto _ = defer_wrapper([&]{       \
        auto _sub = callable;         \
        if((condition)) {             \
            _sub();                   \
        }                             \
})                                    \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<typename T> requires std::is_invocable_v<T>
class defer_wrapper {
    T callable;
public:

    auto call() -> decltype(callable()) { // unused
        return callable();
    }

    explicit defer_wrapper(T func) : callable(func) {}
    ~defer_wrapper() { callable(); }
};


#endif //DEFER_HPP
