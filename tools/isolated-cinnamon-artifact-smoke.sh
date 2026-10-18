#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

# Qualify exact Calendar and Calendar Resources artifacts inside fresh Cinnamon.
# This helper owns the complete isolated desktop/session lifecycle, so release
# qualification does not depend on a pre-existing desktop or named workstation.

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <calendar.deb> <calendar-resources.deb>" >&2
    exit 2
fi

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
DEB=$1
RESOURCES_DEB=$2
UUID=calendar@the-infiltratr
EXPECTED_VERSION=$(cat "$ROOT/VERSION")

[[ -s "$DEB" ]] || {
    echo "Exact CI-qualified Calendar package is unavailable: $DEB" >&2
    exit 1
}
[[ -s "$RESOURCES_DEB" ]] || {
    echo "Exact CI-qualified Calendar Resources package is unavailable: $RESOURCES_DEB" >&2
    exit 1
}

# This test must not write to a real user's installed resource catalogue.
# The release job runs it as root inside a disposable Debian container.
if [[ ! -f /.dockerenv || "$(id -u)" -ne 0 ]]; then
    echo "Exact-resource Cinnamon qualification requires a disposable root container." >&2
    exit 1
fi

[[ "$(dpkg-deb -f "$RESOURCES_DEB" Package)" == "infiltrator-calendar-resources" ]] || {
    echo "Qualified supplemental artifact has the wrong package identity." >&2
    exit 1
}

for command_name in Xvfb dbus-run-session cinnamon gdbus dpkg-deb python3; do
    command -v "$command_name" >/dev/null 2>&1 || {
        printf 'Isolated Cinnamon qualification is missing required command: %s\n' "$command_name" >&2
        exit 1
    }
done

if command -v cjs >/dev/null 2>&1; then
    JS_RUNTIME=$(command -v cjs)
elif command -v gjs >/dev/null 2>&1; then
    JS_RUNTIME=$(command -v gjs)
else
    echo "Neither cjs nor gjs is available for exact native qualification." >&2
    exit 1
fi

STAGE_ROOT=$(mktemp -d)
TEST_ROOT=$(mktemp -d)
TEST_RUNTIME=$(mktemp -d)
DISPLAY_FILE=$(mktemp)
XVFB_PID=

# ShellCheck cannot see that this cleanup function is invoked by trap.
# shellcheck disable=SC2317
cleanup() {
    status=$?
    trap - EXIT HUP INT TERM
    if [[ -n "$XVFB_PID" ]]; then
        kill "$XVFB_PID" >/dev/null 2>&1 || true
        wait "$XVFB_PID" 2>/dev/null || true
    fi

    # Cinnamon may activate short-lived desktop services on the isolated bus.
    # They can take a fraction of a second to observe bus shutdown and may
    # recreate cache/runtime entries while exiting. Cleanup is housekeeping,
    # not qualification evidence: retry it without converting an already
    # successful exact-artifact test into a false failure.
    for path in "$STAGE_ROOT" "$TEST_ROOT" "$TEST_RUNTIME" "$DISPLAY_FILE"; do
        removed=false
        for _ in $(seq 1 20); do
            rm -rf -- "$path" >/dev/null 2>&1 || true
            if [[ ! -e "$path" ]]; then
                removed=true
                break
            fi
            sleep 0.1
        done
        if [[ "$removed" != true ]]; then
            printf 'warning: isolated Calendar cleanup left temporary path %s\n' "$path" >&2
        fi
    done
    exit "$status"
}
trap cleanup EXIT HUP INT TERM

chmod 700 "$TEST_RUNTIME"
dpkg-deb -x "$DEB" "$STAGE_ROOT"
dpkg-deb -x "$RESOURCES_DEB" "$STAGE_ROOT"

# The native Common loader deliberately uses the canonical system capability
# directory. Install ONLY exact handoff modules into this disposable container;
# never make the production loader accept arbitrary environment override paths.
[[ "$(dpkg-deb -f "$RESOURCES_DEB" Version)" == "$EXPECTED_VERSION" ]] || {
    echo "Calendar Resources version does not match the qualified Calendar version." >&2
    exit 1
}
install -d /System/Resources/Calendars /System/Resources/Clocks
for id in hebrew gregorian persian coptic; do
    source="$STAGE_ROOT/System/Resources/Calendars/$id.so"
    [[ -s "$source" ]] || {
        echo "Qualified Calendar Resources package lacks $id.so." >&2
        exit 1
    }
    install -m 0644 "$source" "/System/Resources/Calendars/$id.so"
