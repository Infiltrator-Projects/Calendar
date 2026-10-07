// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar panel controller.
 *
 * This file is the composition root: it owns service lifetimes and wires
 * narrow presentation/transport ports together. Cinnamon-version-specific
 * popup behaviour, panel-label mechanics and popup actor composition live in
 * dedicated modules rather than growing inside the controller.
 */

const Applet = imports.ui.applet;
const CalendarPlus = imports.gi.CalendarPlus;
const CinnamonDesktop = imports.gi.CinnamonDesktop;
const Gio = imports.gi.Gio;
const St = imports.gi.St;
const Gettext = imports.gettext;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Settings = imports.ui.settings;
const Util = imports.misc.util;

const UUID = "calendar-plus@the-infiltratr";
const CALENDAR_PLUS_GETTEXT_DOMAIN = UUID;

Gettext.bindtextdomain(CALENDAR_PLUS_GETTEXT_DOMAIN, "/usr/share/locale");
const CalendarPlusGettext = Gettext.domain(CALENDAR_PLUS_GETTEXT_DOMAIN);

function CP_(text) {
    return CalendarPlusGettext.gettext(text);
}

function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }
    return require("./runtimeSupport");
}

const RuntimeSupport = _loadRuntimeSupport();
const SignalBag = RuntimeSupport.SignalBag;
/* PopupShell owns RuntimeSupport.loadLocalModule("eventView") and the smaller presentation leaf modules. */
const Calendar = RuntimeSupport.loadLocalModule("calendar");
const EventManager = RuntimeSupport.loadLocalModule("eventManager");
const PanelClock = RuntimeSupport.loadLocalModule("panelClock");
const PopupShell = RuntimeSupport.loadLocalModule("popupShell");

class CalendarPlusApplet extends Applet.Applet {
    constructor(orientation, panel_height, instance_id, expectedVersion) {
        super(orientation, panel_height, instance_id);

        try {
            this._initialiseState(orientation, expectedVersion);
            this.setAllowedLayout(Applet.AllowedLayout.BOTH);
            this._buildApplet();
        } catch (error) {
            this._destroy();
            throw error;
        }
    }

    _initialiseState(orientation, expectedVersion) {
        this.orientation = orientation;
        this._destroyed = false;
        this._added_to_panel = false;
        this._keybinding_set = false;
        this.show_events = true;
        this.theme_mode = "system";
        this.keyOpen = "";

        this.menuManager = null;
        this.menu = null;
        this.settings = null;
        this.desktop_settings = null;
        this.clock = null;
        this.system_clock = null;
        this.events_manager = null;
        this.event_list = null;
        this._calendarEventSource = null;
        this._calendar = null;
        this._panelView = null;
        this._popupView = null;
        this._resume_source = null;
        this._temporalPolicyCache = null;

        this._signals = new SignalBag();
        this._eventSignals = new SignalBag();
        this._resumeSignals = new SignalBag();

        const nativeVersion = CalendarPlus.get_version();
        if (typeof expectedVersion !== "string" || expectedVersion.length === 0) {
            throw new Error(`${UUID}: Cinnamon did not provide an applet version.`);
        }
        if (nativeVersion !== expectedVersion) {
            throw new Error(
                `${UUID}: native library ${nativeVersion} does not match ` +
                `applet ${expectedVersion}`
            );
        }

        this._calendar_system = CalendarPlus.CalendarSystem.new("gregorian");
        if (this._calendar_system === null) {
            throw new Error(`${UUID}: native Gregorian calendar unavailable`);
        }
    }

