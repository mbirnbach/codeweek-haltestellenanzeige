#!/usr/bin/env bash
# =============================================================================
# CodeWeek Kassel – Haltestellenanzeige: Workshop-Rechner einrichten
#
# Für Ubuntu / Linux Mint / Debian. Ergebnis: Im Konto "schueler" liegt auf
# dem Schreibtisch ein Starter "Haltestellenanzeige", der VS Code mit dem
# Projekt öffnet. Bauen und Flashen gehen danach ohne Internet.
#
# Alles Projektbezogene liegt im Home-Ordner von "schueler" (Projekt, Token,
# PlatformIO mit Compiler, VS-Code-Erweiterungen und -Einstellungen) und
# verschwindet, wenn das Konto nach dem Workshop zurückgesetzt wird.
#
# Systemweit (bleibt nach dem Zurücksetzen) ändert das Skript nur:
#   - Pakete git, python3-venv, curl usw. und VS Code (+ Microsoft-Paketquelle)
#   - udev-Regel für die USB-Adapter der Displays, brltty entfernt
#   - "schueler" in der Gruppe dialout
# Was davon wirklich geändert wurde, merkt sich das Skript in
# /var/lib/codeweek-workshop/state – "--cleanup" macht genau das rückgängig.
#
# Aufruf im Konto "lehrer" (braucht Internet und sudo):
#
#   curl -fsSL https://raw.githubusercontent.com/mbirnbach/codeweek-haltestellenanzeige/main/tools/setup_workshop_linux.sh -o setup.sh
#   sudo bash setup.sh                 # einrichten (fragt nach dem API-Token)
#   sudo bash setup.sh --cleanup       # nach dem Workshop: Systemänderungen zurücknehmen
#
# Erneut ausführen = neuesten Projektstand holen (Änderungen der Kinder
# werden vorher mit "git stash" gesichert).
#
# Optionen:
#   --user NAME        Konto der Teilnehmenden (Standard: schueler)
#   --secrets DATEI    fertige secrets.h verwenden statt Token abzufragen
#   --branch NAME      Git-Branch (Standard: main)
#   --cleanup          Systemänderungen rückgängig machen (siehe oben)
# =============================================================================
set -euo pipefail

REPO_URL="https://github.com/mbirnbach/codeweek-haltestellenanzeige.git"
REPO_NAME="codeweek-haltestellenanzeige"
TARGET_USER="schueler"
BRANCH="main"
SECRETS_FILE=""
MODE="setup"
API_BASE_URL_DEFAULT="https://nvv-api.edupixel.de"
PIO_UDEV_RULES_URL="https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules"
PIO_INSTALLER_URL="https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py"
UDEV_RULE=/etc/udev/rules.d/99-platformio-udev.rules
STATE_DIR=/var/lib/codeweek-workshop
STATE_FILE="$STATE_DIR/state"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --user) TARGET_USER="$2"; shift 2 ;;
    --secrets) SECRETS_FILE="$2"; shift 2 ;;
    --branch) BRANCH="$2"; shift 2 ;;
    --cleanup) MODE="cleanup"; shift ;;
    -h|--help) sed -n '2,38p' "$0"; exit 0 ;;
    *) echo "Unbekannte Option: $1" >&2; exit 1 ;;
  esac
done

# --- Hilfsfunktionen ---------------------------------------------------------
STEP=0
step() { STEP=$((STEP + 1)); printf '\n\033[1;34m==> [%d] %s\033[0m\n' "$STEP" "$*"; }
ok()   { printf '    \033[32m✓\033[0m %s\n' "$*"; }
warn() { printf '    \033[33m!\033[0m %s\n' "$*"; }
die()  { printf '\n\033[1;31mFEHLER:\033[0m %s\n' "$*" >&2; exit 1; }
# Merkt sich eine Systemänderung für --cleanup (nur einmal pro Eintrag).
remember() { install -d "$STATE_DIR"; grep -qx "$1" "$STATE_FILE" 2>/dev/null || echo "$1" >> "$STATE_FILE"; }
remembered() { grep -qx "$1" "$STATE_FILE" 2>/dev/null; }
# Wirklich installiert? ("dpkg -s" meldet auch entfernte Pakete mit Restdateien.)
installed() { dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "install ok installed"; }

