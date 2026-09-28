#pragma once

#if DISPLAY_BACKEND_CYD

#include <TFT_eSPI.h>

#include "display/Display.h"

// TFT renderer for the ESP32 Cheap Yellow Display (320x240, landscape).
//
// STATUS: skeleton, NOT tested on hardware. The layout logic is complete,
// but driver, pins, rotation, fonts, colours and touch must be verified
// once the boards arrive (see TODO(HARDWARE) markers and README.md).
class CYDDisplay : public Display {
public:
    void begin() override;
    void showBoot(const char* firmwareVersion) override;
    void showSetup(const SetupScreen& setup) override;
    void showStatus(const StatusScreen& status) override;
    void showDepartures(const DisplayModel& model) override;
    void loop() override;
    bool setupGestureDetected() override;

private:
    void drawHeader(const DisplayModel& m);
    void drawRow(const DisplayModel& m, int index, int lineW, int platformW, int timeW);
    void drawFooter(const DisplayModel& m);
    void drawCentered(const String& text, int y, uint16_t color);
    void useFont(bool bold);
    // Truncates `text` (ASCII) with "." until it fits into maxWidth pixels.
    String fit(const String& text, int maxWidth);
    void fillBackground();

    TFT_eSPI tft_;
    DisplayModel last_;
    bool boardOnScreen_ = false;
};

#endif
