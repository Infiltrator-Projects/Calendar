#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
UUID=calendar-plus@the-infiltratr
SOURCE_DIR=src/cinnamon
EXPECTED_VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
APPLET_DIR="${CALENDAR_PLUS_APPLET_DIR:-/usr/share/cinnamon/applets/$UUID}"
ABOUT="${CALENDAR_PLUS_ABOUT:-/usr/libexec/calendar-plus-about}"
JS_RUNTIME="${CALENDAR_PLUS_JS_RUNTIME:-}"
if [ -z "$JS_RUNTIME" ]; then
    if command -v gjs >/dev/null 2>&1; then
        JS_RUNTIME=$(command -v gjs)
    elif command -v cjs >/dev/null 2>&1; then
        JS_RUNTIME=$(command -v cjs)
    else
        echo "Neither gjs nor cjs is available for native typelib qualification." >&2
        exit 1
    fi
fi

case "${XDG_CURRENT_DESKTOP:-}" in
    *Cinnamon*) ;;
    *)
        echo "This integration check must run inside a Cinnamon session." >&2
        exit 1
        ;;
esac

[ -d "$APPLET_DIR" ] || {
    echo "Installed applet not found: $APPLET_DIR" >&2
    exit 1
}
[ -x "$ABOUT" ] || {
    echo "Installed About helper not found: $ABOUT" >&2
    exit 1
}

python3 - "$APPLET_DIR" "$EXPECTED_VERSION" <<'PY'
import json
import sys
from pathlib import Path

applet = Path(sys.argv[1])
expected = sys.argv[2]
metadata = json.loads((applet / "metadata.json").read_text(encoding="utf-8"))
if metadata.get("version") != expected:
    raise SystemExit(
        f"Installed metadata version mismatch: {metadata.get('version')} != {expected}"
    )
PY

