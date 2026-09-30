# Haltestellenanzeige – ESP32 CYD firmware (CodeWeek Kassel)

Firmware for a small live departure display built on an ESP32 "Cheap Yellow
Display" (CYD). Made for a 2-hour CodeWeek workshop for complete beginners: the
core is prepared and robust, and participants only change a small, clearly marked
part (`src/student/`). Participant instructions (German): [WORKSHOP.md](WORKSHOP.md).

The firmware talks **only** to the departure API at `https://nvv-api.edupixel.de`
(backend repo: `öpnv-ks-api`). It knows nothing about HAFAS, MCT, GTFS or SIRI.

## Hardware

| | |
|---|---|
| Board | ESP32-2432S028 ("Cheap Yellow Display") |
| Display | ILI9341, 320×240, used in landscape (rotation 1) |
| Bus | dedicated HSPI: `SPIClass(HSPI).begin(14, 12, 13, 15)` |
| Pins | `TFT_CS 15`, `TFT_DC 2`, `TFT_RST -1`, `TFT_SCK 14`, `TFT_MISO 12`, `TFT_MOSI 13` |
| Backlight | GPIO 21, `HIGH` = on |
| Graphics | Adafruit GFX + Adafruit ILI9341 |
| Touch | not used in v1 (setup via the web portal, BOOT button reopens it) |

