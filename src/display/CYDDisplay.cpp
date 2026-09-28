#if DISPLAY_BACKEND_CYD

#include "display/CYDDisplay.h"

#include "student/config_checks.h"
#include "util/TextUtil.h"

// ============================================================================
// Driver initialisation (hardware specific)
// Pins and panel driver come from the build flags in platformio.ini [env:cyd].
// ============================================================================

// TODO(HARDWARE): verify CYD panel controller (ILI9341 vs. ST7789 variants)
// TODO(HARDWARE): verify rotation (1 or 3, depending on USB port position in the enclosure)
// TODO(HARDWARE): verify backlight pin (GPIO21 on ESP32-2432S028R) and whether PWM dimming is wanted
// TODO(HARDWARE): verify colour order / inversion (some CYD batches need invertDisplay(true) or BGR)
constexpr uint8_t ROTATION = 1;
constexpr bool INVERT_COLORS = false;

// ============================================================================
// Layout (hardware independent, pixel values for 320x240 landscape)
// Tune these once the real screen can be looked at.
// ============================================================================
namespace layout {
constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
constexpr int MARGIN = 4;
constexpr int HEADER_H = 30;
constexpr int FOOTER_H = 24;
constexpr int ROWS_TOP = HEADER_H + 2;
// Rows use the full height; the footer (status bar) only takes space when
// the data is stale/offline.
constexpr int rowHeight(bool withFooter) {
    return (HEIGHT - ROWS_TOP - 2 - (withFooter ? FOOTER_H : 0)) / DEPARTURE_COUNT;
}
constexpr int COLUMN_GAP = 6;
}  // namespace layout

namespace {

// Participant colour names -> RGB565.
uint16_t rgb565(DisplayColor c) {
    switch (c) {
        case COLOR_BLACK:  return TFT_BLACK;
        case COLOR_WHITE:  return TFT_WHITE;
        case COLOR_GRAY:   return TFT_DARKGREY;
        case COLOR_RED:    return TFT_RED;
        case COLOR_ORANGE: return TFT_ORANGE;
        case COLOR_YELLOW: return TFT_YELLOW;
        case COLOR_GREEN:  return TFT_GREEN;
        case COLOR_CYAN:   return TFT_CYAN;
        case COLOR_BLUE:   return TFT_BLUE;
        case COLOR_PURPLE: return TFT_MAGENTA;
    }
    return TFT_WHITE;
}

uint16_t alertColor() { return BACKGROUND_COLOR == COLOR_RED ? TFT_WHITE : TFT_RED; }

uint16_t statusColor(BoardStatus s) {
    switch (s) {
        case BoardStatus::Live: return BACKGROUND_COLOR == COLOR_GREEN ? TFT_WHITE : TFT_GREEN;
        case BoardStatus::Stale: return BACKGROUND_COLOR == COLOR_ORANGE ? TFT_WHITE : TFT_ORANGE;
        case BoardStatus::Offline: return alertColor();
    }
    return rgb565(TEXT_COLOR);
}

}  // namespace

void CYDDisplay::begin() {
    tft_.init();
    tft_.setRotation(ROTATION);
    tft_.invertDisplay(INVERT_COLORS);
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
    tft_.setTextWrap(false);
    fillBackground();
}

void CYDDisplay::loop() {}

bool CYDDisplay::setupGestureDetected() {
    // TODO(HARDWARE): verify touchscreen controller (XPT2046 on its own SPI bus on the CYD)
    // TODO(HARDWARE): verify touch calibration
    // Planned gesture: touch and hold the header area for 3 seconds.
    return false;
}

// Fonts: all of them only contain ASCII glyphs, so text goes through
// text::toAscii() ("Königsplatz" -> "Koenigsplatz").
// TODO(HARDWARE): check font sizes/readability on the real panel; consider
// smooth fonts with umlauts (TFT_eSPI .vlw) if time allows.
void CYDDisplay::useFont(bool bold) {
    tft_.setTextSize(1);
    switch (FONT) {
        case FONT_CLASSIC:
            tft_.setFreeFont(bold ? &FreeSerifBold9pt7b : &FreeSerif9pt7b);
            break;
        case FONT_MODERN:
            tft_.setFreeFont(bold ? &FreeSansBold9pt7b : &FreeSans9pt7b);
            break;
        case FONT_COMPACT:
            tft_.setTextFont(2);
            break;
        case FONT_PIXEL:
            tft_.setTextFont(1);
            tft_.setTextSize(2);
            break;
    }
}

