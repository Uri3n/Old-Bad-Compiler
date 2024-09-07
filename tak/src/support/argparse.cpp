//
// Created by Diago on 2024-08-22.
// This small library is responsible for
// parsing commandline arguments passed to the compiler.
//

#include <argparse.hpp>


tak::argp::Argument
tak::argp::Argument::create(const Value& val, const std::string& str, const size_t pos) {
    Argument a;
    a.pos_   = pos;
    a.str_   = str;
    a.value_ = val;
    return a;
}

tak::argp::Handler
tak::argp::Handler::create(std::vector<std::string>&& chunks) {
    Handler handler;
    handler.chunks_ = chunks;
    return handler;
}

tak::argp::Handler
tak::argp::Handler::create(const int argc, char** argv) {
    Handler handler;
    for( int i = 1; i < argc; i++ ) {
        handler.chunks_.emplace_back(std::string(argv[i]));
    }
    return handler;
}

std::optional<int64_t>
tak::argp::Handler::to_i64(const std::string& chunk) try {
    const auto is_invalid = std::ranges::find_if(chunk, [&](const char c) {
        return !isdigit(static_cast<unsigned char>(c)) && c != '-';
    });

    if(is_invalid != chunk.end()) {
        throw std::exception();
    }

    return { std::stoll(chunk) }; // < This throws
} catch(...) { return std::nullopt; }

std::optional<bool>
tak::argp::Handler::to_bool(const std::string& chunk) {
    if(chunk == "true")  return { true };
    if(chunk == "false") return { false };
    return std::nullopt;
}

[[noreturn]] void
tak::argp::Handler::handle_bad_chunk(const std::string& msg, const size_t pos) {
    assert(pos < chunks_.size() && "Out of bounds position");

    const std::string assembled_chunks = [&]() -> std::string {
        std::string _assembled;
        for(const auto& chunk : chunks_) {
            _assembled += chunk + ' ';
        }
        if(!_assembled.empty() && _assembled.back() == ' ') {
            _assembled.pop_back();
        }

        return _assembled;
    }();

    const size_t offset = [&]() -> size_t {
        size_t _offset = 0;
        for(size_t i = 0; i < pos; i++) {
            _offset += chunks_[i].size() + 1;
        }

        assert(_offset < assembled_chunks.size());
        return _offset;
    }();

    const std::string filler = [&]() -> std::string {
        std::string _filler;
        _filler.resize(assembled_chunks.size());
        std::fill(_filler.begin(), _filler.end(), '~');

        assert(offset < _filler.size());
        _filler[offset] = '^';
        return _filler;
    }();

    const std::string whitespace = [&]() -> std::string {
        std::string _whitespace;
        _whitespace.resize(offset);

        std::fill(_whitespace.begin(), _whitespace.end(), ' ');
        return _whitespace;
    }();

    print("{}", assembled_chunks);
    print("{}", filler);
    print("{}{}", whitespace, msg);
    exit(1);
}

tak::argp::Handler&
tak::argp::Handler::add_parameter(Parameter&& param) {
    params_.emplace_back(param);
    return *this;
}

bool
tak::argp::Handler::is_arg_valid(const std::string& arg_name) {
    const auto param_exists = std::ranges::find_if(params_, [&](const Parameter& p) {
        return p.longf_ == arg_name || p.shortf_ == arg_name;
    });

    return param_exists != params_.end();
}

bool
tak::argp::Handler::type_matches_parameter(const std::string& arg_name, const std::type_index& t) {
    const auto param = std::ranges::find_if(params_, [&](const Parameter& p) {
        return p.longf_ == arg_name || p.shortf_ == arg_name;
    });

    return param->expected_ == t;
}

bool
tak::argp::Handler::is_arg_duplicate(const std::string& arg_name) {
    const auto param = std::ranges::find_if(params_, [&](const Parameter& p) {
        return p.longf_ == arg_name || p.shortf_ == arg_name;
    });

    assert(param != params_.end() && "call is_arg_valid before this method.");
    const auto is_duplicate = std::ranges::find_if(args_, [&](const Argument& a) {
        return a.str_ == param->longf_ || a.str_ == param->shortf_;
    });

    return is_duplicate != args_.end();
}

