//
// Created by Diago on 2024-08-04.
//

#include <io.hpp>
#ifdef TAK_WINDOWS
#include <Windows.h>
static bool win_virtual_sequences_enabled = false;

void
tak::try_enable_windows_virtual_terminal_sequences() {

    win_virtual_sequences_enabled = true;
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if(h_out == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD mode = 0;
    if(!GetConsoleMode(h_out, &mode)) {
        return;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_out, mode);
}
#endif

void
tak::term_set_fg(const termcolor_fg_t foreground) {
#ifdef TAK_WINDOWS
    if(!win_virtual_sequences_enabled) {
        try_enable_windows_virtual_terminal_sequences();
    }
#endif

    if(foreground == TFG_NONE) {
        return;
    }
    std::cout << fmt("\x1b[{}m", std::to_string(foreground));
}

void
tak::term_set_bg(const termcolor_bg_t background) {
#ifdef TAK_WINDOWS
    if(!win_virtual_sequences_enabled) {
        try_enable_windows_virtual_terminal_sequences();
    }
#endif

    if(background == TBG_NONE) {
        return;
    }
    std::cout << fmt("\x1b[{}m", std::to_string(background));
}

void
tak::term_set_style(const uint16_t style_mask) {
#ifdef TAK_WINDOWS
    if(!win_virtual_sequences_enabled) {
        try_enable_windows_virtual_terminal_sequences();
    }
#endif

    if(style_mask == TSTYLE_NONE) {
        return;
    }

    if(style_mask & TSTYLE_BOLD)      std::cout << "\x1b[1m";
    if(style_mask & TSTYLE_ITALIC)    std::cout << "\x1b[3m";
    if(style_mask & TSTYLE_UNDERLINE) std::cout << "\x1b[4m";
}

void
tak::term_reset() {
#ifdef TAK_WINDOWS
    if(!win_virtual_sequences_enabled) {
        return;
    }
#endif
    std::cout << "\x1b[m";
}


