// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native event-store and visible-range orchestration controller.
 *
 * CalendarServer discovery/proxy lifetime belong to calendarServerTransport.js,
 * while filesystem timezone observation belongs to timezoneMonitor.js. This
 * controller owns event-cache semantics, desired range/selection state and the
 * signals exposed to presentation. External tuples cross the boundary once and
 * are immediately normalized by CalendarPlus.EventStore.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const GLib = imports.gi.GLib;
const Signals = imports.signals;
const Mainloop = imports.mainloop;

const APPLET_UUID = "calendar-plus@the-infiltratr";

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
const midnight = RuntimeSupport.midnight;
const sameInstant = RuntimeSupport.sameInstant;
const EventRangeState = RuntimeSupport.loadLocalModule(
    "eventRangeState"
).EventRangeState;
const NativeBoundary = RuntimeSupport.loadLocalModule(
    "nativeBoundary"
).NativeBoundary;
const CalendarServerTransportModule = RuntimeSupport.loadLocalModule(
    "calendarServerTransport"
);
const CalendarServerTransport = CalendarServerTransportModule.CalendarServerTransport;
const STATUS_UNKNOWN = CalendarServerTransportModule.STATUS_UNKNOWN;
const STATUS_NO_CALENDARS = CalendarServerTransportModule.STATUS_NO_CALENDARS;
const TimezoneMonitor = RuntimeSupport.loadLocalModule(
    "timezoneMonitor"
).TimezoneMonitor;

function _civilFields(date) {
    if (date === null) {
        return null;
    }
    if (typeof date.getFullYear === "function" &&
        typeof date.getMonth === "function" &&
        typeof date.getDate === "function") {
        return [date.getFullYear(), date.getMonth() + 1, date.getDate()];
    }
    if (Number.isInteger(date.year) &&
        Number.isInteger(date.month) &&
        Number.isInteger(date.day)) {
        return [date.year, date.month, date.day];
    }
    return null;
}

function _jsDateToLocalDateTime(date) {
    const fields = _civilFields(date);
    if (fields === null) {
        return null;
    }

    const [year, month, day] = fields;
    const value = GLib.DateTime.new_local(year, month, day, 12, 0, 0);
    if (value === null ||
        value.get_year() !== year ||
        value.get_month() !== month ||
        value.get_day_of_month() !== day) {
        return null;
    }
    return value;
}

class EventRecord {
    constructor(row) {
        this.id = row.id;
        this.color = row.color;
        this.summary = row.summary;
        this.all_day = row.allDay;
        this.multi_day = row.multiDay;
        this.start_unix = row.startUnix;
        this.end_unix = row.endUnix;
        this.start_day_unix = row.startDayUnix;
        this.end_day_unix = row.endDayUnix;
        this.modified = row.modified;

        this.start = GLib.DateTime.new_from_unix_local(row.startUnix);
        this.end = GLib.DateTime.new_from_unix_local(row.endUnix);
        this.start_date = GLib.DateTime.new_from_unix_local(row.startDayUnix);
        this.end_date = GLib.DateTime.new_from_unix_local(row.endDayUnix);
        if (this.start === null || this.end === null ||
            this.start_date === null || this.end_date === null) {
            throw new Error("Calendar event is outside the local DateTime domain");
        }
    }

    relation_to_day(day) {
        return CalendarPlus.event_day_relation(
            this.start_day_unix,
            this.end_day_unix,
            day.to_unix()
        );
    }

    timing(now) {
        return CalendarPlus.event_timing(
            this.start_unix,
            this.end_unix,
            now.to_unix()
        ).deep_unpack();
    }
}

class EventSnapshot {
    constructor(variant) {
        const decoded = NativeBoundary.decodeEventSnapshot(variant);
        this.revision = decoded.revision;
        this.events = decoded.events.map((row) => new EventRecord(row));
    }

    get_event_list() {
        return this.events;
    }
}

