# Anleitung: Firmware auf die Haltestellenanzeige flashen

Für das Workshop-Team. (Die Kurzanleitung für Teilnehmende steht in
[WORKSHOP.md](WORKSHOP.md).)

Gerät: **ESP32-2432S028** („Cheap Yellow Display“, CYD), USB-Chip **CH340**.

Es gibt zwei Wege:

| Weg | Wann? | Braucht |
|---|---|---|
| **A – mit PlatformIO** | Normalfall. Auch für angepasste Designs der Teilnehmenden. | VS Code + PlatformIO, Projektordner |
| **B – fertiges Image** | Viele Geräte schnell mit derselben Firmware bespielen. | Nur esptool (oder Browser) + eine `.bin`-Datei |

Beide Wege wurden am echten Gerät getestet (außer dem Browser-Flasher, siehe B3).

---

## 1. Was du brauchst

- Ein **USB-Datenkabel** (Micro-USB bzw. USB-C, je nach Board). Reine Ladekabel
  funktionieren nicht – wenn kein Port auftaucht, zuerst das Kabel tauschen.
- Einen Rechner mit macOS, Windows oder Linux.
- Für Weg A: **VS Code** mit der Erweiterung **PlatformIO IDE**
  (oder PlatformIO Core auf der Kommandozeile).
- Die Datei **`include/secrets.h`** mit dem API-Token (bekommst du vom
  Projektteam, liegt nicht im Git-Repository).
- Beim ersten Bauen: **Internet** (PlatformIO lädt Compiler und Bibliotheken,
  ca. 300 MB).

---

## 2. Einmalige Vorbereitung pro Rechner

### Linux-Workshop-Rechner (Ubuntu/Mint/Debian): automatisch

Für die Workshop-Rechner gibt es ein Skript, das alles erledigt: Pakete,
VS Code mit PlatformIO und deutscher Oberfläche, USB-Rechte für die Displays,
Projekt und `secrets.h` im Konto `schueler`, einmal komplett bauen (danach
geht alles offline) und ein Starter „Haltestellenanzeige“ auf dem Schreibtisch.

Im Konto **lehrer** (braucht Internet und das sudo-Passwort):

```bash
curl -fsSL https://raw.githubusercontent.com/mbirnbach/codeweek-haltestellenanzeige/main/tools/setup_workshop_linux.sh -o setup.sh
sudo bash setup.sh
```

Das Skript fragt nach dem API-Token und läuft dann ohne weitere Rückfragen
durch (beim ersten Mal ca. 10–15 Minuten). Am Ende steht eine Prüfliste.
Danach **neu starten**, als `schueler` anmelden, Starter öffnen – fertig.
Erneut ausführen holt den neuesten Projektstand.

Alles Projektbezogene (Projekt, Token, PlatformIO, VS-Code-Einstellungen)
liegt im Home-Ordner von `schueler` und verschwindet beim Zurücksetzen des
Kontos. Zwischen zwei Gruppen nur die Teilnehmer-Dateien zurücksetzen
(ohne Internet):

```bash
sudo -u schueler /home/schueler/codeweek-haltestellenanzeige/tools/workshop_reset.sh
```

**Nach dem Workshop** (bevor die Rechner zurückgehen):

```bash
sudo bash setup.sh --cleanup
```

Das nimmt die wenigen Systemänderungen zurück, die das Skript selbst gemacht
hat (VS Code, Microsoft-Paketquelle, udev-Regel, brltty, Gruppe `dialout`).
Danach wie geplant das Konto `schueler` zurücksetzen.

Die folgenden Abschnitte 2.1–2.4 beschreiben dasselbe von Hand, z.B. für
macOS oder Windows.


### 2.1 Projekt holen

```bash
git clone https://github.com/mbirnbach/codeweek-haltestellenanzeige.git
cd codeweek-haltestellenanzeige
```

### 2.2 API-Token eintragen

```bash
cp include/secrets.example.h include/secrets.h
```

Dann in `include/secrets.h` den Wert von `API_TOKEN` durch den echten Token
ersetzen. **Diese Datei niemals committen, verschicken oder zeigen.**
(Sie ist in `.gitignore` eingetragen.)

Ohne diese Datei bricht der Build mit der Meldung
`include/secrets.h fehlt` ab.

### 2.3 Einmal bauen (lädt alle Werkzeuge)

In VS Code: Projektordner öffnen, unten in der blauen Leiste auf **✓ (Build)**
klicken. Oder im Terminal:

```bash
pio run
```

Das dauert beim ersten Mal einige Minuten. Am Ende muss `SUCCESS` stehen.
Danach geht alles auch ohne Internet – **das vor dem Workshop auf jedem
Laptop einmal machen.**

