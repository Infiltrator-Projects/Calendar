#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one match in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))


ci = '.github/workflows/ci.yml'
replace_once(
    ci,
    "  bigbedroom-live-probe:\n    name: BigBedroom / live Cinnamon qualification\n    runs-on: bigbedroom\n",
    "  bigbedroom-live-probe:\n    name: BigBedroom / live Cinnamon qualification\n    needs: [debian-package]\n    runs-on: bigbedroom\n",
)
replace_once(
    ci,
    "      - name: Probe exact-source Cinnamon qualification\n        id: live-cinnamon\n        shell: bash\n        run: tools/live-cinnamon-ci-smoke.sh --probe\n",
    "      - name: Download exact qualified release assets\n        uses: actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093 # v4.3.0\n        with:\n          name: calendar-release-${{ github.sha }}\n          path: qualified-release\n      - name: Locate exact qualified package\n        id: qualified-package\n        shell: bash\n        run: |\n          set -euo pipefail\n          deb=$(find qualified-release -maxdepth 1 -type f -name 'calendar_*_amd64.deb' -print -quit)\n          test -n \"$deb\" && test -s \"$deb\"\n          printf 'deb=%s/%s\\n' \"$GITHUB_WORKSPACE\" \"$deb\" >> \"$GITHUB_OUTPUT\"\n      - name: Probe exact-source Cinnamon qualification\n        id: live-cinnamon\n        env:\n          CALENDAR_PLUS_EXACT_DEB: ${{ steps.qualified-package.outputs.deb }}\n        shell: bash\n        run: tools/live-cinnamon-ci-smoke.sh --probe\n",
)
replace_once(
    ci,
    "        env:\n          CALENDAR_PLUS_LIVE_MODE: ${{ steps.live-cinnamon.outputs.mode }}\n          DEB_BUILD_OPTIONS: nocheck\n        run: tools/live-cinnamon-ci-smoke.sh\n\n  icu-compat:",
    "        env:\n          CALENDAR_PLUS_LIVE_MODE: ${{ steps.live-cinnamon.outputs.mode }}\n          CALENDAR_PLUS_EXACT_DEB: ${{ steps.qualified-package.outputs.deb }}\n          DEB_BUILD_OPTIONS: nocheck\n        run: tools/live-cinnamon-ci-smoke.sh\n\n  icu-compat:",
)
replace_once(
    ci,
    "      - name: Run packaging and release gates\n        env:\n          CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED: \"1\"\n        run: make release-check\n",
    "      - name: Run packaging and release gates\n        env:\n          CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED: \"1\"\n        run: make release-check\n      - name: Retain exact qualified release assets\n        uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2\n        with:\n          name: calendar-release-${{ github.sha }}\n          path: |\n            dist/calendar_*_amd64.deb\n            dist/calendar-*-local-folder.run\n          if-no-files-found: error\n          retention-days: 3\n",
)

release = '.github/workflows/release.yml'
replace_once(release, "permissions:\n  contents: read\n", "permissions:\n  actions: read\n  contents: read\n")
replace_once(release, "    permissions:\n      contents: write\n", "    permissions:\n      actions: read\n      contents: write\n")
replace_once(
    release,
    "          submodules: recursive\n\n      - name: Install release dependencies\n",
    "          submodules: recursive\n\n      - name: Download exact CI-qualified release assets\n        uses: actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093 # v4.3.0\n        with:\n          name: calendar-release-${{ github.event.workflow_run.head_sha }}\n          path: approved-assets\n          github-token: ${{ github.token }}\n          run-id: ${{ github.event.workflow_run.id }}\n\n      - name: Install release dependencies\n",
)
replace_once(
    release,
    "          deb=\"dist/calendar_${version}_amd64.deb\"\n          installer=\"dist/calendar-${version}-local-folder.run\"\n          test -f \"$deb\"\n          test -x \"$installer\"\n",
    "          built_deb=\"dist/calendar_${version}_amd64.deb\"\n          built_installer=\"dist/calendar-${version}-local-folder.run\"\n          approved_deb=\"approved-assets/calendar_${version}_amd64.deb\"\n          approved_installer=\"approved-assets/calendar-${version}-local-folder.run\"\n          test -f \"$built_deb\"\n          test -x \"$built_installer\"\n          test -f \"$approved_deb\"\n          test -x \"$approved_installer\"\n          cmp --silent \"$built_deb\" \"$approved_deb\" || {\n            echo 'Rebuilt Debian package differs from the exact CI-qualified artifact.' >&2\n            exit 1\n          }\n          cmp --silent \"$built_installer\" \"$approved_installer\" || {\n            echo 'Rebuilt local installer differs from the exact CI-qualified artifact.' >&2\n            exit 1\n          }\n          deb=\"$approved_deb\"\n          installer=\"$approved_installer\"\n",
)

