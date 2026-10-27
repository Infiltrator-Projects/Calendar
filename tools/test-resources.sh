#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Calendar is a consumer; each provider owns its own qualified release.
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
DIRECTORY=${1:-"$ROOT/build/test-resources"}
mkdir -p "$DIRECTORY"
DIRECTORY=$(CDPATH='' cd -- "$DIRECTORY" && pwd)
if [ -f "$DIRECTORY/providers.json" ]; then
    python3 "$ROOT/tools/temporal-packages.py" verify "$DIRECTORY"
else
    python3 "$ROOT/tools/temporal-packages.py" fetch "$DIRECTORY"
fi
python3 "$ROOT/tools/temporal-packages.py" install "$DIRECTORY"
printf '%s\n' "$DIRECTORY"
