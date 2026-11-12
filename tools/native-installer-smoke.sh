#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(cat "$ROOT/VERSION")
RUN=${1:-"$ROOT/dist/calendar-${VERSION}-local-folder.run"}
ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
NATIVE_VERSION="${VERSION}+nativepgo1"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/output" "$TMP/forbidden-tools"
# CI has these tools for source qualification. Fail if the published native
# installer starts using any of them to build the applet on a user's machine.
for tool in python3 g-ir-scanner g-ir-compiler; do
    cat > "$TMP/forbidden-tools/$tool" <<'EOF'
#!/bin/sh
printf 'Unexpected tool in Calendar native build: %s\n' "$0" >&2
exit 97
EOF
    chmod +x "$TMP/forbidden-tools/$tool"
done

if ! PATH="$TMP/forbidden-tools:$PATH" "$RUN" --build-only --output-dir "$TMP/output" \
    > "$TMP/native-build.log" 2>&1; then
    cat "$TMP/native-build.log" >&2
    exit 1
fi
cat "$TMP/native-build.log"
grep -Fq 'gi-compile-repository' "$TMP/native-build.log"
if grep -Fq 'Unexpected tool in Calendar native build:' "$TMP/native-build.log"; then
    exit 1
fi
for native_flag in -O3 -march=native -mtune=native -flto=auto -fno-semantic-interposition -fno-plt; do
    grep -Eq "^(cc|gcc) .* ${native_flag}( |$)" "$TMP/native-build.log"
done
grep -Fq "PGO pass 1:" "$TMP/native-build.log"
grep -Fq "PGO pass 2:" "$TMP/native-build.log"
grep -Fq -- "-fprofile-generate=" "$TMP/native-build.log"
grep -Fq -- "-fprofile-use=" "$TMP/native-build.log"
grep -Fq "Calendar representative PGO training workload completed." "$TMP/native-build.log"

DEB="$TMP/output/calendar_${NATIVE_VERSION}_${ARCH}.deb"
[ -s "$DEB" ]
[ "$(dpkg-deb -f "$DEB" Package)" = "calendar" ]
[ "$(dpkg-deb -f "$DEB" Version)" = "$NATIVE_VERSION" ]
[ "$(dpkg-deb -f "$DEB" Architecture)" = "$ARCH" ]
dpkg --compare-versions "$NATIVE_VERSION" gt "$VERSION"

dpkg-deb -x "$DEB" "$TMP/native"
grep -q '^Build mode: local hardware-native PGO-trained ' \
    "$TMP/native/usr/share/doc/calendar/BUILD-INFO"
test -f "$TMP/native/usr/lib/$MULTIARCH/libcalendar.so.0.0.0"
test -f \
    "$TMP/native/usr/lib/$MULTIARCH/girepository-1.0/Calendar-1.0.typelib"
test -x "$TMP/native/usr/libexec/calendar-about"
test -f "$TMP/native/usr/share/cinnamon/applets/calendar@the-infiltratr/stylesheet.css"
cmp "$ROOT/docs/Calendar.pdf" "$TMP/native/System/Resources/Documentation/Calendar.pdf"
test ! -e "$TMP/native/usr/share/cinnamon/applets/calendar@the-infiltratr/settings.py"
test ! -e "$TMP/native/usr/share/fonts/truetype/calendar"
test -f \
    "$TMP/native/usr/share/locale/en_AU/LC_MESSAGES/calendar@the-infiltratr.mo"
"$TMP/native/usr/libexec/calendar-about" --print-metadata |
    grep -qx "version=$VERSION"
"$TMP/native/usr/libexec/calendar-about" --print-metadata |
    grep -qx "build-profile=native"

CALENDAR_EXPECTED_VERSION="$VERSION" \
LD_LIBRARY_PATH="$TMP/native/usr/lib/$MULTIARCH" \
GI_TYPELIB_PATH="$TMP/native/usr/lib/$MULTIARCH/girepository-1.0" \
    gjs "$ROOT/tests/smoke-typelib.js" >/dev/null

printf 'Local native installer smoke test passed: %s\n' "$DEB"
