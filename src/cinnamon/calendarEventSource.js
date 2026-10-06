// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Narrow month-view event source.
 *
 * Calendar needs event availability, visible-range updates, day selection,
 * colours and three state notifications. It does not need the concrete
 * transport controller. This adapter owns the signal translation as well as
 * the method forwarding, so Calendar never connects to EventsManager itself.
 */

const Signals = imports.signals;

var CalendarEventSource = class CalendarEventSource {
    constructor(manager) {
        if (!manager) {
            throw new Error("Calendar event manager is required");
        }
        this._manager = manager;
        this._managerConnections = [];

        this._connectManagerSignal("events-updated", "events-updated");
        this._connectManagerSignal(
            "events-manager-ready",
            "events-manager-ready"
        );
        this._connectManagerSignal(
            "has-calendars-changed",
            "has-calendars-changed"
        );
    }

    _connectManagerSignal(sourceSignal, publicSignal) {
        const id = this._manager.connect(sourceSignal, () => {
            if (this._manager) {
                this.emit(publicSignal);
            }
        });
        this._managerConnections.push(id);
    }

    is_active() {
        return this._manager ? this._manager.is_active() : false;
    }

    set_visible_range(firstDate, lastDate, force) {
        if (this._manager) {
            this._manager.set_visible_range(firstDate, lastDate, force);
        }
    }

    get_colors_for_range(firstDate, dayCount, maxColors) {
        return this._manager
            ? this._manager.get_colors_for_range(firstDate, dayCount, maxColors)
            : [];
    }

    select_date(date, force) {
        if (this._manager) {
            this._manager.select_date(date, force);
        }
    }

    destroy() {
        if (this._manager) {
            for (const id of this._managerConnections) {
                if (id > 0) {
                    try {
                        this._manager.disconnect(id);
                    } catch (error) {
                        global.logError(error);
                    }
                }
            }
        }
        this._managerConnections = [];
        this._manager = null;
    }
};
Signals.addSignalMethods(CalendarEventSource.prototype);
