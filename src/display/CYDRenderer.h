#pragma once

#if DISPLAY_BACKEND_CYD || CYD_HOST_RENDER

#include <stdint.h>

#include "display/Display.h"

class Adafruit_GFX;
class GFXcanvas16;

// Draws all screens for the 320x240 landscape CYD panel on any Adafruit_GFX
// surface. Knows nothing about SPI, pins or the display controller: on the
// device it draws on the ILI9341 (see CYDDisplay), on the host it can draw
// into an in-memory canvas (tools/render_cyd.sh) to check the layout.
class CYDRenderer : public Display {
public:
    explicit CYDRenderer(Adafruit_GFX& panel);

    void begin() override;  // panel must already be initialised and rotated
    void showBoot(const char* firmwareVersion) override;
    void showSetup(const SetupScreen& setup) override;
    void showStatus(const StatusScreen& status) override;
    void showDepartures(const DisplayModel& model) override;

    // Moves the message ticker one step to the left. Call regularly
    // (CYDDisplay::loop does, every TICKER_INTERVAL_MS).
    void scrollTicker();
    static constexpr uint32_t TICKER_INTERVAL_MS = 40;
    static constexpr int TICKER_STEP_PX = 2;

private:
    struct Columns {
        int lineW;
        int platformW;
        int timeW;
    };

    void drawHeader(const DisplayModel& m);
    void drawRow(const DisplayModel& m, int index, const Columns& cols);
    void drawFooter(const DisplayModel& m);
    Columns measureColumns(const DisplayModel& m);

    // FONT_LED: board styled like the NVV LED signs at the stops.
    void showLedBoard(const DisplayModel& m);
    void drawLedHeader(const DisplayModel& m);
    void drawLedRow(const DisplayModel& m, int index, const Columns& cols);
    Columns measureLedColumns(const DisplayModel& m);

    // Fills [y, y+height) with `fill` and calls draw(gfx, dy); drawing code
    // uses screen coordinates + dy. Goes through the canvas in slices.
    template <typename Draw>
    void band(int y, int height, uint16_t fill, Draw draw);

    // Band drawing: a horizontal strip is drawn into an off-screen canvas and
    // then copied to the panel in one go, so minute updates don't flicker.
    Adafruit_GFX& beginBand(int height);
    void endBand(int y, int height);

    // Ticker (last row) state.
    int tickerSlot(const DisplayModel& m) const;  // -1 = no ticker
    void startTicker(const DisplayModel& m);
    void drawTicker(const DisplayModel& m);
    String tickerText_;  // encoded for the current font
    int tickerWidth_ = 0;  // pixels
    int tickerOffset_ = 0;  // pixels scrolled so far

    Adafruit_GFX* tft_;
    GFXcanvas16* canvas_ = nullptr;  // nullptr if RAM allocation failed -> draw directly
    int canvasHeight_ = 0;

    DisplayModel last_;
    bool boardOnScreen_ = false;
};

#endif
