#if DISPLAY_BACKEND_CYD || CYD_HOST_RENDER

#include "display/CYDRenderer.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Fonts/FreeSerifBold9pt7b.h>
// Adafruit's classic 5x7 font (CP437 incl. umlauts); FONT_LED draws it dot by dot.
#include <glcdfont.c>

#include <algorithm>

#include "student/config_checks.h"
#include "util/TextUtil.h"

// ============================================================================
// Layout for 320x240
// TODO(HARDWARE): final visual tuning on the assembled device (spacing, font
// sizes, readability from a distance, colours in the enclosure).
// ============================================================================
namespace layout {
constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
constexpr int MARGIN = 5;
constexpr int HEADER_H = 30;
constexpr int ROWS_TOP = HEADER_H + 2;  // header + divider line
constexpr int FOOTER_H = 24;
constexpr int COLUMN_GAP = 7;
// Rows use the full height; the footer (status bar) only takes space when
// the data is stale/offline.
constexpr int rowHeight(bool withFooter) {
    return (HEIGHT - ROWS_TOP - (withFooter ? FOOTER_H : 0)) / DEPARTURE_COUNT;
}
// Column width limits, so one odd value can't squeeze the destination away.
constexpr int MAX_LINE_W = 64;
constexpr int MAX_PLATFORM_W = 36;
constexpr int MAX_TIME_W = 120;
}  // namespace layout

