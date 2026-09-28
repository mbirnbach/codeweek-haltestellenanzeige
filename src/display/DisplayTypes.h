#pragma once

#include <stdint.h>

// Named colours and fonts that participants can choose in
// src/student/display_config.h. Each display backend maps them to its own
// format (RGB565 for the TFT, ANSI escape codes for the Serial monitor), so
// nobody has to deal with hex colour values.

enum DisplayColor : uint8_t {
    COLOR_BLACK,
    COLOR_WHITE,
    COLOR_GRAY,
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_BLUE,
    COLOR_PURPLE,
};

enum DisplayFont : uint8_t {
    FONT_CLASSIC,  // serif, traditional
    FONT_MODERN,   // clean sans-serif (default)
    FONT_COMPACT,  // small, fits more text
    FONT_PIXEL,    // retro, blocky
};

inline const char* displayFontName(DisplayFont font) {
    switch (font) {
        case FONT_CLASSIC: return "CLASSIC";
        case FONT_MODERN:  return "MODERN";
        case FONT_COMPACT: return "COMPACT";
        case FONT_PIXEL:   return "PIXEL";
    }
    return "?";
}