### 2.4 USB-Treiber (CH340)

| System | Was tun |
|---|---|
| macOS | Nichts, der Treiber ist eingebaut. Port heißt `/dev/cu.usbserial-XXXX`. |
| Windows 10/11 | Meist installiert Windows den Treiber automatisch. Falls im Geräte-Manager kein `USB-SERIAL CH340 (COMx)` erscheint: Treiber „CH341SER“ von der Herstellerseite wch-ic.com installieren. |
| Linux | Treiber ist im Kernel. Benutzer muss in der Gruppe `dialout` sein (`sudo usermod -aG dialout $USER`, dann neu anmelden). Unter Ubuntu ggf. `brltty` deinstallieren – es belegt sonst CH340-Geräte. Port heißt `/dev/ttyUSB0`. |

Prüfen, ob das Gerät erkannt wird:

```bash
pio device list
```

---

## 3. Weg A: Flashen mit PlatformIO

### 3.1 In VS Code

1. Display per USB anschließen.
2. Unten in der blauen Leiste auf **→ (Upload)** klicken.
3. Warten, bis im Terminal `SUCCESS` steht (ca. 30 Sekunden).

PlatformIO sucht den Port selbst. Das Gerät startet danach automatisch neu.

### 3.2 Im Terminal

```bash
pio run -t upload
```

Wenn mehrere Geräte angeschlossen sind, den Port angeben:

```bash
pio run -t upload --upload-port /dev/cu.usbserial-1210
```

(Windows: z.B. `--upload-port COM5`, Linux: `--upload-port /dev/ttyUSB0`)

### 3.3 So sieht ein erfolgreicher Upload aus

```
Chip is ESP32-D0WD-V3 (revision v3.1)
MAC: d4:e9:f4:69:f7:30
...
Hash of data verified.
Hard resetting via RTS pin...
========================= [SUCCESS] =========================
```

Gespeicherte Einstellungen (WLAN, Haltestelle) bleiben bei einem Upload
**erhalten**.

---

## 4. Weg B: Fertiges Image ohne PlatformIO

Praktisch, wenn viele Geräte dieselbe Firmware bekommen sollen.

### 4.1 Image erzeugen (einmal, auf einem Rechner mit PlatformIO)

```bash
tools/build_image.sh
```

Ergebnis: `dist/haltestellenanzeige-cyd.bin` (ca. 1,1 MB). Diese Datei
enthält Bootloader, Partitionstabelle und Firmware in einem Stück und wird an
**Adresse 0x0** geschrieben.

> ⚠️ Das Image enthält den API-Token. Nur im Team weitergeben, nicht
> veröffentlichen.

### 4.2 Mit esptool flashen

Mit PlatformIO installiert:

```bash
pio pkg exec --package tool-esptoolpy -- esptool.py --chip esp32 \
  --port /dev/cu.usbserial-1210 --baud 460800 \
  write_flash 0x0 dist/haltestellenanzeige-cyd.bin
```

Ohne PlatformIO (Python vorhanden):

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/cu.usbserial-1210 --baud 460800 \
  write_flash 0x0 haltestellenanzeige-cyd.bin