    _buildApplet() {
        this._panelView = new PopupShell.PanelClockView(this.actor);
        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new PopupShell.CalendarPopupMenu(this, this.orientation);
        this.menu.setCustomStyleClass("calendar-plus-popup");
        this.menuManager.addMenu(this.menu);

        this.settings = new Settings.AppletSettings(this, UUID, this.instance_id);
        this.desktop_settings = new Gio.Settings({
            schema_id: "org.cinnamon.desktop.interface",
        });

        this.clock = new CinnamonDesktop.WallClock();
        this.system_clock = CalendarPlus.SystemClock.new();
        this._signals.connect(
            this.system_clock,
            "tick",
            () => this._updatePanelClock()
        );
        this._signals.connect(
            this.system_clock,
            "policy-changed",
            () => {
                this._temporalPolicyCache = null;
                this._onSettingsChanged();
            }
        );

        this.event_list = new PopupShell.EventList(this.desktop_settings);
        this.events_manager = new EventManager.EventsManager();
        this._calendarEventSource = new PopupShell.CalendarEventSource(
            this.events_manager
        );
        this._calendar = new Calendar.Calendar(
            this.settings,
            this._calendarEventSource,
            this.desktop_settings
        );

        this._wireAgenda();
        this._popupView = new PopupShell.PopupView(
            this.menu,
            this.event_list,
            this._calendar,
            {
                onResetCalendar: () => this._resetCalendar(),
                onLaunchSettings: () => this._onLaunchSettings(),
                onAbout: () => this._onAbout(),
            }
        );

        this._bindSettings();
        this.events_manager.set_enabled(this.show_events);
        this._watchDesktopPreferences();
        this._watchPointerAndMenu();
        this._startResumeMonitor();
    }

    _wireAgenda() {
        this._eventSignals.connect(
            this.events_manager,
            "agenda-date-changed",
            (manager, date) => {
                if (this.event_list && date) {
                    this.event_list.set_date(date);
                }
            }
        );
        this._eventSignals.connect(
            this.events_manager,
            "agenda-events-changed",
            (manager, snapshot, reset, loading) => {
                if (this.event_list) {
                    this.event_list.set_events(snapshot, reset, loading);
                }
            }
        );
        this._eventSignals.connect(
            this.events_manager,
            "events-manager-ready",
            () => this._syncEventVisibility(true)
        );
        this._eventSignals.connect(
            this.events_manager,
            "has-calendars-changed",
            () => this._syncEventVisibility(false)
        );
        this._eventSignals.connect(this.event_list, "launched-calendar", () => {
            if (this.menu) {
                this.menu.toggle();
            }
        });
        for (const [signal, passEvents] of [
            ["start-pass-events", true],
            ["stop-pass-events", false],
        ]) {
            this._eventSignals.connect(this.event_list, signal, () => {
                if (this.menu) {
                    this.menu.setEventPassthrough(passEvents);
                }
            });
        }
        this._eventSignals.connect(
            this._calendar,
            "selected-date-changed",
            () => this._updateClockAndDate()
        );
    }

    _bindSettings() {
        this.settings.bind(
            "show-events",
            "show_events",
            this._onShowEventsChanged
        );
        this.settings.bind(
            "theme-mode",
            "theme_mode",
            this._onThemeModeChanged
        );
        this.settings.bind("keyOpen", "keyOpen", this._setKeybinding);
        this._setKeybinding();
    }

    _onShowEventsChanged() {
        if (this._destroyed || !this._calendar || !this.events_manager) {
            return;
        }
        this.events_manager.set_enabled(this.show_events);
        this._calendar.refreshEventAvailability();
        this._syncEventVisibility(true);
    }

    _onThemeModeChanged() {
        if (this._destroyed) {
            return;
        }
        this._applyThemeMode();
        this._resetLabelWidth();
        this._rebalancePopupWidth();
    }

    _onDesktopClockPreferenceChanged(refreshAgenda) {
        if (this._destroyed || !this._calendar || !this.events_manager) {
            return;
        }
        this._resetLabelWidth();
        this._configureWallClock();
        this._updateClockAndDate();
        if (refreshAgenda && this.events_manager.is_active()) {
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
        }
    }

    _watchDesktopPreferences() {
        this._signals.connect(
            this.desktop_settings,
            "changed::clock-use-24h",
            () => this._onDesktopClockPreferenceChanged(true)
        );
        this._signals.connect(
            this.desktop_settings,
            "changed::clock-show-date",
            () => this._onDesktopClockPreferenceChanged(false)
        );
        this._signals.connect(
            this.desktop_settings,
            "changed::gtk-theme",
            () => this._onThemeModeChanged()
        );
    }

    _systemThemeName() {
        if (!this.desktop_settings) {
            return "";
        }
        try {
            const theme = this.desktop_settings.get_string("gtk-theme");
            return typeof theme === "string" ? theme.toLowerCase() : "";
        } catch (error) {
            return "";
        }
    }

    _systemPrefersDark() {
        return this._systemThemeName().includes("dark");
    }

    _systemUsesHighContrast() {
        const theme = this._systemThemeName().replace(/[-_ ]/g, "");
        return theme.includes("highcontrast");
    }

