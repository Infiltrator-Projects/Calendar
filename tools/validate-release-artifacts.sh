#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Calendar publishes its application and native builder; Common publishes resources.
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(cat "$ROOT/VERSION")
COMMON_VERSION=$(sed -n 's/^INFILTRATR_COMMON_VERSION := //p' "$ROOT/Makefile")
DIST=${1:-"$ROOT/dist"}
ARCH=amd64
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
DEB="$DIST/calendar_${VERSION}_${ARCH}.deb"
RUN="$DIST/calendar-${VERSION}-local-folder.run"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
for artifact in "$DEB" "$RUN"; do test -s "$artifact"; done
test "$(find "$DIST" -maxdepth 1 -type f | wc -l)" -eq 2
test -x "$RUN"
test "$(dpkg-deb -f "$DEB" Package)" = calendar
test "$(dpkg-deb -f "$DEB" Version)" = "$VERSION"
test "$(dpkg-deb -f "$DEB" Architecture)" = "$ARCH"
dpkg-deb -x "$DEB" "$TMP/generic"
STAGE="$TMP/generic"
test -f "$STAGE/usr/lib/$MULTIARCH/libcalendar.so.0.0.0"
test -f "$STAGE/usr/lib/$MULTIARCH/girepository-1.0/Calendar-1.0.typelib"
test -x "$STAGE/usr/libexec/calendar-about"
test -f "$STAGE/usr/share/cinnamon/applets/calendar@the-infiltratr/applet.js"
test -f "$STAGE/usr/share/cinnamon/applets/calendar@the-infiltratr/stylesheet.css"
test -f "$STAGE/usr/share/cinnamon/applets/calendar@the-infiltratr/icon.png"
test ! -e "$STAGE/usr/share/cinnamon/applets/calendar@the-infiltratr/settings.py"
test -f "$STAGE/usr/share/locale/en_AU/LC_MESSAGES/calendar@the-infiltratr.mo"
test ! -e "$STAGE/System/Resources"
test ! -e "$STAGE/usr/share/fonts/truetype/calendar"
"$STAGE/usr/libexec/calendar-about" --print-metadata | grep -qx "version=$VERSION"
"$STAGE/usr/libexec/calendar-about" --print-metadata | grep -qx 'build-profile=generic'
grep -qx "Shared C library: Libraries $COMMON_VERSION" "$STAGE/usr/share/doc/calendar/BUILD-INFO"
if grep -q -- '-march=native' "$STAGE/usr/share/doc/calendar/BUILD-INFO"; then
    echo 'Generic application package contains native-only tuning' >&2
    exit 1
fi
if readelf -d "$STAGE/usr/lib/$MULTIARCH/libcalendar.so.0.0.0" | grep -Eq 'Shared library: \[libicu'; then
    echo 'Calendar application must not acquire the resource ICU dependency' >&2
    exit 1
fi
"$RUN" --verify-only
printf 'Calendar application artifacts verified; shared resources are released by Common.\n'
