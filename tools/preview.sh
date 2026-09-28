#!/usr/bin/env bash
# Shows live departures exactly like the Serial display, on this computer.
# Uses the participant settings in src/student/ - no ESP32 needed.
#
#   tools/preview.sh [stop-id] [--no-color] [--parsed]
#   tools/preview.sh 2200002
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/_secrets.sh

STOP="${1:-2200002}"
shift || true

pio run -s -e preview
api_get "/v1/stops/$STOP/departures?limit=10" --fail-with-body | .pio/build/preview/program "$@"
