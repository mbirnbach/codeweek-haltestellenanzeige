#pragma once

#include <vector>

#include "app/DisplayModel.h"

// Renders the display models as fixed-width text, used by the Serial
// display on the ESP32 and by the host preview tool. Pure function, so the
// exact output is unit-tested.
//
// With `ansi` = true, the participant colours are applied with ANSI escape
// codes (PlatformIO's monitor shows them with the "direct" filter).

namespace board_text {

constexpr int WIDTH = 40;

std::vector<String> departures(const DisplayModel& m, bool ansi);
std::vector<String> status(const StatusScreen& s, bool ansi);
std::vector<String> setup(const SetupScreen& s, bool ansi);

// One-line summary of the participant design settings.
String designSummary();

}  // namespace board_text
