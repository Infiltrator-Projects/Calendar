// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar panel composition root.
 *
 * This controller owns Calendar feature lifetimes and wires narrow semantic
 * ports together. Cinnamon desktop preferences, suspend/resume compatibility,
 * global keybindings and application launching are delegated to the dedicated
 * desktopIntegration module rather than accumulating platform mechanics here.
 */

const Applet = imports.ui.applet;
const NativeCalendar = imports.gi.Calendar;
const Cinnamon = imports.gi.Cinnamon;
const CinnamonDesktop = imports.gi.CinnamonDesktop;
const St = imports.gi.St;
const PopupMenu = imports.ui.popupMenu;
const Settings = imports.ui.settings;

const UUID = "calendar@the-infiltratr";

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
const DesktopIntegration = RuntimeSupport.loadLocalModule("desktopIntegration");
const TemporalPolicy = RuntimeSupport.loadLocalModule("temporalPolicy");
/* PopupShell owns eventView and the smaller presentation leaf modules. */
const CalendarWidget = RuntimeSupport.loadLocalModule("calendar");
const EventManager = RuntimeSupport.loadLocalModule("eventManager");
const PanelClock = RuntimeSupport.loadLocalModule("panelClock");
const PopupShell = RuntimeSupport.loadLocalModule("popupShell");

class CalendarApplet extends Applet.Applet {
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
        this.show_events = true;
        this.show_week_numbers = false;
        this.theme_mode = "system";
        this.keyOpen = "";

        this.menuManager = null;
        this.menu = null;
        this.settings = null;
        this.clock = null;
        this.system_clock = null;
        this.events_manager = null;
        this.event_list = null;
        this._calendarEventSource = null;
        this._calendar = null;
        this._panelView = null;
        this._popupView = null;
        this._temporalPolicy = null;
        this._desktopIntegration = null;

        this._signals = new SignalBag();
        this._eventSignals = new SignalBag();

        const nativeVersion = NativeCalendar.get_version();
        if (typeof expectedVersion !== "string" || expectedVersion.length === 0) {
            throw new Error(`${UUID}: Cinnamon did not provide an applet version.`);
        }
        if (nativeVersion !== expectedVersion) {
            throw new Error(
                `${UUID}: native library ${nativeVersion} does not match ` +
                `applet ${expectedVersion}`
            );
        }

