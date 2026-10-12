// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * CalendarServer transport boundary.
 *
 * This object owns only D-Bus service discovery, proxy lifetime, reconnect
 * policy and SetTimeRange transport.  It deliberately has no event-store,
 * selected-date, timezone or presentation state.  Consumers react to its
 * narrow signals and decide what transport availability means for their own
 * domain state.
 */

const Cinnamon = imports.gi.Cinnamon;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Signals = imports.signals;
const Mainloop = imports.mainloop;

const APPLET_UUID = "calendar-plus@the-infiltratr";
const CALENDAR_SERVER_BUS_NAME = "org.cinnamon.CalendarServer";
var STATUS_UNKNOWN = 0;
var STATUS_NO_CALENDARS = 1;

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

var CalendarServerTransport = class CalendarServerTransport {
    constructor() {
        this._destroyed = false;
        this._busWatchId = 0;
        this._server = null;
        this._connecting = false;
        this._generation = 0;
        this._reconnectTimerId = 0;
        this._serverSignals = new SignalBag();
        this._cancellable = new Gio.Cancellable();
        this._status = STATUS_UNKNOWN;
    }

    get status() {
        return this._status;
    }

    is_ready() {
        return !this._destroyed && this._server !== null;
    }

    start() {
        if (this._destroyed) {
            return;
        }
        if (this._busWatchId === 0) {
            this._busWatchId = Gio.bus_watch_name(
                Gio.BusType.SESSION,
                CALENDAR_SERVER_BUS_NAME,
                Gio.BusNameWatcherFlags.NONE,
                () => this._connect(),
                () => this._serverVanished()
            );
        }
        /* Direct probing preserves D-Bus activation and user-driven re-probes. */
        this._connect();
    }

    _connect() {
        if (this._destroyed || this._server !== null || this._connecting) {
            return;
        }
        this._cancelReconnect();
        this._connecting = true;
        const generation = this._generation;
        Cinnamon.CalendarServerProxy.new_for_bus(
            Gio.BusType.SESSION,
            Gio.DBusProxyFlags.NONE,
            CALENDAR_SERVER_BUS_NAME,
            "/org/cinnamon/CalendarServer",
            this._cancellable,
            (object, result) => this._serverReady(result, generation)
        );
    }

    _serverReady(result, generation) {
        try {
            const server = Cinnamon.CalendarServerProxy.new_for_bus_finish(result);
            if (this._destroyed || generation !== this._generation) {
                return;
            }
            this._connecting = false;
            this._serverSignals.connect(
                server,
                "events-added-or-updated",
                (proxy, payload) => this.emit("events-added-or-updated", payload)
            );
            this._serverSignals.connect(
                server,
                "events-removed",
                (proxy, ids) => this.emit("events-removed", ids)
            );
            this._serverSignals.connect(
                server,
                "client-disappeared",
                () => this.emit("calendar-set-changed")
            );
            this._serverSignals.connect(
                server,
                "notify::status",
                () => this._statusChanged()
            );
            this._server = server;
            this._status = server.status;
            this.emit("ready");
        } catch (error) {
            if (this._destroyed || generation !== this._generation) {
                return;
            }
            this._connecting = false;
            this._serverSignals.disconnectAll();
            this._server = null;
            this._status = STATUS_UNKNOWN;
            this.emit("unavailable", false);
            global.logError(
                `${APPLET_UUID}: could not connect to calendar server: ${error}`
            );
            this._scheduleReconnect();
        }
    }

    _serverVanished() {
        if (this._destroyed) {
            return;
        }
        const expectedEmpty = this._status === STATUS_NO_CALENDARS;
        this._generation += 1;
        this._connecting = false;
        this._cancelReconnect();
        this._serverSignals.disconnectAll();
        this._server = null;
        if (!expectedEmpty) {
            this._status = STATUS_UNKNOWN;
        }
        this.emit("unavailable", expectedEmpty);
        if (!expectedEmpty) {
            this._scheduleReconnect();
        }
    }

    _statusChanged() {
        if (this._destroyed || this._server === null) {
            return;
        }
        const next = this._server.status;
        if (next === this._status || next === STATUS_UNKNOWN) {
            return;
        }
        this._status = next;
        this.emit("status-changed", next);
    }

    request_range(startUnix, endUnix, force, callback) {
        if (!this.is_ready()) {
            return false;
        }
        const server = this._server;
        const generation = this._generation;
        server.call_set_time_range(
            startUnix,
            endUnix,
            Boolean(force),
            this._cancellable,
            (proxy, result) => {
                let succeeded = false;
                try {
                    server.call_set_time_range_finish(result);
                    succeeded = true;
                } catch (error) {
                    if (!this._destroyed && generation === this._generation &&
                        server === this._server) {
                        global.logError(
                            `${APPLET_UUID}: event range request failed: ${error}`
                        );
                    }
                }
                if (typeof callback === "function") {
                    callback(
                        succeeded,
                        !this._destroyed && generation === this._generation &&
                            server === this._server
                    );
                }
            }
        );
        return true;
    }

    _scheduleReconnect() {
        if (this._destroyed || this._reconnectTimerId > 0) {
            return;
        }
        this._reconnectTimerId = Mainloop.timeout_add_seconds(5, () => {
            this._reconnectTimerId = 0;
            this._connect();
            return GLib.SOURCE_REMOVE;
        });
    }

    _cancelReconnect() {
        if (this._reconnectTimerId > 0) {
            Mainloop.source_remove(this._reconnectTimerId);
            this._reconnectTimerId = 0;
        }
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._generation += 1;
        this._connecting = false;
        this._cancelReconnect();
        if (this._busWatchId > 0) {
            Gio.bus_unwatch_name(this._busWatchId);
            this._busWatchId = 0;
        }
        this._serverSignals.disconnectAll();
        this._server = null;
        if (this._cancellable !== null) {
            try {
                this._cancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._cancellable = null;
        this._status = STATUS_UNKNOWN;
    }
};
Signals.addSignalMethods(CalendarServerTransport.prototype);