[[ $EUID -eq 0 ]] || die "Bitte mit sudo starten:  sudo bash $0"
command -v apt-get >/dev/null || die "Dieses Skript ist für Ubuntu, Linux Mint oder Debian (apt)."
export DEBIAN_FRONTEND=noninteractive
# Automatische Updates halten oft kurz nach dem Start die apt-Sperre:
# bis zu 5 Minuten warten statt abzubrechen.
apt-get() { command apt-get -o DPkg::Lock::Timeout=300 "$@"; }

# =============================================================================
# Aufräumen nach dem Workshop
# =============================================================================
if [[ "$MODE" == "cleanup" ]]; then
  echo "Nimmt die Systemänderungen des Setup-Skripts zurück."
  echo "(Den Home-Ordner von '$TARGET_USER' bitte wie geplant zurücksetzen.)"
  [[ -f "$STATE_FILE" ]] || warn "Keine Aufzeichnungen in $STATE_FILE – nur das Nötigste wird geprüft."
  step "Systemänderungen zurücknehmen"
  if remembered "installed-vscode"; then
    apt-get purge -y -qq code >/dev/null 2>&1 && ok "VS Code entfernt"
  else
    ok "VS Code war schon vorher installiert – bleibt"
  fi
  if remembered "added-vscode-repo"; then
    rm -f /etc/apt/sources.list.d/vscode.list /etc/apt/keyrings/packages.microsoft.gpg
    ok "Microsoft-Paketquelle entfernt"
  fi
  if remembered "installed-udev-rule" || [[ -f "$UDEV_RULE" ]]; then
    rm -f "$UDEV_RULE"
    { udevadm control --reload-rules && udevadm trigger; } >/dev/null 2>&1 || true
    ok "udev-Regel entfernt"
  fi
  if remembered "removed-brltty"; then
    apt-get install -y -qq brltty >/dev/null 2>&1 && ok "brltty wieder installiert"
  fi
  if remembered "added-dialout" && id "$TARGET_USER" >/dev/null 2>&1; then
    gpasswd -d "$TARGET_USER" dialout >/dev/null 2>&1 || true
    ok "$TARGET_USER aus Gruppe dialout entfernt"
  fi
  # Pakete wie git/python3-venv/curl bleiben (harmlos, oft schon vorher da).
  rm -rf "$STATE_DIR"
  echo
  printf '\033[1;32mFertig.\033[0m Jetzt noch das Konto "%s" zurücksetzen.\n' "$TARGET_USER"
  exit 0
fi

# =============================================================================
# Einrichten
# =============================================================================
id "$TARGET_USER" >/dev/null 2>&1 || die "Benutzer '$TARGET_USER' gibt es nicht (Option --user)."
USER_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
USER_GROUP=$(id -gn "$TARGET_USER")
PROJECT="$USER_HOME/$REPO_NAME"
PIO="$USER_HOME/.platformio/penv/bin/pio"
SYS_LANG=$(sed -n 's/^LANG=//p' /etc/default/locale 2>/dev/null | tr -d '"' | head -1)

# Befehl im Konto der Teilnehmenden ausführen (dessen HOME, Systemsprache).
as_user() {
  sudo -u "$TARGET_USER" -H env PATH="$USER_HOME/.local/bin:/usr/local/bin:/usr/bin:/bin" \
    LANG="${SYS_LANG:-C.UTF-8}" "$@"
}

if [[ -n "$SECRETS_FILE" && ! -f "$SECRETS_FILE" ]]; then die "Datei $SECRETS_FILE nicht gefunden."; fi
API_TOKEN="${API_TOKEN:-}"
if [[ -z "$SECRETS_FILE" && -z "$API_TOKEN" && ! -f "$PROJECT/include/secrets.h" ]]; then
  echo
  echo "Der API-Token für $API_BASE_URL_DEFAULT wird gebraucht (vom Projektteam)."
  read -r -s -p "API-Token eingeben (Eingabe wird nicht angezeigt): " API_TOKEN < /dev/tty
  echo
  [[ -n "$API_TOKEN" ]] || die "Kein Token eingegeben."