smoke = 'tools/cinnamon-smoke.sh'
replace_once(
    smoke,
    "ABOUT=\"${CALENDAR_PLUS_ABOUT:-/usr/libexec/calendar-plus-about}\"\n",
    "ABOUT=\"${CALENDAR_PLUS_ABOUT:-/usr/libexec/calendar-plus-about}\"\nJS_RUNTIME=\"${CALENDAR_PLUS_JS_RUNTIME:-}\"\nif [ -z \"$JS_RUNTIME\" ]; then\n    if command -v gjs >/dev/null 2>&1; then\n        JS_RUNTIME=$(command -v gjs)\n    elif command -v cjs >/dev/null 2>&1; then\n        JS_RUNTIME=$(command -v cjs)\n    else\n        echo \"Neither gjs nor cjs is available for native typelib qualification.\" >&2\n        exit 1\n    fi\nfi\n",
)
replace_once(
    smoke,
    "CALENDAR_PLUS_EXPECTED_VERSION=\"$EXPECTED_VERSION\" \\\n    gjs \"$ROOT/tests/smoke-typelib.js\" >/dev/null\n",
    "CALENDAR_PLUS_EXPECTED_VERSION=\"$EXPECTED_VERSION\" \\\n    \"$JS_RUNTIME\" \"$ROOT/tests/smoke-typelib.js\" >/dev/null\n",
)

