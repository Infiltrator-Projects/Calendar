#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Architecture/settings contracts for the Cinnamon/native boundary.

These checks target dependency direction, ownership and public contracts rather
than exact constructor signatures or private statement layout. Behavioural
mechanics belong in executable JS/C tests; harmless internal renames must not
make this file fail merely because source text moved within its owning module.
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
    """Return JavaScript source with comments removed for dependency checks."""
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
    event_store_controller = read("eventStoreController.js")
    event_range_state = read("eventRangeState.js")
    event_port = read("calendarEventSource.js")
    native_boundary = read("nativeBoundary.js")
    temporal_policy = read("temporalPolicy.js")
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
    # stay in presentation leaves.
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

    # Popup/Cinnamon compatibility stays isolated from the controller and month
    # view. Detailed lifecycle mechanics are executable module tests.
    require_tokens(popup_menu, "extends Applet.AppletPopupMenu")
    forbid_tokens(js_code(calendar), "passEvents", "AppletPopupMenu")
    forbid_tokens(js_code(event_manager), "passEvents", "AppletPopupMenu")

    # Event orchestration consumes two narrow owned ports. D-Bus/proxy lifetime
    # belongs to the transport and native-store/timezone lifetime belongs to the
    # event-store controller. EventsManager must not reacquire either platform
    # responsibility directly.
    event_manager_modules = loaded_modules(event_manager)
    assert "calendarServerTransport" in event_manager_modules
    assert "eventStoreController" in event_manager_modules
    require_tokens(event_manager, "set_enabled(enabled)", "this._enabled")
    forbid_tokens(
        js_code(event_manager),
        "Settings.AppletSettings",
        "org.cinnamon.desktop.interface",
        "Gio.File",
        "Gio.Cancellable",
        "CalendarPlus.EventStore.new()",
        "CalendarServerProxy",
        "bus_watch_name",
        "call_set_time_range(",
        "eventVariant.deep_unpack",
        "row[3]", "row[4]", "row[5]",
    )
    require_tokens(
        event_transport,
        "CalendarServerProxy", "bus_watch_name", "request_range(",
        'emit("events-added-or-updated"', 'emit("unavailable"',
    )
    forbid_tokens(js_code(event_transport), "CalendarPlus.EventStore.new()")
    require_tokens(
        event_store_controller,
        "CalendarPlus.EventStore.new()",
        'Gio.File.new_for_path("/etc/localtime")',
        "refresh_timezone()",
        "add_or_update_in_range(",
        'emit("timezone-changed")',
    )
    forbid_tokens(
        js_code(event_store_controller),
        "CalendarServerProxy", "bus_watch_name", "request_range(",
        "current_selected", "agenda-events-changed", "Mainloop",
    )
    require_tokens(event_manager, "NativeBoundary.decodeEventSnapshot")

    # The month view receives a narrow event-source port; transport and mutable
    # event-store ownership remain behind EventsManager.
    require_tokens(
        event_port,
        "var CalendarEventSource = class CalendarEventSource",
        "is_active()",
        "set_visible_range(firstDate, lastDate, force)",
        "get_colors_for_range(firstDate, dayCount, maxColors)",
        "select_date(date, force)",
    )
    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "EventStore")
    forbid_tokens(js_code(calendar), "CalendarServer", "Gio.", "bus_watch", "EventStore", "EventManager")

    # Lower presentation modules consume resolved scalar preferences rather
    # than live applet/desktop settings objects.
    require_tokens(event_view, "set_use_24h(use24h)", "this._use24h")
    forbid_tokens(js_code(event_view), "this.settings", "this.desktop_settings")
    forbid_tokens(js_code(calendar), "this.settings", "this.desktop_settings", "bindWithObject")
    forbid_tokens(js_code(panel_clock), "desktopSettings", 'get_boolean("clock-use-24h")')
    require_tokens(calendar, "setShowWeekNumbers(showWeekNumbers)", "setWeekStart(weekStart)")

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

    # System Settings is optional. GJS consumes one effective policy snapshot;
    # SystemClock composes a separate private policy owner rather than carrying
    # provider discovery state alongside the clock engine itself.
    require_tokens(
        applet,
        "CalendarPlus.SystemClock.new()",
        "TemporalPolicy.TemporalPolicyResolver",
        "this._calendar.setCalendarSystem(temporal.calendar)",
    )
    forbid_tokens(js_code(applet), "get_system_policy()", "decodeTemporalPolicy", "deep_unpack")
    require_tokens(
        temporal_policy,
        "get_system_policy()", "NativeBoundary.decodeTemporalPolicy",
        '"mint-cinnamon"', '"infiltrator-system-settings"',
    )
    forbid_tokens(
        js_code(applet),
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
        "CalendarPlusClockEngine *engine;",
        "CalendarPlusTemporalPolicySource *policy_source;",
        "temporal_policy_source_new(",
        "temporal_policy_source_free(",
        "infiltratr_temporal_posix_provider_available",
        "infiltratr_temporal_posix_policy_load",
        '"clock-show-seconds"',
    )
    # The facade must not duplicate the policy source's owned resource fields.
    system_clock_struct = system_clock.split("struct _CalendarPlusSystemClock", 1)[1].split("};", 1)[0]
    forbid_tokens(
        system_clock_struct,
        "GSettings *", "GFileMonitor *", "policy_directory",
        "provider_marker_basename", "effective_policy",
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
    forbid_tokens(
        js_code(calendar),
        "_addCivilDays", "_gregorianWeekday", "while (cellsPlaced < 42)",
        "model.deep_unpack", "record[1]", "record[2]", "record[3]"
    )
    require_tokens(calendar, "NativeBoundary.decodeCalendarGrid", "NativeBoundary.decodeCivilDate")
    require_tokens(native_boundary, "decodeTemporalPolicy", "decodeCalendarGrid", "decodeEventSnapshot")
    forbid_tokens(js_code(applet), "variant.deep_unpack")

    # Request lifecycle state remains one object rather than a constellation of
    # controller flags. These transition names are the state object's public
    # behavioural port, not private EventsManager statement layout.
    require_tokens(event_manager, "new EventRangeState()")
    for transition in (
        "beginRequest()", "finishRequest(", "invalidate()",
        "acceptCompletedRequest()", "nextRetryDelay()", "resetForServerLoss()",
    ):
        assert transition in event_range_state
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