namespace {

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct Rgb {
    uint8_t r, g, b;
};

// Participant colour names -> RGB.
Rgb rgbOf(DisplayColor c) {
    switch (c) {
        case COLOR_BLACK:  return {0, 0, 0};
        case COLOR_WHITE:  return {255, 255, 255};
        case COLOR_GRAY:   return {140, 140, 140};
        case COLOR_RED:    return {255, 40, 40};
        case COLOR_ORANGE: return {255, 150, 0};    // also the amber of the LED signs
        case COLOR_YELLOW: return {255, 225, 0};
        case COLOR_GREEN:  return {40, 220, 60};
        case COLOR_CYAN:   return {0, 220, 255};
        case COLOR_BLUE:   return {25, 65, 165};    // the blue of the NVV stop signs
        case COLOR_PURPLE: return {190, 80, 255};
    }
    return {255, 255, 255};
}

uint16_t toRgb565(DisplayColor c) {
    const Rgb v = rgbOf(c);
    return rgb(v.r, v.g, v.b);
}

uint16_t bg() { return toRgb565(BACKGROUND_COLOR); }
uint16_t alertColor() { return toRgb565(BACKGROUND_COLOR == COLOR_RED ? COLOR_WHITE : COLOR_RED); }

uint16_t statusColor(BoardStatus s) {
    switch (s) {
        case BoardStatus::Live: return toRgb565(TEXT_COLOR);
        case BoardStatus::Stale: return toRgb565(BACKGROUND_COLOR == COLOR_ORANGE ? COLOR_WHITE : COLOR_ORANGE);
        case BoardStatus::Offline: return alertColor();
    }
    return toRgb565(TEXT_COLOR);
}

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------

struct Font {
    const GFXfont* gfx;  // nullptr = built-in 6x8 pixel font
    uint8_t size;        // scale factor
};

enum class Role {
    Header,  // stop name + clock
    Strong,  // line number + time
    Body,    // destination + platform
    Small,   // status bar, secondary text on status screens
    Title,   // big title on status/setup screens
    Tiny,    // secondary info in the LED header (built-in 6x8 font)
};

// Few rows -> more space per row -> bigger text. Line number and time get
// big from 4 rows down, the destination only at 3 rows (it needs the width).
constexpr bool LARGE_ROWS = DEPARTURE_COUNT <= 4;
constexpr bool LARGE_BODY = DEPARTURE_COUNT <= 3;

Font fontFor(Role role) {
    if (role == Role::Tiny) return {nullptr, 1};
    switch (FONT) {
        case FONT_CLASSIC:
            switch (role) {
                case Role::Header: return {&FreeSerifBold9pt7b, 1};
                case Role::Strong: return {LARGE_ROWS ? &FreeSerifBold12pt7b : &FreeSerifBold9pt7b, 1};
                case Role::Body: return {LARGE_BODY ? &FreeSerif12pt7b : &FreeSerif9pt7b, 1};
                case Role::Small: return {&FreeSerif9pt7b, 1};
                case Role::Title: return {&FreeSerifBold12pt7b, 1};
                case Role::Tiny: break;  // handled above
            }
            break;
        case FONT_MODERN:
        case FONT_LED:  // LED look: rows are dot-matrix, everything else Sans
            switch (role) {
                case Role::Header: return {&FreeSansBold9pt7b, 1};
                case Role::Strong: return {LARGE_ROWS ? &FreeSansBold12pt7b : &FreeSansBold9pt7b, 1};
                case Role::Body: return {LARGE_BODY ? &FreeSans12pt7b : &FreeSans9pt7b, 1};
                case Role::Small: return {&FreeSans9pt7b, 1};
                case Role::Title: return {&FreeSansBold12pt7b, 1};
                case Role::Tiny: break;  // handled above
            }
            break;
        case FONT_COMPACT:
            // Always the small size: fits the most text.
            switch (role) {
                case Role::Header:
                case Role::Strong:
                case Role::Title: return {&FreeSansBold9pt7b, 1};
                case Role::Body:
                case Role::Small: return {&FreeSans9pt7b, 1};
                case Role::Tiny: break;  // handled above
            }
            break;
        case FONT_PIXEL:
            return {nullptr, 2};  // 12x16 px blocks
    }
    return {&FreeSans9pt7b, 1};
}

// Text is converted from UTF-8 to one byte per character before drawing:
//  - built-in pixel font: CP437, which contains the German umlauts
//  - GFX fonts (ASCII only): umlauts become Latin-1 bytes (0xE4 = ä ...);
//    drawText() prints the base vowel and adds the two dots itself.
// "ß" becomes "ss" in the GFX fonts, other unknown characters "?".
String encodeFor(const Font& f, const String& utf8) {
    struct Mapping {
        uint8_t utf8Second;  // second byte after 0xC3
        const char* cp437;
        const char* gfx;
    };
    static const Mapping MAP[] = {
        {0xA4, "\x84", "\xE4"}, {0xB6, "\x94", "\xF6"}, {0xBC, "\x81", "\xFC"},  // ä ö ü
        {0x84, "\x8E", "\xC4"}, {0x96, "\x99", "\xD6"}, {0x9C, "\x9A", "\xDC"},  // Ä Ö Ü
        {0x9F, "\xE1", "ss"},   {0xA9, "\x82", "e"},    {0xA8, "\x8A", "e"},     // ß é è
    };
    String out;
    for (unsigned int i = 0; i < utf8.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(utf8[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            continue;
        }
        const uint8_t n = i + 1 < utf8.length() ? static_cast<uint8_t>(utf8[i + 1]) : 0;
        const char* mapped = nullptr;
        if (c == 0xC3) {
            for (const Mapping& m : MAP) {
                if (m.utf8Second == n) mapped = f.gfx ? m.gfx : m.cp437;
            }
        } else if (c == 0xC2 && n == 0xB7) {
            mapped = f.gfx ? "-" : "\xFA";  // ·
        }
        if (mapped) {
            out += mapped;
            ++i;
        } else if ((c & 0xC0) != 0x80) {
            out += '?';  // unknown character; its continuation bytes are skipped
        }
    }
    return out;
}

// Latin-1 umlaut byte -> base vowel (GFX fonts), 0 for anything else.
char umlautBase(uint8_t c) {
    switch (c) {
        case 0xE4: return 'a';
        case 0xF6: return 'o';
        case 0xFC: return 'u';
        case 0xC4: return 'A';
        case 0xD6: return 'O';
        case 0xDC: return 'U';
    }
    return 0;
}

const GFXglyph* glyphFor(const GFXfont* font, uint8_t c) {
    if (const char base = umlautBase(c)) c = static_cast<uint8_t>(base);
    if (c < font->first || c > font->last) return nullptr;
    return &font->glyph[c - font->first];
}

// Width in pixels of one encoded character / of encoded text.
int charAdvance(const Font& f, uint8_t c) {
    if (!f.gfx) return 6 * f.size;
    const GFXglyph* g = glyphFor(f.gfx, c);
    return g ? g->xAdvance * f.size : 0;
}

int advanceWidth(const Font& f, const String& s) {
    int w = 0;
    for (unsigned int i = 0; i < s.length(); ++i) w += charAdvance(f, static_cast<uint8_t>(s[i]));
    return w;
}

// Shortens encoded text with a trailing "." until it fits maxWidth.
String fitWidth(const Font& f, const String& s, int maxWidth) {
    if (maxWidth <= 0) return String();
    if (advanceWidth(f, s) <= maxWidth) return s;
    String cut = s;
    while (cut.length() > 0 && advanceWidth(f, cut + ".") > maxWidth) cut.remove(cut.length() - 1);
    cut.trim();
    return cut.length() > 0 ? cut + "." : cut;
}

enum class Align { Left, Center, Right };

void drawEncoded(Adafruit_GFX& g, const Font& f, const String& s, int left, int midY, uint16_t color);

// Draws UTF-8 text vertically centred on midY; x is the left edge, the
// centre or the right edge depending on align. Never wider than maxWidth.
// Returns the drawn width in pixels.
int drawText(Adafruit_GFX& g, Role role, const String& utf8, int x, int midY, Align align, uint16_t color,
              int maxWidth) {
    const Font f = fontFor(role);
    const String s = fitWidth(f, encodeFor(f, utf8), maxWidth);
    if (s.length() == 0) return 0;
    const int w = advanceWidth(f, s);
    int left = x;
    if (align == Align::Center) left = x - w / 2;
    if (align == Align::Right) left = x - w;

    drawEncoded(g, f, s, left, midY, color);
    return w;
}

// Draws encoded text with its left edge at x, vertically centred on midY.
// Characters completely outside the screen width are skipped (ticker).
void drawEncoded(Adafruit_GFX& g, const Font& f, const String& s, int left, int midY, uint16_t color) {
    g.setFont(f.gfx);
    g.setTextSize(f.size);
    g.setTextColor(color);
    if (f.gfx) {
        // GFX fonts: cursor = baseline. Centre the cap height ('H') on midY.
        const GFXglyph& h = f.gfx->glyph['H' - f.gfx->first];
        g.setCursor(left, midY - (h.yOffset * f.size) / 2);
    } else {
        // Built-in font: cursor = top-left, glyphs are 7 px high.
        g.setCursor(left, midY - (7 * f.size) / 2);
    }
    const int cursorY = g.getCursorY();
    // Character by character: skip what's off-screen, add umlaut dots.
    const int dot = f.gfx && f.gfx->yAdvance >= 28 ? 3 : 2;  // bigger dots for the 12pt fonts
    int x = left;
    for (unsigned int i = 0; i < s.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        const int advance = charAdvance(f, c);
        if (x >= layout::WIDTH) break;
        if (x + advance < 0) {
            x += advance;
            continue;
        }
        g.setCursor(x, cursorY);
        x += advance;
        const char base = f.gfx ? umlautBase(c) : 0;
        if (!base) {
            g.write(c);
            continue;
        }
        const GFXglyph* gl = glyphFor(f.gfx, c);
        const int x0 = g.getCursorX() + gl->xOffset;
        const int top = g.getCursorY() + gl->yOffset;  // top of the vowel
        g.write(static_cast<uint8_t>(base));
        const int y = top - dot - 2;
        g.fillRect(x0 + gl->width / 4 - dot / 2, y, dot, dot, color);
        g.fillRect(x0 + (gl->width * 3) / 4 - dot / 2 + 1, y, dot, dot, color);
    }
}

int textWidth(Role role, const String& utf8) {
    const Font f = fontFor(role);
    return advanceWidth(f, encodeFor(f, utf8));
}

// Distance between two text lines of a role.
int lineHeight(Role role) {
    const Font f = fontFor(role);
    return f.gfx ? (f.gfx->yAdvance * f.size * 4) / 5 : 9 * f.size;
}

// Splits UTF-8 text at a space so that the first part fits maxWidth.
// Returns false if there is no such space (text is then truncated instead).
bool splitTwoLines(Role role, const String& text, int maxWidth, String& first, String& second) {
    int split = -1;
    for (unsigned int i = 1; i < text.length(); ++i) {
        if (text[i] == ' ' && textWidth(role, text.substring(0, i)) <= maxWidth) split = static_cast<int>(i);
    }
    if (split < 0) return false;
    first = text.substring(0, split);
    second = text.substring(split + 1);
    second.trim();
    return true;
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

// ============================================================================
// Initialisation (panel already initialised by the caller)
// ============================================================================

CYDRenderer::CYDRenderer(Adafruit_GFX& panel) : tft_(&panel) {}

void CYDRenderer::begin() {
    tft_->setTextWrap(false);
    tft_->cp437(true);
    tft_->fillScreen(bg());

    // Off-screen buffer for one horizontal band (tallest of header, row, footer).
    canvasHeight_ = std::max(std::max(layout::ROWS_TOP, layout::FOOTER_H), std::max(layout::rowHeight(false), 64));
    canvas_ = new GFXcanvas16(layout::WIDTH, canvasHeight_);
    if (canvas_->getBuffer() == nullptr) {  // not enough RAM: draw directly (may flicker)
        delete canvas_;
        canvas_ = nullptr;
    } else {
        canvas_->setTextWrap(false);
        canvas_->cp437(true);
    }
}

template <typename Draw>
void CYDRenderer::band(int y, int height, uint16_t fill, Draw draw) {
    if (!canvas_) {
        tft_->fillRect(0, y, layout::WIDTH, height, fill);
        draw(*tft_, 0);
        return;
    }
    // Draw in canvas-sized slices; the canvas clips everything outside.
    for (int top = y; top < y + height; top += canvasHeight_) {
        const int h = std::min(canvasHeight_, y + height - top);
        canvas_->fillScreen(fill);
        draw(*canvas_, -top);
        tft_->drawRGBBitmap(0, top, canvas_->getBuffer(), layout::WIDTH, h);
    }
}

// ============================================================================
// Full-screen screens (drawn directly, they change rarely)
// ============================================================================

void CYDRenderer::showBoot(const char* firmwareVersion) {
    using namespace layout;
    boardOnScreen_ = false;
    tft_->fillScreen(bg());
    drawText(*tft_, Role::Title, "Haltestellenanzeige", WIDTH / 2, 100, Align::Center, toRgb565(HEADER_COLOR),
             WIDTH - 2 * MARGIN);
    drawText(*tft_, Role::Small, String("CodeWeek Kassel · v") + firmwareVersion, WIDTH / 2, 140, Align::Center,
             toRgb565(TEXT_COLOR), WIDTH - 2 * MARGIN);
}

void CYDRenderer::showSetup(const SetupScreen& setup) {
    using namespace layout;
    boardOnScreen_ = false;
    const int w = WIDTH - 2 * MARGIN;
    tft_->fillScreen(bg());
    drawText(*tft_, Role::Title, "Einrichtung", WIDTH / 2, 22, Align::Center, toRgb565(HEADER_COLOR), w);
    tft_->drawFastHLine(0, 44, WIDTH, toRgb565(TEXT_COLOR));
    drawText(*tft_, Role::Small, "1. Mit dem Handy ins WLAN:", MARGIN, 68, Align::Left, toRgb565(TEXT_COLOR), w);
    drawText(*tft_, Role::Strong, setup.apName, WIDTH / 2, 96, Align::Center, toRgb565(TIME_COLOR), w);
    drawText(*tft_, Role::Small, "2. Im Browser öffnen:", MARGIN, 132, Align::Left, toRgb565(TEXT_COLOR), w);
    drawText(*tft_, Role::Strong, setup.url, WIDTH / 2, 160, Align::Center, toRgb565(TIME_COLOR), w);
    tft_->drawFastHLine(0, HEIGHT - 44, WIDTH, toRgb565(TEXT_COLOR));
    drawText(*tft_, Role::Small, setup.step, WIDTH / 2, HEIGHT - 22, Align::Center, toRgb565(LINE_COLOR), w);
}

void CYDRenderer::showStatus(const StatusScreen& status) {
    using namespace layout;
    boardOnScreen_ = false;
    const int w = WIDTH - 2 * MARGIN;
    tft_->fillScreen(bg());
    drawText(*tft_, Role::Title, status.title, WIDTH / 2, 80, Align::Center,
             status.isError ? alertColor() : toRgb565(HEADER_COLOR), w);
    drawText(*tft_, Role::Small, status.detail, WIDTH / 2, 124, Align::Center, toRgb565(TEXT_COLOR), w);
    drawText(*tft_, Role::Small, status.hint, WIDTH / 2, 170, Align::Center, toRgb565(TIME_COLOR), w);
}

// ============================================================================
// Departure board
// ============================================================================

Adafruit_GFX& CYDRenderer::beginBand(int height) {
    if (canvas_ && height <= canvasHeight_) {
        canvas_->fillScreen(bg());
        return *canvas_;
    }
    return *tft_;
}

void CYDRenderer::endBand(int y, int height) {
    if (canvas_ && height <= canvasHeight_) {
        tft_->drawRGBBitmap(0, y, canvas_->getBuffer(), layout::WIDTH, height);
    }
}

CYDRenderer::Columns CYDRenderer::measureColumns(const DisplayModel& m) {
    // Widths from the actual content (within limits), so the columns line up
    // and nothing overlaps.
    Columns c;
    c.lineW = textWidth(Role::Strong, "88");
    c.timeW = textWidth(Role::Strong, "9 min");
    c.platformW = 0;
    for (int i = 0; i < m.rowCount; ++i) {
        c.lineW = clampInt(textWidth(Role::Strong, m.rows[i].line), c.lineW, layout::MAX_LINE_W);
        c.timeW = clampInt(textWidth(Role::Strong, m.rows[i].time), c.timeW, layout::MAX_TIME_W);
        if (m.showPlatform) {
            c.platformW = clampInt(textWidth(Role::Body, m.rows[i].platform), c.platformW, layout::MAX_PLATFORM_W);
        }
    }
    return c;
}

void CYDRenderer::showDepartures(const DisplayModel& m) {
    if (FONT == FONT_LED) {
        showLedBoard(m);
        return;
    }
    const bool hasFooter = m.statusText.length() > 0;
    const Columns cols = measureColumns(m);
    const Columns lastCols = measureColumns(last_);
    // Row height changes with the footer, column positions with the content:
    // then everything below the header is redrawn.
    const bool full = !boardOnScreen_ || hasFooter != (last_.statusText.length() > 0);
    const bool relayout = full || cols.lineW != lastCols.lineW || cols.timeW != lastCols.timeW ||
                          cols.platformW != lastCols.platformW || m.showPlatform != last_.showPlatform;

    if (full) tft_->fillScreen(bg());
    if (full || m.stopName != last_.stopName || m.clock != last_.clock) drawHeader(m);

    for (int i = 0; i < DEPARTURE_COUNT; ++i) {
        if (i == tickerSlot(m)) {
            if (full || m.ticker != last_.ticker || i != tickerSlot(last_)) startTicker(m);
            continue;
        }
        const bool changed = relayout || (i < m.rowCount) != (i < last_.rowCount) ||
                             (i < m.rowCount && m.rows[i] != last_.rows[i]) || i == tickerSlot(last_);
        if (changed) drawRow(m, i, cols);
    }

    if (hasFooter && (full || m.statusText != last_.statusText || m.status != last_.status)) drawFooter(m);

    last_ = m;
    boardOnScreen_ = true;
}

void CYDRenderer::drawHeader(const DisplayModel& m) {
    using namespace layout;
    Adafruit_GFX& g = beginBand(ROWS_TOP);  // header starts at y = 0, same coordinates either way
    if (!canvas_) tft_->fillRect(0, 0, WIDTH, ROWS_TOP, bg());

    int clockW = 0;
    if (m.clock.length() > 0) {
        clockW = textWidth(Role::Header, m.clock);
        drawText(g, Role::Header, m.clock, WIDTH - MARGIN, HEADER_H / 2, Align::Right, toRgb565(TEXT_COLOR),
                 WIDTH);
    }
    const int nameW = WIDTH - 2 * MARGIN - (clockW > 0 ? clockW + COLUMN_GAP : 0);
    drawText(g, Role::Header, m.stopName, MARGIN, HEADER_H / 2, Align::Left, toRgb565(HEADER_COLOR), nameW);
    g.drawFastHLine(0, HEADER_H, WIDTH, toRgb565(TEXT_COLOR));
    endBand(0, ROWS_TOP);
}

void CYDRenderer::drawRow(const DisplayModel& m, int index, const Columns& cols) {
    using namespace layout;
    const int rowH = rowHeight(m.statusText.length() > 0);
    const int y = ROWS_TOP + index * rowH;

    Adafruit_GFX& g = beginBand(rowH);
    const int oy = canvas_ ? 0 : y;  // canvas: band-local coordinates
    if (!canvas_) tft_->fillRect(0, y, WIDTH, rowH, bg());
    const int midY = oy + rowH / 2;

    if (index < m.rowCount) {
        const DepartureRow& r = m.rows[index];

        // Line number left, time right (bold).
        drawText(g, Role::Strong, r.line, MARGIN, midY, Align::Left, toRgb565(LINE_COLOR), cols.lineW);
        drawText(g, Role::Strong, r.time, WIDTH - MARGIN, midY, Align::Right,
                 r.cancelled ? alertColor() : toRgb565(TIME_COLOR), cols.timeW);

        // Platform, then destination in the space that is left.
        int right = WIDTH - MARGIN - cols.timeW - COLUMN_GAP;
        if (m.showPlatform && cols.platformW > 0) {
            drawText(g, Role::Body, r.platform, right, midY, Align::Right, toRgb565(TEXT_COLOR), cols.platformW);
            right -= cols.platformW + COLUMN_GAP;
        }
        const int left = MARGIN + cols.lineW + COLUMN_GAP;
        const int destW = right - left;
        const int lh = lineHeight(Role::Body);
        String first, second;
        // Tall rows (few departures): long destinations may use two lines.
        // Cancelled ones stay on one line so the strike-through is unambiguous.
        if (!r.cancelled && rowH >= 2 * lh + 6 && textWidth(Role::Body, r.destination) > destW &&
            splitTwoLines(Role::Body, r.destination, destW, first, second)) {
            drawText(g, Role::Body, first, left, midY - lh / 2, Align::Left, toRgb565(TEXT_COLOR), destW);
            drawText(g, Role::Body, second, left, midY + lh / 2, Align::Left, toRgb565(TEXT_COLOR), destW);
        } else {
            const int drawn =
                drawText(g, Role::Body, r.destination, left, midY, Align::Left, toRgb565(TEXT_COLOR), destW);
            if (r.cancelled) {
                // Strike through the destination: clearly not a normal departure.
                g.drawFastHLine(left, midY, drawn, alertColor());
                g.drawFastHLine(left, midY + 1, drawn, alertColor());
            }
        }
    } else if (index == 0) {
        drawText(g, Role::Body, "Keine Abfahrten in nächster Zeit", WIDTH / 2, midY, Align::Center,
                 toRgb565(TEXT_COLOR), WIDTH - 2 * MARGIN);
    }
    endBand(y, rowH);
}

void CYDRenderer::drawFooter(const DisplayModel& m) {
    using namespace layout;
    const int y = HEIGHT - FOOTER_H;
    Adafruit_GFX& g = beginBand(FOOTER_H);
    const int oy = canvas_ ? 0 : y;
    if (!canvas_) tft_->fillRect(0, y, WIDTH, FOOTER_H, bg());
    g.drawFastHLine(0, oy, WIDTH, toRgb565(TEXT_COLOR));
    drawText(g, Role::Small, m.statusText, MARGIN, oy + FOOTER_H / 2 + 1, Align::Left, statusColor(m.status),
             WIDTH - 2 * MARGIN);
    endBand(y, FOOTER_H);
}


// ============================================================================
// FONT_LED: looks like the departure signs at NVV stops – a blue panel with
// the stop name, below it an amber LED dot-matrix board.
// ============================================================================
namespace {
namespace led {
// Dot pitch: bigger dots when there are few rows.
// The blue header: stop name, clock, status.
constexpr int HEADER_H = 44;
constexpr int PANEL_Y = HEADER_H;
// LED grid in pixels: narrower than tall, like the condensed characters of
// the real signs, so ~26 characters fit on a row. Few rows -> taller LEDs.
constexpr int PITCH_X = 2;
constexpr int PITCH_Y = DEPARTURE_COUNT <= 4 ? 4 : 3;
constexpr int DOT_W = 2;            // lit area of one LED; horizontally the LEDs touch,
constexpr int DOT_H = PITCH_Y - 1;  // vertically the dark gap shows the LED rows
constexpr int X0 = 4;
constexpr int DOTS_X = (layout::WIDTH - 2 * X0) / PITCH_X;  // 156 LEDs per row
// The board fills everything below the header; rows share it evenly.
constexpr int PANEL_DOTS = (layout::HEIGHT - PANEL_Y) / PITCH_Y;
constexpr int ROW_DOTS = (PANEL_DOTS - 2) / DEPARTURE_COUNT;  // 10..15
static_assert(ROW_DOTS >= 10, "LED rows need 8 dots of text plus spacing");
constexpr int TOP_DOTS = (PANEL_DOTS - DEPARTURE_COUNT * ROW_DOTS) / 2;
constexpr int TEXT_DOTS = (ROW_DOTS - 8 + 1) / 2;  // dark LEDs above the text
constexpr int ROW_H = ROW_DOTS * PITCH_Y;
constexpr int GAP = 4;                             // LEDs between columns
constexpr int MAX_LINE = 20;
constexpr int MAX_PLATFORM = 10;
constexpr int MAX_TIME = 48;

constexpr int rowY(int index) { return PANEL_Y + (TOP_DOTS + index * ROW_DOTS) * PITCH_Y; }

uint16_t panelColor() { return rgb(10, 10, 10); }

// Unlit LEDs: a faint version of the text colour, like a real sign.
uint16_t unlitColor() {
    const Rgb c = rgbOf(TEXT_COLOR);
    return rgb(c.r / 8 + 16, c.g / 8 + 16, c.b / 8 + 16);
}

struct Glyph {
    uint8_t firstColumn;
    uint8_t width;
};

// Proportional width from the 5x7 font, like the narrow "i"/"l" on real
// signs. Digits keep full width so minutes line up.
Glyph glyph(uint8_t c) {
    if (c == ' ') return {0, 3};
    if (c >= '0' && c <= '9') return {0, 5};
    const unsigned char* cols = &font[c * 5];
    int first = 0, last = 4;
    while (first < 5 && cols[first] == 0) ++first;
    if (first == 5) return {0, 2};
    while (cols[last] == 0) --last;
    return {static_cast<uint8_t>(first), static_cast<uint8_t>(last - first + 1)};
}

String encode(const String& utf8) { return encodeFor(Font{nullptr, 1}, utf8); }

int width(const String& s) {
    int w = 0;
    for (unsigned int i = 0; i < s.length(); ++i) w += glyph(static_cast<uint8_t>(s[i])).width + 1;
    return w > 0 ? w - 1 : 0;
}

// Cuts with a trailing "." like the real signs ("Rathaus/Fünff.").
String fit(const String& s, int maxDots) {
    if (width(s) <= maxDots) return s;
    String cut = s;
    while (cut.length() > 0 && width(cut + ".") > maxDots) cut.remove(cut.length() - 1);
    cut.trim();
    return cut.length() > 0 ? cut + "." : cut;
}

// All LEDs of `rows` dot rows, switched off.
void grid(Adafruit_GFX& g, int top, int rows) {
    const uint16_t c = unlitColor();
    for (int ry = 0; ry < rows; ++ry) {
        for (int cx = 0; cx < DOTS_X; ++cx) g.fillRect(X0 + cx * PITCH_X, top + ry * PITCH_Y, DOT_W, DOT_H, c);
    }
}

// Switches on the LEDs for already encoded text, starting at LED column `col`
// (may be negative or beyond the board when scrolling: those LEDs are skipped).
void text(Adafruit_GFX& g, const String& s, int col, int top, uint16_t color) {
    for (unsigned int i = 0; i < s.length() && col < DOTS_X; ++i) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        const Glyph gl = glyph(c);
        for (int cx = 0; cx < gl.width; ++cx) {
            if (col + cx < 0 || col + cx >= DOTS_X) continue;
            const uint8_t bits = font[c * 5 + gl.firstColumn + cx];
            for (int ry = 0; ry < 8; ++ry) {
                if (bits & (1 << ry)) g.fillRect(X0 + (col + cx) * PITCH_X, top + ry * PITCH_Y, DOT_W, DOT_H, color);
            }
        }
        col += gl.width + 1;
    }
}

// "Kassel Kirchweg" -> "Kirchweg": the signs only show the stop itself.
String shortStopName(const String& name) {
    const String prefix = "Kassel ";
    if (name.startsWith(prefix) && name.length() > prefix.length()) return name.substring(prefix.length());
    return name;
}

}  // namespace led
}  // namespace


CYDRenderer::Columns CYDRenderer::measureLedColumns(const DisplayModel& m) {
    Columns c;
    c.lineW = led::width("88");
    c.timeW = led::width("9 min");
    c.platformW = 0;
    for (int i = 0; i < m.rowCount; ++i) {
        c.lineW = clampInt(led::width(led::encode(m.rows[i].line)), c.lineW, led::MAX_LINE);
        c.timeW = clampInt(led::width(led::encode(m.rows[i].time)), c.timeW, led::MAX_TIME);
        if (m.showPlatform) {
            c.platformW = clampInt(led::width(led::encode(m.rows[i].platform)), c.platformW, led::MAX_PLATFORM);
        }
    }
    return c;
}

void CYDRenderer::showLedBoard(const DisplayModel& m) {
    const Columns cols = measureLedColumns(m);
    const Columns lastCols = measureLedColumns(last_);
    const bool full = !boardOnScreen_;
    const bool relayout = full || cols.lineW != lastCols.lineW || cols.timeW != lastCols.timeW ||
                          cols.platformW != lastCols.platformW || m.showPlatform != last_.showPlatform;

    if (full) {
        // The whole LED board, all LEDs off (rows are drawn on top).
        band(led::PANEL_Y, layout::HEIGHT - led::PANEL_Y, led::panelColor(),
             [](Adafruit_GFX& g, int dy) { led::grid(g, led::PANEL_Y + dy, led::PANEL_DOTS); });
    }
    if (full || m.stopName != last_.stopName || m.clock != last_.clock || m.statusText != last_.statusText) {
        drawLedHeader(m);
    }
    for (int i = 0; i < DEPARTURE_COUNT; ++i) {
        if (i == tickerSlot(m)) {
            if (full || m.ticker != last_.ticker || i != tickerSlot(last_)) startTicker(m);
            continue;
        }
        const bool changed = relayout || (i < m.rowCount) != (i < last_.rowCount) ||
                             (i < m.rowCount && m.rows[i] != last_.rows[i]) || i == tickerSlot(last_);
        if (changed) drawLedRow(m, i, cols);
    }
    last_ = m;
    boardOnScreen_ = true;
}

void CYDRenderer::drawLedHeader(const DisplayModel& m) {
    using namespace layout;
    band(0, led::HEADER_H, bg(), [&](Adafruit_GFX& g, int dy) {
        const int mid = led::HEADER_H / 2 + dy;
        const bool hasStatus = m.statusText.length() > 0;
        // Right side: clock, and below it the status when data is stale/offline.
        int rightW = 0;
        if (m.clock.length() > 0) {
            rightW = textWidth(Role::Small, m.clock);
            drawText(g, Role::Small, m.clock, WIDTH - MARGIN - 2, hasStatus ? mid - 9 : mid, Align::Right,
                     toRgb565(HEADER_COLOR), WIDTH);
        }
        if (hasStatus) {
            const int w = std::min(textWidth(Role::Tiny, m.statusText), WIDTH / 2);
            rightW = std::max(rightW, w);
            drawText(g, Role::Tiny, m.statusText, WIDTH - MARGIN - 2, m.clock.length() > 0 ? mid + 10 : mid,
                     Align::Right, statusColor(m.status), w);
        }
        const int nameW = WIDTH - 2 * MARGIN - 4 - (rightW > 0 ? rightW + COLUMN_GAP : 0);
        drawText(g, Role::Title, led::shortStopName(m.stopName), MARGIN + 4, mid, Align::Left,
                 toRgb565(HEADER_COLOR), nameW);
    });
}

void CYDRenderer::drawLedRow(const DisplayModel& m, int index, const Columns& cols) {
    const int y = led::rowY(index);
    band(y, led::ROW_H, led::panelColor(), [&](Adafruit_GFX& g, int dy) {
        const int top = y + dy;
        led::grid(g, top, led::ROW_DOTS);
        const int textTop = top + led::TEXT_DOTS * led::PITCH_Y;

        if (index >= m.rowCount) {
            if (index == 0) {
                led::text(g, led::fit(led::encode("Keine Abfahrten"), led::DOTS_X), 0, textTop,
                          toRgb565(TEXT_COLOR));
            }
            return;
        }
        const DepartureRow& r = m.rows[index];

        led::text(g, led::fit(led::encode(r.line), cols.lineW), 0, textTop, toRgb565(LINE_COLOR));

        const String time = led::fit(led::encode(r.time), cols.timeW);
        led::text(g, time, led::DOTS_X - led::width(time), textTop, r.cancelled ? alertColor() : toRgb565(TIME_COLOR));

        int right = led::DOTS_X - cols.timeW - led::GAP;
        if (m.showPlatform && cols.platformW > 0) {
            const String platform = led::fit(led::encode(r.platform), cols.platformW);
            led::text(g, platform, right - led::width(platform), textTop, toRgb565(TEXT_COLOR));
            right -= cols.platformW + led::GAP;
        }
        const int left = cols.lineW + led::GAP;
        led::text(g, led::fit(led::encode(r.destination), right - left), left, textTop, toRgb565(TEXT_COLOR));
    });
}


// ============================================================================
// Ticker: info/disruption messages scroll through the last row
// ============================================================================

int CYDRenderer::tickerSlot(const DisplayModel& m) const {
    return m.ticker.length() > 0 ? DEPARTURE_COUNT - 1 : -1;
}

void CYDRenderer::startTicker(const DisplayModel& m) {
    tickerOffset_ = 0;
    if (FONT == FONT_LED) {
        tickerText_ = led::encode(m.ticker);
        tickerWidth_ = led::width(tickerText_) * led::PITCH_X;
    } else {
        const Font f = fontFor(Role::Small);
        tickerText_ = encodeFor(f, m.ticker);
        tickerWidth_ = advanceWidth(f, tickerText_);
    }
    drawTicker(m);
}

void CYDRenderer::scrollTicker() {
    if (!boardOnScreen_ || tickerSlot(last_) < 0) return;
    tickerOffset_ += TICKER_STEP_PX;
    // Scrolled out completely on the left: start again from the right.
    if (tickerOffset_ > layout::WIDTH + tickerWidth_) tickerOffset_ = 0;
    drawTicker(last_);
}

void CYDRenderer::drawTicker(const DisplayModel& m) {
    const int slot = tickerSlot(m);
    if (FONT == FONT_LED) {
        const int y = led::rowY(slot);
        band(y, led::ROW_H, led::panelColor(), [&](Adafruit_GFX& g, int dy) {
            led::grid(g, y + dy, led::ROW_DOTS);
            const int col = led::DOTS_X - tickerOffset_ / led::PITCH_X;
            led::text(g, tickerText_, col, y + dy + led::TEXT_DOTS * led::PITCH_Y, toRgb565(TEXT_COLOR));
        });
        return;
    }
    const int rowH = layout::rowHeight(m.statusText.length() > 0);
    const int y = layout::ROWS_TOP + slot * rowH;
    band(y, rowH, bg(), [&](Adafruit_GFX& g, int dy) {
        drawEncoded(g, fontFor(Role::Small), tickerText_, layout::WIDTH - tickerOffset_, y + dy + rowH / 2,
                    toRgb565(TEXT_COLOR));
    });
}

#endif  // DISPLAY_BACKEND_CYD || CYD_HOST_RENDER
