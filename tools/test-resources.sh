#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Optional integration-test setup. Resource binaries come from Common's release;
# Calendar neither builds nor republishes the shared capability package.
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^INFILTRATR_TEMPORAL_RESOURCES_VERSION := //p' "$ROOT/Makefile")
DIRECTORY=${1:-"$ROOT/build/test-resources"}
mkdir -p "$DIRECTORY"
DIRECTORY=$(CDPATH='' cd -- "$DIRECTORY" && pwd)
NAME="infiltrator-calendar-resources_${VERSION}_amd64.deb"
BASE="https://github.com/Infiltrator-Projects/Libraries/releases/download/v${VERSION}"
curl -fL --retry 3 "$BASE/$NAME" -o "$DIRECTORY/$NAME"
curl -fL --retry 3 "$BASE/SHA256SUMS.txt" -o "$DIRECTORY/SHA256SUMS.txt"
(cd "$DIRECTORY" && grep "  $NAME\$" SHA256SUMS.txt | sha256sum --check -)
if [ "$(id -u)" -eq 0 ]; then
    apt-get install -y "$DIRECTORY/$NAME"
else
    sudo apt-get install -y "$DIRECTORY/$NAME"
fi
printf '%s\n' "$DIRECTORY/$NAME"
