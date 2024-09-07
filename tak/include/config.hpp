//
// Created by Diago on 2024-09-06.
//

#ifndef GLOBAL_CONFIG_HPP
#define GLOBAL_CONFIG_HPP
#include <io.hpp>
#include <cassert>

namespace tak {
    enum log_level_t : uint8_t {
        LOG_LEVEL_DISABLED,
        LOG_LEVEL_ENABLED,
        LOG_LEVEL_TRACE
    };

    enum opt_level_t : uint8_t {
        OPTIMIZATION_LEVEL_00,
        OPTIMIZATION_LEVEL_01,
        OPTIMIZATION_LEVEL_02,
    };

    enum config_flags : uint32_t {
        CONFIG_FLAGS_NONE   = 0UL,
        CONFIG_DUMP_LLVM    = 1UL,
        CONFIG_DUMP_SYMBOLS = 1UL << 1,
        CONFIG_DUMP_AST     = 1UL << 2,
        CONFIG_WARN_IS_ERR  = 1UL << 4,
        CONFIG_TIME_ACTIONS = 1UL << 5,
        CONFIG_DUMP_TYPES   = 1UL << 6,
    };

    class Config {
        ~Config() = default;
        Config()  = default;

        std::string input_;
        std::string output_;
        std::string arch_;
        opt_level_t opt_     = OPTIMIZATION_LEVEL_00;
        log_level_t log_lvl_ = LOG_LEVEL_DISABLED;
        uint32_t    flags_   = CONFIG_FLAGS_NONE;
        uint16_t    jobs_    = 1;
        bool        is_ok_   = false;
    public:
        auto input_file_name()  -> const std::string&;
        auto output_file_name() -> const std::string&;
        auto arch()             -> std::optional<std::string>;
        auto flags()   const    -> uint32_t;
        auto opt_lvl() const    -> opt_level_t;
        auto log_lvl() const    -> log_level_t;
        auto ok()      const    -> bool;

        static Config& get();
        static Config& init(
            const std::string& input,
            const std::string& output,
            const std::optional<std::string>& arch,
            opt_level_t opt       = OPTIMIZATION_LEVEL_00,
            log_level_t log_level = LOG_LEVEL_DISABLED,
            uint32_t    flags     = CONFIG_FLAGS_NONE,
            uint16_t    max_jobs  = 1
        );
    };


}

#endif //GLOBAL_CONFIG_HPP
