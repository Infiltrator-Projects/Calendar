// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * CalendarServer transport and native event-store controller.
 *
 * This module owns no Cinnamon actors. CalendarServer tuples cross D-Bus once
 * and are immediately normalised by CalendarPlus.EventStore; an optional
 * agenda view receives detached snapshot records. Separating transport from
 * presentation makes event fetching testable without building the popup UI.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const Cinnamon = imports.gi.Cinnamon;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Signals = imports.signals;
const Mainloop = imports.mainloop;

const APPLET_UUID = "calendar-plus@the-infiltratr";
const STATUS_UNKNOWN = 0;
const STATUS_NO_CALENDARS = 1;
const CALENDAR_SERVER_BUS_NAME = "org.cinnamon.CalendarServer";

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

function _eventVariantOverlapsRange(eventVariant, rangeStart, rangeEnd) {
    try {
        if (eventVariant === null ||
            typeof eventVariant.deep_unpack !== "function") {
            return true;
        }
        const row = eventVariant.deep_unpack();
        if (!Array.isArray(row) || row.length < 7) {
            return true;
        }

        const allDay = Boolean(row[3]);
        const start = Number(row[4]);
        const end = Number(row[5]);
        if (!Number.isFinite(start) || !Number.isFinite(end)) {
            return true;
        }

        return allDay
            ? start <= rangeEnd && end > rangeStart
            : start <= rangeEnd && end >= rangeStart;
    } catch (error) {
        /*
         * Structural/type validation remains native. If transport inspection
         * cannot prove that a row lies outside the desired range, pass it to C
         * rather than creating a second tuple validator in JavaScript.
         */
        return true;
    }
}

class EventRecord {
    constructor(row) {
        const [
            id,
            color,
            summary,
            allDay,
            multiDay,
            startUnix,
            endUnix,
            startDayUnix,
            endDayUnix,
            modified,
        ] = row;

        this.id = id;
        this.color = color;
        this.summary = summary;
        this.all_day = allDay;
        this.multi_day = multiDay;
        this.start_unix = startUnix;
        this.end_unix = endUnix;
        this.start_day_unix = startDayUnix;
        this.end_day_unix = endDayUnix;
        this.modified = modified;

        this.start = GLib.DateTime.new_from_unix_local(startUnix);
        this.end = GLib.DateTime.new_from_unix_local(endUnix);
        this.start_date = GLib.DateTime.new_from_unix_local(startDayUnix);
        this.end_date = GLib.DateTime.new_from_unix_local(endDayUnix);
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
        const [revision, rows] = variant.deep_unpack();
        this.timestamp = revision;
        this.events = [];
        for (const row of rows) {
            try {
                this.events.push(new EventRecord(row));
            } catch (error) {
                global.logError(error);
            }
        }
    }

    get_event_list() {
        return this.events;
    }
}

