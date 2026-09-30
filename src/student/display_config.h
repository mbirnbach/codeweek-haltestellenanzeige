#pragma once

#include "display/DisplayTypes.h"

// ======================================================
// CODEWEEK 2026
//
// Diesen Bereich dürft ihr verändern.
// Alles andere ist bereits vorbereitet.
// ======================================================
//
// Tipp: Ändere immer nur den Wert rechts vom "=".
//       Jede Zeile endet mit einem Semikolon ;


// ------------------------------------------------------
// 1) Wie viele Abfahrten sollen angezeigt werden?
//    Erlaubt: 3, 4, 5 oder 6
// ------------------------------------------------------
constexpr int DEPARTURE_COUNT = 5;


// ------------------------------------------------------
// 2) Was steht da, wenn der Bus gleich abfährt (0 Minuten)?
//    Beispiele: "sofort", "jetzt", "gleich", "da!", "los!"
//    Höchstens 8 Zeichen. Die Anführungszeichen "" müssen bleiben!
// ------------------------------------------------------
constexpr const char* NOW_TEXT = "sofort";


// ------------------------------------------------------
// 2b) Was steht da, wenn eine Fahrt ausfällt?
//    Beispiele: "entfällt", "fällt aus", "AUSFALL", "nix da"
//    Höchstens 8 Zeichen.
// ------------------------------------------------------
constexpr const char* CANCELLED_TEXT = "entfällt";


// ------------------------------------------------------
// 3) Farben
//    Du kannst wählen aus:
//    COLOR_BLACK   (schwarz)    COLOR_WHITE   (weiß)
//    COLOR_GRAY    (grau)       COLOR_RED     (rot)
//    COLOR_ORANGE  (orange)     COLOR_YELLOW  (gelb)
//    COLOR_GREEN   (grün)       COLOR_CYAN    (türkis)
//    COLOR_BLUE    (blau)       COLOR_PURPLE  (lila)
//
//    Voreingestellt: wie die echten Anzeigen an der Haltestelle
//    (blaues Schild, orange Leuchtschrift).
// ------------------------------------------------------
constexpr DisplayColor BACKGROUND_COLOR = COLOR_BLUE;    // Hintergrund
constexpr DisplayColor TEXT_COLOR       = COLOR_ORANGE;  // Ziel, z.B. "Druseltal"
constexpr DisplayColor LINE_COLOR       = COLOR_ORANGE;  // Liniennummer, z.B. "4"
constexpr DisplayColor TIME_COLOR       = COLOR_ORANGE;  // Minuten, z.B. "3 min"
constexpr DisplayColor HEADER_COLOR     = COLOR_WHITE;   // Name der Haltestelle oben


// ------------------------------------------------------
// 4) Schriftart
//    FONT_CLASSIC  (klassisch, mit Füßchen)
//    FONT_MODERN   (modern, schlicht)
//    FONT_COMPACT  (klein, passt mehr Text hin)
//    FONT_PIXEL    (Retro-Pixel-Look)
//    FONT_LED      (Leuchtpunkte wie an der echten Haltestelle)
// ------------------------------------------------------
constexpr DisplayFont FONT = FONT_LED;


// ------------------------------------------------------
// 5) Extras: an (true) oder aus (false)
// ------------------------------------------------------
constexpr bool SHOW_CLOCK    = true;   // Uhrzeit oben rechts anzeigen?
constexpr bool SHOW_PLATFORM = false;  // Steig/Gleis (z.B. "A") anzeigen?


// ======================================================
// Ende des Bereichs für euch.
// Bonus-Aufgabe: src/student/student_code.cpp
// ======================================================
