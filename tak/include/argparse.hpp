//
// Created by Diago on 2024-08-22.
//

#ifndef ARGPARSE_HPP
#define ARGPARSE_HPP
#include <string>
#include <variant>
#include <cassert>
#include <functional>
#include <ranges>
#include <typeindex>
#include <optional>
#include <io.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak::argp {
    template<typename T>
    concept valid_argument =
           std::is_same_v<T, std::string>
        || std::is_same_v<T, int64_t>
        || std::is_same_v<T, bool>
        || std::is_same_v<T, std::monostate>;

    class Argument;
    class Parameter;
    class Handler;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tak::argp::Argument {
public:
    using Empty = std::monostate;
    using Value = std::variant<std::string, int64_t, bool, Empty>;

    Value       value_;
    std::string str_;
    size_t      pos_ = 0;

    template<valid_argument T>
    static Argument create(const T& val, const std::string& str, size_t pos = 0);
    static Argument create(const Value& val, const std::string& str, size_t pos = 0);

    ~Argument() = default;
    Argument()  = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tak::argp::Parameter {
    Parameter()  = default;
public:
    std::string longf_;
    std::string shortf_;
    std::string description_;
    std::optional<Argument::Value> default_;
    std::function<std::optional<std::string>(Argument::Value&)> pred_;

    std::type_index expected_ = typeid(Argument::Empty);
    bool required_            = false;

    template<valid_argument T>
    static Parameter create(
        const std::string& longf,
        const std::string& shortf,
        const T& _default,
        const std::string& desc      = "",
        const bool required          = false,
        const decltype(pred_)& pred  = decltype(pred_)()
    );

    template<valid_argument T>
    static Parameter create(
        const std::string& longf,
        const std::string& shortf,
        const std::string& desc      = "",
        const bool required          = false,
        const decltype(pred_)& pred  = decltype(pred_)()
    );

    ~Parameter() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tak::argp::Handler {
    bool is_arg_valid(const std::string& arg_name);
    bool is_arg_duplicate(const std::string& arg_name);
    bool type_matches_parameter(const std::string& arg_name, const std::type_index&);
    void check_predicates();
    void check_defaults();
    void check_required();
    Handler() = default;
public:
    std::vector<std::string> chunks_;
    std::vector<Parameter> params_;
    std::vector<Argument>  args_;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<valid_argument T>
    Handler& get(const std::string& name, std::optional<T>& out);
    Handler& add_parameter(Parameter&& param);
    Handler& parse();
    void display_help(const std::optional<std::string>& msg = std::nullopt);
    [[noreturn]] void handle_bad_chunk(const std::string& msg, size_t pos);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static std::optional<int64_t> to_i64(const std::string& chunk);
    static std::optional<bool>    to_bool(const std::string& chunk);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static Handler create(std::vector<std::string>&& chunks);
    static Handler create(int argc, char** argv);
    ~Handler() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<tak::argp::valid_argument T>
auto tak::argp::Argument::create(
    const T& val,
    const std::string& str,
    const size_t pos
) -> Argument {

    Argument a;
    a.pos_   = pos;
    a.str_   = str;
    a.value_ = val;
    return a;
}

template<tak::argp::valid_argument T>
auto tak::argp::Parameter::create(
const std::string& longf,
    const std::string& shortf,
    const T& _default,
    const std::string& desc,
    const bool required,
    const decltype(pred_)& pred
) -> Parameter {

    assert(longf.starts_with("--") && "Use UNIX-style argument syntax.");
    assert(shortf.starts_with("-") && "Use UNIX-style argument syntax.");

    Parameter p;
    p.expected_     = typeid(T);
    p.longf_        = longf;
    p.shortf_       = shortf;
    p.required_     = required;
    p.description_  = desc;
    p.default_      = { _default };
    p.pred_         = pred;
    return p;
}

template<tak::argp::valid_argument T>
auto tak::argp::Parameter::create(
const std::string& longf,
    const std::string& shortf,
    const std::string& desc,
    const bool required,
    const decltype(pred_)& pred
) -> Parameter {

    assert(longf.starts_with("--") && "Use UNIX-style argument syntax.");
    assert(shortf.starts_with("-") && "Use UNIX-style argument syntax.");

    Parameter p;
    p.expected_     = typeid(T);
    p.longf_        = longf;
    p.shortf_       = shortf;
    p.description_  = desc;
    p.required_     = required;
    p.default_      = std::nullopt;
    p.pred_         = pred;
    return p;
}

template<tak::argp::valid_argument T>
auto tak::argp::Handler::get(const std::string& name, std::optional<T>& out) -> Handler& {
    const auto param = std::ranges::find_if(params_, [&](const Parameter& p) {
        return p.longf_ == name || p.shortf_ == name;
    });

    assert(param != params_.end() && "calling get() for nonexistant parameter.");
    auto arg = std::ranges::find_if(args_, [&](const Argument& a) {
        return a.str_ == param->longf_ || a.str_ == param->shortf_;
    });

    if(arg == args_.end()) {
        out = std::nullopt;
    } else {
        out = std::optional<T>(std::get<T>(arg->value_));
    }

    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //ARGPARSE_HPP