void CYDDisplay::fillBackground() { tft_.fillScreen(rgb565(BACKGROUND_COLOR)); }

String CYDDisplay::fit(const String& text, int maxWidth) {
    if (maxWidth <= 0) return String();
    if (tft_.textWidth(text) <= maxWidth) return text;
    String s = text;
    while (s.length() > 1 && tft_.textWidth(s + ".") > maxWidth) {
        s.remove(s.length() - 1);
    }
    s.trim();
    return s + ".";
}

void CYDDisplay::drawCentered(const String& text, int y, uint16_t color) {
    tft_.setTextColor(color);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(fit(text::toAscii(text), layout::WIDTH - 2 * layout::MARGIN), layout::WIDTH / 2, y);
}

void CYDDisplay::showBoot(const char* firmwareVersion) {
    boardOnScreen_ = false;
    fillBackground();
    useFont(true);
    drawCentered("Haltestellenanzeige", 90, rgb565(HEADER_COLOR));
    useFont(false);
    drawCentered(String("CodeWeek Kassel - v") + firmwareVersion, 130, rgb565(TEXT_COLOR));
}

void CYDDisplay::showSetup(const SetupScreen& setup) {
    boardOnScreen_ = false;
    fillBackground();
    useFont(true);
    drawCentered("Einrichtung", 24, rgb565(HEADER_COLOR));
    useFont(false);
    drawCentered("1. Mit dem Handy ins WLAN:", 70, rgb565(TEXT_COLOR));
    drawCentered(setup.apName, 96, rgb565(TIME_COLOR));
    drawCentered("2. Im Browser oeffnen:", 136, rgb565(TEXT_COLOR));
    drawCentered(setup.url, 162, rgb565(TIME_COLOR));
    drawCentered(setup.step, 214, rgb565(LINE_COLOR));
}

void CYDDisplay::showStatus(const StatusScreen& status) {
    boardOnScreen_ = false;
    fillBackground();
    useFont(true);
    drawCentered(status.title, 80, status.isError ? alertColor() : rgb565(HEADER_COLOR));
    useFont(false);
    drawCentered(status.detail, 124, rgb565(TEXT_COLOR));
    drawCentered(status.hint, 170, rgb565(TIME_COLOR));
}

void CYDDisplay::showDepartures(const DisplayModel& m) {
    // Only redraw what changed (no flicker every minute).
    const bool hasFooter = m.statusText.length() > 0;
    // Footer appearing/disappearing changes the row height: redraw everything.
    const bool full = !boardOnScreen_ || hasFooter != (last_.statusText.length() > 0);
    if (full) fillBackground();

    if (full || m.stopName != last_.stopName || m.clock != last_.clock) drawHeader(m);

    // Column widths from the widest content, so columns never overlap.
    useFont(true);
    int lineW = tft_.textWidth("88");
    int platformW = 0;
    int timeW = tft_.textWidth("99 min");
    for (int i = 0; i < m.rowCount; ++i) {
        lineW = max(lineW, min(static_cast<int>(tft_.textWidth(text::toAscii(m.rows[i].line))), 60));
        timeW = max(timeW, min(static_cast<int>(tft_.textWidth(text::toAscii(m.rows[i].time))), 110));
    }
    useFont(false);
    if (m.showPlatform) {
        for (int i = 0; i < m.rowCount; ++i) {
            platformW = max(platformW, min(static_cast<int>(tft_.textWidth(text::toAscii(m.rows[i].platform))), 30));
        }
    }

    const bool columnsChanged = m.rowCount != last_.rowCount || m.showPlatform != last_.showPlatform;
    for (int i = 0; i < DEPARTURE_COUNT; ++i) {
        const bool changed = full || columnsChanged || i >= last_.rowCount || i >= m.rowCount ||
                             m.rows[i] != last_.rows[i];
        if (changed) drawRow(m, i, lineW, platformW, timeW);
    }

    if (hasFooter && (full || m.statusText != last_.statusText || m.status != last_.status)) drawFooter(m);

    last_ = m;
    boardOnScreen_ = true;
}