```

Erfolgreich, wenn `Hash of data verified.` erscheint.

### 4.3 Im Browser (nicht getestet)

Mit Chrome oder Edge geht es auch ohne Installation über den offiziellen
Espressif-Flasher **https://espressif.github.io/esptool-js/**:
„Connect“ → Port wählen → Flash Address **0x0** → Datei
`haltestellenanzeige-cyd.bin` wählen → „Program“.

---

## 5. Nach dem Flashen: Gerät einrichten

Ein frisches Gerät zeigt **„Einrichtung“** mit einem WLAN-Namen wie
`Abfahrt-Setup-F730`. Die letzten vier Zeichen sind bei jedem Gerät anders
(aus der MAC-Adresse) – im Workshop am besten auf das Gerät schreiben.

1. Mit dem Handy das WLAN **`Abfahrt-Setup-XXXX`** verbinden (offen, ohne Passwort).
2. Die Einrichtungsseite öffnet sich von selbst, sonst im Browser
   **http://192.168.4.1** aufrufen.
3. WLAN auswählen, Passwort eingeben, **Verbinden**.
   Das Handy verliert dabei evtl. kurz die Verbindung zum Setup-WLAN –
   dann einfach wieder verbinden.
4. Haltestelle suchen (z.B. „Kirchweg“) und antippen.
5. **Speichern und starten**. Das Gerät startet neu und zeigt nach wenigen
   Sekunden die Abfahrten.

**Später ändern:** Die **BOOT**-Taste 3 Sekunden gedrückt halten
(nicht RST). Das Gerät startet neu in die Einrichtung; WLAN und Haltestelle
sind vorausgefüllt. Wird die Einrichtung 10 Minuten nicht benutzt (oder
das Gerät vom Strom getrennt), läuft es mit den alten Einstellungen weiter.

---

## 6. Kontrolle über den seriellen Monitor

```bash
pio device monitor
```

(Beenden mit `Strg+C`.) Normaler Start eines eingerichteten Geräts:

```
[display] CYD ILI9341 320x240 ready
[app] stop 2200005 'Kassel Kirchweg', Wi-Fi 'MeinWLAN'
[wifi] connected, IP 192.168.1.42, RSSI -53 dBm
[api] HTTP 200, 2797 bytes, 3264 ms
[app] 10 departures
[app] state FETCHING_DATA -> ONLINE
```

Bei einem frischen Gerät ist `nvs_open failed: NOT_FOUND` normal – es sind
noch keine Einstellungen gespeichert.

Befehle (eintippen + Enter):

| Befehl | Wirkung |
|---|---|
| `status` | Zustand, WLAN, Haltestelle, freier Speicher |
| `fetch` | Abfahrten sofort neu laden |
| `setup` | Neustart in die Einrichtung (Einstellungen bleiben) |
| `reset` | Einstellungen löschen, Neustart in die Einrichtung |

Der WLAN-Empfang steht hinter `RSSI`: −50 dBm ist gut, unter −85 dBm wird es
wackelig.

---

## 7. Gerät komplett zurücksetzen

Löscht **alles** (Firmware und gespeicherte Einstellungen):

```bash
pio run -t erase
```

oder mit esptool:

```bash
pio pkg exec --package tool-esptoolpy -- esptool.py --chip esp32 \
  --port /dev/cu.usbserial-1210 erase_flash
```

Danach wie in Abschnitt 3 oder 4 neu flashen. Nur die Einstellungen löschen
geht einfacher mit dem Befehl `reset` (Abschnitt 6).

---

## 8. Viele Geräte nacheinander

1. Gerät anstecken.
2. `pio run -t upload` (oder Weg B).
3. Auf `SUCCESS` / `Hash of data verified.` warten.
4. Den angezeigten Setup-Namen (`Abfahrt-Setup-XXXX`) auf das Gerät schreiben.
5. Abstecken, nächstes Gerät.

Pro Gerät ca. 30 Sekunden. Der Port ist meist bei jedem Gerät derselbe, wenn
dieselbe USB-Buchse benutzt wird.

---

## 9. Fehlerbehebung

| Problem | Lösung |
|---|---|
| Kein Port / `pio device list` zeigt nichts | Anderes USB-Kabel (Datenkabel!), andere Buchse, kein USB-Hub. Windows/Linux: Treiber und Rechte prüfen (2.4). |
| `Failed to connect to ESP32: No serial data received` / `Wrong boot mode` | **BOOT** gedrückt halten, Upload starten, loslassen sobald `Connecting...` erscheint. Notfalls BOOT halten und kurz **RST** drücken. |
| `Could not open port … busy` / `Resource busy` | Der serielle Monitor (oder ein anderes Programm) hat den Port offen – schließen und nochmal. |
| `include/secrets.h fehlt` | Abschnitt 2.2. |
| Build bricht mit `FEHLER in display_config.h` ab | Ungültiger Wert in den Teilnehmer-Einstellungen; die Meldung sagt, welcher. |
| Display bleibt schwarz | Serieller Monitor (Abschnitt 6): Erscheint `[display] CYD ILI9341 320x240 ready`? Wenn nein, neu flashen. Wenn ja, Kabel/Stromversorgung prüfen. |
| „Kein WLAN“ auf dem Display | WLAN-Name/Passwort falsch oder zu weit weg (`RSSI` prüfen). Nur 2,4-GHz-WLANs funktionieren. Neu einrichten mit BOOT 3 s. |
| „Keine Abfahrtsdaten – Server lehnt Zugang ab“ | API-Token in `secrets.h` falsch oder gesperrt. Token prüfen, neu bauen und flashen. |
| Setup-WLAN taucht auf dem Handy nicht auf | Gerät zeigt „Einrichtung“? Kurz warten, WLAN-Liste am Handy neu laden. Notfalls Befehl `setup` über den seriellen Monitor. |
| Schul-WLAN mit Anmeldeseite (Captive Portal) oder mit Benutzername + Passwort (WPA2-Enterprise, z.B. eduroam) | Funktioniert nicht – das Gerät kann nur WLANs mit einem einfachen Passwort (WPA2-Personal) oder offene WLANs. Ein anderes WLAN oder einen Handy-Hotspot benutzen. |
