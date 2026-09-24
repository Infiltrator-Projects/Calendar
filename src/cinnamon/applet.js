// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar panel controller.
 *
 * Architecture
 * ------------
 * Cinnamon owns actors, menus, settings bindings and desktop integration.
 * libcalendar-plus owns deterministic calendar arithmetic, non-standard time
 * systems and their boundary-aligned timer.  This file is intentionally the
 * narrow orchestration layer between those two worlds.
 *
 * Lifecycle invariant
 * -------------------
 * _initialiseState establishes every cleanup field before validating the
 * native version or building actors. Later construction can therefore fail
 * without making _destroy() guess which resources exist.
 */

const Applet = imports.ui.applet;
const CalendarPlus = imports.gi.CalendarPlus;
const CinnamonDesktop = imports.gi.CinnamonDesktop;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Pango = imports.gi.Pango;
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

function _addStyleClass(actor, styleClass) {
    if (actor && typeof actor.add_style_class_name === "function") {
        actor.add_style_class_name(styleClass);
    }
}

/*
 * Bootstrap only the shared runtime helper here.  Once loaded, it owns the
 * Cinnamon 6.4/6.6/6.7 module-resolution seam for every feature module.
 */
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
const Calendar = RuntimeSupport.loadLocalModule("calendar");
const EventManager = RuntimeSupport.loadLocalModule("eventManager");
const EventView = RuntimeSupport.loadLocalModule("eventView");

const PanelClock = RuntimeSupport.loadLocalModule("panelClock");

/*
 * Keep the panel clock visually stable without forcing St.Bin.min_width.
 * Cinnamon moved this latch into the container's preferred-width contract
 * after Clutter began warning when a child's natural width fell below a
 * previously forced minimum. The label remains free to report its real
 * natural width; only the bin's request is latched.
 */
const LatchedWidthBin = GObject.registerClass(
class LatchedWidthBin extends St.Bin {
    _init(params = {}) {
        super._init(params);
        this._latchedWidth = 0;
    }

    resetLatch() {
        this._latchedWidth = 0;
        this.updateLatch();
    }

    updateLatch() {
        const label = this.get_child();
        if (!label) {
            return;
        }

        const [, naturalWidth] = label.get_preferred_width(-1);
        if (naturalWidth <= 0) {
            return;
        }

        /*
         * A live clock must never contract on an ordinary tick: proportional
         * glyph advances otherwise move neighbouring panel content and make
         * the time appear to "breathe". Grow to the widest value observed for
         * the current presentation, then shrink only through resetLatch() when
         * a real layout input (mode/theme/orientation/panel size) changes.
         */
        if (naturalWidth > this._latchedWidth) {
            this._latchedWidth = naturalWidth;
            this.queue_relayout();
        }
    }

    vfunc_get_preferred_width(forHeight) {
        const [minimum, natural] = super.vfunc_get_preferred_width(forHeight);
        return [
            Math.max(minimum, this._latchedWidth),
            Math.max(natural, this._latchedWidth),
        ];
    }
});

class CalendarPlusApplet extends Applet.Applet {
    constructor(orientation, panel_height, instance_id, expectedVersion) {
        super(orientation, panel_height, instance_id);

        try {
            this._initialiseState(orientation, expectedVersion);
            this._createPanelLabel();
            this.setAllowedLayout(Applet.AllowedLayout.BOTH);
            this._buildApplet();
        } catch (error) {
            this._destroy();
            throw error;
        }
    }

    _createPanelLabel() {
        _addStyleClass(this.actor, "calendar-plus-applet");
        const label = new St.Label({
            style_class: "applet-label calendar-plus-panel-clock",
        });
        label.reactive = true;
        label.track_hover = true;
        label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        const holder = new LatchedWidthBin();
        holder.set_child(label);
        this.actor.add(holder, { y_align: St.Align.MIDDLE, y_fill: false });
        this.actor.set_label_actor(label);

        this._clockLabel = label;
        this._labelBin = holder;
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
        this._calendar = null;
        this._popupBody = null;
        this._calendarColumn = null;
        this._resume_source = null;

        this._signals = new SignalBag();
        this._eventSignals = new SignalBag();
        this._resumeSignals = new SignalBag();

        /*
         * Version identity is a hard boundary, not a feature probe.  Mixing an
         * applet with a stale typelib can otherwise fail much later through a
         * missing symbol and leave Cinnamon with a partially built menu.
         */
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

        this._calendar_system =
            CalendarPlus.CalendarSystem.new("gregorian");
        if (this._calendar_system === null) {
            throw new Error(`${UUID}: native Gregorian calendar unavailable`);
        }
    }