    _watchPointerAndMenu() {
        this._signals.connect(this.menu, "open-state-changed", (menu, open) => {
            if (this._destroyed || !open) {
                return;
            }
            this.events_manager.start_events();
            this._resetCalendar();
            this._rebalancePopupWidth();
        });
    }

    _startResumeMonitor() {
        try {
            const LoginManager = imports.misc.loginManager;
            if (LoginManager && typeof LoginManager.getLoginManager === "function") {
                this._resume_source = LoginManager.getLoginManager();
                this._resumeSignals.connect(
                    this._resume_source,
                    "prepare-for-sleep",
                    (suspending) => {
                        if (!suspending) {
                            this._onResume();
                        }
                    }
                );
                return;
            }
        } catch (error) {
            /* Older Cinnamon releases use the UPower compatibility signal below. */
        }

        const UPowerGlib = imports.gi.UPowerGlib;
        this._resume_source = new UPowerGlib.Client();
        try {
            this._resumeSignals.connect(
                this._resume_source,
                "notify-resume",
                () => this._onResume()
            );
        } catch (error) {
            this._resumeSignals.connect(
                this._resume_source,
                "notify::resume",
                () => this._onResume()
            );
        }
    }

    _onResume() {
        if (this._destroyed) {
            return;
        }
        const config = this._clockConfig();
        if (PanelClock.isNativeClockMode(config.mode) && this.system_clock) {
            this.system_clock.stop();
            this._syncSystemClock();
        }
        this._updateClockAndDate();
    }

    _applyThemeMode() {
        if (!this.menu || !this.menu.actor) {
            return;
        }

        let effectiveTheme = this.theme_mode;
        if (effectiveTheme !== "day" &&
            effectiveTheme !== "night" &&
            effectiveTheme !== "system") {
            effectiveTheme = "system";
        }
        if (effectiveTheme === "system") {
            if (this._systemUsesHighContrast()) {
                this.menu.setCustomStyleClass("calendar-plus-popup");
                return;
            }
            effectiveTheme = this._systemPrefersDark() ? "night" : "day";
        }

        this.menu.setCustomStyleClass(
            `calendar-plus-popup calendar-plus-theme-${effectiveTheme}`
        );
    }

    _onSettingsChanged() {
        if (this._destroyed || !this._calendar || !this.events_manager) {
            return;
        }

        this._applyThemeMode();
        this._resetLabelWidth();
        this._syncCalendarSystem();
        this._configureWallClock();
        this._syncSystemClock();
        this._updateClockAndDate();
        this._syncEventVisibility(false);
    }

    _systemTemporalPolicy() {
        const fallback = {
            mode: "standard",
            showSeconds: false,
            locationConfigured: false,
            latitude: 0.0,
            longitude: 0.0,
            calendar: "gregorian",
            authority: "mint-cinnamon",
            providerAvailable: false,
        };

        if (!this.system_clock) {
            return fallback;
        }
        if (this._temporalPolicyCache !== null) {
            return this._temporalPolicyCache;
        }

        try {
            const variant = this.system_clock.get_system_policy();
            if (!variant) {
                throw new Error("missing effective temporal policy");
            }
            const [
                mode,
                calendar,
                showSeconds,
                locationConfigured,
                latitude,
                longitude,
                authority,
                providerAvailable,
            ] = variant.deep_unpack();

            const numericLatitude = Number(latitude);
            const numericLongitude = Number(longitude);
            const modeIsValid =
                mode === "standard" ||
                mode === PanelClock.CLOCK_MODE_STANDARD_24 ||
                mode === PanelClock.CLOCK_MODE_STANDARD_12 ||
                PanelClock.isNativeClockMode(mode);
            const calendarIsValid =
                typeof calendar === "string" &&
                calendar.length > 0 &&
                CalendarPlus.CalendarSystem.new(calendar) !== null;

            if (typeof mode !== "string" || !modeIsValid ||
                !calendarIsValid ||
                !Number.isFinite(numericLatitude) ||
                numericLatitude < -90 || numericLatitude > 90 ||
                !Number.isFinite(numericLongitude) ||
                numericLongitude < -180 || numericLongitude > 180 ||
                (authority !== "mint-cinnamon" &&
                 authority !== "infiltrator-system-settings")) {
                throw new Error("invalid effective temporal policy");
            }

            const resolved = Object.freeze({
                mode,
                showSeconds: Boolean(showSeconds),
                locationConfigured: Boolean(locationConfigured),
                latitude: numericLatitude,
                longitude: numericLongitude,
                calendar,
                authority,
                providerAvailable: Boolean(providerAvailable),
            });
            this._temporalPolicyCache = resolved;
            return resolved;
        } catch (error) {
            global.logError(error);
            this._temporalPolicyCache = Object.freeze(fallback);
            return this._temporalPolicyCache;
        }
    }