fi

echo
echo "Richte ein für '$TARGET_USER' – Projekt: $PROJECT"

# --- 1. Systempakete ---------------------------------------------------------
step "Systempakete installieren"
apt-get update -qq
apt-get install -y -qq git curl wget ca-certificates gpg python3 python3-venv python3-pip \
  xdg-user-dirs libglib2.0-bin >/dev/null
ok "git, python3, python3-venv, curl installiert"

# --- 2. USB-Zugriff ----------------------------------------------------------
step "USB-Zugriff auf die Displays (CH340) einrichten"
if installed brltty; then
  # brltty (für Braillezeilen) belegt unter Ubuntu CH340-Adapter -> kein Port.
  apt-get remove -y -qq brltty >/dev/null
  remember "removed-brltty"
  ok "brltty entfernt (hat USB-Seriell-Adapter blockiert; --cleanup installiert es wieder)"
else
  ok "brltty nicht installiert"
fi
install -d /etc/udev/rules.d
[[ -f "$UDEV_RULE" ]] || remember "installed-udev-rule"
curl -fsSL "$PIO_UDEV_RULES_URL" -o "$UDEV_RULE"
{ udevadm control --reload-rules && udevadm trigger; } >/dev/null 2>&1 || true
ok "PlatformIO-udev-Regel installiert (Display-Port für Benutzer freigegeben)"
if getent group dialout >/dev/null && ! id -nG "$TARGET_USER" | grep -qw dialout; then
  usermod -aG dialout "$TARGET_USER"
  remember "added-dialout"
fi
ok "$TARGET_USER ist in der Gruppe dialout"

# --- 3. VS Code --------------------------------------------------------------
step "VS Code installieren"
if installed code; then
  ok "VS Code ist installiert ($(dpkg-query -W -f='${Version}' code))"
else
  install -d -m 0755 /etc/apt/keyrings
  curl -fsSL https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor --yes -o /etc/apt/keyrings/packages.microsoft.gpg
  chmod 0644 /etc/apt/keyrings/packages.microsoft.gpg
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" \
    > /etc/apt/sources.list.d/vscode.list
  remember "added-vscode-repo"
  apt-get update -qq
  apt-get install -y -qq code >/dev/null
  remember "installed-vscode"
  ok "VS Code installiert ($(dpkg-query -W -f='${Version}' code))"
fi

# --- 4. PlatformIO Core (im Home-Ordner) -------------------------------------
step "PlatformIO Core für '$TARGET_USER' installieren"
if [[ -x "$PIO" ]]; then
  ok "PlatformIO ist schon installiert ($(as_user "$PIO" --version))"
else
  TMP_INSTALLER=$(mktemp /tmp/get-platformio.XXXXXX.py)
  curl -fsSL "$PIO_INSTALLER_URL" -o "$TMP_INSTALLER"
  chmod 0644 "$TMP_INSTALLER"
  as_user python3 "$TMP_INSTALLER" >/dev/null
  rm -f "$TMP_INSTALLER"
  ok "PlatformIO installiert ($(as_user "$PIO" --version))"
fi
as_user mkdir -p "$USER_HOME/.local/bin"
for cmd in pio platformio; do as_user ln -sf "$USER_HOME/.platformio/penv/bin/$cmd" "$USER_HOME/.local/bin/$cmd"; done
ok "pio auch im Terminal verfügbar (~/.local/bin)"

# --- 5. VS-Code-Erweiterungen und Einstellungen ------------------------------
step "VS-Code-Erweiterungen installieren"
for ext in platformio.platformio-ide ms-vscode.cpptools ms-ceintl.vscode-language-pack-de; do
  as_user code --install-extension "$ext" --force >/dev/null 2>&1 \
    || die "Erweiterung $ext ließ sich nicht installieren (Internet?)."
  ok "$ext"
