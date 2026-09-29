#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

# Qualify the exact source revision inside a real Cinnamon session. A probe
# distinguishes a real pass from an unavailable host configuration so CI never
# labels an older installed applet as successfully tested.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
UUID=calendar-plus@the-infiltratr
EXPECTED_VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
UID_NOW=$(id -u)
RUNTIME_DIR="/run/user/$UID_NOW"
SYSTEM_APPLET_DIR="/usr/share/cinnamon/applets/$UUID"
USER_APPLET_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/cinnamon/applets/$UUID"
ABOUT=/usr/libexec/calendar-plus-about

detect_session() {
    case "${XDG_CURRENT_DESKTOP:-}" in
        *Cinnamon*) return 0 ;;
    esac

    if command -v pgrep >/dev/null 2>&1 &&
       pgrep -u "$UID_NOW" -x cinnamon >/dev/null 2>&1 &&
       [ -S "$RUNTIME_DIR/bus" ]; then
        export XDG_CURRENT_DESKTOP=Cinnamon
        export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$RUNTIME_DIR}"
        export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$RUNTIME_DIR/bus}"
        return 0
    fi
    return 1
}

installed_version() {
    if [ ! -f "$SYSTEM_APPLET_DIR/metadata.json" ]; then
        printf '\n'
        return
    fi
    python3 - "$SYSTEM_APPLET_DIR/metadata.json" <<'PY'
import json
import sys
print(json.load(open(sys.argv[1], encoding="utf-8")).get("version", ""))
PY
}