    _syncCalendarSystem() {
        const temporal = this._systemTemporalPolicy();
        if (!this._calendar_system ||
            this._calendar_system.get_id() !== temporal.calendar) {
            const candidate = CalendarPlus.CalendarSystem.new(temporal.calendar);
            if (candidate) {
                this._calendar_system = candidate;
                this._calendar.setCalendarSystem(temporal.calendar);
            }
        }
    }

    _clockConfig() {
        const temporal = this._systemTemporalPolicy();
        return {
            mode: temporal.mode,
            showSeconds: temporal.showSeconds,
            locationConfigured: temporal.locationConfigured,
            latitude: temporal.latitude,
            longitude: temporal.longitude,
            vertical: this._isVerticalPanel(),
            desktopSettings: this.desktop_settings,
            primaryCalendar: temporal.calendar,
        };
    }

    _syncSystemClock() {
        PanelClock.syncNativeClock(this.system_clock, this._clockConfig());
    }

    _configureWallClock() {
        PanelClock.configureWallClock(this.clock, this._clockConfig());
    }

    _updatePanelClock() {
        if (this._destroyed || !this.clock || !this._panelView) {
            return;
        }

        const config = this._clockConfig();
        if (this._effectiveClockMode !== config.mode) {
            this._effectiveClockMode = config.mode;
            PanelClock.configureWallClock(this.clock, config);
            PanelClock.syncNativeClock(this.system_clock, config);
        }

        const text = PanelClock.panelText(
            this.clock,
            this.system_clock,
            config,
            this._calendar_system
        );
        if (text) {
            this._panelView.setText(text);
        }
    }

    _updateClockAndDate() {
        if (this._destroyed || !this.clock || !this._calendar ||
            !this.events_manager || !this.event_list || !this._popupView) {
            return;
        }

        this._updatePanelClock();
        const display = PanelClock.todayDisplay(
            this.clock,
            this._calendar_system,
            this._clockConfig()
        );
        const dayName = PanelClock.dayName(this.clock);
        this._popupView.updateToday(
            dayName,
            display.shortDate,
            this._calendar.todaySelected()
        );

        this.set_applet_tooltip(display.tooltip);
        this.event_list.refresh_variations();
    }

    _syncEventVisibility(forceRefresh) {
        if (!this.event_list || !this.events_manager ||
            !this._calendar || !this._popupView) {
            return;
        }
        this._popupView.setAgendaVisible(
            this.events_manager.should_show_event_pane()
        );
        if (forceRefresh && this.events_manager.is_active()) {
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
        }
    }

    _rebalancePopupWidth() {
        if (!this._destroyed && this._popupView) {
            this._popupView.rebalanceWidth();
        }
    }

    _setKeybinding() {
        if (this._destroyed) {
            return;
        }
        this._removeKeybinding();
        if (!this.keyOpen) {
            return;
        }

        if (Main.keybindingManager.addXletHotKey) {
            Main.keybindingManager.addXletHotKey(
                this,
                "calendar-open",
                this.keyOpen,
                () => this._openMenu()
            );
        } else {
            Main.keybindingManager.addHotKey(
                `${UUID}-open-${this.instance_id}`,
                this.keyOpen,
                () => this._openMenu()
            );
        }
        this._keybinding_set = true;
    }

    _removeKeybinding() {
        if (!this._keybinding_set) {
            return;
        }
        try {
            if (Main.keybindingManager.removeXletHotKey) {
                Main.keybindingManager.removeXletHotKey(this, "calendar-open");
            } else {
                Main.keybindingManager.removeHotKey(
                    `${UUID}-open-${this.instance_id}`
                );
            }
        } catch (error) {
            global.logError(error);
        }
        this._keybinding_set = false;
    }

    _openMenu() {
        if (!this._destroyed && this.menu) {
            this.menu.toggle();
        }
    }