done

step "VS-Code-Einstellungen für Anfänger"
as_user mkdir -p "$USER_HOME/.config/Code/User" "$USER_HOME/.vscode"
as_user python3 - "$USER_HOME/.config/Code/User/settings.json" "$USER_HOME/.vscode/argv.json" <<'PY'
import json, os, re, sys

def load(path):
    try:
        with open(path) as f:
            return json.loads(re.sub(r"^\s*//.*$", "", f.read(), flags=re.M))
    except Exception:
        return {}

settings_path, argv_path = sys.argv[1], sys.argv[2]
settings = load(settings_path)
settings.update({
    # Keine Rückfragen und Startseiten
    "security.workspace.trust.enabled": False,
    "workbench.startupEditor": "none",
    "workbench.welcomePage.walkthroughs.openOnInstall": False,
    "workbench.tips.enabled": False,
    "extensions.ignoreRecommendations": True,
    # Nichts aktualisiert sich während des Workshops
    "update.mode": "none",
    "extensions.autoUpdate": False,
    "extensions.autoCheckUpdates": False,
    "telemetry.telemetryLevel": "off",
    # Änderungen werden immer gespeichert, auch vor Build/Upload
    "files.autoSave": "afterDelay",
    "files.autoSaveDelay": 500,
    "task.saveBeforeRun": "always",
    # Ruhige Oberfläche
    "editor.minimap.enabled": False,
    "git.enabled": False,
    "chat.disableAIFeatures": True,
    # PlatformIO: den installierten Core benutzen, keine Startseite
    "platformio-ide.useBuiltinPIOCore": True,
    "platformio-ide.disablePIOHomeStartup": True,
    "platformio-ide.activateOnlyOnPlatformIOProject": True,
})
with open(settings_path, "w") as f:
    json.dump(settings, f, indent=2)

argv = load(argv_path)
argv["locale"] = "de"
with open(argv_path, "w") as f:
    json.dump(argv, f, indent=2)
PY
ok "Autospeichern, keine Vertrauens-Abfrage, keine Updates, deutsche Oberfläche"

# --- 6. Projekt ----------------------------------------------------------------
step "Projekt holen"
if [[ -d "$PROJECT/.git" ]]; then
  if [[ -n "$(as_user git -C "$PROJECT" status --porcelain --untracked-files=no)" ]]; then
    as_user git -C "$PROJECT" stash push -q -m "setup_workshop_linux $(date +%F_%T)"
    warn "Änderungen im Projekt gesichert (git stash) – neuester Stand wird geholt."
  fi
  as_user git -C "$PROJECT" fetch -q origin "$BRANCH"
  as_user git -C "$PROJECT" checkout -q "$BRANCH"
  as_user git -C "$PROJECT" reset -q --hard "origin/$BRANCH"
  ok "Auf dem neuesten Stand ($(as_user git -C "$PROJECT" log -1 --format='%h %s'))"
else
  as_user git clone -q --branch "$BRANCH" "$REPO_URL" "$PROJECT"
  ok "Geklont nach $PROJECT ($(as_user git -C "$PROJECT" log -1 --format='%h %s'))"
fi

step "API-Token (include/secrets.h)"
SECRETS="$PROJECT/include/secrets.h"
if [[ -n "$SECRETS_FILE" ]]; then
  install -m 0600 -o "$TARGET_USER" -g "$USER_GROUP" "$SECRETS_FILE" "$SECRETS"
  ok "secrets.h aus $SECRETS_FILE übernommen"
elif [[ -n "$API_TOKEN" ]]; then
  [[ "$API_TOKEN" =~ ^[A-Za-z0-9._~-]+$ ]] || die "Der Token enthält ungültige Zeichen."
  ( umask 077; cat > "$SECRETS" <<EOF
#pragma once

// Erzeugt von tools/setup_workshop_linux.sh – nicht weitergeben, nicht committen.
#define API_BASE_URL "$API_BASE_URL_DEFAULT"
#define API_TOKEN "$API_TOKEN"
EOF
  )
  chown "$TARGET_USER:$USER_GROUP" "$SECRETS"
  ok "secrets.h angelegt"
