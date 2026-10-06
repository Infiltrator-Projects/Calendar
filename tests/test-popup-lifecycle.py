#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Regression contract for Calendar's isolated Cinnamon popup boundary."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APPLET = (ROOT / "src/cinnamon/applet.js").read_text(encoding="utf-8")
POPUP = (ROOT / "src/cinnamon/popupMenu.js").read_text(encoding="utf-8")


def require(fragment: str) -> None:
    assert fragment in POPUP, f"missing popup lifecycle contract: {fragment}"


def main() -> None:
    # Cinnamon-private lifecycle work is physically isolated from the applet
    # composition root rather than merely grouped into a class in applet.js.
    assert "class CalendarPopupMenu" not in APPLET
    assert 'RuntimeSupport.loadLocalModule("popupShell")' in APPLET
    assert "new PopupShell.CalendarPopupMenu(this, this.orientation)" in APPLET

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

    require("this._cancelCloseGuard();")
    require("this._setActorReactive(true);")

    require("setEventPassthrough(enabled)")
    require("this.passEvents = this.isOpen && Boolean(enabled);")
    assert "this.menu.passEvents =" not in APPLET

    require("this._calendarOrientation = orientation;")
    require("this._calendarLauncher = launcher;")
    assert "this._orientation" not in POPUP
    assert "this.sourceActor" not in POPUP
    assert "this.launcher" not in POPUP

    require("global.workspace_manager.get_active_workspace()")
    require("workspace.get_work_area_for_monitor(monitor.index)")
    require("rightEdge - naturalWidth")
    assert "Main.layoutManager.getWorkAreaForMonitor" not in POPUP


if __name__ == "__main__":
    main()
