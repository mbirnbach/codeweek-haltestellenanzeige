#!/usr/bin/env bash
# Smoke test of the live departure API with the token from include/secrets.h.
#   tools/api_check.sh [search-text] [stop-id]
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/_secrets.sh

QUERY="${1:-königsplatz}"
STOP="${2:-2200002}"
ENC_QUERY=$(python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$QUERY")
fail=0
check() { if [[ "$2" == "$3" ]]; then echo "ok    $1"; else echo "FAIL  $1 (expected $2, got $3)"; fail=1; fi; }

echo "API: $API_BASE_URL"
check "GET /healthz" 200 "$(curl -s -o /dev/null -w '%{http_code}' --max-time 15 "$API_BASE_URL/healthz")"
check "GET /v1/stops without token -> 401" 401 \
  "$(curl -s -o /dev/null -w '%{http_code}' --max-time 15 "$API_BASE_URL/v1/stops?query=$ENC_QUERY")"
check "GET /v1/stops with wrong token -> 401" 401 \
  "$(curl -s -o /dev/null -w '%{http_code}' --max-time 15 -H 'Authorization: Bearer wrong-token-000000000000000000000000' "$API_BASE_URL/v1/stops?query=$ENC_QUERY")"
check "GET /v1/stops?query=$QUERY with token" 200 "$(api_get "/v1/stops?query=$ENC_QUERY" -o /dev/null -w '%{http_code}')"
check "GET /v1/stops/$STOP/departures with token" 200 \
  "$(api_get "/v1/stops/$STOP/departures?limit=6" -o /dev/null -w '%{http_code}')"

echo
echo "Stop search '$QUERY':"
api_get "/v1/stops?query=$ENC_QUERY" | python3 -c 'import json,sys
for s in json.load(sys.stdin)[:5]: print("  %-8s %s" % (s["id"], s["name"]))'
exit $fail