    _resetCalendar() {
        if (!this._destroyed && this._calendar) {
            this._calendar.setDate(new Date(), true);
        }
    }

    _isVerticalPanel() {
        return this.orientation === St.Side.LEFT ||
            this.orientation === St.Side.RIGHT;
    }

    _resetLabelWidth() {
        if (this._panelView) {
            this._panelView.resetWidth();
        }
    }

    _onLaunchSettings() {
        if (this.menu) {
            this.menu.close();
        }

        const temporal = this._systemTemporalPolicy();
        if (temporal.providerAvailable) {
            try {
                const appInfo = Gio.DesktopAppInfo.new(
                    "org.infiltrator.SystemSettings.desktop"
                );
                if (appInfo) {
                    appInfo.launch([], global.create_app_launch_context());
                    return;
                }
            } catch (error) {
                global.logError(error);
            }
        }
        Util.spawnCommandLine("cinnamon-settings calendar");
    }

    openAbout() {
        this._onAbout();
    }

    configureApplet(tab = 0) {
        super.configureApplet(tab);
    }

    _onAbout() {
        if (this.menu) {
            this.menu.close(false);
        }

        try {
            const appInfo = Gio.DesktopAppInfo.new(
                "infiltrator-calendar-about.desktop"
            );
            if (!appInfo) {
                throw new Error("Calendar About desktop identity is unavailable");
            }
            appInfo.launch([], global.create_app_launch_context());
        } catch (error) {
            global.logError(error);
        }
    }

    on_applet_clicked() {
        this._openMenu();
    }

    on_applet_added_to_panel() {
        if (this._destroyed || this._added_to_panel) {
            return;
        }
        this._added_to_panel = true;

        this._signals.connect(this.clock, "notify::clock", () => {
            this._updateClockAndDate();
        });
        this._signals.connect(Main.themeManager, "theme-set", () => {
            this._applyThemeMode();
            this._resetLabelWidth();
            this._rebalancePopupWidth();
        });
        this._signals.connect(global.settings, "changed::panel-edit-mode", () => {
            this._resetLabelWidth();
        });

        this._onSettingsChanged();
        this.events_manager.start_events();
        this._resetCalendar();
    }

    on_panel_height_changed() {
        this._resetLabelWidth();
    }

    on_orientation_changed(orientation) {
        if (this._destroyed) {
            return;
        }
        this.orientation = orientation;
        if (this.menu) {
            this.menu.setOrientation(orientation);
        }
        this._onSettingsChanged();
    }

    on_applet_removed_from_panel() {
        this._destroy();
    }

    _destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._added_to_panel = false;

        this._removeKeybinding();
        this._resumeSignals.disconnectAll();
        this._eventSignals.disconnectAll();
        this._signals.disconnectAll();

        if (this.system_clock) {
            try {
                this.system_clock.stop();
            } catch (error) {
                global.logError(error);
            }
            this.system_clock = null;
        }

        if (this._popupView) {
            this._popupView.destroy();
            this._popupView = null;
        }

        if (this._calendar) {
            try {
                this._calendar.destroy();
            } catch (error) {
                global.logError(error);
            }
            this._calendar = null;
        }

        if (this._calendarEventSource) {
            this._calendarEventSource.destroy();
            this._calendarEventSource = null;
        }

        if (this.events_manager) {
            try {
                this.events_manager.destroy();
            } catch (error) {
                global.logError(error);
            }
            this.events_manager = null;
        }

        if (this.event_list) {
            try {
                this.event_list.destroy();
            } catch (error) {
                global.logError(error);
            }
            this.event_list = null;
        }

        if (this.settings) {
            try {
                this.settings.finalize();
            } catch (error) {
                global.logError(error);
            }
            this.settings = null;
        }

        if (this.menu) {
            try {
                this.menu.destroy();
            } catch (error) {
                global.logError(error);
            }
        }

        if (this._panelView) {
            this._panelView.destroy();
            this._panelView = null;
        }

        this._calendar_system = null;
        this.desktop_settings = null;
        this.clock = null;
        this.menu = null;
        this.menuManager = null;
        this._resume_source = null;
        this._temporalPolicyCache = null;
    }
}

function main(metadata, orientation, panel_height, instance_id) {
    return new CalendarPlusApplet(
        orientation,
        panel_height,
        instance_id,
        metadata.version
    );
}
