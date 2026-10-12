// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * System-timezone observation boundary.
 *
 * EventsManager needs only a semantic "timezone changed" notification. Gio
 * file-monitor lifetime, cancellation and /etc/localtime ownership stay here
 * so event/cache orchestration does not also become a filesystem controller.
 */

const Gio = imports.gi.Gio;
const Signals = imports.signals;

const APPLET_UUID = "calendar-plus@the-infiltratr";
const LOCALTIME_PATH = "/etc/localtime";
const ETC_PATH = "/etc";

var TimezoneMonitor = class TimezoneMonitor {
    constructor() {
        this._destroyed = false;
        this._cancellable = new Gio.Cancellable();
        this._monitor = null;
        this._signalId = 0;
        this._directoryFallback = false;
    }

    start() {
        if (this._destroyed || this._monitor !== null) {
            return;
        }

        const timezone = Gio.File.new_for_path(LOCALTIME_PATH);
        try {
            this._monitor = timezone.monitor_file(
                Gio.FileMonitorFlags.NONE,
                this._cancellable
            );
            this._connectMonitor();
            return;
        } catch (directError) {
            this._monitor = null;
            try {
                const etcDirectory = Gio.File.new_for_path(ETC_PATH);
                this._monitor = etcDirectory.monitor_directory(
                    Gio.FileMonitorFlags.NONE,
                    this._cancellable
                );
                this._directoryFallback = true;
                this._connectMonitor();
                return;
            } catch (fallbackError) {
                this._monitor = null;
                this._directoryFallback = false;
                global.logError(
                    `${APPLET_UUID}: could not monitor system timezone: ` +
                    `${directError}; fallback failed: ${fallbackError}`
                );
            }
        }
    }

    _connectMonitor() {
        this._signalId = this._monitor.connect(
            "changed",
            (monitor, file, otherFile) => {
                if (this._destroyed) {
                    return;
                }
                if (this._directoryFallback &&
                    !this._isLocaltimePath(file) &&
                    !this._isLocaltimePath(otherFile)) {
                    return;
                }
                this.emit("changed");
            }
        );
    }

    _isLocaltimePath(file) {
        return file !== null && file !== undefined &&
            typeof file.get_basename === "function" &&
            file.get_basename() === "localtime";
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        if (this._monitor !== null && this._signalId > 0) {
            try {
                this._monitor.disconnect(this._signalId);
            } catch (error) {
                global.logError(error);
            }
        }
        this._signalId = 0;
        if (this._monitor !== null) {
            try {
                this._monitor.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._monitor = null;
        this._directoryFallback = false;
        if (this._cancellable !== null) {
            try {
                this._cancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._cancellable = null;
    }
};
Signals.addSignalMethods(TimezoneMonitor.prototype);
