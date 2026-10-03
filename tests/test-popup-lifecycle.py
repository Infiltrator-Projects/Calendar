#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Regression contract for Calendar's Cinnamon popup input lifecycle."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APPLET = (ROOT / "src/cinnamon/applet.js").read_text(encoding="utf-8")


def require(fragment: str) -> None:
    assert fragment in APPLET, f"missing popup lifecycle contract: {fragment}"


def main() -> None:
    # A logically closed Calendar popup must never remain a reactive Clutter
    # actor over unrelated application windows, even if Cinnamon abandons a
    # close animation before its normal completion callback.
    require("const POPUP_CLOSE_GUARD_MS = 750;")
    require("class CalendarPopupMenu extends Applet.AppletPopupMenu")
    require("_enforceClosedInputState()")
    require("this.passEvents = false;")
    require("this.actor.reactive = false;")
    require("this._closeGuardSource = GLib.timeout_add(")
    require("this.actor.hide();")
    require("this.actor.set_size(-1, -1);")
    require("this.actor.opacity = 255;")

    # Reopening must cancel stale cleanup and explicitly restore input.
    require("this._cancelCloseGuard();")
    require("this.actor.reactive = true;")

    # Agenda scroll pass-through is valid only while the popup is actually
    # open; a late scrollbar signal must not resurrect closed-popup input.
    require("this.menu.passEvents = this.menu.isOpen && passEvents;")

    # The applet's open-state observer independently enforces the same closed
    # input contract at the orchestration boundary.
    require("if (!open) {")
    require("menu.passEvents = false;")
    require("menu.actor.reactive = false;")


if __name__ == "__main__":
    main()
