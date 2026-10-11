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

var TimezoneMonitor = class TimezoneMonitor {
    constructor() {
        this._destroyed = false;
        this._cancellable = new Gio.Cancellable();
        this._monitor = null;
        this._signalId = 0;
    }

    start() {
        if (this._destroyed || this._monitor !== null) {
            return;
        }
        try {
            const timezone = Gio.File.new_for_path("/etc/localtime");
            this._monitor = timezone.monitor_file(
                Gio.FileMonitorFlags.NONE,
                this._cancellable
            );
            this._signalId = this._monitor.connect(
                "changed",
                () => {
                    if (!this._destroyed) {
                        this.emit("changed");
                    }
                }
            );
        } catch (error) {
            global.logError(
                `${APPLET_UUID}: could not monitor system timezone: ${error}`
            );
        }
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
