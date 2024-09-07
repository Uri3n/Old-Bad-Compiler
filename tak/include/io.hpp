//
// Created by Diago on 2024-07-02.
//

#ifndef IO_HPP
#define IO_HPP
#include <string>
#include <format>
#include <iostream>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    enum termcolor_fg_t : uint16_t {
        TFG_NONE    = 0,
        TFG_BLACK   = 30,
        TFG_RED     = 91, // bright red
        TFG_GREEN   = 32,
        TFG_YELLOW  = 33,
        TFG_BLUE    = 34,
        TFG_MAGENTA = 35,
        TFG_CYAN    = 36,
        TFG_WHITE   = 37,
    };

    enum termcolor_bg_t : uint16_t {
        TBG_NONE    = 0,
        TBG_BLACK   = 40,
        TBG_RED     = 41,
        TBG_GREEN   = 42,
        TBG_YELLOW  = 43,
        TBG_BLUE    = 44,
        TBG_MAGENTA = 45,
        TBG_CYAN    = 46,
        TBG_WHITE   = 47,
    };

    enum term_style_t : uint16_t {
        TSTYLE_NONE      = 0,
        TSTYLE_BOLD      = 1,
        TSTYLE_ITALIC    = 1 << 1, // Not widely supported on most OS's.
        TSTYLE_UNDERLINE = 1 << 2,
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void try_enable_windows_virtual_terminal_sequences(); // Enables Windows virtual terminal escape sequences
    void term_set_fg(termcolor_fg_t foreground);          // Sets the terminal foreground color.
    void term_set_bg(termcolor_bg_t background);          // Sets the terminal background color.
    void term_set_style(uint16_t style_mask);             // Sets the terminal text style (underline, bold, etc).
    void term_reset();                                    // Resets any escape sequences applied.

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

    template<termcolor_fg_t fg = TFG_NONE, termcolor_bg_t bg = TBG_NONE, uint16_t s = TSTYLE_NONE, typename ... Args>
    static void print(const std::format_string<Args...> fmt, Args... args) {
        std::string output;
        bool term_changed = false;

        try {
            output = std::vformat(fmt.get(), std::make_format_args(args...));
        } catch(const std::format_error& e) {
            output = std::string("FORMAT ERROR: ") + e.what();
        } catch(...) {
            output = "UNKNOWN FORMATTING ERROR";
        }

        if(fg != TFG_NONE)   { term_set_fg(fg);   term_changed = true; }
        if(bg != TBG_NONE)   { term_set_bg(bg);   term_changed = true; }
        if(s != TSTYLE_NONE) { term_set_style(s); term_changed = true; }

        std::cout << output << std::endl;

        if(term_changed) term_reset();
    }

    template<typename ... Args>
    static void underline(const std::format_string<Args...> fmt, Args... args) {
        tak::print<TFG_NONE, TBG_NONE, TSTYLE_UNDERLINE>(fmt, args...);
    }

    template<typename ... Args>
    static void bold(const std::format_string<Args...> fmt, Args... args) {
        tak::print<TFG_NONE, TBG_NONE, TSTYLE_BOLD>(fmt, args...);
    }

    template<typename ... Args>
    static void bold_underline(const std::format_string<Args...> fmt, Args... args) {
        tak::print<TFG_NONE, TBG_NONE, TSTYLE_BOLD | TSTYLE_UNDERLINE>(fmt, args...);
    }

    template<typename ... Args>
    static void red_bold(const std::format_string<Args...> fmt, Args... args) {
        tak::print<TFG_RED, TBG_NONE, TSTYLE_BOLD>(fmt, args...);
    }
}
#endif //IO_HPP
