// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Cinnamon desktop/lifecycle integration owned outside the Calendar controller.
 *
 * The applet supplies semantic callbacks. This module owns Gio.Settings reads,
 * desktop/theme observation, suspend/resume compatibility, global keybindings
 * and launching desktop applications. Calendar presentation therefore consumes
 * resolved values rather than carrying Cinnamon service mechanics itself.
 */

const Gio = imports.gi.Gio;
const Main = imports.ui.main;
const Util = imports.misc.util;

const UUID = "calendar@the-infiltratr";

var DesktopIntegration = class DesktopIntegration {
    constructor(applet, SignalBag, callbacks = {}) {
        this._applet = applet;
        this._callbacks = callbacks;
        this._signals = new SignalBag();
        this._resumeSignals = new SignalBag();
        this._settings = new Gio.Settings({
            schema_id: "org.cinnamon.desktop.interface",
        });
        this._warnings = new Set();
        this._resumeSource = null;
        this._keybindingSet = false;
        this._panelWatchersStarted = false;
        this._destroyed = false;
    }

    start() {
        if (this._destroyed) {
            return;
        }
        this._signals.connect(
            this._settings,
            "changed::first-day-of-week",
            () => this._call("onWeekStartChanged")
        );
        this._signals.connect(
            this._settings,
            "changed::clock-use-24h",
            () => this._call("onClockPreferenceChanged", true)
        );
        this._signals.connect(
            this._settings,
            "changed::clock-show-date",
            () => this._call("onClockPreferenceChanged", false)
        );
        this._signals.connect(
            this._settings,
            "changed::gtk-theme",
            () => this._call("onThemeChanged")
        );
        this._startResumeMonitor();
    }

    startPanelWatchers() {
        if (this._destroyed || this._panelWatchersStarted) {
            return;
        }
        this._panelWatchersStarted = true;
        this._signals.connect(Main.themeManager, "theme-set", () => {
            this._call("onThemeChanged");
        });
        this._signals.connect(global.settings, "changed::panel-edit-mode", () => {
            this._call("onPanelEditModeChanged");
        });
    }

    _call(name, ...args) {
        const callback = this._callbacks[name];
        if (!this._destroyed && typeof callback === "function") {
            callback(...args);
        }
    }

    _reportPreferenceFailure(key, error) {
        if (this._warnings.has(key)) {
            return;
        }
        this._warnings.add(key);
        global.logError(
            `${UUID}: could not read Cinnamon desktop setting '${key}'; ` +
            `using the compatibility default: ${error}`
        );
    }

    _boolean(key, fallback = false) {
        try {
            return this._settings.get_boolean(key);
        } catch (error) {
            this._reportPreferenceFailure(key, error);
            return fallback;
        }
    }

    uses24h() {
        return this._boolean("clock-use-24h", false);
    }

    showsDate() {
        return this._boolean("clock-show-date", false);
    }

    _themeName() {
        try {
            const theme = this._settings.get_string("gtk-theme");
            return typeof theme === "string" ? theme.toLowerCase() : "";
        } catch (error) {
            this._reportPreferenceFailure("gtk-theme", error);
            return "";
        }
    }

    prefersDark() {
        return this._themeName().includes("dark");
    }

    usesHighContrast() {
        return this._themeName().replace(/[-_ ]/g, "").includes("highcontrast");
    }

    setKeybinding(accelerator, callback) {
        if (this._destroyed) {
            return;
        }
        this._removeKeybinding();
        if (!accelerator || typeof callback !== "function") {
            return;
        }

        if (Main.keybindingManager.addXletHotKey) {
            Main.keybindingManager.addXletHotKey(
                this._applet,
                "calendar-open",
                accelerator,
                callback
            );
        } else {
            Main.keybindingManager.addHotKey(
                `${UUID}-open-${this._applet.instance_id}`,
                accelerator,
                callback
            );
        }
        this._keybindingSet = true;
    }

    _removeKeybinding() {
        if (!this._keybindingSet) {
            return;
        }
        try {
            if (Main.keybindingManager.removeXletHotKey) {
                Main.keybindingManager.removeXletHotKey(
                    this._applet,
                    "calendar-open"
                );
            } else {
                Main.keybindingManager.removeHotKey(
                    `${UUID}-open-${this._applet.instance_id}`
                );
            }
        } catch (error) {
            global.logError(error);
        }
        this._keybindingSet = false;
    }

    _startResumeMonitor() {
        try {
            const LoginManager = imports.misc.loginManager;
            if (LoginManager && typeof LoginManager.getLoginManager === "function") {
                this._resumeSource = LoginManager.getLoginManager();
                this._resumeSignals.connect(
                    this._resumeSource,
                    "prepare-for-sleep",
                    (suspending) => {
                        if (!suspending) {
                            this._call("onResume");
                        }
                    }
                );
                return;
            }
        } catch (error) {
            /* Older Cinnamon releases use the UPower compatibility signal. */
        }

        const UPowerGlib = imports.gi.UPowerGlib;
        this._resumeSource = new UPowerGlib.Client();
        try {
            this._resumeSignals.connect(
                this._resumeSource,
                "notify-resume",
                () => this._call("onResume")
            );
        } catch (error) {
            this._resumeSignals.connect(
                this._resumeSource,
                "notify::resume",
                () => this._call("onResume")
            );
        }
    }

    launchSettings(providerAvailable) {
        if (providerAvailable) {
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

    launchAbout() {
        try {
            const appInfo = Gio.DesktopAppInfo.new(
                "calendar-about.desktop"
            );
            if (!appInfo) {
                throw new Error("Calendar About desktop identity is unavailable");
            }
            appInfo.launch([], global.create_app_launch_context());
        } catch (error) {
            global.logError(error);
        }
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._removeKeybinding();
        this._resumeSignals.disconnectAll();
        this._signals.disconnectAll();
        this._settings = null;
        this._resumeSource = null;
        this._warnings.clear();
        this._callbacks = {};
        this._applet = null;
    }
};