void
tak::argp::Handler::check_predicates() {
    bool failed = false;
    for(const auto& param : params_) {
        const auto matching_arg = std::ranges::find_if(args_, [&](const Argument& a) {
            return a.str_ == param.longf_ || a.str_ == param.shortf_;
        });

        if(!param.pred_ || matching_arg == args_.end()) {
            continue;
        }

        const auto pred_result = param.pred_(matching_arg->value_);
        failed = pred_result.has_value(); // has_value : Indicates an error occurred.
        if(failed) {
            red_bold("{}", fmt("Error parsing command-line argument \"{}\":", matching_arg->str_));
            print("\"{}\"", *pred_result);
            print("");
        }
    }

    if(failed) exit(1);
}

void
tak::argp::Handler::display_help(const std::optional<std::string>& msg) {
    if(msg) {
        print("{}", *msg);
    }

    print("{:<30} {:<65} {:<8}", "Flag Name", "Description", "Type");
    print("{:=<30} {:=<65} {:=<8}", "=", "=", "=");

    for(const auto& param : params_) {
        const std::string expected_str = [&]() -> std::string {
            if(param.expected_ == typeid(bool))        return "Boolean";
            if(param.expected_ == typeid(std::string)) return "String";
            if(param.expected_ == typeid(int64_t))     return "Integer";
            return "None";
        }();

        const std::string out = fmt(
            "{:<30} {:<65} {:<8}",
            param.longf_ + ' ' + param.shortf_,
            param.description_,
            expected_str
        );

        print("{}", out);
    }
}

void
tak::argp::Handler::check_defaults() {
    for(const auto& param : params_) {
        const auto matching_arg = std::ranges::find_if(args_, [&](const Argument& a) {
            return a.str_ == param.longf_ || a.str_ == param.shortf_;
        });

        if(matching_arg == args_.end() && param.default_.has_value()) {
           args_.emplace_back(Argument::create(*param.default_, param.longf_));
        }
    }
}

void
tak::argp::Handler::check_required() {
    bool failed = false;
    for(const auto& param : params_) {
        if(!param.required_) {
            continue;
        }

        const auto matching_arg = std::ranges::find_if(args_, [&](const Argument& a) {
            return a.str_ == param.longf_ || a.str_ == param.shortf_;
        });

        failed = matching_arg == args_.end();
        if(failed) {
            red_bold("Error parsing command-line arguments:");
            print("{}", fmt("Flag \"{} / {}\" is required. Please specify this flag alongside its value.", param.longf_, param.shortf_));
            print("");
        }
    }

    if(failed) exit(1);
}

tak::argp::Handler&
tak::argp::Handler::parse() {
    std::optional<std::string> curr_flag = std::nullopt;

    for(size_t i = 0; i < chunks_.size(); i++) {
        const size_t not_hyphen = chunks_[i].find_first_not_of('-');

        if(not_hyphen == 1 || not_hyphen == 2) {
            if(!is_arg_valid(chunks_[i])) {
                handle_bad_chunk(fmt("\"{}\" is not a valid argument.", chunks_[i]), i);
            }
            if(is_arg_duplicate(chunks_[i])) {
                handle_bad_chunk(fmt("Duplicate argument, this was already passed.", chunks_[i]), i);
            }
            if(curr_flag.has_value()) {
                args_.emplace_back(Argument::create(Argument::Empty(), *curr_flag, i));
            }

            curr_flag = chunks_[i];
            continue;
        }

        if(!curr_flag.has_value()) {
            handle_bad_chunk("Argument value was passed without providing a flag.", i);
        }

        std::type_index arg_t = typeid(Argument::Empty);
        if(const auto as_bool = to_bool(chunks_[i])) {
            arg_t = typeid(bool);
            args_.emplace_back(Argument::create(*as_bool, *curr_flag, i - 1));
        }
        else if(const auto as_i64 = to_i64(chunks_[i])) {
            arg_t = typeid(int64_t);
            args_.emplace_back(Argument::create(*as_i64, *curr_flag, i - 1));
        }
        else {
            arg_t = typeid(std::string);
            args_.emplace_back(Argument::create(chunks_[i], *curr_flag, i - 1));
        }

        if(!type_matches_parameter(*curr_flag, arg_t)) {
            handle_bad_chunk(fmt("argument for flag \"{}\" does not match the expected type.", *curr_flag), i);
        }

        curr_flag = std::nullopt;
    }

    // There may be an extra "empty" argument at the end of the input stream.
    if(curr_flag.has_value()) {
        args_.emplace_back(Argument::create(Argument::Empty(), *curr_flag, chunks_.size() - 1));
    }

    check_defaults();
    check_required();
    check_predicates();
    return *this;
}


