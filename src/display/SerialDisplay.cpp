#include "display/SerialDisplay.h"

#include "display/BoardFormatter.h"

#ifndef SERIAL_DISPLAY_ANSI_COLORS
#define SERIAL_DISPLAY_ANSI_COLORS 0
#endif

void SerialDisplay::begin() {
    Serial.println();
    Serial.println("[display] Serial display (no TFT)");
    Serial.printf("[display] %s\n", board_text::designSummary().c_str());
}

void SerialDisplay::showBoot(const char* firmwareVersion) {
    StatusScreen s;
    s.title = "Haltestellenanzeige";
    s.detail = String("CodeWeek Kassel · v") + firmwareVersion;
    s.hint = "Startet ...";
    print(board_text::status(s, SERIAL_DISPLAY_ANSI_COLORS));
}

void SerialDisplay::showSetup(const SetupScreen& setup) { print(board_text::setup(setup, SERIAL_DISPLAY_ANSI_COLORS)); }

void SerialDisplay::showStatus(const StatusScreen& status) {
    print(board_text::status(status, SERIAL_DISPLAY_ANSI_COLORS));
}

void SerialDisplay::showDepartures(const DisplayModel& model) {
    print(board_text::departures(model, SERIAL_DISPLAY_ANSI_COLORS));
}

void SerialDisplay::print(const std::vector<String>& lines) {
    Serial.println();
    for (const String& line : lines) Serial.println(line);
}

#if DISPLAY_BACKEND_SERIAL
Display& createDisplay() {
    static SerialDisplay display;
    return display;
}
#endif
