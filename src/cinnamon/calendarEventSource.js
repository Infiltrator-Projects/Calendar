// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Narrow month-view event source.
 *
 * Calendar needs event availability, visible-range updates, day selection,
 * colours and three state signals. It does not need the concrete transport
 * controller. This adapter keeps Calendar independent from EventsManager's
 * reconnect, D-Bus and cache implementation details.
 */

var CalendarEventSource = class CalendarEventSource {
    constructor(manager) {
        if (!manager) {
            throw new Error("Calendar event manager is required");
        }
        this._manager = manager;
    }

    connect(signal, callback) {
        return this._manager ? this._manager.connect(signal, callback) : 0;
    }

    disconnect(id) {
        if (this._manager && id > 0) {
            this._manager.disconnect(id);
        }
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
        this._manager = null;
    }
};