else
  ok "secrets.h ist schon vorhanden"
fi
grep -q 'replace-me' "$SECRETS" && die "secrets.h enthält noch den Platzhalter 'replace-me'."
TOKEN_VALUE=$(sed -n 's/^#define API_TOKEN "\(.*\)"/\1/p' "$SECRETS")
BASE_VALUE=$(sed -n 's/^#define API_BASE_URL "\(.*\)"/\1/p' "$SECRETS")
HTTP=$(curl -s -o /dev/null -w '%{http_code}' --max-time 20 \
  -H @<(printf 'Authorization: Bearer %s\n' "$TOKEN_VALUE") "$BASE_VALUE/v1/stops?query=Kirchweg" || true)
case "$HTTP" in
  200) ok "Token funktioniert (Testabfrage an $BASE_VALUE)" ;;
  401) die "Der Server lehnt den Token ab (401). Bitte Token prüfen." ;;
  *)   warn "Server-Test nicht eindeutig (HTTP $HTTP) – Internet prüfen. Es geht trotzdem weiter." ;;
esac
unset TOKEN_VALUE API_TOKEN

# --- 7. Einmal bauen -----------------------------------------------------------
step "Firmware einmal bauen (lädt Compiler und Bibliotheken – beim ersten Mal einige Minuten)"
BUILD_LOG=$(mktemp /tmp/codeweek-build.XXXXXX.log)
if ! as_user bash -c "cd '$PROJECT' && '$PIO' run -e cyd" > "$BUILD_LOG" 2>&1; then
  tail -30 "$BUILD_LOG"
  die "Build fehlgeschlagen (Log: $BUILD_LOG)"
fi
ok "Build erfolgreich: $(grep -E '^Flash:' "$BUILD_LOG" | head -1)"
# PlatformIO lädt das Upload-Werkzeug (esptool) erst beim ersten Upload.
# Upload ohne Gerät anstoßen: lädt das Werkzeug in der passenden Version
# und scheitert dann erwartungsgemäß. So braucht der Workshop kein Internet.
as_user bash -c "cd '$PROJECT' && '$PIO' run -e cyd -t upload --upload-port /dev/kein-geraet" >/dev/null 2>&1 || true
if [[ -d "$USER_HOME/.platformio/packages/tool-esptoolpy" ]]; then
  ok "Upload-Werkzeug (esptool) vorab geladen"
else
  warn "Upload-Werkzeug nicht vorab geladen – beim ersten Upload wird Internet gebraucht"
fi
as_user bash -c "cd '$PROJECT' && '$PIO' project init --ide vscode -e cyd" >/dev/null 2>&1 \
  && ok "VS-Code-Projektdateien erzeugt" || warn "VS-Code-Projektdateien nicht erzeugt (macht VS Code beim Öffnen selbst)"

# --- 8. Starter --------------------------------------------------------------
step "Starter auf dem Schreibtisch und im Anwendungsmenü"
# Schreibtisch-Ordner wie bei der Anmeldung anlegen (Name nach Systemsprache,
# z.B. "Schreibtisch"), damit der Starter im richtigen Ordner landet.
as_user xdg-user-dirs-update >/dev/null 2>&1 || true
DESKTOP_DIR=$(as_user xdg-user-dir DESKTOP 2>/dev/null || true)
[[ -n "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$USER_HOME" ]] || DESKTOP_DIR="$USER_HOME/Desktop"
as_user mkdir -p "$DESKTOP_DIR" "$USER_HOME/.local/share/applications"

OPEN_SCRIPT="$USER_HOME/.local/bin/haltestellenanzeige-oeffnen"
as_user tee "$OPEN_SCRIPT" >/dev/null <<EOF
#!/usr/bin/env bash
# Öffnet das CodeWeek-Projekt mit der Teilnehmer-Datei in VS Code.
exec code "$PROJECT" "$PROJECT/src/student/display_config.h"
EOF
as_user chmod +x "$OPEN_SCRIPT"

