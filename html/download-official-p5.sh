#!/usr/bin/env sh
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DESTINATION="$SCRIPT_DIR/lib/p5.min.js"
URL="https://cdn.jsdelivr.net/npm/p5@1.11.11/lib/p5.min.js"

printf '%s\n' "Downloading official p5.js 1.11.11..."
if command -v curl >/dev/null 2>&1; then
  curl --fail --location "$URL" --output "$DESTINATION"
elif command -v wget >/dev/null 2>&1; then
  wget --output-document="$DESTINATION" "$URL"
else
  printf '%s\n' "Error: install curl or wget, then run this script again." >&2
  exit 1
fi
printf '%s\n' "Saved to $DESTINATION"
printf '%s\n' "Open index-local-p5.html to use the local official library."
