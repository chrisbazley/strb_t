#!/usr/bin/env bash
set -euo pipefail
source "$GITHUB_ACTION_PATH/snapshot.env"
archive="$RUNNER_TEMP/sdcc-aarch64.tar.bz2"
if [[ ! -f "$archive" ]]; then
  curl --fail --location --retry 3 --max-time 180 \
    "https://github.com/chrisbazley/strb_t/releases/download/sdcc-r$REVISION/$ARCHIVE" \
    --output "$archive"
fi
printf '%s  %s\n' "$SHA256" "$archive" | sha256sum --check
tar --no-same-owner -xjf "$archive" -C "$RUNNER_TEMP"
bin="$RUNNER_TEMP/sdcc/bin"
"$bin/sdcc" --version
# Ensure the selected compiler actually implements the warning used by CI.
probe=$(mktemp -d)
trap 'rm -rf "$probe"' EXIT
printf 'int *probe(void) { return 0; }\n' > "$probe/probe.c"
"$bin/sdcc" -mz80 --enable-warning 378 -S "$probe/probe.c" -o "$probe/probe.asm" > "$probe/diagnostics" 2>&1 || { cat "$probe/diagnostics"; exit 1; }
grep -F 'warning 378:' "$probe/diagnostics"
echo "$bin" >> "$GITHUB_PATH"
