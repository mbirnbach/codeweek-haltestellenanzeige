#include "student/student_code.h"
#include "student/display_config.h"

// ======================================================
// CODEWEEK 2026 – BONUS-AUFGABE (freiwillig)
//
// Diesen Bereich dürft ihr verändern.
// Alles andere ist bereits vorbereitet.
// ======================================================
//
// Diese Funktion bekommt die Minuten bis zur Abfahrt
// (zum Beispiel 0, 1, 7 oder 25) und gibt zurück,
// welcher Text auf dem Bildschirm stehen soll.
//
//   minutes   ->  Rückgabe
//   0         ->  "sofort"   (das ist NOW_TEXT aus display_config.h)
//   7         ->  "7 min"
//
// Ideen:
//   ⭐   Schreib "Min." statt "min"
//   ⭐⭐  Bei 1 Minute: "1 Minute" statt "1 min"
//   ⭐⭐⭐ Ab 60 Minuten: "1 h" statt "60 min"
//
// Tipp: Höchstens ca. 8 Zeichen, sonst wird der Text abgeschnitten.
// Wenn etwas schiefgeht: Einfach den Original-Code von unten wiederherstellen.
//
// Original-Code:
//     if (minutes <= 0) {
//         return NOW_TEXT;
//     }
//     return String(minutes) + " min";

String formatMinutes(int minutes) {
    if (minutes <= 0) {
        return NOW_TEXT;
    }

    return String(minutes) + " min";
}

// ======================================================
// Ende des Bereichs für euch.
// ======================================================