        this._calendar_system = NativeCalendar.CalendarSystem.new("gregorian");
        if (this._calendar_system === null) {
            throw new Error(`${UUID}: native Gregorian calendar unavailable`);
        }
    }

    _buildApplet() {
        this._panelView = new PopupShell.PanelClockView(this.actor);
        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new PopupShell.CalendarPopupMenu(this, this.orientation);
        this.menu.setCustomStyleClass("calendar-popup");
        this.menuManager.addMenu(this.menu);

        this.settings = new Settings.AppletSettings(this, UUID, this.instance_id);
        this._desktopIntegration = new DesktopIntegration.DesktopIntegration(
            this,
            SignalBag,
            {
                onWeekStartChanged: () => this._onWeekStartChanged(),
                onClockPreferenceChanged: (refreshAgenda) =>
                    this._onDesktopClockPreferenceChanged(refreshAgenda),
                onThemeChanged: () => this._onThemeModeChanged(),
                onResume: () => this._onResume(),
                onPanelEditModeChanged: () => this._resetLabelWidth(),
            }
        );
        this._desktopIntegration.start();

        this.clock = new CinnamonDesktop.WallClock();
        this.system_clock = NativeCalendar.SystemClock.new();
        this._temporalPolicy = new TemporalPolicy.TemporalPolicyResolver(
            this.system_clock,
            PanelClock.isNativeClockMode
        );
        this._signals.connect(
            this.system_clock,
            "tick",
            () => this._updatePanelClock()
        );
        this._signals.connect(
            this.system_clock,
            "policy-changed",
            () => {
                this._temporalPolicy.invalidate();
                this._onSettingsChanged();
            }
        );

        this.show_week_numbers = Boolean(
            this.settings.getValue("show-week-numbers")
        );
        this.event_list = new PopupShell.EventList(
            this._desktopUses24h(),
            this._calendar_system
        );
        this.events_manager = new EventManager.EventsManager();
        this._calendarEventSource = new PopupShell.CalendarEventSource(
            this.events_manager
        );
        this._calendar = new CalendarWidget.Calendar(
            this._calendarEventSource,
            Cinnamon.util_get_week_start(),
            this.show_week_numbers
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
        this._watchPointerAndMenu();
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
            (manager, snapshot, reset, unavailable) => {
                if (this.event_list) {
                    this.event_list.set_events(snapshot, reset, unavailable);
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
            "show-week-numbers",
            "show_week_numbers",
            this._onShowWeekNumbersChanged
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

    _onShowWeekNumbersChanged() {
        if (!this._destroyed && this._calendar) {
            this._calendar.setShowWeekNumbers(this.show_week_numbers);
        }
    }

    _onWeekStartChanged() {
        if (!this._destroyed && this._calendar) {
            this._calendar.setWeekStart(Cinnamon.util_get_week_start());
        }
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
        this.event_list.set_use_24h(this._desktopUses24h());
        this._configureWallClock();
        this._updateClockAndDate();
        if (refreshAgenda && this.events_manager.is_active()) {
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
        }
    }

    _desktopUses24h() {
        return this._desktopIntegration
            ? this._desktopIntegration.uses24h()
            : false;
    }

    _desktopShowsDate() {
        return this._desktopIntegration
            ? this._desktopIntegration.showsDate()
            : false;
    }

    _systemPrefersDark() {
        return this._desktopIntegration
            ? this._desktopIntegration.prefersDark()
            : false;
    }

    _systemUsesHighContrast() {
        return this._desktopIntegration
            ? this._desktopIntegration.usesHighContrast()
            : false;
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
                this.menu.setCustomStyleClass("calendar-popup");
                return;
            }
            effectiveTheme = this._systemPrefersDark() ? "night" : "day";
        }

        this.menu.setCustomStyleClass(
            `calendar-popup calendar-theme-${effectiveTheme}`
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

    _syncCalendarSystem() {
        const temporal = this._temporalPolicy.get();
        if (!this._calendar_system ||
            this._calendar_system.get_id() !== temporal.calendar) {
            const candidate = NativeCalendar.CalendarSystem.new(temporal.calendar);
            if (candidate) {
                this._calendar_system = candidate;
                this._calendar.setCalendarSystem(temporal.calendar);
            }
        }
        if (this.event_list) {
            this.event_list.set_calendar_system(this._calendar_system);
        }
    }

    _clockConfig() {
        return this._temporalPolicy.clockConfig({
            vertical: this._isVerticalPanel(),
            use24h: this._desktopUses24h(),
            showDate: this._desktopShowsDate(),
        });
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
        if (!this._destroyed && this._desktopIntegration) {
            this._desktopIntegration.setKeybinding(
                this.keyOpen,
                () => this._openMenu()
            );
        }
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
        if (this._desktopIntegration && this._temporalPolicy) {
            this._desktopIntegration.launchSettings(
                this._temporalPolicy.get().providerAvailable
            );
        }
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
        if (this._desktopIntegration) {
            this._desktopIntegration.launchAbout();
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
        if (this._desktopIntegration) {
            this._desktopIntegration.startPanelWatchers();
        }

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

        this._eventSignals.disconnectAll();
        this._signals.disconnectAll();

        if (this._desktopIntegration) {
            this._desktopIntegration.destroy();
            this._desktopIntegration = null;
        }

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
        this.clock = null;
        this.menu = null;
        this.menuManager = null;
        this._temporalPolicy = null;
    }
}

function main(metadata, orientation, panel_height, instance_id) {
    return new CalendarApplet(
        orientation,
        panel_height,
        instance_id,
        metadata.version
    );
}