done
for id in decimal internet unix hexadecimal binary julian modified-julian standard-12 standard-24 chinese-ke mean-solar sidereal chinese-time; do
    source="$STAGE_ROOT/System/Resources/Clocks/$id.so"
    [[ -s "$source" ]] || {
        echo "Qualified Clock Resources package lacks $id.so." >&2
        exit 1
    }
    install -m 0644 "$source" "/System/Resources/Clocks/$id.so"
done

EXACT_ABOUT="$STAGE_ROOT/usr/libexec/calendar-about"
EXACT_TYPELIB=$(find "$STAGE_ROOT/usr/lib" -type f -name 'Calendar-1.0.typelib' -print -quit)
EXACT_LIBRARY=$(find "$STAGE_ROOT/usr/lib" -type f -name 'libcalendar.so.0.0.0' -print -quit)
if [[ ! -x "$EXACT_ABOUT" || -z "$EXACT_TYPELIB" || -z "$EXACT_LIBRARY" ]]; then
    echo "Exact Calendar package is missing its native qualification payload." >&2
    exit 1
fi
TYPELIB_DIR=$(dirname "$EXACT_TYPELIB")
LIBRARY_DIR=$(dirname "$EXACT_LIBRARY")

ACTUAL_VERSION=$(LD_LIBRARY_PATH="$LIBRARY_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$EXACT_ABOUT" --print-metadata 2>/dev/null | sed -n 's/^version=//p' | head -n 1)
[[ "$ACTUAL_VERSION" == "$EXPECTED_VERSION" ]] || {
    printf 'Exact artifact native version %s does not match source %s.\n' \
        "$ACTUAL_VERSION" "$EXPECTED_VERSION" >&2
    exit 1
}

GI_TYPELIB_PATH="$TYPELIB_DIR${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}" \
LD_LIBRARY_PATH="$LIBRARY_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
CALENDAR_EXPECTED_VERSION="$EXPECTED_VERSION" \
    "$JS_RUNTIME" "$ROOT/tests/smoke-typelib.js" >/dev/null

mkdir -p \
    "$TEST_ROOT/home/.config/autostart" \
    "$TEST_ROOT/config" \
    "$TEST_ROOT/cache" \
    "$TEST_ROOT/data/cinnamon/applets/$UUID"
cp -a "$ROOT/src/cinnamon/." "$TEST_ROOT/data/cinnamon/applets/$UUID/"
EXACT_ICON="$STAGE_ROOT/usr/share/cinnamon/applets/$UUID/icon.png"
if [[ -f "$EXACT_ICON" ]]; then
    cp "$EXACT_ICON" "$TEST_ROOT/data/cinnamon/applets/$UUID/icon.png"
fi

# Let Xvfb atomically choose an unused display so concurrently scheduled hosted
# jobs never contend for a hard-coded X display number.
: > "$DISPLAY_FILE"
Xvfb -displayfd 3 -screen 0 1280x720x24 -nolisten tcp \
    3>"$DISPLAY_FILE" >"$TEST_ROOT/xvfb.log" 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 50); do
    [[ -s "$DISPLAY_FILE" ]] && break
    kill -0 "$XVFB_PID" 2>/dev/null || {
        cat "$TEST_ROOT/xvfb.log" >&2 || true
        echo "Isolated X server exited before publishing a display number." >&2
        exit 1
    }
    sleep 0.1
done
[[ -s "$DISPLAY_FILE" ]] || {
    cat "$TEST_ROOT/xvfb.log" >&2 || true
    echo "Isolated X server did not publish a display number." >&2
    exit 1
}
DISPLAY_NUMBER=$(tr -d '[:space:]' < "$DISPLAY_FILE")
[[ "$DISPLAY_NUMBER" =~ ^[0-9]+$ ]] || {
    printf 'Invalid isolated X display number: %s\n' "$DISPLAY_NUMBER" >&2
    exit 1
}

export HOME="$TEST_ROOT/home"
export XDG_CONFIG_HOME="$TEST_ROOT/config"
export XDG_CACHE_HOME="$TEST_ROOT/cache"
export XDG_DATA_HOME="$TEST_ROOT/data"
export XDG_RUNTIME_DIR="$TEST_RUNTIME"
export XDG_CURRENT_DESKTOP=Cinnamon
export XDG_SESSION_DESKTOP=cinnamon
export XDG_SESSION_TYPE=x11
export DISPLAY=":$DISPLAY_NUMBER"
export GI_TYPELIB_PATH="$TYPELIB_DIR${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"
export LD_LIBRARY_PATH="$LIBRARY_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBGL_ALWAYS_SOFTWARE=1
export CALENDAR_TEST_UUID="$UUID"
export CALENDAR_TEST_VERSION="$EXPECTED_VERSION"