void CYDDisplay::drawHeader(const DisplayModel& m) {
    using namespace layout;
    tft_.fillRect(0, 0, WIDTH, HEADER_H, rgb565(BACKGROUND_COLOR));
    useFont(true);
    tft_.setTextDatum(ML_DATUM);

    int clockW = 0;
    if (m.clock.length() > 0) {
        clockW = tft_.textWidth(m.clock);
        tft_.setTextColor(rgb565(TEXT_COLOR));
        tft_.setTextDatum(MR_DATUM);
        tft_.drawString(m.clock, WIDTH - MARGIN, HEADER_H / 2);
        tft_.setTextDatum(ML_DATUM);
    }
    const int nameW = WIDTH - 2 * MARGIN - (clockW > 0 ? clockW + COLUMN_GAP : 0);
    tft_.setTextColor(rgb565(HEADER_COLOR));
    tft_.drawString(fit(text::toAscii(m.stopName), nameW), MARGIN, HEADER_H / 2);
    tft_.drawFastHLine(0, HEADER_H, WIDTH, rgb565(TEXT_COLOR));
}

void CYDDisplay::drawRow(const DisplayModel& m, int index, int lineW, int platformW, int timeW) {
    using namespace layout;
    const int rowH = rowHeight(m.statusText.length() > 0);
    const int y = ROWS_TOP + index * rowH;
    const int midY = y + rowH / 2;
    tft_.fillRect(0, y, WIDTH, rowH, rgb565(BACKGROUND_COLOR));

    if (index >= m.rowCount) {
        if (index == 0) {
            useFont(false);
            drawCentered("Keine Abfahrten in naechster Zeit", midY, rgb565(TEXT_COLOR));
        }
        return;
    }
    const DepartureRow& r = m.rows[index];

    // Line number (left) and time (right) in bold.
    useFont(true);
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(rgb565(LINE_COLOR));
    tft_.drawString(fit(text::toAscii(r.line), lineW), MARGIN, midY);

    tft_.setTextDatum(MR_DATUM);
    tft_.setTextColor(r.cancelled ? alertColor() : rgb565(TIME_COLOR));
    tft_.drawString(fit(text::toAscii(r.time), timeW), WIDTH - MARGIN, midY);

    // Platform and destination in regular weight, in the space between.
    useFont(false);
    int right = WIDTH - MARGIN - timeW - COLUMN_GAP;
    if (m.showPlatform && platformW > 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(rgb565(TEXT_COLOR));
        tft_.drawString(fit(text::toAscii(r.platform), platformW), right, midY);
        right -= platformW + COLUMN_GAP;
    }
    const int left = MARGIN + lineW + COLUMN_GAP;
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(rgb565(TEXT_COLOR));
    tft_.drawString(fit(text::toAscii(r.destination), right - left), left, midY);

    if (r.cancelled) {
        // Strike through the destination so it can't be mistaken for a normal departure.
        tft_.drawFastHLine(left, midY, right - left, alertColor());
    }
}

void CYDDisplay::drawFooter(const DisplayModel& m) {
    using namespace layout;
    const int y = HEIGHT - FOOTER_H;
    tft_.fillRect(0, y, WIDTH, FOOTER_H, rgb565(BACKGROUND_COLOR));
    tft_.drawFastHLine(0, y, WIDTH, rgb565(TEXT_COLOR));
    useFont(false);
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(statusColor(m.status));
    tft_.drawString(fit(text::toAscii(m.statusText), WIDTH - 2 * MARGIN), MARGIN, y + FOOTER_H / 2);
}

Display& createDisplay() {
    static CYDDisplay display;
    return display;
}

#endif  // DISPLAY_BACKEND_CYD
