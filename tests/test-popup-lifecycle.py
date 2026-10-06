#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Regression contracts for Calendar's Cinnamon composition boundaries."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APPLET = (ROOT / "src/cinnamon/applet.js").read_text(encoding="utf-8")
POPUP = (ROOT / "src/cinnamon/popupMenu.js").read_text(encoding="utf-8")
PANEL_VIEW = (ROOT / "src/cinnamon/panelView.js").read_text(encoding="utf-8")
POPUP_VIEW = (ROOT / "src/cinnamon/popupView.js").read_text(encoding="utf-8")
EVENT_SOURCE = (ROOT / "src/cinnamon/calendarEventSource.js").read_text(
    encoding="utf-8"
)
CALENDAR = (ROOT / "src/cinnamon/calendar.js").read_text(encoding="utf-8")


def require(fragment: str) -> None:
    assert fragment in POPUP, f"missing popup lifecycle contract: {fragment}"


def main() -> None:
    # The applet is a composition root, not a home for actor implementations.
    assert "class CalendarPopupMenu" not in APPLET
    assert "class LatchedWidthBin" not in APPLET
    assert 'RuntimeSupport.loadLocalModule("popupShell")' in APPLET
    assert "new PopupShell.PanelClockView(this.actor)" in APPLET
    assert "new PopupShell.CalendarPopupMenu(this, this.orientation)" in APPLET
    assert "new PopupShell.PopupView(" in APPLET
    assert "class LatchedWidthBin" in PANEL_VIEW
    assert "class PopupView" in POPUP_VIEW

    # Cinnamon-sensitive popup behaviour is physically isolated and must not
    # depend on AppletPopupMenu's private animation bookkeeping.
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
    require("!this.actor.visible")
    assert "this.animating" not in POPUP

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

    # The month view receives a narrow event-source adapter, not EventsManager.
    assert "new PopupShell.CalendarEventSource(" in APPLET
    assert "this._calendarEventSource" in APPLET
    assert "EventManager" not in CALENDAR
    assert "Signals.addSignalMethods(CalendarEventSource.prototype);" in EVENT_SOURCE
    assert 'this._connectManagerSignal("events-updated", "events-updated")' in EVENT_SOURCE
    assert '"events-manager-ready"' in EVENT_SOURCE
    assert '"has-calendars-changed"' in EVENT_SOURCE
    assert "connect(signal, callback)" not in EVENT_SOURCE
    assert "this._manager.disconnect(id);" in EVENT_SOURCE


if __name__ == "__main__":
    main()