var EventsManager = class EventsManager {
    constructor(settings, desktop_settings, event_list) {
        this.settings = settings;
        this.desktop_settings = desktop_settings;
        this._destroyed = false;
        this._inited = false;
        this._bus_watch_id = 0;
        this._calendar_server = null;
        this._calendar_server_connecting = false;
        this._calendar_server_generation = 0;
        this._reconnect_timer_id = 0;
        this._range_retry_timer_id = 0;
        this._range_retry_attempt = 0;
        this._serverSignals = new SignalBag();
        this._cancellable = new Gio.Cancellable();
        this._cached_state = STATUS_UNKNOWN;
        this._reload_id = 0;
        this._force_reload_pending = false;
        this._event_list = event_list || null;
        this.current_range_start = null;
        this.current_range_end = null;
        /*
         * Keep civil endpoints separately from transport instants. A timezone
         * change can change the Unix instant that represents local midnight
         * without changing which Gregorian cells the calendar requested.
         */
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;
        this.current_selected_date = null;
        this.current_selected_civil = null;
        this._range_request_generation = 0;
        this._range_request_pending = false;
        this._range_request_succeeded = false;
        this._range_accepting_events = false;
        this._queued_range_force = false;
        this.last_update_timestamp = 0;
        this.event_store = CalendarPlus.EventStore.new();

        this._timezone_monitor = null;
        this._timezone_monitor_signal_id = 0;
        this._startTimezoneMonitor();
    }

    start_events() {
        if (this._destroyed || this._inited) {
            return;
        }

        /*
         * Cinnamon now owns backend selection behind CalendarServer.  Watch
         * that stable service itself so a backend/server restart reconnects
         * immediately, while still connecting directly to allow D-Bus
         * activation instead of waiting for Evolution Data Server first.
         */
        if (this._bus_watch_id === 0) {
            this._bus_watch_id = Gio.bus_watch_name(
                Gio.BusType.SESSION,
                CALENDAR_SERVER_BUS_NAME,
                Gio.BusNameWatcherFlags.NONE,
                () => this._connectCalendarServer(),
                () => this._calendarServerVanished()
            );
        }
        this._connectCalendarServer();
    }

    _connectCalendarServer() {
        if (this._destroyed || this._calendar_server !== null ||
            this._calendar_server_connecting) {
            return;
        }

        this._cancelReconnect();
        this._calendar_server_connecting = true;
        const generation = this._calendar_server_generation;
        Cinnamon.CalendarServerProxy.new_for_bus(
            Gio.BusType.SESSION,
            Gio.DBusProxyFlags.NONE,
            CALENDAR_SERVER_BUS_NAME,
            "/org/cinnamon/CalendarServer",
            this._cancellable,
            (object, result) => this._calendarServerReady(result, generation)
        );
    }

    _calendarServerReady(result, generation) {
        try {
            const server = Cinnamon.CalendarServerProxy.new_for_bus_finish(result);
            if (this._destroyed || generation !== this._calendar_server_generation) {
                return;
            }

            this._calendar_server_connecting = false;
            this._serverSignals.connect(
                server,
                "events-added-or-updated",
                (proxy, payload) => this._ingestEvents(payload)
            );
            this._serverSignals.connect(
                server,
                "events-removed",
                (proxy, ids) => this._removeEvents(ids)
            );
            this._serverSignals.connect(
                server,
                "client-disappeared",
                () => this._calendarSetChanged()
            );
            this._serverSignals.connect(
                server,
                "notify::status",
                () => this._statusChanged()
            );

            this._calendar_server = server;
            this._cached_state = server.status;
            this._inited = true;
            this.emit("events-manager-ready");
        } catch (error) {
            if (!this._destroyed && generation === this._calendar_server_generation) {
                this._calendar_server_connecting = false;
                this._serverSignals.disconnectAll();
                this._calendar_server = null;
                this._inited = false;
                if (this.event_store !== null) {
                    this.event_store.clear();
                }
                if (this._event_list !== null) {
                    this._event_list.set_events(null, false, true);
                }
                global.logError(
                    `${APPLET_UUID}: could not connect to calendar server: ${error}`
                );
                this._scheduleReconnect();
            }
        }
    }

    _calendarServerVanished() {
        if (this._destroyed) {
            return;
        }

        /*
         * STATUS_NO_CALENDARS is authoritative and CalendarServer deliberately
         * exits after publishing it. Preserve that state across the expected
         * disappearance instead of misclassifying it as transport failure.
         */
        const expectedEmpty = this._cached_state === STATUS_NO_CALENDARS;

        this._calendar_server_generation += 1;
        this._calendar_server_connecting = false;
        this._cancelReconnect();
        this._serverSignals.disconnectAll();
        this._calendar_server = null;
        this._inited = false;
        if (!expectedEmpty) {
            this._cached_state = STATUS_UNKNOWN;
        }
        this._cancelRangeRetry();

        if (this.event_store !== null) {
            this.event_store.clear();
        }
        /*
         * The pane deliberately remains allocated while CalendarServer is
         * reconnecting so popup geometry does not jump.  Clear presentation
         * state at the same time as the native store; otherwise select_date()
         * refuses to repaint while inactive and stale appointments can remain
         * visible after the transport has disappeared.
         */
        if (this._event_list !== null) {
            this._event_list.set_events(null, false, !expectedEmpty);
        }
        this.current_range_start = null;
        this.current_range_end = null;
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;
        this._range_request_generation += 1;
        this._range_request_pending = false;
        this._range_request_succeeded = false;
        this._range_accepting_events = false;
        this._queued_range_force = false;
        this.last_update_timestamp = 0;
        this.emit("has-calendars-changed");
        this.emit("events-updated");
        /*
         * Unexpected loss gets bounded reconnects. An authoritative empty
         * service is re-probed on the next user-driven start_events() call or
         * immediately if the watched bus name reappears.
         */
        if (!expectedEmpty) {
            this._scheduleReconnect();
        }
    }

    _scheduleReconnect() {
        if (this._destroyed || this._reconnect_timer_id > 0) {
            return;
        }

        this._reconnect_timer_id = Mainloop.timeout_add_seconds(5, () => {
            this._reconnect_timer_id = 0;
            this._connectCalendarServer();
            return GLib.SOURCE_REMOVE;
        });
    }

    _cancelReconnect() {
        if (this._reconnect_timer_id > 0) {
            Mainloop.source_remove(this._reconnect_timer_id);
            this._reconnect_timer_id = 0;
        }
    }

    _cancelRangeRetry() {
        if (this._range_retry_timer_id > 0) {
            Mainloop.source_remove(this._range_retry_timer_id);
            this._range_retry_timer_id = 0;
        }
    }

    _scheduleRangeRetry() {
        if (this._destroyed || this._range_retry_timer_id > 0 ||
            this._range_request_pending ||
            this.current_range_start === null ||
            this.current_range_end === null) {
            return;
        }

        const delay = Math.min(
            60,
            Math.pow(2, Math.min(this._range_retry_attempt, 5))
        );
        this._range_retry_attempt += 1;
        this._range_retry_timer_id = Mainloop.timeout_add_seconds(
            delay,
            () => {
                this._range_retry_timer_id = 0;
                if (!this._destroyed && !this._range_request_pending &&
                    this._calendar_server !== null &&
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

    _startTimezoneMonitor() {
        try {
            const timezone = Gio.File.new_for_path("/etc/localtime");
            this._timezone_monitor = timezone.monitor_file(
                Gio.FileMonitorFlags.NONE,
                this._cancellable
            );
            this._timezone_monitor_signal_id = this._timezone_monitor.connect(
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
        if (this._destroyed || this.event_store === null) {
            return;
        }

        /*
         * Close admission before touching timezone-derived membership. Signals
         * from the old CalendarServer view are not generation-tagged and must
         * not repopulate the index during the deferred forced reload.
         */
        this._range_accepting_events = false;
        this.event_store.refresh_timezone();
        this._invalidateCurrentRange(false);
        this.emit("events-updated");
    }

    _invalidateCurrentRange(clearStore) {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        /*
         * All event-universe invalidations share one ordering invariant:
         * close admission first, mark the accepted request stale, then either
         * queue a replacement behind the in-flight request or reload the newest
         * civil range. This removes idle-window races between invalidation and
         * queue_reload().
         */
        this._range_accepting_events = false;
        this._range_request_succeeded = false;
        if (clearStore) {
            this.event_store.clear();
        }

        if (this._range_request_pending) {
            this._queued_range_force = true;
        } else if (this.current_range_start !== null &&
                   this.current_range_end !== null) {
            this.queue_reload(true);
        }
    }

    _ingestEvents(payload) {
        if (this._destroyed || this.event_store === null ||
            !this._range_accepting_events ||
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
            /*
             * CalendarServer has no request-generation token on event signals.
             * Reject rows that cannot belong to the newest desired range before
             * they reach the native index. Native code remains authoritative
             * for tuple validation and interval normalization.
             */
            if (!_eventVariantOverlapsRange(
                    eventVariant, rangeStart, rangeEnd)) {
                continue;
            }
            changed = this.event_store.add_or_update(
                eventVariant,
                this.last_update_timestamp
            ) || changed;
        }

        if (changed) {
            this.emit("events-updated");
        }
    }

    _removeEvents(serializedIds) {
        if (this._destroyed || this.event_store === null ||
            typeof serializedIds !== "string" || serializedIds.length === 0) {
            return;
        }

        /*
         * CalendarServer removal signals contain IDs only: there is no range
         * generation or revision with which to prove that a late removal came
         * from the current EDS view. Treat every removal as cache invalidation
         * and rebuild the newest desired range instead of deleting an ID from a
         * possibly newer generation. This is correct for both genuine current
         * removals and delayed signals from a stopped view.
         */
        this._invalidateCurrentRange(true);
        this.emit("events-updated");
    }

    _calendarSetChanged() {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        /*
         * Client disappearance changes the membership of the event universe.
         * A clean reload is safer than trying to infer which cached rows came
         * from the removed EDS client.
         */
        /*
         * Keep the grid-owned desired range. A client-set change invalidates
         * the result for that range, not the range itself. The shared
         * invalidation path closes event admission before the deferred reload,
         * including while another SetTimeRange call is still in flight.
         */
        this._invalidateCurrentRange(true);
        this.emit("events-updated");
    }

    _statusChanged() {
        if (this._destroyed || this._calendar_server === null) {
            return;
        }
        const next = this._calendar_server.status;
        if (next === this._cached_state || next === STATUS_UNKNOWN) {
            return;
        }

        this._cached_state = next;
        this.queue_reload(true);
        this.emit("has-calendars-changed");
    }

    _requestVisibleRange(first, last, force, clearStore) {
        if (this._destroyed || this._calendar_server === null ||
            this.event_store === null || this._range_request_pending) {
            return;
        }

        if (clearStore) {
            this.event_store.clear();
            this.emit("events-updated");
        }
        this._range_accepting_events = false;

        const exclusiveEnd = last.add_days(1);
        if (exclusiveEnd === null) {
            global.logError(
                `${APPLET_UUID}: visible event range exceeds DateTime limits.`
            );
            return;
        }

        const requestGeneration = ++this._range_request_generation;
        this._range_request_pending = true;
        this._range_request_succeeded = false;
        const requestedStart = first;
        const requestedEnd = last;
        this.last_update_timestamp = GLib.get_monotonic_time();

        this._calendar_server.call_set_time_range(
            first.to_unix(),
            exclusiveEnd.to_unix() - 1,
            Boolean(force),
            null,
            (server, result) => {
                let succeeded = false;
                try {
                    server.call_set_time_range_finish(result);
                    succeeded = true;
                } catch (error) {
                    if (!this._destroyed &&
                        requestGeneration === this._range_request_generation) {
                        global.logError(
                            `${APPLET_UUID}: event range request failed: ${error}`
                        );
                    }
                }

                if (this._destroyed ||
                    requestGeneration !== this._range_request_generation) {
                    return;
                }

                this._range_request_pending = false;
                this._range_request_succeeded = succeeded;

                const desiredChanged =
                    !sameInstant(requestedStart, this.current_range_start) ||
                    !sameInstant(requestedEnd, this.current_range_end);
                if (desiredChanged || this._queued_range_force) {
                    const queuedForce = this._queued_range_force;
                    this._queued_range_force = false;
                    const nextStart = this.current_range_start;
                    const nextEnd = this.current_range_end;
                    this._cancelRangeRetry();
                    this._range_retry_attempt = 0;
                    if (nextStart !== null && nextEnd !== null) {
                        /*
                         * CalendarServer does not tag event signals with the
                         * originating range request. Never overlap two range
                         * calls: after the older request has completed, clear
                         * its rows before issuing the newest desired range.
                         */
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
                    /*
                     * CalendarServer accepts the range before its asynchronous
                     * backend starts emitting. Event admission opens only after
                     * that acceptance boundary. Every real request has already
                     * replaced the local store, so an empty refresh requires no
                     * silence timer or guessed "refresh complete" delay.
                     */
                    this._range_accepting_events = true;
                    this._cancelRangeRetry();
                    this._range_retry_attempt = 0;
                } else {
                    this._range_accepting_events = false;
                    this._scheduleRangeRetry();
                }
            }
        );
    }

    set_visible_range(firstDate, lastDate, force) {
        if (this._destroyed || this._calendar_server === null ||
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
        const needsRetry = !this._range_request_pending &&
            !this._range_request_succeeded;
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
            this._range_retry_attempt = 0;
        }

        if (this._range_request_pending) {
            /*
             * Preserve only the newest desired range. This serializes
             * CalendarServer traffic and prevents an obsolete request from
             * overlapping the next request's refresh token.
             */
            this._queued_range_force =
                this._queued_range_force || Boolean(force) || changed;
            return;
        }

        this._queued_range_force = false;
        /*
         * Every actual CalendarServer request owns a fresh local generation.
         * Replacing the store up front makes an empty response correct by
         * construction instead of relying on a timed stale-row cull.
         */
        this._requestVisibleRange(first, last, force, true);
    }

    queue_reload(force) {
        if (this._reload_id > 0) {
            Mainloop.source_remove(this._reload_id);
            this._reload_id = 0;
        }
        if (this._destroyed) {
            return;
        }
        this._force_reload_pending = this._force_reload_pending || Boolean(force);
        this._reload_id = Mainloop.idle_add(() => {
            this._reload_id = 0;
            const forceReload = this._force_reload_pending;
            this._force_reload_pending = false;

            if (!this._destroyed) {
                /*
                 * A forced queued reload means authoritative server data, not
                 * merely repainting the selected agenda from the local cache.
                 * The calendar view still owns range calculation; reuse the
                 * last range it supplied rather than inventing one here.
                 */
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

        /*
         * A jurisdiction can skip an entire civil date. The calendar grid still
         * owns that date coordinate, but there is no Unix interval to query for
         * appointments. Present the date itself using UTC as a formatting-only
         * carrier and keep its agenda empty.
         */
        if (day === null) {
            this.current_selected_date = null;
            if (this._event_list !== null) {
                const display = GLib.DateTime.new_utc(
                    year, month, dayOfMonth, 12, 0, 0
                );
                if (display !== null) {
                    this._event_list.set_date(display);
                }
                this._event_list.set_events(null, false);
            }
            return;
        }

        /*
         * Selection is presentation state, not transport state. Update the
         * agenda heading immediately even while CalendarServer is still being
         * activated; the eventual server readiness only controls event data.
         */
        this.current_selected_date = day;
        if (this._event_list !== null) {
            this._event_list.set_date(day);
        }

        if (!this.is_active()) {
            return;
        }

        if (this._event_list !== null) {
            this._event_list.set_events(
                this._snapshot_for_date(day),
                previousCivil === null || changedMonth || Boolean(force)
            );
        }
    }

    _snapshot_for_date(day) {
        if (this._destroyed || this.event_store === null || day === null) {
            return null;
        }
        const snapshot = new EventSnapshot(
            this.event_store.get_snapshot(
                day.to_unix(),
                GLib.DateTime.new_now_local().to_unix()
            )
        );
        return snapshot.events.length === 0 ? null : snapshot;
    }

    get_colors_for_range(js_date, dayCount, maximumColorsPerDay) {
        if (this._destroyed || this.event_store === null ||
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
        /*
         * Popup geometry must not depend on asynchronous D-Bus activation.
         * When events are enabled, reserve the agenda pane while CalendarServer
         * is connecting or temporarily unavailable.  Collapse it only after an
         * authoritative server status says that no calendars exist.
         */
        const status = this._calendar_server !== null
            ? this._calendar_server.status
            : this._cached_state;
        return !this._destroyed &&
            this.settings.getValue("show-events") &&
            status !== STATUS_NO_CALENDARS;
    }

    is_active() {
        return !this._destroyed &&
            this._inited &&
            this.settings.getValue("show-events") &&
            this._calendar_server !== null &&
            this._calendar_server.status !== STATUS_NO_CALENDARS;
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._inited = false;
        this._calendar_server_generation += 1;
        this._range_request_generation += 1;
        this._range_request_pending = false;
        this._range_request_succeeded = false;
        this._range_accepting_events = false;
        this._queued_range_force = false;
        this._calendar_server_connecting = false;
        this.current_range_start_civil = null;
        this.current_range_end_civil = null;

        if (this._bus_watch_id > 0) {
            Gio.bus_unwatch_name(this._bus_watch_id);
            this._bus_watch_id = 0;
        }
        if (this._cancellable !== null) {
            try {
                this._cancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
        }

        this._cancelReconnect();
        this._cancelRangeRetry();
        if (this._reload_id > 0) {
            Mainloop.source_remove(this._reload_id);
            this._reload_id = 0;
        }

        if (this._timezone_monitor !== null) {
            if (this._timezone_monitor_signal_id > 0) {
                try {
                    this._timezone_monitor.disconnect(this._timezone_monitor_signal_id);
                } catch (error) {
                    global.logError(error);
                }
            }
            try {
                this._timezone_monitor.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._timezone_monitor_signal_id = 0;
        this._timezone_monitor = null;

        this._serverSignals.disconnectAll();
        this._calendar_server = null;

        this._event_list = null;
        if (this.event_store !== null) {
            this.event_store.clear();
            this.event_store = null;
        }

        this._cancellable = null;
        this.settings = null;
        this.desktop_settings = null;
        this.current_selected_date = null;
        this.current_selected_civil = null;
        this.current_range_start = null;
        this.current_range_end = null;
    }
};
Signals.addSignalMethods(EventsManager.prototype);
