#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Static contracts for settings that cross the Cinnamon/C boundary."""

import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
METADATA = json.loads((PROJECT_ROOT / "src/cinnamon/metadata.json").read_text())
VERSION = METADATA["version"]
APPLET_DIR = PROJECT_ROOT / "src/cinnamon"


def main() -> None:
    schema = json.loads(
        (APPLET_DIR / "settings-schema.json").read_text(encoding="utf-8")
    )
    applet_source = (APPLET_DIR / "applet.js").read_text(encoding="utf-8")
    calendar_source = (APPLET_DIR / "calendar.js").read_text(encoding="utf-8")
    event_source = (APPLET_DIR / "eventView.js").read_text(encoding="utf-8")
    event_manager_source = (APPLET_DIR / "eventManager.js").read_text(encoding="utf-8")
    runtime_source = (APPLET_DIR / "runtimeSupport.js").read_text(encoding="utf-8")
    panel_clock_source = (APPLET_DIR / "panelClock.js").read_text(encoding="utf-8")
    system_clock_source = (
        PROJECT_ROOT / "src/adapters/system-clock.c"
    ).read_text(encoding="utf-8")
    forbidden_temporal_settings = {
        "clock-section",
        "follow-system-temporal",
        "clock-mode",
        "show-seconds",
        "location-section",
        "location-configured",
        "latitude",
        "longitude",
        "calendar-section",
        "primary-calendar",
        "secondary-calendar",
        "use-custom-format",
        "custom-format",
        "custom-tooltip-format",
        "format-button",
    }
    assert not forbidden_temporal_settings.intersection(schema)
    assert "get_system_mode()" in applet_source
    assert "get_system_calendar()" in applet_source
    assert "get_system_show_seconds()" in applet_source
    assert "get_system_location_configured()" in applet_source
    assert "get_system_latitude()" in applet_source
    assert "get_system_longitude()" in applet_source
    assert "follow_system_temporal" not in applet_source
    assert "secondary_calendar" not in applet_source
    assert "_secondary_date" not in applet_source

    theme = schema["theme-mode"]
    assert theme["type"] == "combobox"
    assert theme["default"] == "system"
    assert list(theme["options"].values()) == ["system", "day", "night"]
    assert 'this.settings.bind("theme-mode", "theme_mode", this._onSettingsChanged);' in applet_source
    assert 'this.theme_mode = "system";' in applet_source
    assert 'changed::${key}' in applet_source
    assert '"gtk-theme"' in applet_source
    assert 'this._systemPrefersDark() ? "night" : "day"' in applet_source
    assert "this._systemUsesHighContrast()" in applet_source
    assert "super.configureApplet(tab);" in applet_source
    assert not (APPLET_DIR / "settings.py").exists()
    assert 'this.menu.setCustomStyleClass("calendar-plus-popup");' in applet_source
    assert '`calendar-plus-popup calendar-plus-theme-${effectiveTheme}`' in applet_source
    assert 'setCustomStyleClass("calendar-background")' not in applet_source
    assert '_addStyleClass(this.menu.actor, "calendar-plus-popup")' not in applet_source
    stylesheet = (APPLET_DIR / "stylesheet.css").read_text(encoding="utf-8")
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

    common_typography = common_design["typography"]
    ui_family = common_typography["ui_family"]
    brand_family = common_typography["brand_family"]
    ui_regular_weight = common_typography["ui_regular_weight"]
    ui_bold_weight = common_typography["ui_bold_weight"]
    brand_weight = common_typography["brand_weight"]

    assert f'font-family: "{ui_family}";' in stylesheet
    assert f"font-weight: {ui_regular_weight};" in stylesheet
    assert f"font-weight: {ui_bold_weight};" in stylesheet
    assert "BEGIN GENERATED COMMON TYPOGRAPHY TOKENS" in stylesheet
    assert "FONT_UI_REGULAR" not in applet_source
    assert "FONT_UI_BOLD" not in applet_source
    assert "FONT_PANEL_CLOCK" not in applet_source
    assert "_applyTypography(" not in applet_source

    theme_css = stylesheet.split("Theme policy", 1)[1]
    canonical_colours = {
        value.lower()
        for mode in ("day", "night")
        for value in common_design["theme"]["palettes"][mode].values()
    }
    import re
    for colour in re.findall(r"#[0-9A-Fa-f]{6}", theme_css):
        assert colour.lower() in canonical_colours, (
            f"Calendar theme CSS has a private colour outside Common: {colour}"
        )

    for mode in ("day", "night"):
        palette = common_design["theme"]["palettes"][mode]
        for role in (
            "background",
            "card",
            "border",
            "text",
            "heading",
            "summary",
            "surface_hover",
            "status_border",
            "neutral_accent",
            "accent_foreground",
        ):
            assert palette[role].lower() in theme_css.lower(), (
                f"Calendar {mode} CSS does not consume Common role {role}"
            )

    # Configuration is rendered by Cinnamon's own xlet-settings process.
    # Calendar contributes schema/behaviour only, eliminating its Python/GTK
    # runtime host while preserving every setting.
    assert "external-configuration-app" not in METADATA
    assert "configureApplet(tab = 0)" in applet_source
    assert "super.configureApplet(tab);" in applet_source

    # Calendar has no local temporal authority. A valid System Settings policy
    # enriches the stock Mint behaviour; without one, the native facade falls
    # back to Cinnamon/locale settings instead of requiring System Settings.
    assert 'this.settings.bind("show-seconds"' not in applet_source
    assert 'this.settings.bind("latitude"' not in applet_source
    assert 'this.settings.bind("longitude"' not in applet_source
    assert '"primary-calendar"' not in applet_source
    assert '"secondary-calendar"' not in applet_source
    assert "this.clock_mode" not in applet_source
    assert "this.show_seconds" not in applet_source
    assert "this.latitude =" not in applet_source
    assert "this.longitude =" not in applet_source
    assert "useCustomFormat" not in applet_source
    assert "customFormat" not in applet_source
    assert "customTooltipFormat" not in applet_source
    assert "useCustomFormat" not in panel_clock_source
    assert "customFormat" not in panel_clock_source
    assert "customTooltipFormat" not in panel_clock_source
    assert "CalendarPlus.SystemClock.new()" in applet_source
    assert "this.system_clock.get_system_calendar()" in applet_source
    assert "this._calendar.setCalendarSystem(temporal.calendar)" in applet_source
    assert "systemClock.start_at_location(" in panel_clock_source
    assert "time_mode_requires_longitude(" in panel_clock_source
    assert "time_mode_requires_latitude(" in panel_clock_source
    assert '"N/A LOC"' in panel_clock_source

    assert "CalendarPlus.CalendarSystem.new(" in calendar_source
    assert "this._calendar.setCalendarSystem(" in applet_source
    assert "this._calendarSystem.add_months_parts(" in calendar_source
    assert "this._calendarSystem.add_years_parts(" in calendar_source
    assert "this._calendarSystem.build_grid(" in calendar_source
    assert "CalendarPlus.DatePart.DAY" in calendar_source
    assert "CalendarPlus.DatePart.SHORT" in panel_clock_source
    assert "CalendarPlus.DatePart.FULL" in panel_clock_source
    assert ".format_date_part(" in calendar_source
    assert ".format_date_part(" not in applet_source
    assert ".format_date_part(" in panel_clock_source
    assert ".format_date(" not in calendar_source
    assert ".format_date(" not in applet_source
    assert ".format_date(" not in panel_clock_source
    assert "while (cellsPlaced < 42)" not in calendar_source

    # Date equality, work-week semantics and navigation are native contracts.
    # JavaScript passes typed date parts and never maintains a parallel date
    # arithmetic implementation.
    assert "CalendarPlus.date_same(" in calendar_source
    assert "CalendarPlus.date_is_work_day(" in calendar_source
    assert "function _isWorkDay(" not in calendar_source
    assert "_dateFromIso" not in calendar_source

    # CalendarServer tuples, interval queries, culling and sorting belong to
    # the native store. JavaScript retains only Cinnamon's D-Bus and actor APIs.
    assert "CalendarPlus.EventStore.new()" in event_manager_source
    assert "this.event_store.add_or_update(" in event_manager_source
    assert "this.event_store.get_snapshot(" in event_manager_source
    assert "this.event_store.get_colors(" in event_manager_source
    assert "this.event_store.refresh_timezone()" in event_manager_source
    assert "CalendarPlus.event_day_relation(" in event_manager_source
    assert "CalendarPlus.event_timing(" in event_manager_source
    assert "CalendarPlus.EventState." in event_source
    assert "CalendarPlus.EventDayRelation." in event_source
    for legacy in (
        "starts_on_day(date)",
        "ends_on_day(date)",
        "started_before_day(date)",
        "ended_before_day(date)",
        "ends_after_day(date)",
        "started_after_day(date)",
    ):
        assert legacy not in event_manager_source
        assert legacy not in event_source
    assert "class EventDataList" not in event_manager_source
    assert "this.events_by_date" not in event_manager_source

    # Seconds follow the richer Infiltrator policy when present and otherwise
    # mirror Cinnamon's stock clock-show-seconds setting through the native
    # facade. Calendar itself still owns no duplicate seconds preference.
    assert 'get_boolean("clock-show-seconds")' not in applet_source
    assert "get_system_show_seconds()" in applet_source
    assert '"clock-show-seconds"' in applet_source
    assert "load_persisted_temporal_policy" in system_clock_source
    assert 'g_find_program_in_path("system-settings")' in system_clock_source
    assert '"org.cinnamon.desktop.interface"' in system_clock_source
    assert '"clock-show-seconds"' in system_clock_source
    assert "g_settings_schema_has_key" in system_clock_source

    # The settings menu prefers Infiltrator System Settings but must remain
    # useful on an ordinary Mint installation where that program is absent.
    assert 'GLib.find_program_in_path("system-settings")' in applet_source
    assert 'Util.spawnCommandLine("system-settings")' in applet_source
    assert 'Util.spawnCommandLine("cinnamon-settings calendar")' in applet_source

    # Standard horizontal clocks need all combinations of date, 12/24-hour
    # mode and seconds while retaining Cinnamon's locale-aware formatting.
    required_formats = {
        "withDate24Seconds",
        "withDate12Seconds",
        "withDate24",
        "withDate12",
        "withoutDate24Seconds",
        "withoutDate12Seconds",
        "withoutDate24",
        "withoutDate12",
    }
    assert all(name in panel_clock_source for name in required_formats)

    # The three conventional choices stay in Cinnamon's locale-aware path.
    assert 'var CLOCK_MODE_STANDARD = "standard";' in panel_clock_source
    assert 'var CLOCK_MODE_STANDARD_24 = "standard-24";' in panel_clock_source
    assert 'var CLOCK_MODE_STANDARD_12 = "standard-12";' in panel_clock_source
    assert "function isNativeClockMode(mode)" in panel_clock_source
    assert 'RuntimeSupport.loadLocalModule("panelClock")' in applet_source
    assert "PanelClock.panelText(" in applet_source
    assert "PanelClock.todayDisplay(" in applet_source

    # Construction must be atomic. Essential native state is established
    # before actors are built; a failed build cleans up and rethrows instead
    # of leaving Cinnamon with a partly initialised applet.
    assert "this._initialiseState(orientation, expectedVersion);" in applet_source
    assert "this._buildApplet();" in applet_source
    assert "this._destroy();\n            throw error;" in applet_source
    assert applet_source.index("this._initialiseState(orientation, expectedVersion);") < \
        applet_source.index("this._buildApplet();")
    assert "on_applet_removed_from_panel()" in applet_source
    assert "this.events_manager.destroy();" in applet_source
    assert "this.settings.finalize();" in applet_source

    # D-Bus watches, cancellables, GLib sources and proxy signals have a
    # deterministic lifecycle when the applet is removed or construction
    # aborts.
    assert "this._bus_watch_id = 0;" in event_manager_source
    assert "this._serverSignals = new SignalBag();" in event_manager_source
    assert "this._cancellable = new Gio.Cancellable();" in event_manager_source
    assert "Gio.bus_unwatch_name(this._bus_watch_id);" in event_manager_source
    assert "() => this._calendarServerVanished()" in event_manager_source
    assert "this._scheduleReconnect();" in event_manager_source
    assert "this._cancelReconnect();" in event_manager_source
    assert "this._cancellable.cancel();" in event_manager_source
    assert 'Gio.File.new_for_path("/etc/localtime")' in event_manager_source
    assert "this._timezone_monitor.cancel();" in event_manager_source
    assert "destroy()" in event_manager_source
    assert "this._bus_watch_id\n" not in event_manager_source

    # The month view is also a long-lived signal/source owner. SignalBag keeps
    # those ownership groups explicit, and pending GLib sources are cancelled
    # before the actor graph is destroyed.
    assert "this._eventSignals = new SignalBag();" in calendar_source
    assert "this._desktopSignals = new SignalBag();" in calendar_source
    assert "this._actorSignals = new SignalBag();" in calendar_source
    assert "_cancel_set_date_idle()" in calendar_source
    assert "this._calendar.destroy();" in applet_source
    assert calendar_source.count('"style-changed"') == 1
    assert "disconnectAll()" in calendar_source
    assert "destroy() {" in calendar_source

    # Event fetching follows the native 42-cell grid rather than assuming that
    # every primary calendar shares Gregorian month boundaries.
    assert "set_visible_range(firstDate, lastDate, force)" in event_manager_source
    assert "this.events_manager.set_visible_range(" in calendar_source
    assert "fetch_month_events" not in event_manager_source
    assert "current_month_year" not in event_manager_source

    # A stale native library must be rejected explicitly rather than allowed
    # to fail later through a missing or incompatible symbol.
    assert "const APP_VERSION" not in applet_source
    assert "metadata.version" in applet_source
    assert "CalendarPlus.get_version()" in applet_source
    assert applet_source.index("this._signals = new SignalBag();") < \
        applet_source.index("CalendarPlus.get_version()")
    assert "native library ${nativeVersion} does not match " in applet_source
    assert "`applet ${expectedVersion}`" in applet_source

    # Shared runtime mechanics live in one module; feature modules own only
    # their domain state. Transport and agenda presentation are separate.
    assert runtime_source.count("var SignalBag = class SignalBag") == 1
    assert runtime_source.count("var midnight = function midnight") == 1
    assert runtime_source.count("var sameInstant = function sameInstant") == 1
    for source in (applet_source, calendar_source, event_source, event_manager_source, panel_clock_source):
        assert "class SignalBag" not in source
    for source in (event_source, event_manager_source):
        assert "function _midnight" not in source
        assert "function _sameInstant" not in source
        assert "RuntimeSupport.midnight" in source
        assert "RuntimeSupport.sameInstant" in source
    assert 'RuntimeSupport.loadLocalModule("eventManager")' in applet_source
    assert "class EventList" not in event_manager_source
    assert "class EventRow" not in event_manager_source
    assert "var EventList = class EventList" in event_source
    assert '_("Calendar events")' in calendar_source
    assert 'accessible_name: accessibleParts.join(", ")' in calendar_source
    assert 'this.actor.set_accessible_name(accessibleParts.join(", "));' in event_source
    assert "class EventRow" in event_source
    assert "use_custom_format" not in applet_source
    assert "custom_tooltip_format" not in applet_source
    assert "_onFormatSettingsChanged" not in applet_source
    assert "_cancelFormatDebounce" not in applet_source
    assert "formattedTooltip.capitalize()" not in panel_clock_source
    assert "new EventManager.EventsManager(" in applet_source

    # Calendar-owned interface text uses its own installed gettext domain;
    # Cinnamon-provided desktop strings remain in Cinnamon's catalogue.
    for source in (applet_source, calendar_source, event_source):
        assert '"calendar-plus@the-infiltratr"' in source
        assert "Gettext.bindtextdomain(" in source
    for label in (
        "Previous month",
        "Next month",
        "Previous year",
        "Next year",
        "Week",
    ):
        assert f'CP_("{label}")' in calendar_source
    assert 'CP_("Show today")' in applet_source
    assert 'CP_("About Calendar")' in applet_source
    # Every About entry point uses the same Cinnamon/St dialog. Calendar no
    # longer launches a GTK helper solely to display About information.
    assert "openAbout()" in applet_source
    assert "this._onAbout();" in applet_source
    assert "new ModalDialog.ModalDialog()" in applet_source
    assert "new Dialog.MessageDialogContent" not in applet_source
    assert 'icon_name: "infiltratr-calendar"' in applet_source
    assert 'style_class: "calendar-plus-about"' in applet_source
    assert 'style_class: "calendar-plus-about-title"' in applet_source
    assert 'style_class: "calendar-plus-about-version"' in applet_source
    assert 'style_class: "calendar-plus-about-build"' in applet_source
    assert 'CalendarPlus.get_build_profile_label()' in applet_source
    assert 'style_class: "calendar-plus-about-author"' in applet_source
    assert 'text: "Shannon Smith"' in applet_source
    assert 'text: "GPL-3.0-or-later"' in applet_source
    assert 'Util.spawnCommandLine("/usr/libexec/calendar-plus-about")' not in applet_source
    assert "xlet-about-dialog" not in applet_source
    assert 'CP_("Open selected date in Calendar")' in event_source
    assert 'CP_("Open Calendar")' in event_source

    # New interactive surfaces remain reachable without a pointer.
    assert "class CalendarPlusApplet extends Applet.Applet" in applet_source
    assert "can_focus: true" in calendar_source
    assert "can_focus: canLaunch" in event_source
    assert event_source.count('find_program_in_path("gnome-calendar")') == 1
    assert "clickable: this._canLaunchCalendar" in event_source
    assert "reactive: this._canLaunchCalendar" in event_source
    assert "can_focus: this._canLaunchCalendar" in event_source
    assert "accessible_role: Atk.Role.LIST_ITEM" in event_source
    assert "accessible_name:" in calendar_source

    # Calendar cells use a roving focus target: only the selected day enters
    # the Tab sequence, while spatial keys navigate inside the 42-cell grid.
    for key_name in (
        "KEY_Left", "KEY_Right", "KEY_Up", "KEY_Down",
        "KEY_Page_Up", "KEY_Page_Down", "KEY_Home", "KEY_End",
    ):
        assert f"Clutter.{key_name}" in calendar_source
    assert '"key-press-event"' in calendar_source
    assert "grab_key_focus()" in calendar_source
    assert "can_focus: isSelected" in calendar_source
    assert "Clutter.ModifierType.SHIFT_MASK" in calendar_source
    assert 'button.connect("clicked", () => this.setDate(date, false));' in calendar_source
    assert (
        'if (this.events_enabled) {\n                this.setDate(date, false);'
        not in calendar_source
    )

    metadata = json.loads(
        (APPLET_DIR / "metadata.json").read_text(encoding="utf-8")
    )
    assert metadata["cinnamon-version"] == ["6.4", "6.6", "6.7"]
    assert "external-configuration-app" not in metadata


if __name__ == "__main__":
    main()