Controller, pinout, backlight and colours were verified on a real board with
Adafruit GFX/ILI9341 (colour fills and text). The complete firmware has not yet
been checked on the assembled device; see [Remaining hardware
TODOs](#remaining-hardware-todos).

## Architecture

```
src/
  main.cpp                  setup()/loop(): creates the display, runs AppController
  app/
    AppController.*         state machine, fetch timing/backoff, reconfigure triggers
    AppState.h              BOOTING, CONFIG_PORTAL, CONNECTING_WIFI, ONLINE, ...
    DisplayModel.h          what a screen shows (rows, status text, ...)
    DisplayModelBuilder.*   API data + time + student settings -> DisplayModel   [host-tested]
  api/
    DepartureApi.*          HTTPS client, bearer token, error mapping
    DepartureParser.*       JSON -> models (ArduinoJson 7, filtered)               [host-tested]
    Models.h                Stop, Departure, StationBoard, ApiResult
    RootCertificates.h      trusted root CAs for TLS
  config/
    AppSettings.h           maintainer settings: timeouts, poll interval, pins
    DeviceConfig.*          Wi-Fi + stop config and validation rules             [host-tested]
    ConfigStore.*           NVS (Preferences) persistence
    ConfigPortal.*          access point + captive portal + stop search proxy
    PortalPage.h            the portal's single HTML page
  network/
    WifiManager.*           STA connect / reconnect
    TimeSync.*              NTP, Europe/Berlin with DST, fallback to API server time
  display/
    Display.h               backend interface
    DisplayTypes.h          named colours and fonts for participants
    BoardFormatter.*        text rendering of all screens                         [host-tested]
    SerialDisplay.*         prints BoardFormatter output (env serial-dev)
    CYDDisplay.*            CYD hardware init: HSPI, ILI9341, backlight (env cyd)
    CYDRenderer.*           draws all CYD screens on any Adafruit_GFX surface
  student/                  ← the ONLY part participants edit
    display_config.h        count, colours, font, "0 min" text, toggles
    student_code.cpp        bonus: formatMinutes()
    config_checks.h         static_asserts with German error messages (not for editing)
  host/preview_main.cpp     host preview program (env preview)
include/secrets.h           API URL + token (git-ignored)
test/                       host unit tests + captured API fixtures
tools/                      preview.sh, api_check.sh, portal_mock.py
```

Data flow: `DepartureApi` → `StationBoard` → (every second) `buildDisplayModel()`
→ `Display::showDepartures()`. The display backends only draw the model; neither
contains application decisions, so the Serial output is a faithful stand-in for
the TFT. Displays are only redrawn when the model changed.

## Build profiles

| Env | Target | Purpose |
|---|---|---|
| `serial-dev` | any ESP32 | whole application, screens printed to Serial (ANSI colours) |
| `cyd` (default) | CYD | the real device: ILI9341 via Adafruit GFX |
| `native` | host | unit tests: `pio test -e native` |
| `preview` | host | renders JSON like the Serial display: `tools/preview.sh` |

```bash
pio run                                # build for the CYD (default env)
pio run -t upload                      # flash the CYD
pio run -e serial-dev -t upload        # flash with Serial-only output
pio device monitor                     # Serial output (commands: status, fetch, setup, reset)
pio test -e native                     # host unit tests
tools/api_check.sh                     # live API smoke test (token from secrets.h)
tools/preview.sh 2200002               # live board rendered on this computer
python3 tools/portal_mock.py           # setup page on http://localhost:8765
tools/render_cyd.sh                    # CYD screens as PNGs, several student settings
```

**Before the workshop:** `default_envs = cyd` is set, so the VS Code Build/Upload
buttons target the CYD. Run `pio run -e cyd` once on every
laptop with internet so the toolchain and libraries are cached. Participants then
don't need to install anything.

## Secrets

```bash
cp include/secrets.example.h include/secrets.h   # then set API_TOKEN
```

`include/secrets.h` is git-ignored; the build fails with a clear message if it's
missing. The token is a dedicated read-only CodeWeek token. It is only used in
`DepartureApi` (Authorization header), never logged, never sent to the portal
page/browser, never shown on screen. The tools pass it to curl via a header file
descriptor, not on the command line. Note: anyone with physical access to a
device can read the token from flash (accepted risk; revoke the token in the
backend's `API_TOKENS` if needed).

## API contract (as used)

- `GET /v1/stops?query=<2–64 chars>` → `[{"id","name"}]` (portal stop search)
- `GET /v1/stops/<id>/departures?limit=10` → `{stop, sourceUpdatedAt, servedAt, stale, departures[]}`
  Each departure: `line, destination, scheduled, estimated|null, platformScheduled?,
  platformEstimated?, realtime, cancelled, partiallyCancelled, messages[]`.
- Header `Authorization: Bearer <token>` on all `/v1` calls.
- Errors `{"error":{"code","message"}}`; 401 → token problem, 404 → unknown stop,
  429 → rate limit, 502/503 → no (fresh enough) upstream data.

Only the fields above are parsed (ArduinoJson filter). Departures are sorted by
`estimated` else `scheduled`.

Polling: immediately after connecting, then every 60 s (the backend caches NVV
for ~60 s). Failures: 10 s, 20 s, 40 s … max 2 min (401/404: 2 min directly).
Between fetches the countdown is computed locally every second; a departure is
dropped 60 s after its time. 10 departures are requested so rows refill locally.

## Time

NTP (`pool.ntp.org`, `time.google.com`, `ptbtime1.ptb.de`) with TZ
`CET-1CEST,M3.5.0,M10.5.0/3` (Europe/Berlin incl. DST). If NTP is blocked, the
clock is set from the API's `servedAt` after the first successful fetch. Without
any clock, rows show the departure time (`13:25`) instead of minutes.

## Display states

| State | Screen |
|---|---|
| BOOTING | boot screen |
| UNCONFIGURED → CONFIG_PORTAL | setup screen: AP name, URL, current step |
| CONNECTING_WIFI | "Verbinde mit WLAN" (or last data marked KEIN WLAN) |
| FETCHING_DATA | "Lade Abfahrten" (first fetch only) |
| ONLINE | departures, no status bar |
| STALE | departures, footer `VERALTET · Stand hh:mm` (backend `stale`, our fetch failing, or data > 3 min old) |
| WIFI_ERROR | last data with `KEIN WLAN · Stand hh:mm`; without data: "Kein WLAN" screen |
| API_ERROR | "Keine Abfahrtsdaten" with reason (token / unknown stop / server) |

Cancelled departures show `CANCELLED_TEXT` (student setting, default
`entfällt`) in the time column (red), never as
a normal time. This is core logic, not participant code.

## NVS

Namespace `haltestelle`:

| Key | Type | Content |
|---|---|---|
| `wifi_ssid` | string | Wi-Fi name |
| `wifi_password` | string | Wi-Fi password (never sent to the browser) |
| `stop_id` | string | e.g. `2200002` (what the API needs) |
| `stop_name` | string | e.g. `Kassel Königsplatz` (display) |
| `configured` | bool | written last; `false` → setup portal on boot |

Participant design options are compile-time constants, not stored in NVS.

## Setup portal

When no valid config exists (or on reconfigure), the device opens an open access
point `Abfahrt-Setup-XXXX` (last MAC bytes) in AP+STA mode, answers all DNS
queries with `192.168.4.1` (captive portal) and serves one page:

| Route | |
|---|---|
| `GET /` | page (plain HTML/JS); every unknown path redirects here (captive checks) |
| `GET /config/state` | AP name, saved SSID, `hasPassword` flag, Wi-Fi test state |
| `GET /config/wifi/scan` | async scan; `{scanning, networks:[{ssid,quality,secure}]}` |
| `POST /config/wifi` | `ssid`, `password` → starts connection test |
| `GET /config/wifi/status` | `idle / connecting / connected / failed` |
| `GET /config/stops?q=` | ESP calls `/v1/stops` with the token, returns only `id`+`name` |
| `POST /config/save` | `stop_id`, `stop_name` → NVS, restart |

Notes: when the ESP joins the router, its AP follows the router's channel and
phones may briefly drop the setup network (the page tolerates this and tells the
user). On reconfigure the saved Wi-Fi is tried immediately and an empty password
field keeps the saved password. The portal closes after 10 min without use on an
already configured device. `SETUP_AP_PASSWORD` in `AppSettings.h` can make the
AP password-protected.

**Reconfigure:** hold BOOT (GPIO0) for 3 s, or Serial command `setup` (keeps
config, prefilled) / `reset` (clears NVS). Both restart the device with a flag
in RTC memory (`setupRequest`), so the portal always starts on the same clean
boot path as the first setup. `Display::setupGestureDetected()` is the hook for
a touch gesture (not used in v1).

## TLS

`api/RootCertificates.h` contains root CAs (not leaf/intermediate): Google Trust
Services R1/R4 + GlobalSign (current chain: WE1 → GTS Root R4, checked
2026-09-28), ISRG X1/X2 (Let's Encrypt), SSL.com 2022. If the API switches to
another CA, requests fail with a TLS error in the Serial log – add that root.

## CYD renderer

`CYDDisplay` only initialises the hardware (HSPI bus, ILI9341, rotation,
backlight) and forwards to `CYDRenderer`, which draws everything on an
`Adafruit_GFX&`. Nothing outside these two classes includes Adafruit headers.

- Layout for 320×240: header (stop name, clock) 30 px, rows fill the rest;
  the status bar (24 px) only appears when data is stale/offline.
- Rows: line number left, time right, platform and destination in between.
  Column widths come from the content within fixed limits; destinations are
  cut with "." to their column (measured in pixels), so they never reach the
  time column. With 3–4 rows, long destinations wrap onto two lines.
- `FONT_LED` (default): styled like the NVV stop signs – a 44 px blue panel
  (`BACKGROUND_COLOR`) with the stop name (without "Kassel "), clock and, if
  stale/offline, the status; below it a dark LED board filling the rest.
  Text is Adafruit's 5×7 font drawn LED by LED on a 2×3 px grid (2×4 px with
  ≤4 rows), proportional letters, ~26 characters per row, real umlauts (CP437
  glyphs). Rows share the board height evenly. Unlit LEDs are a faint version
  of `TEXT_COLOR`.
- Message ticker: the model's `ticker` (messages of the upcoming departures,
  e.g. "RT1: Fahrplanänderungen …", each once, joined with `+++`) takes the
  last row and scrolls right-to-left (2 px every 40 ms, `CYDDisplay::loop`).
  It pauses while an API request is running (~1–3 s per minute). The Serial
  display prints it as a `»` line.
- Other fonts: `FONT_MODERN` FreeSans, `FONT_CLASSIC` FreeSerif (12 pt with ≤4 rows,
  else 9 pt), `FONT_COMPACT` always 9 pt, `FONT_PIXEL` built-in 6×8 font ×2.
  The GFX fonts are ASCII-only: umlauts are drawn as vowel + two dots, `ß` as
  `ss`. The pixel font uses its CP437 umlaut glyphs.
- Each band (header, row, status bar) is drawn into a `GFXcanvas16` and
  copied in one go: no flicker on minute updates; only changed rows are redrawn.
- `tools/render_cyd.sh` compiles the same renderer + Adafruit GFX for the host
  and writes PNGs of all screens for several student settings to
  `.pio/cyd-render/` – for checking layout changes without flashing.

## Remaining hardware TODOs

```bash
grep -rn "TODO(HARDWARE)" src platformio.ini
```

- Rotation 1 vs 3 (both landscape) depending on the USB port position in the
  enclosure (`ROTATION` in `CYDDisplay.cpp`).
- Final visual tuning on the assembled device: spacing, font sizes,
  readability from a distance, colours behind the enclosure window
  (`layout::` in `CYDRenderer.cpp`).
- BOOT button reachable in the enclosure (`AppSettings.h`).
- Optional later: backlight dimming (PWM on GPIO 21), touch (XPT2046) as a
  second way to open setup via `Display::setupGestureDetected()`.

## Tests

- `pio test -e native` – parser (real captured responses in `test/fixtures/`),
  time/DST, UTF-8 truncation, display model (count, countdown, cancellations,
  stale), text board layout, config validation. Expectations that depend on
  participant settings are derived from them, so tests still pass after changes.
- `tools/api_check.sh` – live: health, 401 without/with wrong token, search,
  departures.
- `tools/preview.sh` – live board rendered on the host with the participant
  settings.
- `tools/portal_mock.py` – the real portal page against simulated ESP routes.

- `tools/render_cyd.sh` – CYD screens rendered on the host (same drawing code).

Still to be checked on the device: Wi-Fi, TLS handshake on the ESP32, NVS,
captive-portal behaviour on real phones, the full firmware on the panel.
