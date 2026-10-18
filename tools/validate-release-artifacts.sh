#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(cat "$ROOT/VERSION")
COMMON_VERSION=$(sed -n 's/^INFILTRATR_COMMON_VERSION := //p' "$ROOT/Makefile")
DIST=${1:-"$ROOT/dist"}
ARCH=amd64
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
DEB="$DIST/calendar_${VERSION}_${ARCH}.deb"
RESOURCES_DEB="$DIST/infiltrator-calendar-resources_${VERSION}_${ARCH}.deb"
RUN="$DIST/calendar-${VERSION}-local-folder.run"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

for artifact in "$DEB" "$RESOURCES_DEB" "$RUN"; do
    [ -s "$artifact" ] || {
        echo "Missing release artifact: $artifact" >&2
        exit 1
    }
done

[ "$(find "$DIST" -maxdepth 1 -type f | wc -l)" -eq 3 ] || {
    echo "The uploaded release directory must contain exactly three files" >&2
    exit 1
}
[ -x "$RUN" ] || {
    echo "Local builder is not executable: $RUN" >&2
    exit 1
}

[ "$(dpkg-deb -f "$DEB" Package)" = "calendar" ]
[ "$(dpkg-deb -f "$DEB" Version)" = "$VERSION" ]
[ "$(dpkg-deb -f "$DEB" Architecture)" = "$ARCH" ]
[ "$(dpkg-deb -f "$RESOURCES_DEB" Package)" = "infiltrator-calendar-resources" ]
[ "$(dpkg-deb -f "$RESOURCES_DEB" Version)" = "$VERSION" ]
[ "$(dpkg-deb -f "$RESOURCES_DEB" Architecture)" = "$ARCH" ]

dpkg-deb -x "$DEB" "$TMP/generic"
dpkg-deb -x "$RESOURCES_DEB" "$TMP/resources"

test -f "$TMP/generic/usr/lib/$MULTIARCH/libcalendar.so.0.0.0"
test -f \
    "$TMP/generic/usr/lib/$MULTIARCH/girepository-1.0/Calendar-1.0.typelib"
test -f \
    "$TMP/generic/usr/share/cinnamon/applets/calendar@the-infiltratr/applet.js"
test -f \
    "$TMP/generic/usr/share/cinnamon/applets/calendar@the-infiltratr/stylesheet.css"
test -f \
    "$TMP/generic/usr/share/cinnamon/applets/calendar@the-infiltratr/icon.png"
test ! -e \
    "$TMP/generic/usr/share/cinnamon/applets/calendar@the-infiltratr/settings.py"
test -x "$TMP/generic/usr/libexec/calendar-about"
test -f "$TMP/generic/usr/share/icons/hicolor/256x256/apps/infiltratr-calendar.png"
test -f "$TMP/generic/usr/share/app-install/icons/calendar.png"
test ! -e "$TMP/generic/usr/share/fonts/truetype/calendar"
test -f \
    "$TMP/resources/usr/share/locale/en_AU/LC_MESSAGES/calendar@the-infiltratr.mo"

# The application package must not own machine calendar resources. The resource
# package is the sole Debian owner, so apt remove calendar leaves them intact.
for resource in gregorian hebrew persian coptic iso-week julian revised-julian islamic-umalqura islamic-civil islamic-tbla islamic bahai buddhist ethiopian ethiopic-amete-alem chinese dangi indian japanese minguo roman byzantine egyptian-nabonassar armenian-traditional mayan french-republican swedish-historical international-fixed world positivist; do
    test ! -e "$TMP/generic/System/Resources/Calendars/$resource.so"
    test -f "$TMP/resources/System/Resources/Calendars/$resource.so"
done
[ "$(find "$TMP/resources/System/Resources/Calendars" -maxdepth 1 -type f -name '*.so' | wc -l)" -eq 30 ]

"$TMP/generic/usr/libexec/calendar-about" --print-metadata |
    grep -qx "version=$VERSION"
"$TMP/generic/usr/libexec/calendar-about" --print-metadata |
    grep -qx "build-profile=generic"
