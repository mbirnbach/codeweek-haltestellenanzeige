#pragma once

// Compile-time checks for the participant settings in display_config.h.
// This file is included by the core code (not by display_config.h), so the
// checks always run, even if someone edits the student file heavily.
//
// The messages are shown by the compiler, so they are written in German for
// the participants.

#include "student/display_config.h"

namespace config_checks {

// Counts UTF-8 characters (not bytes), so "grün" counts as 4.
constexpr int utf8Chars(const char* s) {
    return (*s == '\0') ? 0 : (((*s & 0xC0) != 0x80) ? 1 : 0) + utf8Chars(s + 1);
}

}  // namespace config_checks

static_assert(DEPARTURE_COUNT >= 3,
              "\n\n  FEHLER in display_config.h: DEPARTURE_COUNT ist zu klein."
              "\n  Bitte eine Zahl von 3 bis 6 eintragen.\n");
static_assert(DEPARTURE_COUNT <= 6,
              "\n\n  FEHLER in display_config.h: DEPARTURE_COUNT ist zu groß."
              "\n  Mehr als 6 Abfahrten passen nicht auf den Bildschirm."
              "\n  Bitte eine Zahl von 3 bis 6 eintragen.\n");

static_assert(NOW_TEXT != nullptr && config_checks::utf8Chars(NOW_TEXT) >= 1,
              "\n\n  FEHLER in display_config.h: NOW_TEXT ist leer."
              "\n  Schreib ein Wort zwischen die Anführungszeichen, z.B. \"jetzt\".\n");
static_assert(config_checks::utf8Chars(NOW_TEXT) <= 8,
              "\n\n  FEHLER in display_config.h: NOW_TEXT ist zu lang."
              "\n  Höchstens 8 Zeichen, sonst passt es nicht auf den Bildschirm.\n");

static_assert(CANCELLED_TEXT != nullptr && config_checks::utf8Chars(CANCELLED_TEXT) >= 1,
              "\n\n  FEHLER in display_config.h: CANCELLED_TEXT ist leer."
              "\n  Schreib ein Wort zwischen die Anführungszeichen, z.B. \"entfällt\".\n");
static_assert(config_checks::utf8Chars(CANCELLED_TEXT) <= 8,
              "\n\n  FEHLER in display_config.h: CANCELLED_TEXT ist zu lang."
              "\n  Höchstens 8 Zeichen, sonst passt es nicht auf den Bildschirm.\n");

static_assert(TEXT_COLOR != BACKGROUND_COLOR,
              "\n\n  FEHLER in display_config.h: TEXT_COLOR und BACKGROUND_COLOR sind gleich."
              "\n  Dann könnte man den Text nicht lesen! Wähle zwei verschiedene Farben.\n");
static_assert(LINE_COLOR != BACKGROUND_COLOR,
              "\n\n  FEHLER in display_config.h: LINE_COLOR und BACKGROUND_COLOR sind gleich."
              "\n  Dann könnte man die Liniennummer nicht lesen!\n");
static_assert(TIME_COLOR != BACKGROUND_COLOR,
              "\n\n  FEHLER in display_config.h: TIME_COLOR und BACKGROUND_COLOR sind gleich."
              "\n  Dann könnte man die Minuten nicht lesen!\n");
static_assert(HEADER_COLOR != BACKGROUND_COLOR,
              "\n\n  FEHLER in display_config.h: HEADER_COLOR und BACKGROUND_COLOR sind gleich."
              "\n  Dann könnte man den Haltestellen-Namen nicht lesen!\n");
