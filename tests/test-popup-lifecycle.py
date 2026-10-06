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
    # Cinnamon-private lifecycle work stays behind one compatibility boundary.
    # A logically closed popup must never remain a reactive Clutter actor over
    # unrelated windows, even if Cinnamon abandons its close animation.
    require("const POPUP_CLOSE_GUARD_MS = 750;")
    require("class CalendarPopupMenu extends Applet.AppletPopupMenu")
    require("_enforceClosedInputState()")
    require("_setActorReactive(reactive)")
    require("this.actor.reactive = Boolean(reactive);")
    require("this.passEvents = false;")
    require("this._closeGuardSource = GLib.timeout_add(")
    require("this.actor.hide();")
    require("this.actor.set_size(-1, -1);")
    require("this.actor.opacity = 255;")

    # Reopening cancels stale cleanup and restores input inside the adapter.
    require("this._cancelCloseGuard();")
    require("this._setActorReactive(true);")

    # Agenda scroll pass-through has a narrow public method and cannot make a
    # closed popup reactive. The applet must not write passEvents directly.
    require("setEventPassthrough(enabled)")
    require("this.passEvents = this.isOpen && Boolean(enabled);")
    require("this.menu.setEventPassthrough(passEvents);")
    assert "this.menu.passEvents =" not in APPLET

    # Orientation and launcher state used by the compatibility workaround are
    # owned by the adapter rather than read through inherited private fields.
    require("this._calendarOrientation = orientation;")
    require("this._calendarLauncher = launcher;")
    assert "this._orientation" not in APPLET
    assert "this.sourceActor" not in APPLET
    assert "this.launcher" not in APPLET


if __name__ == "__main__":
    main()
