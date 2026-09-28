# Sourced by the tools: reads API_BASE_URL / API_TOKEN from include/secrets.h.
# The token is only ever passed to curl through a header file on stdin/fd,
# never on the command line (visible in `ps`) and never printed.
SECRETS_FILE="$(dirname "${BASH_SOURCE[0]}")/../include/secrets.h"
if [[ ! -f "$SECRETS_FILE" ]]; then
  echo "include/secrets.h fehlt (Vorlage: include/secrets.example.h)" >&2
  exit 1
fi
API_BASE_URL=$(sed -n 's/^#define API_BASE_URL "\(.*\)"/\1/p' "$SECRETS_FILE")
API_TOKEN=$(sed -n 's/^#define API_TOKEN "\(.*\)"/\1/p' "$SECRETS_FILE")
if [[ -z "$API_TOKEN" || "$API_TOKEN" == "replace-me" ]]; then
  echo "API_TOKEN in include/secrets.h ist nicht gesetzt" >&2
  exit 1
fi

# api_get <path-and-query> [extra curl args...]
api_get() {
  local path="$1"; shift
  curl -sS --max-time 15 -H @<(printf 'Authorization: Bearer %s\n' "$API_TOKEN") "$@" "$API_BASE_URL$path"
}
