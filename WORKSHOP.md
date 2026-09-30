# 🚏 Deine eigene Abfahrtsanzeige

Willkommen bei der CodeWeek! Heute baust du eine kleine Anzeige, die live zeigt,
wann der nächste Bus oder die nächste Tram an deiner Haltestelle abfährt.

Alles Schwierige (WLAN, Internet, Daten vom NVV) ist schon fertig programmiert.
**Du gestaltest, wie deine Anzeige aussieht.**

---

## So geht's

### 1. Öffne die Datei `src/student/display_config.h`

Links in der Seitenleiste: `src` → `student` → `display_config.h`

Du darfst nur den Bereich zwischen den Linien mit **CODEWEEK 2026** ändern.
Ändere immer nur den Wert **rechts vom `=`**.

### 2. Wähle deine Farben

```cpp
constexpr DisplayColor BACKGROUND_COLOR = COLOR_BLUE;
constexpr DisplayColor TEXT_COLOR       = COLOR_ORANGE;
constexpr DisplayColor LINE_COLOR       = COLOR_ORANGE;
constexpr DisplayColor TIME_COLOR       = COLOR_ORANGE;
```

Am Anfang sieht deine Anzeige aus wie die echte an der Haltestelle:
blaues Schild, orange Leuchtschrift. Mach sie zu deiner eigenen!

Diese Farben gibt es:
`COLOR_BLACK` `COLOR_WHITE` `COLOR_GRAY` `COLOR_RED` `COLOR_ORANGE`
`COLOR_YELLOW` `COLOR_GREEN` `COLOR_CYAN` `COLOR_BLUE` `COLOR_PURPLE`

### 3. Wähle 3 bis 6 Abfahrten

```cpp
constexpr int DEPARTURE_COUNT = 5;
```

### 4. Ändere den Text für „0 Minuten“

```cpp
constexpr const char* NOW_TEXT = "sofort";
```

Zum Beispiel `"jetzt"`, `"gleich"`, `"da!"` oder `"los!"`.
Höchstens 8 Zeichen. Die Anführungszeichen `" "` müssen bleiben!

Genauso kannst du ändern, was bei einer ausgefallenen Fahrt steht:

```cpp
constexpr const char* CANCELLED_TEXT = "entfällt";
```

### 5. Wähle eine Schrift

```cpp
constexpr DisplayFont FONT = FONT_LED;
```

`FONT_LED` (Leuchtpunkte wie an der Haltestelle) · `FONT_CLASSIC` · `FONT_MODERN` ·
`FONT_COMPACT` · `FONT_PIXEL`

### 6. Optional: Uhr und Steig an oder aus

```cpp
constexpr bool SHOW_CLOCK    = true;
constexpr bool SHOW_PLATFORM = false;
```

`true` = an, `false` = aus.

### 7. Klicke auf **Build** (✓ unten in der blauen Leiste)

Wenn unten `SUCCESS` steht: super! 🎉

Wenn ein **FEHLER** kommt: Lies die Meldung – sie sagt dir meistens genau, was
nicht stimmt. Typische Fehler:
- Semikolon `;` am Zeilenende vergessen
- Anführungszeichen `"` beim Text vergessen
- Farbe falsch geschrieben (z.B. `COLOR_Blau` statt `COLOR_BLUE`)

### 8. Verbinde das Display mit dem USB-Kabel

### 9. Klicke auf **Upload** (→ unten in der blauen Leiste)

Warte, bis `SUCCESS` erscheint. Fertig!

---

## Beim ersten Start: Einrichten mit dem Handy

1. Das Display zeigt **EINRICHTUNG** und einen WLAN-Namen wie `Abfahrt-Setup-3F2A`.
2. Verbinde dein Handy mit diesem WLAN.
3. Eine Seite öffnet sich (sonst im Browser `http://192.168.4.1` eingeben).
4. WLAN auswählen, Passwort eingeben, **Verbinden**.
5. Haltestelle suchen (z.B. „Königsplatz“) und antippen.
6. **Speichern und starten** – fertig!

Später nochmal ändern? **BOOT-Taste 3 Sekunden gedrückt halten.**

---

## ⭐ Bonus-Aufgaben

Für alle, die schneller fertig sind:

⭐ **Eigener Text:** Ändere „sofort“ zu deinem eigenen Wort.

⭐⭐ **Minuten selbst programmieren:** Öffne `src/student/student_code.cpp`.
Dort ist die Funktion `formatMinutes`. Sie bekommt die Minuten als Zahl und gibt
den Text zurück, der angezeigt wird. Probier z.B.:
- `"Min."` statt `"min"`
- bei genau 1 Minute: `"1 Minute"`

⭐⭐⭐ **Profi-Aufgabe:** Ab 60 Minuten soll z.B. `"1 h"` statt `"60 min"` stehen.
Tipp: `minutes / 60` rechnet die Stunden aus.

Etwas kaputt gemacht? Kein Problem – im Kommentar über der Funktion steht der
Original-Code zum Zurückkopieren.

---

## Was bedeutet was auf der Anzeige?

| Anzeige | Bedeutung |
|---|---|
| (nichts unten) | Alles aktuell |
| `VERALTET` | Die Daten sind etwas älter – der Server hatte gerade Probleme |
| `KEIN WLAN` | Keine Verbindung – es werden die letzten Daten gezeigt |
| `entfällt` | Diese Fahrt fällt aus (Text in `CANCELLED_TEXT` änderbar) |
| Laufschrift unten | Hinweise, z.B. Umleitungen oder Zusatzfahrten |