# A new session bus ensures the qualification process cannot accidentally talk
# to or mutate another Cinnamon desktop.
dbus-run-session -- bash -euo pipefail <<'INNER'
CINNAMON_PID=

# ShellCheck cannot see that this cleanup function is invoked by trap.
# shellcheck disable=SC2317
cleanup_cinnamon() {
    status=$?
    trap - EXIT HUP INT TERM
    if [[ -n "$CINNAMON_PID" ]]; then
        kill "$CINNAMON_PID" >/dev/null 2>&1 || true
        wait "$CINNAMON_PID" 2>/dev/null || true
    fi
    exit "$status"
}
trap cleanup_cinnamon EXIT HUP INT TERM

cinnamon --replace --sm-disable >"$XDG_CACHE_HOME/cinnamon-calendar-ci.log" 2>&1 &
CINNAMON_PID=$!

ready=false
for _ in $(seq 1 80); do
    if gdbus call --session \
        --dest org.Cinnamon \
        --object-path /org/Cinnamon \
        --method org.Cinnamon.Eval \
        'true' >/dev/null 2>&1; then
        ready=true
        break
    fi
    kill -0 "$CINNAMON_PID" 2>/dev/null || break
    sleep 0.25
done
if [[ "$ready" != true ]]; then
    cat "$XDG_CACHE_HOME/cinnamon-calendar-ci.log" >&2 || true
    echo "Isolated Cinnamon did not become ready." >&2
    exit 1
fi

version_result=$(gdbus call --session \
    --dest org.Cinnamon \
    --object-path /org/Cinnamon \
    --method org.Cinnamon.Eval \
    'imports.gi.versions.Calendar="1.0"; imports.gi.Calendar.get_version()')
[[ "$version_result" == *"$CALENDAR_TEST_VERSION"* ]] || {
    printf 'Isolated Cinnamon loaded the wrong Calendar version: %s\n' \
        "$version_result" >&2
    exit 1
}

enable_result=$(gdbus call --session \
    --dest org.Cinnamon \
    --object-path /org/Cinnamon \
    --method org.Cinnamon.Eval \
    "(() => { global.settings.set_strv('enabled-applets', ['panel1:right:0:$CALENDAR_TEST_UUID:9001']); AppletManager.onEnabledAppletsChanged(); return global.settings.get_strv('enabled-applets'); })()")
[[ "$enable_result" == *"$CALENDAR_TEST_UUID"* ]] || {
    printf 'Cinnamon rejected the isolated Calendar applet definition: %s\n' \
        "$enable_result" >&2
    exit 1
}

running=false
for _ in $(seq 1 80); do
    instance_result=$(gdbus call --session \
        --dest org.Cinnamon \
        --object-path /org/Cinnamon \
        --method org.Cinnamon.Eval \
        "AppletManager.getRunningInstancesForUuid(\"$CALENDAR_TEST_UUID\").length" \
        2>/dev/null || true)
    if [[ "$instance_result" =~ \'[1-9][0-9]*\' ]]; then
        running=true
        break
    fi
    sleep 0.25
done
if [[ "$running" != true ]]; then
    cat "$XDG_CACHE_HOME/cinnamon-calendar-ci.log" >&2 || true
    echo "Exact Calendar applet did not start inside isolated Cinnamon." >&2
    exit 1
fi

health_result=$(gdbus call --session \
    --dest org.Cinnamon \
    --object-path /org/Cinnamon \
    --method org.Cinnamon.Eval \
    "(() => { const a=AppletManager.getRunningInstancesForUuid(\"$CALENDAR_TEST_UUID\")[0]; a._updateClockAndDate(); a._resetCalendar(); a._onResume(); a._rebalancePopupWidth(); if (a.menu) { a.menu.open(); a.menu.close(); } return {healthy:true,version:imports.gi.Calendar.get_version()}; })()")
[[ "$health_result" == *'healthy\":true'* || "$health_result" == *'healthy":true'* ]] || {
    printf 'Exact Calendar applet health evaluation failed: %s\n' "$health_result" >&2
    exit 1
}
[[ "$health_result" == *"$CALENDAR_TEST_VERSION"* ]] || {
    printf 'Exact Calendar applet used the wrong native runtime: %s\n' "$health_result" >&2
    exit 1
}

printf 'Calendar %s passed isolated Cinnamon exact-artifact qualification.\n' \
    "$CALENDAR_TEST_VERSION"
INNER