LAUNCHER_CONTENT="[Desktop Entry]
Type=Application
Name=Haltestellenanzeige
Comment=CodeWeek-Projekt in VS Code öffnen
Exec=$OPEN_SCRIPT
Icon=vscode
Terminal=false
Categories=Development;"
printf '%s\n' "$LAUNCHER_CONTENT" | as_user tee "$USER_HOME/.local/share/applications/haltestellenanzeige.desktop" >/dev/null
printf '%s\n' "$LAUNCHER_CONTENT" | as_user tee "$DESKTOP_DIR/haltestellenanzeige.desktop" >/dev/null
as_user chmod +x "$DESKTOP_DIR/haltestellenanzeige.desktop"
# Ubuntu/GNOME: als vertrauenswürdig markieren (klappt nur mit Sitzung, sonst
# einmal per Rechtsklick "Starten erlauben").
as_user dbus-launch gio set "$DESKTOP_DIR/haltestellenanzeige.desktop" metadata::trusted true >/dev/null 2>&1 || true
ok "$DESKTOP_DIR/haltestellenanzeige.desktop"

# --- 9. Abschlussprüfung -----------------------------------------------------
step "Abschlussprüfung"
FAIL=0
check() { if eval "$2" >/dev/null 2>&1; then ok "$1"; else warn "NICHT OK: $1"; FAIL=1; fi; }
check "VS Code vorhanden"                         "command -v code"
check "PlatformIO-Erweiterung installiert"        "as_user code --list-extensions | grep -qi platformio.platformio-ide"
check "Deutsches Sprachpaket installiert"         "as_user code --list-extensions | grep -qi vscode-language-pack-de"
check "pio funktioniert als $TARGET_USER"          "as_user pio --version"
check "Projekt vorhanden"                         "test -f '$PROJECT/platformio.ini'"
check "secrets.h vorhanden"                       "test -f '$SECRETS'"
check "Firmware gebaut"                           "test -f '$PROJECT/.pio/build/cyd/firmware.bin'"
check "Upload-Werkzeug vorhanden (offline-fähig)" "test -d '$USER_HOME/.platformio/packages/tool-esptoolpy'"
check "Starter auf dem Schreibtisch"              "test -x '$DESKTOP_DIR/haltestellenanzeige.desktop'"
check "udev-Regel vorhanden"                      "test -f '$UDEV_RULE'"
check "brltty nicht installiert"                  "! installed brltty"
check "Alle Dateien gehören $TARGET_USER"          "test -z \"\$(find '$USER_HOME' -xdev ! -user '$TARGET_USER' -print -quit)\""
if ls /dev/ttyUSB* >/dev/null 2>&1; then
  ok "Display angeschlossen: $(ls /dev/ttyUSB* | tr '\n' ' ')"
else
  warn "Kein Display angeschlossen (zum Testen eines anstecken: es erscheint als /dev/ttyUSB0)"
fi

echo
if [[ $FAIL -eq 0 ]]; then printf '\033[1;32mFertig!\033[0m\n'; else printf '\033[1;33mFertig, aber mit Warnungen (siehe oben).\033[0m\n'; fi
cat <<EOF

Nächste Schritte:
  1. Rechner neu starten.
  2. Als '$TARGET_USER' anmelden, auf dem Schreibtisch "Haltestellenanzeige" öffnen.
     (Ubuntu fragt evtl. einmal "Starten erlauben" – Rechtsklick auf den Starter.
      Alternativ im Anwendungsmenü "Haltestellenanzeige" suchen.)
  3. Display anstecken, unten in der blauen Leiste auf → (Upload) klicken.

Zwischen zwei Gruppen die Teilnehmer-Dateien zurücksetzen (ohne Internet):
  sudo -u $TARGET_USER $PROJECT/tools/workshop_reset.sh

Nach dem Workshop:
  sudo bash $0 --cleanup      (Systemänderungen zurücknehmen)
  danach das Konto '$TARGET_USER' zurücksetzen (entfernt Projekt, Token, PlatformIO).
EOF
