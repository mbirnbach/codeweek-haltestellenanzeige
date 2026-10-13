#!/usr/bin/env bash
# Setzt die Teilnehmer-Dateien (src/student/) auf den Originalzustand zurück,
# z.B. zwischen zwei Workshop-Gruppen. Alles andere bleibt, auch secrets.h
# und die bereits heruntergeladenen Werkzeuge. Braucht kein Internet.
#
#   tools/workshop_reset.sh            (im Konto der Teilnehmenden)
#   sudo -u schueler ~schueler/codeweek-haltestellenanzeige/tools/workshop_reset.sh
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ -n "$(git status --porcelain -- src/student)" ]]; then
  # Sicherheitskopie, falls doch noch jemand seine Version braucht.
  backup="$HOME/workshop-sicherung/$(date +%F_%H-%M-%S)"
  mkdir -p "$backup"
  cp -r src/student "$backup/"
  echo "Sicherung der alten Dateien: $backup"
fi
git checkout -- src/student
git clean -fdq -- src/student
echo "src/student/ ist wieder im Originalzustand."
