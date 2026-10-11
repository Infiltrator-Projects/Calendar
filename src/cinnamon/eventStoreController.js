// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native event-store lifetime and timezone-derived cache ownership.
 *
 * EventsManager owns request/selection orchestration; this object owns the
 * mutable native store and the platform timezone monitor that can invalidate
 * its derived civil-day membership. No CalendarServer transport or presentation
 * state crosses this boundary.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const Gio = imports.gi.Gio;
const Signals = imports.signals;

const APPLET_UUID = "calendar-plus@the-infiltratr";

var EventStoreController = class EventStoreController {
    constructor(store = null) {
        this._destroyed = false;
        this._store = store || CalendarPlus.EventStore.new();
        this._timezoneCancellable = new Gio.Cancellable();
        this._timezoneMonitor = null;
        this._timezoneMonitorSignalId = 0;
        this._startTimezoneMonitor();
    }

    _startTimezoneMonitor() {
        try {
            const timezone = Gio.File.new_for_path("/etc/localtime");
            this._timezoneMonitor = timezone.monitor_file(
                Gio.FileMonitorFlags.NONE,
                this._timezoneCancellable
            );
            this._timezoneMonitorSignalId = this._timezoneMonitor.connect(
                "changed",
                () => this._timezoneChanged()
            );
        } catch (error) {
            global.logError(
                `${APPLET_UUID}: could not monitor system timezone: ${error}`
            );
        }
    }

    _timezoneChanged() {
        if (this._destroyed || this._store === null) {
            return;
        }
        this._store.refresh_timezone();
        this.emit("timezone-changed");
    }

    clear() {
        if (!this._destroyed && this._store !== null) {
            this._store.clear();
        }
    }

    ingest(payload, updateTimestamp, rangeStart, rangeEnd) {
        if (this._destroyed || this._store === null || payload === null ||
            typeof payload.unpack !== "function") {
            return false;
        }

        let changed = false;
        try {
            for (const eventVariant of payload.unpack()) {
                changed = this._store.add_or_update_in_range(
                    eventVariant,
                    updateTimestamp,
                    rangeStart,
                    rangeEnd
                ) || changed;
            }
        } catch (error) {
            global.logError(`${APPLET_UUID}: invalid CalendarServer event payload: ${error}`);
            return false;
        }
        return changed;
    }

    snapshot_variant(dayUnix, nowUnix) {
        if (this._destroyed || this._store === null) {
            return null;
        }
        return this._store.get_snapshot(dayUnix, nowUnix);
    }

    color_range_variant(dayUnix, dayCount, nowUnix, maximumColorsPerDay) {
        if (this._destroyed || this._store === null) {
            return null;
        }
        return this._store.get_color_range(
            dayUnix,
            dayCount,
            nowUnix,
            maximumColorsPerDay
        );
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;

        if (this._timezoneMonitor !== null) {
            if (this._timezoneMonitorSignalId > 0) {
                try {
                    this._timezoneMonitor.disconnect(this._timezoneMonitorSignalId);
                } catch (error) {
                    global.logError(error);
                }
            }
            try {
                this._timezoneMonitor.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._timezoneMonitorSignalId = 0;
        this._timezoneMonitor = null;

        if (this._timezoneCancellable !== null) {
            try {
                this._timezoneCancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
            this._timezoneCancellable = null;
        }

        if (this._store !== null) {
            this._store.clear();
            this._store = null;
        }
    }
};
Signals.addSignalMethods(EventStoreController.prototype);
