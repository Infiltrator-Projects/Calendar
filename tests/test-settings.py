#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Architecture and settings contracts that cross the Cinnamon/C boundary."""

import json
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
APPLET_DIR = PROJECT_ROOT / "src/cinnamon"
METADATA = json.loads((APPLET_DIR / "metadata.json").read_text(encoding="utf-8"))


def read(name: str) -> str:
    return (APPLET_DIR / name).read_text(encoding="utf-8")


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

    # applet.js is a composition root, not the owner of Cinnamon popup/actor
    # mechanics. Those concerns are physically separated and loaded as one
    # presentation shell so Cinnamon-version compatibility has one boundary.
    assert "class CalendarPopupMenu" not in applet
    assert "GObject.registerClass(" not in applet
    assert "new St.BoxLayout" not in applet
    assert 'RuntimeSupport.loadLocalModule("popupShell")' in applet
    assert "new PopupShell.PanelClockView(this.actor)" in applet
    assert "new PopupShell.CalendarPopupMenu(this, this.orientation)" in applet
    assert "new PopupShell.PopupView(" in applet
    assert "new PopupShell.EventList(" in applet
    assert "var PanelClockView = PanelViewModule.PanelClockView" in popup_shell
    assert "var CalendarPopupMenu = PopupMenuModule.CalendarPopupMenu" in popup_shell
    assert "var PopupView = PopupViewModule.PopupView" in popup_shell

    # Established popup composition remains agenda-left / calendar-right.
    agenda_insert = popup_view.index("this._body.add_actor(this._eventList.actor);")
    calendar_insert = popup_view.index("this._body.add_actor(this._calendarColumn);")
    assert agenda_insert < calendar_insert
    assert 'CP_("Show today")' in popup_view
    assert 'CP_("About Calendar")' in popup_view

    # All AppletPopupMenu internals and right-edge placement live in the popup
    # adapter. No controller or month-view code may reach those details.
    assert "class CalendarPopupMenu extends Applet.AppletPopupMenu" in popup_menu
    assert 'launcher.locationLabel !== "right"' in popup_menu
    assert "this._calendarLauncher = launcher;" in popup_menu
    assert "this._calendarOrientation = orientation;" in popup_menu
    assert "this._orientation" not in popup_menu
    assert "this.sourceActor" not in popup_menu
    assert "this.launcher" not in popup_menu
    assert "this.animating" not in popup_menu
    assert "global.workspace_manager.get_active_workspace()" in popup_menu
    assert "workspace.get_work_area_for_monitor(monitor.index)" in popup_menu
    assert "Main.layoutManager.getWorkAreaForMonitor" not in popup_menu
    assert "rightEdge - naturalWidth" in popup_menu
    assert "passEvents" not in calendar
    assert "passEvents" not in event_manager

    # Clock-width hysteresis is a presentation component, not controller state.
    assert "GObject.registerClass(" in panel_view
    assert "class LatchedWidthBin extends St.Bin" in panel_view
    assert "vfunc_get_preferred_width(forHeight)" in panel_view
    assert "new LatchedWidthBin({ x_align: St.Align.END })" in panel_view
    assert "if (naturalWidth > this._latchedWidth)" in panel_view
    assert "naturalWidth < this._latchedWidth" not in panel_view
    assert "_labelBin" not in applet
    assert 'font-feature-settings: "tnum" 1;' in stylesheet

    # The month view receives a narrow event-source port rather than the D-Bus
    # transport controller. The adapter owns signal translation as well as the
    # four operations the month grid needs, so Calendar never subscribes to the
    # concrete EventsManager directly.
    assert "new PopupShell.CalendarEventSource(" in applet
    assert "this._calendarEventSource," in applet
    assert "this.events_manager,\n            this.desktop_settings" not in applet
    assert "var CalendarEventSource = class CalendarEventSource" in event_port
    assert "Signals.addSignalMethods(CalendarEventSource.prototype);" in event_port
    assert "this._manager.connect(sourceSignal" in event_port
    assert "this._manager.disconnect(id);" in event_port
    assert "connect(signal, callback)" not in event_port
    for operation in (
        "is_active()", "set_visible_range(firstDate, lastDate, force)",
        "get_colors_for_range(firstDate, dayCount, maxColors)",
        "select_date(date, force)",
    ):
        assert operation in event_port
    for transport_detail in (
        "CalendarServer", "Gio.", "bus_watch", "reconnect", "event_store",
    ):
        assert transport_detail not in event_port

    # Theme policy remains platform-authoritative and uses Common tokens.
    theme = schema["theme-mode"]
    assert theme["type"] == "combobox"
    assert theme["default"] == "system"
    assert list(theme["options"].values()) == ["system", "day", "night"]
    assert '"theme-mode",' in applet
    assert '"show-events",' in applet
    assert 'this.theme_mode = "system";' in applet
    assert '"changed::clock-use-24h"' in applet
    assert '"changed::clock-show-date"' in applet
    assert '"changed::gtk-theme"' in applet
    assert 'this._systemPrefersDark() ? "night" : "day"' in applet
    assert "this._systemUsesHighContrast()" in applet
    assert 'this.menu.setCustomStyleClass("calendar-plus-popup");' in applet
    assert '`calendar-plus-popup calendar-plus-theme-${effectiveTheme}`' in applet
    assert ".calendar-plus-popup.calendar-plus-theme-day" in stylesheet
    assert ".calendar-plus-popup.calendar-plus-theme-night" in stylesheet

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
    assert f'font-family: "{typography["ui_family"]}";' in stylesheet
    assert f'font-weight: {typography["ui_regular_weight"]};' in stylesheet
    assert f'font-weight: {typography["ui_bold_weight"]};' in stylesheet
    assert "BEGIN GENERATED COMMON TYPOGRAPHY TOKENS" in stylesheet

    theme_css = stylesheet.split("Theme policy", 1)[1]
    canonical_colours = {
        value.lower()
        for mode in ("day", "night")
        for value in common_design["theme"]["palettes"][mode].values()
    }
    for colour in re.findall(r"#[0-9A-Fa-f]{6}", theme_css):
        assert colour.lower() in canonical_colours

    # System Settings is an optional richer temporal authority. Calendar keeps
    # no duplicate clock/location/calendar preference state.
    assert "get_system_policy()" in applet
    assert "CalendarPlus.SystemClock.new()" in applet
    assert "this.system_clock.get_system_policy()" in applet
    assert "CalendarPlus.CalendarSystem.new(calendar) !== null" in applet
    assert "this._calendar.setCalendarSystem(temporal.calendar)" in applet
    for legacy in (
        "get_system_mode()", "get_system_calendar()",
        "get_system_show_seconds()", "get_system_location_configured()",
        "get_system_latitude()", "get_system_longitude()",
        "follow_system_temporal", "secondary_calendar", "useCustomFormat",
        "customFormat", "customTooltipFormat",
    ):
        assert legacy not in applet
    assert "infiltratr_temporal_posix_provider_available" in system_clock
    assert "infiltratr_temporal_posix_policy_load" in system_clock
    assert '"clock-show-seconds"' in system_clock

    # Native calendar/event logic remains native; JavaScript is the Cinnamon
    # presentation/transport boundary rather than a second arithmetic engine.
    assert "CalendarPlus.CalendarSystem.new(" in calendar
    assert "this._calendarSystem.add_days_parts(" in calendar
    assert "this._calendarSystem.add_months_parts(" in calendar
    assert "this._calendarSystem.add_years_parts(" in calendar
    assert "system.build_grid(" in calendar
    assert "CalendarPlus.date_same(" in calendar
    assert "CalendarPlus.date_weekday(" in calendar
    assert "_addCivilDays" not in calendar
    assert "_gregorianWeekday" not in calendar
    assert "CalendarPlus.date_is_work_day(" in calendar
    assert "while (cellsPlaced < 42)" not in calendar
    assert "CalendarPlus.EventStore.new()" in event_manager
    assert "this.event_store.add_or_update(" in event_manager
    assert "this.event_store.get_snapshot(" in event_manager
    assert "this.event_store.get_color_range(" in event_manager
    assert "this.event_store.refresh_timezone()" in event_manager
    assert "new EventRangeState()" in event_manager
    for legacy_flag in (
        "_range_request_generation", "_range_request_pending",
        "_range_request_succeeded", "_range_accepting_events",
        "_queued_range_force", "_range_retry_attempt",
    ):
        assert legacy_flag not in event_manager
    for transition in (
        "beginRequest()", "finishRequest(", "invalidate()",
        "acceptCompletedRequest()", "nextRetryDelay()",
        "resetForServerLoss()",
    ):
        assert transition in event_range_state
    assert "CalendarPlus.event_day_relation(" in event_manager
    assert "CalendarPlus.event_timing(" in event_manager

    # Event transport publishes presentation state; it does not own EventList.
    assert 'this.emit("agenda-date-changed", date);' in event_manager
    assert '"agenda-events-changed"' in event_manager
    assert "this._event_list" not in event_manager
    assert "set_events(" not in event_manager
    assert '"agenda-date-changed"' in applet
    assert '"agenda-events-changed"' in applet
    assert "this.event_list.set_date(date);" in applet
    assert "this.event_list.set_events(snapshot, reset, loading);" in applet

    # Standard/native clock modes remain centralized in panelClock.js.
    for name in (
        "withDate24Seconds", "withDate12Seconds", "withDate24", "withDate12",
        "withoutDate24Seconds", "withoutDate12Seconds", "withoutDate24",
        "withoutDate12",
    ):
        assert name in panel_clock
    assert 'var CLOCK_MODE_STANDARD_24 = "standard-24";' in panel_clock
    assert 'var CLOCK_MODE_STANDARD_12 = "standard-12";' in panel_clock
    assert "function isNativeClockMode(mode)" in panel_clock
    assert "CalendarPlus.time_mode_from_string(mode)" in panel_clock
    assert "PanelClock.panelText(" in applet
    assert "PanelClock.todayDisplay(" in applet

    # Construction and teardown stay atomic across every extracted component.
    assert "this._initialiseState(orientation, expectedVersion);" in applet
    assert "this._buildApplet();" in applet
    assert "this._destroy();\n            throw error;" in applet
    assert "this.events_manager.destroy();" in applet
    assert "this.settings.finalize();" in applet
    assert "this._calendarEventSource.destroy();" in applet
    assert "this._popupView.destroy();" in applet
    assert "this._panelView.destroy();" in applet

    # Runtime mechanics stay centralized and long-lived owners use SignalBag.
    assert runtime.count("var SignalBag = class SignalBag") == 1
    assert runtime.count("var midnight = function midnight") == 1
    assert runtime.count("var sameInstant = function sameInstant") == 1
    for source in (applet, calendar, event_view, event_manager, panel_clock):
        assert "class SignalBag" not in source
    assert "this._eventSignals = new SignalBag();" in calendar
    assert "this._desktopSignals = new SignalBag();" in calendar
    assert "this._actorSignals = new SignalBag();" in calendar
    assert "disconnectAll()" in calendar

    # CalendarServer lifecycle resources remain deterministic.
    for fragment in (
        "this._bus_watch_id = 0;", "this._serverSignals = new SignalBag();",
        "this._cancellable = new Gio.Cancellable();",
        "Gio.bus_unwatch_name(this._bus_watch_id);",
        "this._scheduleReconnect();", "this._cancelReconnect();",
        "this._cancellable.cancel();",
        'Gio.File.new_for_path("/etc/localtime")',
        "this._timezone_monitor.cancel();",
    ):
        assert fragment in event_manager

    # UI identity, launch surfaces and accessibility remain intact.
    assert '"calendar-plus@the-infiltratr"' in applet
    assert '"calendar-plus@the-infiltratr"' in calendar
    assert '"calendar-plus@the-infiltratr"' in event_view
    assert '"calendar-plus@the-infiltratr"' in popup_view
    assert "Gettext.bindtextdomain(" in popup_view
    assert "openAbout()" in applet
    assert '"infiltrator-calendar-about.desktop"' in applet
    assert 'Util.spawnCommandLine("cinnamon-settings calendar")' in applet
    assert '"changed::calendar-backend"' in event_view
    assert '"clockenstein-calendar"' in event_view
    assert '["gnome-calendar", "--uuid", uuid]' in event_view
    assert "accessible_role: Atk.Role.LIST_ITEM" in event_view
    assert "can_focus: true" in calendar

    for key_name in (
        "KEY_Left", "KEY_Right", "KEY_Up", "KEY_Down",
        "KEY_Page_Up", "KEY_Page_Down", "KEY_Home", "KEY_End",
    ):
        assert f"Clutter.{key_name}" in calendar
    assert '"key-press-event"' in calendar
    assert "grab_key_focus()" in calendar
    assert "can_focus: isSelected" in calendar

    assert METADATA["cinnamon-version"] == ["6.4", "6.6", "6.7"]
    assert "external-configuration-app" not in METADATA
    assert not (APPLET_DIR / "settings.py").exists()


if __name__ == "__main__":
    main()
