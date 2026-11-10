#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Calendar is a consumer; each provider owns its own qualified release.
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
DIRECTORY=${1:-"$ROOT/build/test-resources"}
mkdir -p "$DIRECTORY"
DIRECTORY=$(CDPATH='' cd -- "$DIRECTORY" && pwd)
if [ -f "$DIRECTORY/providers.json" ]; then
    bash "$ROOT/tools/temporal-packages.sh" verify "$DIRECTORY"
else
    bash "$ROOT/tools/temporal-packages.sh" fetch "$DIRECTORY"
fi
bash "$ROOT/tools/temporal-packages.sh" install "$DIRECTORY"
printf '%s\n' "$DIRECTORY"
