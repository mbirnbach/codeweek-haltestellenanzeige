#pragma once

#include "display/Display.h"

// Prints each screen as a text block on the Serial monitor. Main
// development target until the CYD hardware is verified.
class SerialDisplay : public Display {
public:
    void begin() override;
    void showBoot(const char* firmwareVersion) override;
    void showSetup(const SetupScreen& setup) override;
    void showStatus(const StatusScreen& status) override;
    void showDepartures(const DisplayModel& model) override;

private:
    void print(const std::vector<String>& lines);
};