installed_runtime_matches() {
    manifest="$ROOT/src/cinnamon/runtime-sources.sha256"
    [ -f "$manifest" ] || return 1
    command -v sha256sum >/dev/null 2>&1 || return 1

    while read -r expected relative; do
        case "$relative" in
            src/cinnamon/*)
                installed="$SYSTEM_APPLET_DIR/${relative#src/cinnamon/}"
                [ -f "$installed" ] || return 1
                actual=$(sha256sum "$installed" | awk '{print $1}')
                [ "$actual" = "$expected" ] || return 1
                ;;
        esac
    done < "$manifest"
    return 0
}


native_version_matches() {
    [ -x "$ABOUT" ] || return 1
    actual=$("$ABOUT" --print-metadata 2>/dev/null |
        sed -n 's/^version=//p' | head -n 1)
    [ "$actual" = "$EXPECTED_VERSION" ]
}

reload_calendar_applet() {
    command -v gdbus >/dev/null 2>&1 || return 1
    script=$(cat <<EOF
(() => {
    const Extension = imports.ui.extension;
    Extension.reloadExtension("$UUID", Extension.Type.APPLET);
    return true;
})()
EOF
)
    gdbus call --session \
        --dest org.Cinnamon \
        --object-path /org/Cinnamon \
        --method org.Cinnamon.Eval \
        "$script" | grep -Eq '^\(true,'
}

if [ "${1:-}" = "--probe" ]; then
    ready=false
    mode=none

    if ! detect_session; then
        echo "::notice::No active Cinnamon session is exposed to this runner; live desktop qualification is not applicable."
    else
        current="$(installed_version)"
        if [ "$current" = "$EXPECTED_VERSION" ] &&
           installed_runtime_matches; then
            ready=true
            mode=installed
        elif command -v sudo >/dev/null 2>&1 &&
             sudo -n true >/dev/null 2>&1; then
            ready=true
            mode=install
        elif native_version_matches && [ -d "$SYSTEM_APPLET_DIR" ]; then
            ready=true
            mode=user-runtime
            echo "::notice::The native Calendar ABI/version is current; exact JavaScript runtime files will be staged in the user's Cinnamon applet directory for live qualification."
        else
            echo "::warning::Cinnamon is active but neither the exact Calendar source revision nor a same-version native runtime suitable for user-local staging is available."
        fi
    fi

    if [ -n "${GITHUB_OUTPUT:-}" ]; then
        printf 'ready=%s\nmode=%s\n' "$ready" "$mode" >> "$GITHUB_OUTPUT"
    fi
    printf 'Calendar live qualification: ready=%s mode=%s\n' "$ready" "$mode"
    exit 0
fi

detect_session || {
    echo "No active Cinnamon session is available for blocking qualification." >&2
    exit 1
}

mode="${CALENDAR_PLUS_LIVE_MODE:-installed}"
if [ "$mode" = "install" ]; then
    for command_name in dpkg-buildpackage gdbus sudo; do
        command -v "$command_name" >/dev/null 2>&1 || {
            printf 'Cinnamon qualification runner is missing required command: %s\n' "$command_name" >&2
            exit 1
        }
    done
    sudo -n true >/dev/null 2>&1 || {
        echo "Passwordless sudo is required to install the exact CI package on this runner." >&2
        exit 1
    }

    cd "$ROOT"
    CALENDAR_PLUS_BUILD_MODE=generic dpkg-buildpackage -us -uc -b
    DEB="../infiltrator-calendar_${EXPECTED_VERSION}_amd64.deb"
    [ -s "$DEB" ] || {
        printf 'Current Calendar package was not produced: %s\n' "$DEB" >&2
        exit 1
    }
    sudo -n apt-get install -y --no-install-recommends "$DEB"

    RELOAD_SCRIPT=$(cat <<EOF
(() => {
    const Extension = imports.ui.extension;
    Extension.reloadExtension("$UUID", Extension.Type.APPLET);
    return true;
})()
EOF
)
    gdbus call --session         --dest org.Cinnamon         --object-path /org/Cinnamon         --method org.Cinnamon.Eval         "$RELOAD_SCRIPT" | grep -Eq '^\(true,' || {
            echo 'Cinnamon refused to reload the newly installed Calendar applet.' >&2
            exit 1
        }
fi

if [ "$mode" = "user-runtime" ]; then
    native_version_matches || {
        echo "Installed Calendar native runtime does not match the source version." >&2
        exit 1
    }

    backup_root=$(mktemp -d)
    had_user_runtime=false
    if [ -e "$USER_APPLET_DIR" ]; then
        mv "$USER_APPLET_DIR" "$backup_root/original"
        had_user_runtime=true
    fi

    # ShellCheck cannot see that this cleanup function is invoked by trap.
    # shellcheck disable=SC2317
    cleanup_user_runtime() {
        status=$?
        trap - EXIT HUP INT TERM
        rm -rf "$USER_APPLET_DIR"
        if [ "$had_user_runtime" = true ] && [ -e "$backup_root/original" ]; then
            mkdir -p "$(dirname "$USER_APPLET_DIR")"
            mv "$backup_root/original" "$USER_APPLET_DIR"
        fi
        reload_calendar_applet >/dev/null 2>&1 || true
        rm -rf "$backup_root"
        exit "$status"
    }
    trap cleanup_user_runtime EXIT HUP INT TERM

    mkdir -p "$USER_APPLET_DIR"
    cp -a "$ROOT/src/cinnamon/." "$USER_APPLET_DIR/"
    if [ -f "$SYSTEM_APPLET_DIR/icon.png" ]; then
        cp "$SYSTEM_APPLET_DIR/icon.png" "$USER_APPLET_DIR/icon.png"
    fi

    reload_calendar_applet || {
        echo "Cinnamon refused to reload the user-local Calendar runtime." >&2
        exit 1
    }
    sleep 1
    CALENDAR_PLUS_APPLET_DIR="$USER_APPLET_DIR" \
        "$ROOT/tools/cinnamon-smoke.sh"
    exit 0
fi

current="$(installed_version)"
[ "$current" = "$EXPECTED_VERSION" ] || {
    printf 'Installed Calendar %s does not match source %s.\n' "$current" "$EXPECTED_VERSION" >&2
    exit 1
}

exec "$ROOT/tools/cinnamon-smoke.sh"
