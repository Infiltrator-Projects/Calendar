#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Architecture/settings contracts for the Cinnamon/native boundary.

These checks enforce ownership and dependency direction. Behavioural details
belong in executable JS/C tests; harmless renames and local refactors must not
fail merely because source spelling changed.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
APPLET_DIR = PROJECT_ROOT / "src/cinnamon"
METADATA = json.loads((APPLET_DIR / "metadata.json").read_text(encoding="utf-8"))
JS_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
JS_LINE_COMMENT_RE = re.compile(r"//.*?$", re.MULTILINE)


def read(name: str) -> str:
    return (APPLET_DIR / name).read_text(encoding="utf-8")


def js_code(source: str) -> str:
    return JS_LINE_COMMENT_RE.sub("", JS_BLOCK_COMMENT_RE.sub("", source))


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
    event_transport = read("calendarServerTransport.js")
    event_range_state = read("eventRangeState.js")
    event_port = read("calendarEventSource.js")
    native_boundary = read("nativeBoundary.js")
    temporal_policy = read("temporalPolicy.js")
    panel_clock = read("panelClock.js")
    popup_menu = read("popupMenu.js")
    popup_shell = read("popupShell.js")
    runtime = read("runtimeSupport.js")
    stylesheet = read("stylesheet.css")
    system_clock = (
        PROJECT_ROOT / "src/adapters/system-clock.c"
    ).read_text(encoding="utf-8")
    policy_source = (
        PROJECT_ROOT / "src/adapters/temporal-policy-source.inc"
    ).read_text(encoding="utf-8")

    forbidden_temporal_settings = {
        "clock-section", "follow-system-temporal", "clock-mode", "show-seconds",
        "location-section", "location-configured", "latitude", "longitude",
        "calendar-section", "primary-calendar", "secondary-calendar",
        "use-custom-format", "custom-format", "custom-tooltip-format",
        "format-button",
    }
    assert not forbidden_temporal_settings.intersection(schema)

    # Composition root wires feature modules; concrete views stay in leaves.
    forbid_tokens(js_code(applet), "class CalendarPopupMenu", "class LatchedWidthBin")
    assert {
        "calendar", "eventManager", "panelClock", "popupShell", "temporalPolicy"
    }.issubset(loaded_modules(applet))
    assert "nativeBoundary" not in loaded_modules(applet)
    require_tokens(
        popup_shell,
        "CalendarEventSourceModule.CalendarEventSource",
        "EventViewModule.EventList",
        "PanelViewModule.PanelClockView",
        "PopupMenuModule.CalendarPopupMenu",
        "PopupViewModule.PopupView",
    )

    # Popup compatibility stays isolated from month/event controllers.
    require_tokens(popup_menu, "Applet.AppletPopupMenu")
    forbid_tokens(js_code(calendar), "passEvents", "AppletPopupMenu")
    forbid_tokens(js_code(event_manager), "passEvents", "AppletPopupMenu")

    # EventsManager may depend on its abstract transport/state collaborators but
    # never on applet settings, CalendarServer proxy details or actor classes.
    require_tokens(event_manager, "CalendarServerTransport", "EventRangeState")
    forbid_tokens(
        js_code(event_manager),
        "this.settings",
        "this.desktop_settings",
        "Settings.AppletSettings",
        "org.cinnamon.desktop.interface",
        "CalendarServerProxy",
        "bus_watch_name",
        "call_set_time_range(",
        "AppletPopupMenu",
    )
    require_tokens(event_manager, "set_enabled(")
    require_tokens(applet, "EventManager.EventsManager", "events_manager.set_enabled")

    # Lower presentation modules consume resolved values rather than live
    # settings objects.
    forbid_tokens(js_code(event_view), "this.settings", "this.desktop_settings")
    forbid_tokens(js_code(calendar), "this.settings", "this.desktop_settings", "bindWithObject")
    forbid_tokens(js_code(panel_clock), "desktopSettings", "get_boolean(\"clock-use-24h\")")
    require_tokens(event_view, "set_use_24h(")
    require_tokens(calendar, "setShowWeekNumbers(", "setWeekStart(")

    # Month view receives a narrow event-source port. Transport and mutable
    # native event-store ownership remain behind EventsManager.
    for operation in (
        "is_active()",
        "set_visible_range(",
        "get_colors_for_range(",
        "select_date(",
    ):
        assert operation in event_port
    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "event_store")
    require_tokens(event_transport, "CalendarServerProxy", "bus_watch_name", "request_range(")
    require_tokens(event_manager, "add_or_update_in_range(", "NativeBoundary.decodeEventSnapshot")
    forbid_tokens(js_code(calendar), "CalendarServer", "Gio.", "bus_watch", "event_store")

    # Theme policy remains platform-authoritative and Common supplies design
    # tokens. Verify semantic values, not statement placement.
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

    # System Settings is optional. GJS consumes one coherent policy snapshot.
    require_tokens(applet, "CalendarPlus.SystemClock.new()", "TemporalPolicy.TemporalPolicyResolver")
    forbid_tokens(js_code(applet), "get_system_policy()", "decodeTemporalPolicy", "deep_unpack")
    require_tokens(
        temporal_policy,
        "get_system_policy()",
        "NativeBoundary.decodeTemporalPolicy",
        '"mint-cinnamon"',
        '"infiltrator-system-settings"',
    )
    forbid_tokens(
        js_code(applet),
        "get_system_mode()",
        "get_system_calendar()",
        "get_system_show_seconds()",
        "get_system_location_configured()",
        "get_system_latitude()",
        "get_system_longitude()",
    )

    # Native facade owns clock composition only; policy-source owns discovery,
    # persistence fallback and monitoring. This is an ownership boundary, not a
    # requirement on exact function layout.
    require_tokens(system_clock, "CalendarPlusClockEngine", "CalendarPlusTemporalPolicySource")
    forbid_tokens(
        system_clock,
        "infiltratr_temporal_posix_provider_available",
        "infiltratr_temporal_posix_policy_load",
        "g_file_monitor_directory(",
        "g_settings_schema_source_lookup(",
    )
    require_tokens(
        policy_source,
        "infiltratr_temporal_posix_provider_available",
        "infiltratr_temporal_posix_policy_load",
        '"clock-show-seconds"',
        "g_file_monitor_directory(",
    )

    # Calendar arithmetic belongs to native APIs, not a second JS chronology.
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
    forbid_tokens(
        js_code(calendar),
        "_addCivilDays",
        "_gregorianWeekday",
        "while (cellsPlaced < 42)",
        "model.deep_unpack",
        "record[1]",
        "record[2]",
        "record[3]",
    )
    require_tokens(calendar, "NativeBoundary.decodeCalendarGrid", "NativeBoundary.decodeCivilDate")
    require_tokens(native_boundary, "decodeTemporalPolicy", "decodeCalendarGrid", "decodeEventSnapshot")

    # Request lifecycle remains one state object rather than controller flags.
    require_tokens(event_manager, "EventRangeState")
    for capability in (
        "beginRequest",
        "finishRequest",
        "invalidate",
        "acceptCompletedRequest",
        "nextRetryDelay",
        "resetForServerLoss",
    ):
        assert capability in event_range_state
    forbid_tokens(
        js_code(event_manager),
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
        assert "class SignalBag" not in js_code(source)
    require_tokens(calendar, "SignalBag", "disconnectAll()")
    require_tokens(event_manager, "SignalBag", "disconnectAll()")

    # Public identity/accessibility compatibility remains stable.
    require_tokens(applet, '"calendar-plus@the-infiltratr"', "openAbout()")
    require_tokens(event_view, '"changed::calendar-backend"', "Atk.Role.LIST_ITEM")
    require_tokens(calendar, '"key-press-event"', "grab_key_focus()")
    assert METADATA["cinnamon-version"] == ["6.4", "6.6", "6.7"]
    assert "external-configuration-app" not in METADATA
    assert not (APPLET_DIR / "settings.py").exists()


if __name__ == "__main__":
    main()