grep -q '^Build mode: generic amd64-compatible (Debian/Mint ICU runtime bridge)$' \
    "$TMP/generic/usr/share/doc/calendar/BUILD-INFO"
grep -q "^Shared C library: Libraries ${COMMON_VERSION}$" \
    "$TMP/generic/usr/share/doc/calendar/BUILD-INFO"
if grep -q -- '-march=native' \
    "$TMP/generic/usr/share/doc/calendar/BUILD-INFO"; then
    echo "Generic package contains native CPU tuning" >&2
    exit 1
fi
RUNTIME_LIB="$TMP/generic/usr/lib/$MULTIARCH/libcalendar.so.0.0.0"
# Public releases must come from the ordinary monolithic source build. A
# compatibility-assembled split library can work at runtime while silently
# weakening the published link-time ABI, so reject that topology outright.
for private_library in libcalendar-base.so.0 libcpicu.so.0; do
    if [ -e "$TMP/generic/usr/lib/$MULTIARCH/$private_library" ]; then
        echo "Generic package contains forbidden compatibility library: $private_library" >&2
        exit 1
    fi
done
if readelf -d "$RUNTIME_LIB" 2>/dev/null | \
        grep -Eq 'Shared library: \[libicu[^]]*\.so\.[0-9]+'; then
    echo "Generic package has a direct build-host ICU SONAME dependency" >&2
    exit 1
fi
for resource in gregorian hebrew persian coptic iso-week julian revised-julian islamic-umalqura islamic-civil islamic-tbla islamic bahai buddhist ethiopian ethiopic-amete-alem chinese dangi indian japanese minguo roman byzantine egyptian-nabonassar armenian-traditional mayan french-republican swedish-historical international-fixed world positivist; do
    if readelf -d "$TMP/resources/System/Resources/Calendars/$resource.so" 2>/dev/null | \
            grep -Eq 'Shared library: \[libicu[^]]*\.so\.[0-9]+'; then
        echo "Calendar resource $resource has a direct build-host ICU SONAME dependency" >&2
        exit 1
    fi
done
python3 "$ROOT/tests/test-abi.py" --library "$RUNTIME_LIB"
nm -D --defined-only --format=posix "$RUNTIME_LIB" | \
    awk '$1 ~ /^calendar_/ { sub(/@.*/, "", $1); print $1 }' | \
    LC_ALL=C sort -u > "$TMP/generic-exports.actual"
sed -n 's/^[[:space:]]*\(calendar_[A-Za-z0-9_]*\);[[:space:]]*$/\1/p' \
    "$ROOT/src/abi/calendar.map" | LC_ALL=C sort -u > \
    "$TMP/generic-exports.expected"
diff -u "$TMP/generic-exports.expected" "$TMP/generic-exports.actual"

printf '%s\n' \
    infiltratr_temporal_calendar_provider_v1 \
    infiltratr_temporal_resource_metadata_v1 | LC_ALL=C sort -u > \
    "$TMP/resource-exports.expected"
for resource in gregorian hebrew persian coptic iso-week julian revised-julian islamic-umalqura islamic-civil islamic-tbla islamic bahai buddhist ethiopian ethiopic-amete-alem chinese dangi indian japanese minguo roman byzantine egyptian-nabonassar armenian-traditional mayan french-republican swedish-historical international-fixed world positivist; do
    nm -D --defined-only --format=posix \
        "$TMP/resources/System/Resources/Calendars/$resource.so" | \
        awk '{ sub(/@.*/, "", $1); print $1 }' | LC_ALL=C sort -u > \
        "$TMP/resource-exports.actual"
    diff -u "$TMP/resource-exports.expected" "$TMP/resource-exports.actual"
done

sed '/^__ARCHIVE_BELOW__$/q' "$RUN" > "$TMP/installer-header.sh"
sh -n "$TMP/installer-header.sh"
"$RUN" --verify-only >/dev/null

printf 'Uploaded release artifacts validated for Calendar %s and Calendar Resources.\n' \
    "$VERSION"
