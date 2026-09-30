#if DISPLAY_BACKEND_CYD

#include "display/CYDDisplay.h"

#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include "display/CYDRenderer.h"

// ============================================================================
// Hardware: ESP32-2432S028, ILI9341 on its own HSPI bus.
// Pinout and controller verified on the real board (colours + text).
// ============================================================================
namespace pins {
constexpr int8_t TFT_CS = 15;
constexpr int8_t TFT_DC = 2;
constexpr int8_t TFT_RST = -1;  // not connected: panel resets with the ESP32
constexpr int8_t TFT_SCK = 14;
constexpr int8_t TFT_MISO = 12;
constexpr int8_t TFT_MOSI = 13;
constexpr int8_t TFT_BL = 21;  // backlight, HIGH = on
}  // namespace pins

// Landscape 320x240. Rotations 1 and 3 are both landscape, 180° apart.
// TODO(HARDWARE): pick 1 or 3 to match the USB port position in the enclosure.
constexpr uint8_t ROTATION = 1;

void CYDDisplay::begin() {
    // Backlight stays off until the panel is cleared (no flash of random pixels).
    pinMode(pins::TFT_BL, OUTPUT);
    digitalWrite(pins::TFT_BL, LOW);

    spi_ = new SPIClass(HSPI);
    spi_->begin(pins::TFT_SCK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);

    tft_ = new Adafruit_ILI9341(spi_, pins::TFT_DC, pins::TFT_CS, pins::TFT_RST);
    tft_->begin();
    tft_->setRotation(ROTATION);

    renderer_ = new CYDRenderer(*tft_);
    renderer_->begin();  // clears the screen in the background colour

    digitalWrite(pins::TFT_BL, HIGH);
    Serial.printf("[display] CYD ILI9341 %dx%d ready\n", tft_->width(), tft_->height());
}

void CYDDisplay::showBoot(const char* firmwareVersion) { renderer_->showBoot(firmwareVersion); }
void CYDDisplay::showSetup(const SetupScreen& setup) { renderer_->showSetup(setup); }
void CYDDisplay::showStatus(const StatusScreen& status) { renderer_->showStatus(status); }
void CYDDisplay::showDepartures(const DisplayModel& model) { renderer_->showDepartures(model); }

void CYDDisplay::loop() {
    if (millis() - lastTickerMs_ < CYDRenderer::TICKER_INTERVAL_MS) return;
    lastTickerMs_ = millis();
    renderer_->scrollTicker();
}

Display& createDisplay() {
    static CYDDisplay display;
    return display;
}

#endif  // DISPLAY_BACKEND_CYD
