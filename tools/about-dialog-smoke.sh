#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

set -eu

ABOUT=${1:-./build/calendar-plus-about}

[ -x "$ABOUT" ] || {
    echo "Calendar About helper is not executable: $ABOUT" >&2
    exit 1
}
command -v xdotool >/dev/null 2>&1 || {
    echo "xdotool is required for the About-dialog smoke test." >&2
    exit 1
}

run_dialog()
{
    "$ABOUT" &
    pid=$!
    window=
    attempt=0

    while [ "$attempt" -lt 80 ]; do
        window=$(xdotool search --onlyvisible --name '^About Calendar$' \
            2>/dev/null | head -n 1 || true)
        [ -n "$window" ] && break
        if ! kill -0 "$pid" 2>/dev/null; then
            wait "$pid" || true
            echo "Calendar About helper exited before showing its dialog." >&2
            return 1
        fi
        sleep 0.1
        attempt=$((attempt + 1))
    done

    if [ -z "$window" ]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        echo "Calendar About dialog did not become visible." >&2
        return 1
    fi

    xdotool key --window "$window" Escape

    attempt=0
    while kill -0 "$pid" 2>/dev/null && [ "$attempt" -lt 80 ]; do
        sleep 0.1
        attempt=$((attempt + 1))
    done

    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        echo "Calendar About dialog did not close after Escape." >&2
        return 1
    fi

    wait "$pid"
}

run_dialog
run_dialog
printf '%s\n' 'Calendar About open/close/reopen smoke passed.'
