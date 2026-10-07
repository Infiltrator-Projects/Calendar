#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Architecture/settings contracts for the Cinnamon/native boundary.

These checks intentionally target dependency direction and public contracts,
not exact statement layout. Behavioural details belong in the executable JS/C
tests; harmless renames or refactors inside an owning module should not make
this file fail merely because source text moved around.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
APPLET_DIR = PROJECT_ROOT / "src/cinnamon"
METADATA = json.loads((APPLET_DIR / "metadata.json").read_text(encoding="utf-8"))


def read(name: str) -> str:
    return (APPLET_DIR / name).read_text(encoding="utf-8")


def require_tokens(source: str, *tokens: str) -> None:
    for token in tokens:
        assert token in source, f"missing contract token: {token}"


def forbid_tokens(source: str, *tokens: str) -> None:
    for token in tokens:
        assert token not in source, f"forbidden coupling token: {token}"


def loaded_modules(source: str) -> set[str]:
    return set(re.findall(r'loadLocalModule\(\s*["\']([^"\']+)["\']\s*\)', source))


def main() -> None:
    schema = json.loads(read("settings-schema.json"))
    applet = read("applet.js")
    calendar = read("calendar.js")
    event_view = read("eventView.js")
    event_manager = read("eventManager.js")
    event_range_state = read("eventRangeState.js")
    event_port = read("calendarEventSource.js")
    panel_clock = read("panelClock.js")
    panel_view = read("panelView.js")
    popup_menu = read("popupMenu.js")
    popup_view = read("popupView.js")
    popup_shell = read("popupShell.js")
    runtime = read("runtimeSupport.js")
    stylesheet = read("stylesheet.css")
    system_clock = (
        PROJECT_ROOT / "src/adapters/system-clock.c"
    ).read_text(encoding="utf-8")

    forbidden_temporal_settings = {
        "clock-section", "follow-system-temporal", "clock-mode", "show-seconds",
        "location-section", "location-configured", "latitude", "longitude",
        "calendar-section", "primary-calendar", "secondary-calendar",
        "use-custom-format", "custom-format", "custom-tooltip-format",
        "format-button",
    }
    assert not forbidden_temporal_settings.intersection(schema)

    # Composition root: it wires modules, but concrete actor implementations
    # stay in the presentation leaves.
    forbid_tokens(applet, "class CalendarPopupMenu", "class LatchedWidthBin")
    assert {"calendar", "eventManager", "panelClock", "popupShell"}.issubset(
        loaded_modules(applet)
    )
    require_tokens(
        popup_shell,
        "CalendarEventSourceModule.CalendarEventSource",
        "EventViewModule.EventList",
        "PanelViewModule.PanelClockView",
        "PopupMenuModule.CalendarPopupMenu",
        "PopupViewModule.PopupView",
    )

    # Popup/Cinnamon compatibility stays isolated from the controller and month
    # view. The dedicated popup regression test covers the detailed mechanics.
    require_tokens(popup_menu, "class CalendarPopupMenu extends Applet.AppletPopupMenu")
    forbid_tokens(calendar, "passEvents", "AppletPopupMenu")
    forbid_tokens(event_manager, "passEvents", "AppletPopupMenu")

    # The event transport controller must not know Cinnamon's concrete applet
    # settings object or preference keys. The composition root reduces the UI
    # preference to one abstract enabled boolean.
    assert re.search(r"class EventsManager\s*\{\s*constructor\s*\(\s*\)", event_manager)
    require_tokens(event_manager, "set_enabled(enabled)", "this._enabled")
    forbid_tokens(
        event_manager,
        "this.settings",
        "this.desktop_settings",
        "getValue(\"show-events\")",
        "Settings.AppletSettings",
        "org.cinnamon.desktop.interface",
    )
    assert re.search(r"new\s+EventManager\.EventsManager\s*\(\s*\)", applet)
    assert re.search(
        r"events_manager\.set_enabled\s*\(\s*this\.show_events\s*\)", applet
    )

    # Agenda presentation needs the desktop clock preference, not the complete
    # Calendar applet settings object.
    assert re.search(r"class EventList\s*\{\s*constructor\s*\(\s*desktop_settings\s*\)", event_view)
    forbid_tokens(event_view, "this.settings", "constructor(settings,")
    assert re.search(
        r"new\s+PopupShell\.EventList\s*\(\s*this\.desktop_settings\s*\)", applet
    )

    # The month view receives a narrow event-source port; D-Bus and the native
    # mutable event store remain behind EventsManager.
    require_tokens(
        event_port,
        "var CalendarEventSource = class CalendarEventSource",
        "is_active()",
        "set_visible_range(firstDate, lastDate, force)",
        "get_colors_for_range(firstDate, dayCount, maxColors)",
        "select_date(date, force)",
    )
    forbid_tokens(event_port, "CalendarServer", "Gio.", "bus_watch", "event_store")
    forbid_tokens(calendar, "CalendarServer", "Gio.", "bus_watch", "event_store", "EventManager")

    # Theme policy remains platform-authoritative and Common supplies the design
    # token catalogue. Verify semantic values rather than statement placement.
    theme = schema["theme-mode"]
    assert theme["type"] == "combobox"
    assert theme["default"] == "system"
    assert set(theme["options"].values()) == {"system", "day", "night"}
    require_tokens(applet, '"changed::gtk-theme"', "calendar-plus-theme-${effectiveTheme}")

    common_design = json.loads(
        (
            PROJECT_ROOT
            / "src/vendor/infiltratr-common/design/infiltrator-design-v1.json"
        ).read_text(encoding="utf-8")
    )
    assert common_design["theme"]["modes"] == ["system", "day", "night"]
    assert common_design["theme"]["default_mode"] == "system"
    assert common_design["theme"]["system_policy"] == "platform_authoritative"
    typography = common_design["typography"]
    require_tokens(
        stylesheet,
        f'font-family: "{typography["ui_family"]}";',
        f'font-weight: {typography["ui_regular_weight"]};',
        f'font-weight: {typography["ui_bold_weight"]};',
        "BEGIN GENERATED COMMON TYPOGRAPHY TOKENS",
    )

    theme_css = stylesheet.split("Theme policy", 1)[1]
    canonical_colours = {
        value.lower()
        for mode in ("day", "night")
        for value in common_design["theme"]["palettes"][mode].values()
    }
    for colour in re.findall(r"#[0-9A-Fa-f]{6}", theme_css):
        assert colour.lower() in canonical_colours

    # System Settings is optional. Calendar consumes the one native effective
    # policy snapshot and keeps legacy per-field preference APIs out of GJS.
    require_tokens(
        applet,
        "CalendarPlus.SystemClock.new()",
        "get_system_policy()",
        "this._calendar.setCalendarSystem(temporal.calendar)",
    )
    forbid_tokens(
        applet,
        "get_system_mode()",
        "get_system_calendar()",
        "get_system_show_seconds()",
        "get_system_location_configured()",
        "get_system_latitude()",
        "get_system_longitude()",
        "follow_system_temporal",
        "secondary_calendar",
        "useCustomFormat",
        "customFormat",
        "customTooltipFormat",
    )
    require_tokens(
        system_clock,
        "infiltratr_temporal_posix_provider_available",
        "infiltratr_temporal_posix_policy_load",
        '"clock-show-seconds"',
    )

    # Calendar arithmetic belongs to the native API, not a second JavaScript
    # chronology implementation.
    require_tokens(
        calendar,
        "CalendarPlus.CalendarSystem.new(",
        ".add_days_parts(",
        ".add_months_parts(",
        ".add_years_parts(",
        ".build_grid(",
        "CalendarPlus.date_same(",
        "CalendarPlus.date_weekday(",
        "CalendarPlus.date_is_work_day(",
    )
    forbid_tokens(calendar, "_addCivilDays", "_gregorianWeekday", "while (cellsPlaced < 42)")

    # Request lifecycle state remains one object rather than a constellation of
    # controller flags.
    require_tokens(event_manager, "new EventRangeState()")
    for transition in (
        "beginRequest()", "finishRequest(", "invalidate()",
        "acceptCompletedRequest()", "nextRetryDelay()", "resetForServerLoss()",
    ):
        assert transition in event_range_state
    forbid_tokens(
        event_manager,
        "_range_request_generation",
        "_range_request_pending",
        "_range_request_succeeded",
        "_range_accepting_events",
        "_queued_range_force",
        "_range_retry_attempt",
    )

    # Runtime mechanics are centralized and long-lived owners use SignalBag.
    assert runtime.count("var SignalBag = class SignalBag") == 1
    for source in (applet, calendar, event_view, event_manager, panel_clock):
        assert "class SignalBag" not in source
    require_tokens(calendar, "new SignalBag()", "disconnectAll()")
    require_tokens(event_manager, "new SignalBag()", "disconnectAll()")

    # Public identity/accessibility compatibility remains stable.
    require_tokens(applet, '"calendar-plus@the-infiltratr"', "openAbout()")
    require_tokens(event_view, '"changed::calendar-backend"', "accessible_role: Atk.Role.LIST_ITEM")
    require_tokens(calendar, '"key-press-event"', "grab_key_focus()")
    assert METADATA["cinnamon-version"] == ["6.4", "6.6", "6.7"]
    assert "external-configuration-app" not in METADATA
    assert not (APPLET_DIR / "settings.py").exists()


if __name__ == "__main__":
    main()
