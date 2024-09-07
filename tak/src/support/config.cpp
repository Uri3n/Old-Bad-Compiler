//
// Created by Diago on 2024-09-06.
//

#include <config.hpp>

tak::Config&
tak::Config::get() {
    static Config cfg;
    return cfg;
}

tak::Config&
tak::Config::init(
    const std::string& input,
    const std::string& output,
    const std::optional<std::string>& arch,
    const opt_level_t opt,
    const log_level_t log_level,
    const uint32_t flags,
    const uint16_t max_jobs
) {
    auto& cfg = get();
    assert(!input.empty()  && "no input file provided.");
    assert(!output.empty() && "no output file provided.");
    assert(!cfg.ok()       && "config has already been initialized.");

    cfg.input_   = input;
    cfg.output_  = output;
    cfg.opt_     = opt;
    cfg.log_lvl_ = log_level;
    cfg.flags_   = flags;
    cfg.jobs_    = max_jobs;
    cfg.is_ok_   = true;
    cfg.arch_    = arch.value_or("");
    return cfg;
}

const std::string&
tak::Config::input_file_name() {
    assert(is_ok_);
    return input_;
}

const std::string&
tak::Config::output_file_name() {
    assert(is_ok_);
    return output_;
}

std::optional<std::string>
tak::Config::arch() {
    if(arch_.empty()) {
        return std::nullopt;
    }
    return arch_;
}

uint32_t
tak::Config::flags() const {
    assert(is_ok_);
    return flags_;
}

tak::opt_level_t
tak::Config::opt_lvl() const {
    assert(is_ok_);
    return opt_;
}

tak::log_level_t
tak::Config::log_lvl() const {
    assert(is_ok_);
    return log_lvl_;
}

bool tak::Config::ok() const {
    return is_ok_;
}