live = 'tools/live-cinnamon-ci-smoke.sh'
replace_once(
    live,
    "        if [ \"$current\" = \"$EXPECTED_VERSION\" ] &&\n           installed_runtime_matches; then\n            ready=true\n            mode=installed\n",
    "        if [ -n \"${CALENDAR_PLUS_EXACT_DEB:-}\" ] &&\n           [ -s \"$CALENDAR_PLUS_EXACT_DEB\" ] &&\n           command -v dpkg-deb >/dev/null 2>&1 &&\n           { command -v cjs >/dev/null 2>&1 || command -v gjs >/dev/null 2>&1; }; then\n            ready=true\n            mode=artifact-runtime\n            echo \"::notice::The exact CI-qualified Debian package will provide the native typelib/library while the exact source applet is staged user-locally.\"\n        elif [ \"$current\" = \"$EXPECTED_VERSION\" ] &&\n           installed_runtime_matches; then\n            ready=true\n            mode=installed\n",
)
marker = "if [ \"$mode\" = \"user-runtime\" ]; then\n"
artifact_block = r'''if [ "$mode" = "artifact-runtime" ]; then
    [ -n "${CALENDAR_PLUS_EXACT_DEB:-}" ] && [ -s "$CALENDAR_PLUS_EXACT_DEB" ] || {
        echo "Exact CI-qualified Calendar package is unavailable." >&2
        exit 1
    }
    command -v dpkg-deb >/dev/null 2>&1 || {
        echo "dpkg-deb is required for exact artifact qualification." >&2
        exit 1
    }
    if command -v cjs >/dev/null 2>&1; then
        js_runtime=$(command -v cjs)
    elif command -v gjs >/dev/null 2>&1; then
        js_runtime=$(command -v gjs)
    else
        echo "Neither cjs nor gjs is available for exact native qualification." >&2
        exit 1
    fi

    backup_root=$(mktemp -d)
    stage_root=$(mktemp -d)
    had_user_runtime=false
    if [ -e "$USER_APPLET_DIR" ]; then
        mv "$USER_APPLET_DIR" "$backup_root/original"
        had_user_runtime=true
    fi

    # shellcheck disable=SC2317
    cleanup_artifact_runtime() {
        status=$?
        trap - EXIT HUP INT TERM
        rm -rf "$USER_APPLET_DIR"
        if [ "$had_user_runtime" = true ] && [ -e "$backup_root/original" ]; then
            mkdir -p "$(dirname "$USER_APPLET_DIR")"
            mv "$backup_root/original" "$USER_APPLET_DIR"
        fi
        reload_calendar_applet >/dev/null 2>&1 || true
        rm -rf "$backup_root" "$stage_root"
        exit "$status"
    }
    trap cleanup_artifact_runtime EXIT HUP INT TERM

    dpkg-deb -x "$CALENDAR_PLUS_EXACT_DEB" "$stage_root"
    exact_about="$stage_root/usr/libexec/calendar-plus-about"
    exact_typelib=$(find "$stage_root/usr/lib" -type f -name 'CalendarPlus-1.0.typelib' -print -quit)
    exact_library=$(find "$stage_root/usr/lib" -type f -name 'libcalendar-plus.so.0.0.0' -print -quit)
    [ -x "$exact_about" ] && [ -n "$exact_typelib" ] && [ -n "$exact_library" ] || {
        echo "Exact Calendar package is missing its native qualification payload." >&2
        exit 1
    }
    typelib_dir=$(dirname "$exact_typelib")
    library_dir=$(dirname "$exact_library")

    actual=$(LD_LIBRARY_PATH="$library_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$exact_about" --print-metadata 2>/dev/null | sed -n 's/^version=//p' | head -n 1)
    [ "$actual" = "$EXPECTED_VERSION" ] || {
        printf 'Exact artifact native version %s does not match source %s.\n' "$actual" "$EXPECTED_VERSION" >&2
        exit 1
    }

    GI_TYPELIB_PATH="$typelib_dir${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}" \
    LD_LIBRARY_PATH="$library_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    CALENDAR_PLUS_EXPECTED_VERSION="$EXPECTED_VERSION" \
        "$js_runtime" "$ROOT/tests/smoke-typelib.js" >/dev/null

    mkdir -p "$USER_APPLET_DIR"
    cp -a "$ROOT/src/cinnamon/." "$USER_APPLET_DIR/"
    exact_icon="$stage_root/usr/share/cinnamon/applets/$UUID/icon.png"
    if [ -f "$exact_icon" ]; then
        cp "$exact_icon" "$USER_APPLET_DIR/icon.png"
    elif [ -f "$SYSTEM_APPLET_DIR/icon.png" ]; then
        cp "$SYSTEM_APPLET_DIR/icon.png" "$USER_APPLET_DIR/icon.png"
    fi

    reload_calendar_applet || {
        echo "Cinnamon refused to reload the exact user-local Calendar runtime." >&2
        exit 1
    }
    sleep 1
    GI_TYPELIB_PATH="$typelib_dir${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}" \
    LD_LIBRARY_PATH="$library_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    CALENDAR_PLUS_ABOUT="$exact_about" \
    CALENDAR_PLUS_JS_RUNTIME="$js_runtime" \
    CALENDAR_PLUS_APPLET_DIR="$USER_APPLET_DIR" \
        "$ROOT/tools/cinnamon-smoke.sh"
    exit 0
fi

'''
replace_once(live, marker, artifact_block + marker)