var EventsManager = class EventsManager {
    constructor(transport = null, timezoneMonitor = null) {
        this._enabled = true;
        this._destroyed = false;
        this._transport = transport || new CalendarServerTransport();
        this._timezoneMonitor = timezoneMonitor || new TimezoneMonitor();
        this._transportSignals = new SignalBag();
        this._timezoneSignals = new SignalBag();
        this._range_retry_timer_id = 0;
        this._reload_id = 0;
        this._force_reload_pending = false;
        this.current_range_start = null;
        this.current_range_end = null;
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;
        this.current_selected_date = null;
        this.current_selected_civil = null;
        this._rangeState = new EventRangeState();
        this.last_update_timestamp = 0;
        this.event_store = CalendarPlus.EventStore.new();

        this._wireTransport();
        this._wireTimezone();
    }

    set_enabled(enabled) {
        if (this._destroyed) {
            return;
        }
        const next = Boolean(enabled);
        if (next === this._enabled) {
            return;
        }

        this._enabled = next;
        this._cancelRangeRetry();
        if (this._reload_id > 0) {
            Mainloop.source_remove(this._reload_id);
            this._reload_id = 0;
        }
        this._force_reload_pending = false;
        this._rangeState.resetForServerLoss();
        this.last_update_timestamp = 0;

        if (!next) {
            if (this.event_store !== null) {
                this.event_store.clear();
            }
            this._publishAgendaEvents(null, false);
        } else if (this._transport !== null) {
            this._transport.start();
            if (this._transport.is_ready() &&
                this.current_range_start_civil !== null &&
                this.current_range_end_civil !== null) {
                this.queue_reload(true);
            }
        }

        this.emit("has-calendars-changed");
        this.emit("events-updated");
    }

    _publishAgendaDate(date) {
        if (!this._destroyed && date !== null) {
            this.emit("agenda-date-changed", date);
        }
    }

    _publishAgendaEvents(snapshot, reset, unavailable = false) {
        if (!this._destroyed) {
            this.emit(
                "agenda-events-changed",
                snapshot,
                Boolean(reset),
                Boolean(unavailable)
            );
        }
    }

    _wireTransport() {
        this._transportSignals.connect(
            this._transport,
            "ready",
            () => this._transportReady()
        );
        this._transportSignals.connect(
            this._transport,
            "unavailable",
            (transport, expectedEmpty) => this._transportUnavailable(expectedEmpty)
        );
        this._transportSignals.connect(
            this._transport,
            "events-added-or-updated",
            (transport, payload) => this._ingestEvents(payload)
        );
        this._transportSignals.connect(
            this._transport,
            "events-removed",
            (transport, ids) => this._removeEvents(ids)
        );
        this._transportSignals.connect(
            this._transport,
            "calendar-set-changed",
            () => this._calendarSetChanged()
        );
        this._transportSignals.connect(
            this._transport,
            "status-changed",
            (transport, status) => this._statusChanged(status)
        );
    }

    _wireTimezone() {
        this._timezoneSignals.connect(
            this._timezoneMonitor,
            "changed",
            () => this._timezoneChanged()
        );
        this._timezoneMonitor.start();
    }

    start_events() {
        if (!this._destroyed && this._enabled && this._transport !== null) {
            this._transport.start();
        }
    }

    _transportReady() {
        if (this._destroyed) {
            return;
        }
        if (this._enabled &&
            this.current_range_start_civil !== null &&
            this.current_range_end_civil !== null) {
            this.queue_reload(true);
        }
        this.emit("events-manager-ready");
    }

    _transportUnavailable(expectedEmpty) {
        if (this._destroyed) {
            return;
        }
        this._cancelRangeRetry();
        if (this.event_store !== null) {
            this.event_store.clear();
        }
        this._publishAgendaEvents(null, false, !Boolean(expectedEmpty));
        this.current_range_start = null;
        this.current_range_end = null;
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;
        this._rangeState.resetForServerLoss();
        this.last_update_timestamp = 0;
        this.emit("has-calendars-changed");
        this.emit("events-updated");
    }

    _cancelRangeRetry() {
        if (this._range_retry_timer_id > 0) {
            Mainloop.source_remove(this._range_retry_timer_id);
            this._range_retry_timer_id = 0;
        }
    }

    _scheduleRangeRetry() {
        if (this._destroyed || !this._enabled ||
            this._range_retry_timer_id > 0 ||
            this._rangeState.pending ||
            this.current_range_start === null ||
            this.current_range_end === null) {
            return;
        }

        const delay = this._rangeState.nextRetryDelay();
        this._range_retry_timer_id = Mainloop.timeout_add_seconds(
            delay,
            () => {
                this._range_retry_timer_id = 0;
                if (!this._destroyed && this._enabled &&
                    !this._rangeState.pending &&
                    this._transport !== null && this._transport.is_ready() &&
                    this.current_range_start !== null &&
                    this.current_range_end !== null) {
                    this._requestVisibleRange(
                        this.current_range_start,
                        this.current_range_end,
                        true,
                        true
                    );
                }
                return GLib.SOURCE_REMOVE;
            }
        );
    }

    _timezoneChanged() {
        if (this._destroyed || !this._enabled || this.event_store === null) {
            return;
        }

        this._rangeState.closeAdmission();
        this.event_store.refresh_timezone();
        this._invalidateCurrentRange(false);
        this.emit("events-updated");
    }

    _invalidateCurrentRange(clearStore) {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        const requestPending = this._rangeState.invalidate();
        if (clearStore) {
            this.event_store.clear();
        }

        if (!this._enabled) {
            return;
        }
        if (requestPending) {
            this._rangeState.queueForce(true);
        } else if (this.current_range_start !== null &&
                   this.current_range_end !== null) {
            this.queue_reload(true);
        }
    }

    _ingestEvents(payload) {
        if (this._destroyed || !this._enabled || this.event_store === null ||
            !this._rangeState.acceptingEvents ||
            this.current_range_start === null ||
            this.current_range_end === null) {
            return;
        }

        const exclusiveEnd = this.current_range_end.add_days(1);
        if (exclusiveEnd === null) {
            return;
        }
        const rangeStart = this.current_range_start.to_unix();
        const rangeEnd = exclusiveEnd.to_unix() - 1;

        let changed = false;
        for (const eventVariant of payload.unpack()) {
            changed = this.event_store.add_or_update_in_range(
                eventVariant,
                this.last_update_timestamp,
                rangeStart,
                rangeEnd
            ) || changed;
        }

        if (changed) {
            this.emit("events-updated");
        }
    }

    _removeEvents(serializedIds) {
        if (this._destroyed || !this._enabled || this.event_store === null ||
            !this._rangeState.acceptingEvents ||
            typeof serializedIds !== "string" || serializedIds.length === 0) {
            return;
        }
        if (this.event_store.remove(serializedIds)) {
            this.emit("events-updated");
        }
    }

    _calendarSetChanged() {
        if (this._destroyed || this.event_store === null) {
            return;
        }
        this._invalidateCurrentRange(true);
        this.emit("events-updated");
    }

    _statusChanged(next) {
        if (this._destroyed || next === STATUS_UNKNOWN) {
            return;
        }
        if (this._enabled) {
            this.queue_reload(true);
        }
        this.emit("has-calendars-changed");
    }

    _requestVisibleRange(first, last, force, clearStore) {
        if (this._destroyed || !this._enabled || this._transport === null ||
            !this._transport.is_ready() || this.event_store === null ||
            this._rangeState.pending) {
            return;
        }

        if (clearStore) {
            this.event_store.clear();
            this.emit("events-updated");
        }
        this._rangeState.closeAdmission();

        const exclusiveEnd = last.add_days(1);
        if (exclusiveEnd === null) {
            global.logError(
                `${APPLET_UUID}: visible event range exceeds DateTime limits.`
            );
            return;
        }

        const requestGeneration = this._rangeState.beginRequest();
        if (requestGeneration === 0) {
            return;
        }
        const requestedStart = first;
        const requestedEnd = last;
        this.last_update_timestamp = GLib.get_monotonic_time();

        const started = this._transport.request_range(
            first.to_unix(),
            exclusiveEnd.to_unix() - 1,
            Boolean(force),
            (succeeded, currentTransport) => {
                if (this._destroyed || !this._enabled || !currentTransport ||
                    !this._rangeState.isCurrent(requestGeneration)) {
                    return;
                }
                if (!this._rangeState.finishRequest(
                        requestGeneration, succeeded)) {
                    return;
                }

                const desiredChanged =
                    !sameInstant(requestedStart, this.current_range_start) ||
                    !sameInstant(requestedEnd, this.current_range_end);
                if (desiredChanged || this._rangeState.queuedForce) {
                    const queuedForce = this._rangeState.consumeQueuedForce();
                    const nextStart = this.current_range_start;
                    const nextEnd = this.current_range_end;
                    this._cancelRangeRetry();
                    this._rangeState.resetRetry();
                    if (nextStart !== null && nextEnd !== null) {
                        this._requestVisibleRange(
                            nextStart,
                            nextEnd,
                            queuedForce || desiredChanged,
                            true
                        );
                    }
                    return;
                }

                if (succeeded) {
                    this._rangeState.acceptCompletedRequest();
                    this._cancelRangeRetry();
                    this._rangeState.resetRetry();
                } else {
                    this._rangeState.closeAdmission();
                    this._scheduleRangeRetry();
                }
            }
        );
        if (!started && this._rangeState.finishRequest(requestGeneration, false)) {
            this._scheduleRangeRetry();
        }
    }

    set_visible_range(firstDate, lastDate, force) {
        if (this._destroyed || this._transport === null ||
            this.event_store === null) {
            return;
        }

        const firstFields = _civilFields(firstDate);
        const lastFields = _civilFields(lastDate);
        if (firstFields === null || lastFields === null) {
            global.logError(`${APPLET_UUID}: invalid visible civil event range.`);
            return;
        }

        const first = midnight(_jsDateToLocalDateTime(firstDate));
        const last = midnight(_jsDateToLocalDateTime(lastDate));
        if (first === null || last === null) {
            global.logError(
                `${APPLET_UUID}: visible range has no representable local day boundary.`
            );
            return;
        }
        if (last.to_unix() < first.to_unix()) {
            global.logError(`${APPLET_UUID}: invalid visible event range.`);
            return;
        }

        const changed = !sameInstant(first, this.current_range_start) ||
            !sameInstant(last, this.current_range_end);
        const needsRetry = !this._rangeState.pending &&
            !this._rangeState.succeeded;
        if (!changed && !force && !needsRetry) {
            return;
        }

        this.current_range_start = first;
        this.current_range_end = last;
        this.current_range_start_civil = {
            year: firstFields[0],
            month: firstFields[1],
            day: firstFields[2],
        };
        this.current_range_end_civil = {
            year: lastFields[0],
            month: lastFields[1],
            day: lastFields[2],
        };
        if (changed) {
            this._cancelRangeRetry();
            this._rangeState.resetRetry();
        }

        if (!this._enabled || !this._transport.is_ready()) {
            this._rangeState.invalidate();
            return;
        }
        if (this._rangeState.pending) {
            this._rangeState.queueForce(Boolean(force) || changed);
            return;
        }

        this._rangeState.clearQueuedForce();
        this._requestVisibleRange(first, last, force, true);
    }

    queue_reload(force) {
        if (this._reload_id > 0) {
            Mainloop.source_remove(this._reload_id);
            this._reload_id = 0;
        }
        if (this._destroyed || !this._enabled) {
            return;
        }
        this._force_reload_pending = this._force_reload_pending || Boolean(force);
        this._reload_id = Mainloop.idle_add(() => {
            this._reload_id = 0;
            const forceReload = this._force_reload_pending;
            this._force_reload_pending = false;

            if (!this._destroyed && this._enabled) {
                if (forceReload &&
                    this.current_range_start_civil !== null &&
                    this.current_range_end_civil !== null) {
                    this.set_visible_range(
                        this.current_range_start_civil,
                        this.current_range_end_civil,
                        true
                    );
                }

                const selectedDate =
                    this.current_selected_civil !== null
                        ? this.current_selected_civil
                        : new Date();
                this.select_date(selectedDate, forceReload);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    select_date(date, force) {
        if (this._destroyed) {
            return;
        }

        const fields = _civilFields(date);
        if (fields === null) {
            global.logError(`${APPLET_UUID}: invalid selected civil date.`);
            return;
        }
        const [year, month, dayOfMonth] = fields;
        const previousCivil = this.current_selected_civil;
        const changedMonth = previousCivil !== null &&
            (previousCivil.year !== year || previousCivil.month !== month);
        const day = midnight(_jsDateToLocalDateTime(date));

        this.current_selected_civil = {
            year,
            month,
            day: dayOfMonth,
        };

        if (day === null) {
            this.current_selected_date = null;
            const display = GLib.DateTime.new_utc(
                year, month, dayOfMonth, 12, 0, 0
            );
            if (display !== null) {
                this._publishAgendaDate(display);
            }
            this._publishAgendaEvents(null, false);
            return;
        }

        this.current_selected_date = day;
        this._publishAgendaDate(day);

        if (!this.is_active()) {
            return;
        }

        let snapshot;
        try {
            snapshot = this._snapshot_for_date(day);
        } catch (error) {
            global.logError(
                `${APPLET_UUID}: native event snapshot contract violation: ${error}`
            );
            this._publishAgendaEvents(null, false, true);
            return;
        }
        this._publishAgendaEvents(
            snapshot,
            previousCivil === null || changedMonth || Boolean(force)
        );
    }

    _snapshot_for_date(day) {
        if (this._destroyed || this.event_store === null || day === null) {
            return null;
        }
        const snapshotVariant = this.event_store.get_snapshot(
            day.to_unix(),
            GLib.DateTime.new_now_local().to_unix()
        );
        if (snapshotVariant === null) {
            throw new Error("native event store returned no snapshot");
        }
        const snapshot = new EventSnapshot(snapshotVariant);
        return snapshot.events.length === 0 ? null : snapshot;
    }

    get_colors_for_range(js_date, dayCount, maximumColorsPerDay) {
        if (this._destroyed || !this._enabled || this.event_store === null ||
            !Number.isInteger(dayCount) || dayCount <= 0 ||
            !Number.isInteger(maximumColorsPerDay) ||
            maximumColorsPerDay < 0) {
            return [];
        }

        const day = midnight(_jsDateToLocalDateTime(js_date));
        if (day === null) {
            return [];
        }

        const variant = this.event_store.get_color_range(
            day.to_unix(),
            dayCount,
            GLib.DateTime.new_now_local().to_unix(),
            maximumColorsPerDay
        );
        return variant === null ? [] : variant.deep_unpack();
    }

    should_show_event_pane() {
        const status = this._transport !== null
            ? this._transport.status
            : STATUS_UNKNOWN;
        return !this._destroyed &&
            this._enabled &&
            status !== STATUS_NO_CALENDARS;
    }

    is_active() {
        return !this._destroyed &&
            this._enabled &&
            this._transport !== null &&
            this._transport.is_ready() &&
            this._transport.status !== STATUS_NO_CALENDARS;
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._enabled = false;
        this._rangeState.destroy();
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;

        this._cancelRangeRetry();
        if (this._reload_id > 0) {
            Mainloop.source_remove(this._reload_id);
            this._reload_id = 0;
        }

        this._timezoneSignals.disconnectAll();
        if (this._timezoneMonitor !== null) {
            this._timezoneMonitor.destroy();
            this._timezoneMonitor = null;
        }

        this._transportSignals.disconnectAll();
        if (this._transport !== null) {
            this._transport.destroy();
            this._transport = null;
        }

        if (this.event_store !== null) {
            this.event_store.clear();
            this.event_store = null;
        }

        this.current_selected_date = null;
        this.current_selected_civil = null;
        this.current_range_start = null;
        this.current_range_end = null;
    }
};
Signals.addSignalMethods(EventsManager.prototype);