    _buildApplet() {
        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new Applet.AppletPopupMenu(this, this.orientation);
        /*
         * PopupMenu.setCustomStyleClass() rebuilds the complete actor style
         * class list.  Keep Calendar's identity in that authoritative slot:
         * adding calendar-plus-popup first and then calling setCustomStyleClass()
         * silently removed it, so every Day/Night selector missed the visible
         * popup actor.
         */
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
            () => this._onSettingsChanged()
        );

        this.event_list = new EventView.EventList(
            this.settings,
            this.desktop_settings
        );
        this.events_manager = new EventManager.EventsManager(
            this.settings,
            this.desktop_settings,
            this.event_list
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

        this._buildPopupContents();
        this._bindSettings();
        this._watchDesktopPreferences();
        this._watchPointerAndMenu();
        this._startResumeMonitor();
    }

    _buildPopupContents() {
        const body = new St.BoxLayout({
            style_class: "calendar-main-box",
            vertical: false,
        });
        this._popupBody = body;
        this.menu.addActor(body);

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
                    this.menu.passEvents = passEvents;
                }
            });
        }
        body.add_actor(this.event_list.actor);

        const calendarColumn = new St.BoxLayout({ vertical: true });
        this._calendarColumn = calendarColumn;
        this.go_home_button = new St.Button({
            style_class: "calendar-today-home-button",
            x_align: Clutter.ActorAlign.CENTER,
            reactive: true,
            can_focus: true,
            accessible_name: CP_("Show today"),
        });
        this._today_box = new St.BoxLayout({ vertical: true });
        this.go_home_button.set_child(this._today_box);
        this.go_home_button.connect("clicked", () => this._resetCalendar());

        this._day = new St.Label({ style_class: "calendar-today-day-label" });
        this._date = new St.Label({ style_class: "calendar-today-date-label" });
        this._today_box.add_actor(this._day);
        this._today_box.add_actor(this._date);
        calendarColumn.add_actor(this.go_home_button);

        this._calendar = new Calendar.Calendar(this.settings, this.events_manager);
        this._eventSignals.connect(
            this._calendar,
            "selected-date-changed",
            () => this._updateClockAndDate()
        );
        calendarColumn.add_actor(this._calendar.actor);
        body.add_actor(calendarColumn);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const dateTimeSettings = new PopupMenu.PopupMenuItem(
            _("Date and Time Settings")
        );
        dateTimeSettings.connect("activate", () => this._onLaunchSettings());
        this.menu.addMenuItem(dateTimeSettings);

        const aboutItem = new PopupMenu.PopupMenuItem(CP_("About Calendar"));
        aboutItem.connect("activate", () => this._onAbout());
        this.menu.addMenuItem(aboutItem);
    }

    _bindSettings() {
        this.settings.bind("show-events", "show_events", this._onSettingsChanged);
        this.settings.bind("theme-mode", "theme_mode", this._onSettingsChanged);
        this.settings.bind("keyOpen", "keyOpen", this._setKeybinding);
        this._setKeybinding();
    }

    _watchDesktopPreferences() {
        for (const key of [
            "clock-use-24h",
            "clock-show-date",
            "gtk-theme",
        ]) {
            this._signals.connect(
                this.desktop_settings,
                `changed::${key}`,
                () => this._onSettingsChanged()
            );
        }
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
            if (!open || this._destroyed) {
                return;
            }
            this._resetCalendar();
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
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

        /*
         * Native timers use monotonic scheduling.  Suspend pauses that clock,
         * so restarting recalculates the next visible boundary from current
         * civil time rather than firing with a stale pre-suspend remainder.
         */
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

        /*
         * Persist the effective theme inside PopupMenu's custom style class.
         * Cinnamon rewrites menu.actor's style classes whenever orientation
         * changes; add_style_class_name() therefore made forced themes fragile.
         * setCustomStyleClass() is the supported durable ownership point.
         */
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
        this._syncEventVisibility(true);
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

            if (typeof mode !== "string" || mode.length === 0 ||
                typeof calendar !== "string" || calendar.length === 0 ||
                (authority !== "mint-cinnamon" &&
                 authority !== "infiltrator-system-settings")) {
                throw new Error("invalid effective temporal policy");
            }

            return {
                mode,
                showSeconds: Boolean(showSeconds),
                locationConfigured: Boolean(locationConfigured),
                latitude: Number(latitude),
                longitude: Number(longitude),
                calendar,
                authority,
                providerAvailable: Boolean(providerAvailable),
            };
        } catch (error) {
            global.logError(error);
            return fallback;
        }
    }

    _syncCalendarSystem() {
        const temporal = this._systemTemporalPolicy();

        if (!this._calendar_system ||
            this._calendar_system.get_id() !== temporal.calendar) {
            const candidate = CalendarPlus.CalendarSystem.new(
                temporal.calendar
            );
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
        if (this._destroyed || !this.clock) {
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
            this._clockLabel.set_text(text);
            this._updateLabelWidth();
        }
    }

    _updateClockAndDate() {
        if (this._destroyed || !this.clock || !this._calendar ||
            !this.events_manager || !this.go_home_button) {
            return;
        }

        this._updatePanelClock();

        const display = PanelClock.todayDisplay(
            this.clock,
            this._calendar_system,
            this._clockConfig()
        );
        const dayName = PanelClock.dayName(this.clock);

        const selectedToday = this._calendar.todaySelected();
        this.go_home_button.reactive = !selectedToday;
        this.go_home_button.set_style_class_name(
            selectedToday
                ? "calendar-today-home-button"
                : "calendar-today-home-button-enabled"
        );

        this._day.set_text(dayName);
        this._date.set_text(display.shortDate);
        this.go_home_button.set_accessible_name(
            `${CP_("Show today")}: ${dayName}, ${display.shortDate}`
        );

        const tooltip = display.tooltip;
        this.set_applet_tooltip(tooltip);
        this.events_manager.select_date(this._calendar.getSelectedDate());
    }

    _syncEventVisibility(forceRefresh) {
        if (!this.event_list || !this.events_manager || !this._calendar) {
            return;
        }
        this.event_list.actor.visible =
            this.events_manager.should_show_event_pane();
        this._rebalancePopupWidth();
        if (forceRefresh && this.events_manager.is_active()) {
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
        }
    }

    _rebalancePopupWidth() {
        if (this._destroyed || !this._calendarColumn || !this.event_list) {
            return;
        }

        /*
         * Cinnamon's calendar theme gives the agenda a generous natural
         * width. With the full month grid beside it, that can leave the month
         * side cramped even though the popup still has room to grow. Measure
         * the active theme rather than baking pixel dimensions into the applet:
         * the month may grow toward the agenda's natural width, but never by
         * more than 35 percent over its own natural request.
         *
         * Reset min_width before measurement so a previous font/theme result
         * does not become part of the next natural-width request. That keeps
         * the popup able to shrink again after a theme or scaling change.
         */
        this._calendarColumn.min_width = 0;
        if (!this.event_list.actor.visible) {
            return;
        }

        const [, agendaNatural] =
            this.event_list.actor.get_preferred_width(-1);
        const [, calendarNatural] =
            this._calendarColumn.get_preferred_width(-1);
        if (agendaNatural <= 0 || calendarNatural <= 0 ||
            agendaNatural <= calendarNatural) {
            return;
        }

        const target = Math.ceil(Math.min(
            agendaNatural,
            calendarNatural * 1.35
        ));
        this._calendarColumn.min_width = target;
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
        if (this._labelBin) {
            this._labelBin.resetLatch();
        }
    }

    _updateLabelWidth() {
        if (this._labelBin) {
            this._labelBin.updateLatch();
        }
    }

    _onLaunchSettings() {
        if (this.menu) {
            this.menu.close();
        }

        /*
         * Provider capability, not PATH, decides whether our richer authority
         * is installed. Launch through the package-owned desktop identity so an
         * unrelated executable named "system-settings" cannot impersonate the
         * authority. Mint remains the deterministic fallback.
         */
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

    /*
     * Cinnamon's Applet base class normally opens its generic metadata dialog
     * from the right-click context menu.  That dialog exposes the internal UUID
     * and only a small subset of Calendar metadata.  Keep every About
     * entry point on the same native dialog so the application presents one
     * consistent identity regardless of how the user opens it.
     */
    openAbout() {
        this._onAbout();
    }

    /*
     * Keep settings inside Cinnamon's own xlet-settings surface. Calendar
     * contributes only the JSON schema and applet behaviour; it no longer
     * ships a Python/GTK settings host of its own.
     */
    configureApplet(tab = 0) {
        super.configureApplet(tab);
    }

    _onAbout() {
        if (this.menu) {
            this.menu.close(false);
        }
        Util.spawnCommandLine("/usr/libexec/calendar-plus-about");
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

        if (this._calendar) {
            try {
                this._calendar.destroy();
            } catch (error) {
                global.logError(error);
            }
            this._calendar = null;
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

        this._calendar_system = null;
        this.desktop_settings = null;
        this.clock = null;
        this.menu = null;
        this.menuManager = null;
        this._popupBody = null;
        this._calendarColumn = null;
        this._resume_source = null;
        this.go_home_button = null;
        this._day = null;
        this._date = null;
        this._today_box = null;
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
