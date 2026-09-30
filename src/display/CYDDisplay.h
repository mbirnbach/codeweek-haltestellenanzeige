#pragma once

#if DISPLAY_BACKEND_CYD

#include "display/Display.h"

class Adafruit_ILI9341;
class CYDRenderer;
class SPIClass;

// The physical ESP32-2432S028 "Cheap Yellow Display": ILI9341 on a dedicated
// HSPI bus, backlight on GPIO 21, landscape 320x240.
//
// This class only brings up the hardware; all drawing is done by
// CYDRenderer. Only these two classes know about Adafruit GFX – the rest of
// the application talks to the Display interface.
//
// No touch input in v1: setup happens in the web portal, the BOOT button
// reopens it.
class CYDDisplay : public Display {
public:
    void begin() override;
    void showBoot(const char* firmwareVersion) override;
    void showSetup(const SetupScreen& setup) override;
    void showStatus(const StatusScreen& status) override;
    void showDepartures(const DisplayModel& model) override;
    void loop() override;  // scrolls the message ticker

private:
    SPIClass* spi_ = nullptr;
    Adafruit_ILI9341* tft_ = nullptr;
    CYDRenderer* renderer_ = nullptr;
    uint32_t lastTickerMs_ = 0;
};

#endif