while read -r expected relative; do
    case "$relative" in
        "$SOURCE_DIR"/*)
            installed="$APPLET_DIR/${relative#"$SOURCE_DIR"/}"
            [ -f "$installed" ] || {
                echo "Installed runtime file missing: $installed" >&2
                exit 1
            }
            actual=$(sha256sum "$installed" | awk '{print $1}')
            [ "$actual" = "$expected" ] || {
                echo "Installed runtime hash mismatch: $installed" >&2
                exit 1
            }
            ;;
    esac
done < "$ROOT/$SOURCE_DIR/runtime-sources.sha256"

"$ABOUT" --print-metadata | grep -qx "version=$EXPECTED_VERSION"

CALENDAR_PLUS_EXPECTED_VERSION="$EXPECTED_VERSION" \
    "$JS_RUNTIME" "$ROOT/tests/smoke-typelib.js" >/dev/null

command -v gdbus >/dev/null 2>&1 || {
    echo "gdbus is required for live Cinnamon qualification." >&2
    exit 1
}
command -v gsettings >/dev/null 2>&1 || {
    echo "gsettings is required for live Cinnamon qualification." >&2
    exit 1
}

ENABLED=$(gsettings get org.cinnamon enabled-applets)
case "$ENABLED" in
    *"$UUID"*) ;;
    *)
        PANELS=$(gsettings get org.cinnamon panels-enabled)
        NEXT_ID=$(gsettings get org.cinnamon next-applet-id)
        TEMP_ENABLED=$(python3 - "$ENABLED" "$PANELS" "$NEXT_ID" "$UUID" <<'PY'
import ast
import sys


def parse_list(raw):
    raw = raw.strip()
    if raw.startswith('@as '):
        raw = raw[4:]
    value = ast.literal_eval(raw)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise SystemExit('Cinnamon settings value is not a string list')
    return value


enabled = parse_list(sys.argv[1])
panels = parse_list(sys.argv[2])
if not panels:
    raise SystemExit('Cinnamon has no enabled panel available for qualification')

panel_number = panels[0].split(':', 1)[0]
panel = f'panel{panel_number}'
positions = []
instance_ids = []
for item in enabled:
    parts = item.split(':')
    if len(parts) >= 3 and parts[0] == panel and parts[1] == 'right':
        try:
            positions.append(int(parts[2]))
        except ValueError:
            pass
    if len(parts) >= 5:
        try:
            instance_ids.append(int(parts[4]))
        except ValueError:
            pass

try:
    next_id = int(sys.argv[3])
except ValueError as exc:
    raise SystemExit('Cinnamon next-applet-id is not numeric') from exc

position = max(positions, default=-1) + 1
instance_id = max([next_id - 1, *instance_ids], default=-1) + 1
enabled.append(f'{panel}:right:{position}:{sys.argv[4]}:{instance_id}')
print(repr(enabled))
PY
)

        restore_enabled_applets() {
            status=$?
            trap - EXIT HUP INT TERM
            gsettings set org.cinnamon enabled-applets "$ENABLED" >/dev/null 2>&1 || true
            exit "$status"
        }
        trap restore_enabled_applets EXIT HUP INT TERM
        gsettings set org.cinnamon enabled-applets "$TEMP_ENABLED"

        # Cinnamon applies enabled-applets asynchronously. Give the real desktop
        # a bounded opportunity to instantiate the temporary test applet.
        attempt=0
        while [ "$attempt" -lt 20 ]; do
            RUNNING=$(gdbus call --session \
                --dest org.Cinnamon \
                --object-path /org/Cinnamon \
                --method org.Cinnamon.Eval \
                "AppletManager.getRunningInstancesForUuid(\"$UUID\").length > 0" 2>/dev/null || true)
            case "$RUNNING" in
                *"true"*) break ;;
            esac
            attempt=$((attempt + 1))
            sleep 0.25
        done
        case "$RUNNING" in
            *"true"*) ;;
            *)
                echo "Calendar could not be temporarily enabled for live qualification." >&2
                exit 1
                ;;
        esac
        ;;
esac

EVAL_SCRIPT=$(cat <<EOF
(() => {
    const applets = AppletManager.getRunningInstancesForUuid("$UUID");
    if (!applets || applets.length === 0)
        throw new Error("Calendar has no running Cinnamon instance");

    for (const applet of applets) {
        if (!applet || applet._destroyed)
            throw new Error("Calendar instance is not healthy");

        applet._updateClockAndDate();
        applet._resetCalendar();

        applet._is_entered = true;
        applet._configureWallClock();
        applet._updatePanelClock();
        applet._is_entered = false;
        applet._configureWallClock();

        applet._onResume();
        applet._rebalancePopupWidth();

        if (applet.menu) {
            applet.menu.open();
            applet.menu.close();
        }
    }

    return { count: applets.length, healthy: true };
})()
EOF
)

EVAL_RESULT=$(gdbus call --session \
    --dest org.Cinnamon \
    --object-path /org/Cinnamon \
    --method org.Cinnamon.Eval \
    "$EVAL_SCRIPT") || {
        echo "Calendar live Cinnamon evaluation failed." >&2
        exit 1
    }

printf '%s\n' "$EVAL_RESULT" | grep -Eq '^\(true,' || {
    echo "Calendar live Cinnamon evaluation returned failure: $EVAL_RESULT" >&2
    exit 1
}
printf '%s\n' "$EVAL_RESULT" | grep -Fq '"healthy":true' || {
    echo "Calendar live Cinnamon evaluation did not report a healthy instance." >&2
    exit 1
}

if [ -f "$HOME/.xsession-errors" ]; then
    if tail -n 1000 "$HOME/.xsession-errors" | \
        grep -E "\[$UUID\].*(Failed to load|Could not create)|$UUID.*(ReferenceError|TypeError)" >/dev/null; then
        echo "A Calendar load error is present in the current session log." >&2
        exit 1
    fi
fi

printf 'Calendar %s passed installed hashes, native providers, live applet interaction and Cinnamon enablement checks.\n' \
    "$EXPECTED_VERSION"
