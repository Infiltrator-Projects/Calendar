#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

make clean
rm -rf dist
shellcheck tools/*.sh tools/local-installer.sh.in

if [ "${CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED:-0}" = "1" ]; then
    printf 'Calendar source qualification supplied by exact-SHA CI; not rerunning the behavioural suite.\n'
else
    make check
fi

# Package construction, reproducibility and installer smoke validate packaging
# mechanics. They must not recursively rerun the already-completed source suite.
case " ${DEB_BUILD_OPTIONS:-} " in
    *" nocheck "*) ;;
    *) DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS:+$DEB_BUILD_OPTIONS }nocheck" ;;
esac
export DEB_BUILD_OPTIONS

make package-source
make package-local-installer
make reproducible-build

VERSION=$(sed -n 's/^VERSION := //p' Makefile)
ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
[ "$ARCH" = "amd64" ] || {
    echo "Generic rollout package must be built on amd64, not $ARCH" >&2
    exit 1
}
dpkg-source -b .
DSC="../infiltrator-calendar_${VERSION}.dsc"
if [ ! -s "$DSC" ]; then
    echo "Debian source package was not produced" >&2
    exit 1
fi

CALENDAR_PLUS_BUILD_MODE=generic dpkg-buildpackage -us -uc -b

DEB="../infiltrator-calendar_${VERSION}_${ARCH}.deb"
if [ ! -s "$DEB" ]; then
    echo "Debian package was not produced" >&2
    exit 1
fi

dpkg-deb --info "$DEB" >/dev/null
dpkg-deb --contents "$DEB" >/dev/null
command -v lintian >/dev/null || {
    echo "Missing release dependency: lintian" >&2
    exit 1
}
lintian --fail-on error "$DEB"
install -m 0644 "$DEB" "dist/calendar_${VERSION}_${ARCH}.deb"

tools/validate-release-artifacts.sh dist
tools/native-installer-smoke.sh \
    "dist/calendar-${VERSION}-local-folder.run"

printf 'Release gates passed: generic=%s; installer=%s; dsc=%s\n' \
    "dist/calendar_${VERSION}_${ARCH}.deb" \
    "dist/calendar-${VERSION}-local-folder.run" \
    "$DSC"
