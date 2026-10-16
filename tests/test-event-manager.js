#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");
let nextId = 1;
const timeouts = new Map();
const idles = new Map();
const localOffsets = new Map();
const unavailableLocalTimes = new Set();
const observations = {
    clears: 0,
    addCalls: [],
    removeCalls: [],
    rangeRequests: [],
    transportStarts: 0,
    timezoneCancels: 0,
    timezoneDisconnects: 0,
    transportDestroys: 0,
    logs: [],
    snapshotRows: [],
    invalidLocalInstants: new Set(),
};
function addSignalMethods(prototype) {
    prototype.connect = function(signal, callback) {
        if (!this.__signals) this.__signals = new Map();
        const id = nextId++;
        this.__signals.set(id, [signal, callback]);
        return id;
    };
    prototype.disconnect = function(id) {
        if (this.__signals) this.__signals.delete(id);
    };
    prototype.emit = function(signal, ...args) {
        if (!this.__signals) return;
        for (const [name, callback] of this.__signals.values()) {
            if (name === signal) callback(this, ...args);
        }
    };
}
class SignalBag {
    constructor() { this.entries = []; }
    connect(object, signal, callback) {
        const id = object.connect(signal, callback);
        this.entries.push([object, id]);
        return id;
    }
    disconnectAll() {
        for (const [object, id] of this.entries.splice(0)) object.disconnect(id);
    }
}
function civilKey(y, m, d) {
    return `${y}-${m}-${d}`;
}
function localTimeKey(y, m, d, h, min) {
    return `${y}-${m}-${d}-${h}-${min}`;
}
class FakeDateTime {
    constructor(unix, civil = null) {
        this.unix = unix;
        this.civil = civil;
    }
    to_unix() { return this.unix; }
    get_year() {
        return this.civil ? this.civil.year :
            new Date(this.unix * 1000).getUTCFullYear();
    }
    get_month() {
        return this.civil ? this.civil.month :
            new Date(this.unix * 1000).getUTCMonth() + 1;
    }
    get_day_of_month() {
        return this.civil ? this.civil.day :
            new Date(this.unix * 1000).getUTCDate();
    }
    add_days(days) {
        const year = this.get_year();
        const month = this.get_month();
        const day = this.get_day_of_month();
        const h = this.civil ? this.civil.hour : 0;
        const min = this.civil ? this.civil.minute : 0;
        const sec = this.civil ? this.civil.second : 0;
        const shifted = new Date(Date.UTC(year, month - 1, day + days, 12, 0, 0));
        return DateTime.new_local(
            shifted.getUTCFullYear(),
            shifted.getUTCMonth() + 1,
            shifted.getUTCDate(),
            h,
            min,
            sec
        );
    }
}
const DateTime = {
    new_local(y, m, d, h, min, sec) {
        if (unavailableLocalTimes.has(localTimeKey(y, m, d, h, min))) {
            return null;
        }
        const offset = localOffsets.get(civilKey(y, m, d)) || 0;
        const unix = Date.UTC(y, m - 1, d, h, min, sec) / 1000 - offset;
        return new FakeDateTime(unix, {
            year: y, month: m, day: d, hour: h, minute: min, second: sec,
        });
    },
    new_utc(y, m, d, h, min, sec) {
        return new FakeDateTime(Date.UTC(y, m - 1, d, h, min, sec) / 1000, {
            year: y, month: m, day: d, hour: h, minute: min, second: sec,
        });
    },
    new_from_unix_local(value) {
        return observations.invalidLocalInstants.has(value)
            ? null
            : new FakeDateTime(value);
    },
    new_now_local() { return new FakeDateTime(0); },
};
class FakeTimezoneSource {
    start() { this.started = true; }
    destroy() { observations.timezoneCancels += 1; }
}
addSignalMethods(FakeTimezoneSource.prototype);
const originalTimezoneDisconnect = FakeTimezoneSource.prototype.disconnect;
FakeTimezoneSource.prototype.disconnect = function(id) {
    observations.timezoneDisconnects += 1;
    originalTimezoneDisconnect.call(this, id);
};
class FakeTransport {
    constructor() {
        this.status = 0;
        this.ready = false;
        this.requests = [];
        addSignalMethods(Object.getPrototypeOf(this));
    }
    is_ready() { return this.ready; }
    start() { observations.transportStarts += 1; }
    request_range(start, end, force, callback) {
        if (!this.ready) return false;
        this.requests.push({ start, end, force, callback });
        observations.rangeRequests.push([start, end, force]);
        return true;
    }
    complete(index, succeeded, current = true) {
        this.requests[index].callback(succeeded, current);
    }
    destroy() { observations.transportDestroys += 1; }
}
const nativeBoundaryModule = {
    NativeBoundary: {
        decodeEventSnapshot(variant) {
            const [revision, rows] = variant.deep_unpack();
            return {
                revision,
                events: rows.map((row) => ({
                    id: row[0], color: row[1], summary: row[2], allDay: row[3],
                    multiDay: row[4], startUnix: row[5], endUnix: row[6],
                    startDayUnix: row[7], endDayUnix: row[8], modified: row[9],
                })),
            };
        },
    },
};
const context = {
    console,
    global: {
        logError(error) { observations.logs.push(String(error)); },
    },
    require(name) {
        assert.equal(name, "./runtimeSupport");
        return {
            SignalBag,
            midnight(value) {
                const year = value.get_year();
                const month = value.get_month();
                const day = value.get_day_of_month();
                for (let minuteOfDay = 0; minuteOfDay < 24 * 60; minuteOfDay++) {
                    const candidate = DateTime.new_local(
                        year,
                        month,
                        day,
                        Math.floor(minuteOfDay / 60),
                        minuteOfDay % 60,
                        0
                    );
                    if (candidate !== null &&
                        candidate.get_year() === year &&
                        candidate.get_month() === month &&
                        candidate.get_day_of_month() === day) {
                        return candidate;
                    }
                }
                return null;
            },
            sameInstant(a, b) {
                return a !== null && b !== null && a.to_unix() === b.to_unix();
            },
            loadLocalModule(moduleName) {
                if (moduleName === "eventRangeState") return context.__rangeModule;
                if (moduleName === "nativeBoundary") return nativeBoundaryModule;
                if (moduleName === "calendarServerTransport") {
                    return {
                        CalendarServerTransport: FakeTransport,
                        STATUS_UNKNOWN: 0,
                        STATUS_NO_CALENDARS: 1,
                    };
                }
                if (moduleName === "timezoneMonitor") {
                    return { TimezoneMonitor: FakeTimezoneSource };
                }
                throw new Error(`unexpected module ${moduleName}`);
            },
        };
    },
    imports: {
        ui: {},
        signals: { addSignalMethods },
        mainloop: {
            timeout_add_seconds(delay, callback) {
                const id = nextId++;
                timeouts.set(id, callback);
                return id;
            },
            idle_add(callback) {
                const id = nextId++;
                idles.set(id, callback);
                return id;
            },
            source_remove(id) {
                timeouts.delete(id);
                idles.delete(id);
            },
        },
        gi: {
            GLib: {
                DateTime,
                SOURCE_REMOVE: false,
                get_monotonic_time() { return 123456; },
            },
            Calendar: {
                event_day_relation() { return 0; },
                event_timing() {
                    return { deep_unpack() { return [0, 0, 0]; } };
                },
                EventStore: {
                    new() {
                        return {
                            clear() { observations.clears += 1; },
                            refresh_timezone() {},
                            add_or_update_in_range(event, timestamp, start, end) {
                                observations.addCalls.push([event, timestamp, start, end]);
                                return true;
                            },
                            remove(serializedIds) {
                                observations.removeCalls.push(serializedIds);
                                return true;
                            },
                            get_snapshot() {
                                return {
                                    deep_unpack() {
                                        return [1, observations.snapshotRows];
                                    },
                                };
                            },
                            get_color_range() {
                                return { deep_unpack() { return []; } };
                            },
                        };
                    },
                },
            },
        },
    },
};
vm.createContext(context);
const rangeSource = fs.readFileSync(
    path.join(root, "src", "cinnamon", "eventRangeState.js"), "utf8"
);
context.__rangeModule = vm.runInContext(
    `(() => {${rangeSource}\nreturn { EventRangeState };})()`,
    context,
    { filename: "eventRangeState.js" }
);
const managerSource = fs.readFileSync(
    path.join(root, "src", "cinnamon", "eventManager.js"), "utf8"
);
vm.runInContext(
    `${managerSource}\nglobalThis.__EventsManager = EventsManager;`,
    context,
    { filename: "eventManager.js" }
);
const EventsManager = context.__EventsManager;

const transport = new FakeTransport();
transport.ready = true;
transport.status = 2;
const manager = new EventsManager(transport);
const first = new Date(Date.UTC(2026, 7, 2, 12, 0, 0));
const last = new Date(Date.UTC(2026, 8, 12, 12, 0, 0));
manager.set_visible_range(first, last, false);
assert.equal(observations.rangeRequests.length, 1);
assert.equal(
    observations.rangeRequests[0][1] - observations.rangeRequests[0][0],
    42 * 86400 - 1
);
transport.complete(0, true);
manager.set_visible_range(first, last, false);
assert.equal(
    observations.rangeRequests.length,
    1,
    "successful identical range must be stable"
);
manager.set_visible_range(first, last, true);
assert.equal(
    observations.rangeRequests.length,
    2,
    "forced refresh must cross the transport port"
);
transport.complete(1, true);
manager._ingestEvents({ unpack() { return [{ opaque: true }]; } });
assert.equal(
    observations.addCalls.length,
    1,
    "admitted rows must go directly to the native store boundary"
);
const requestsBeforeRemoval = observations.rangeRequests.length;
manager._removeEvents("gone-a::gone-b");
assert.deepEqual(
    observations.removeCalls,
    ["gone-a::gone-b"],
    "removals must use the native incremental delete path"
);
assert.equal(
    observations.rangeRequests.length,
    requestsBeforeRemoval,
    "incremental deletion must not force a full range re-fetch"
);
manager.set_visible_range(first, last, true);
transport.complete(2, false);
assert.equal(timeouts.size, 1, "range failure must retain bounded retry behaviour");
const retry = timeouts.values().next().value;
timeouts.clear();
retry();
assert.equal(observations.rangeRequests.length, 4);
const presented = [];
manager.emit = (signal, ...args) => presented.push([signal, ...args]);
transport.ready = false;
transport.emit("unavailable", false);
assert.equal(manager._rangeState.acceptingEvents, false);
assert.ok(observations.clears > 0, "transport loss must invalidate native cached rows");
assert.ok(presented.some(([signal]) => signal === "has-calendars-changed"));
assert.equal(manager.current_range_start_civil.year, first.getFullYear());
assert.equal(manager.current_range_start_civil.month, first.getMonth() + 1);
assert.equal(manager.current_range_start_civil.day, first.getDate());
assert.equal(manager.current_range_end_civil.year, last.getFullYear());
assert.equal(manager.current_range_end_civil.month, last.getMonth() + 1);
assert.equal(manager.current_range_end_civil.day, last.getDate());
const rangesBeforeReconnect = observations.rangeRequests.length;
transport.ready = true;
transport.emit("ready");
assert.equal(idles.size, 1, "transport recovery must schedule one desired-range reload");
const reconnectRefresh = idles.values().next().value;
idles.clear();
reconnectRefresh();
assert.equal(
    observations.rangeRequests.length,
    rangesBeforeReconnect + 1,
    "transport recovery must re-request the preserved visible range"
);
assert.deepEqual(
    observations.rangeRequests.at(-1).slice(0, 2),
    observations.rangeRequests[0].slice(0, 2),
    "recovery must restore the exact desired visible range"
);
assert.equal(
    observations.rangeRequests.at(-1)[2],
    true,
    "recovery must force a fresh CalendarServer range"
);
transport.status = 1;
assert.equal(
    manager.should_show_event_pane(),
    false,
    "authoritative empty status collapses the agenda"
);
manager.destroy();
manager.destroy();
assert.equal(observations.transportDestroys, 1);
assert.equal(observations.timezoneDisconnects, 1);
assert.equal(observations.timezoneCancels, 1);

/*
 * A local civil range is not a count of 86400-second buckets. Model an offset
 * transition between two adjacent days and assert that EventsManager asks for
 * the correct civil endpoint rather than manufacturing a 48-hour interval.
 */
localOffsets.set(civilKey(2026, 10, 4), 10 * 3600);
localOffsets.set(civilKey(2026, 10, 5), 11 * 3600);
localOffsets.set(civilKey(2026, 10, 6), 11 * 3600);
const dstTransport = new FakeTransport();
dstTransport.ready = true;
dstTransport.status = 2;
const dstManager = new EventsManager(dstTransport);
const beforeDstRequestCount = observations.rangeRequests.length;
dstManager.set_visible_range(
    { year: 2026, month: 10, day: 4 },
    { year: 2026, month: 10, day: 5 },
    false
);
assert.equal(observations.rangeRequests.length, beforeDstRequestCount + 1);
const dstRange = observations.rangeRequests.at(-1);
assert.equal(
    dstRange[1] - dstRange[0],
    47 * 3600 - 1,
    "visible event ranges must follow local civil days across offset changes"
);
dstManager.destroy();
localOffsets.clear();

/* A zone may skip midnight while retaining the rest of the civil date. */
unavailableLocalTimes.add(localTimeKey(2026, 10, 7, 0, 0));
const midnightTransport = new FakeTransport();
midnightTransport.ready = true;
midnightTransport.status = 2;
const midnightManager = new EventsManager(midnightTransport);
const beforeMidnightRequestCount = observations.rangeRequests.length;
midnightManager.set_visible_range(
    { year: 2026, month: 10, day: 7 },
    { year: 2026, month: 10, day: 7 },
    false
);
assert.equal(observations.rangeRequests.length, beforeMidnightRequestCount + 1);
assert.equal(
    observations.rangeRequests.at(-1)[0],
    Date.UTC(2026, 9, 7, 0, 1, 0) / 1000,
    "range start must use the first representable instant on a skipped-midnight day"
);
midnightManager.destroy();
unavailableLocalTimes.clear();

const disabledTransport = new FakeTransport();
disabledTransport.ready = true;
disabledTransport.status = 2;
const disabledManager = new EventsManager(disabledTransport);
const rangesBeforeDisable = observations.rangeRequests.length;
const startsBeforeDisable = observations.transportStarts;
disabledManager.set_enabled(false);
disabledManager.start_events();
disabledManager.set_visible_range(first, last, false);
disabledManager._ingestEvents({ unpack() { return [{ ignored: true }]; } });
assert.equal(
    observations.rangeRequests.length,
    rangesBeforeDisable,
    "disabled events must not issue visible-range traffic"
);
assert.equal(
    observations.transportStarts,
    startsBeforeDisable,
    "disabled events must not activate the transport"
);
assert.equal(
    observations.addCalls.length,
    1,
    "disabled events must not ingest CalendarServer rows"
);
disabledManager.set_enabled(true);
assert.equal(
    observations.transportStarts,
    startsBeforeDisable + 1,
    "re-enabling events must reactivate transport discovery"
);
assert.equal(idles.size, 1, "re-enabling must schedule one refresh of the desired range");
const resumeRefresh = idles.values().next().value;
idles.clear();
resumeRefresh();
assert.equal(
    observations.rangeRequests.length,
    rangesBeforeDisable + 1,
    "re-enabling must fetch the preserved visible range exactly once"
);
disabledManager.destroy();

const snapshotTransport = new FakeTransport();
snapshotTransport.ready = true;
snapshotTransport.status = 2;
const snapshotManager = new EventsManager(snapshotTransport);
const snapshotSignals = [];
snapshotManager.emit = (signal, ...args) => snapshotSignals.push([signal, ...args]);
observations.snapshotRows = [
    ["valid", "#112233", "Valid", false, false, 100, 200, 100, 100, 1],
    ["invalid", "#445566", "Invalid", false, false, 300, 400, 300, 300, 1],
];
observations.invalidLocalInstants.add(300);
snapshotManager.select_date(new Date(Date.UTC(2026, 7, 2, 12, 0, 0)), false);
const unavailable = snapshotSignals.find(
    ([signal]) => signal === "agenda-events-changed"
);
assert.notEqual(unavailable, undefined, "snapshot failure must publish agenda state");
assert.deepEqual(
    unavailable.slice(1),
    [null, false, true],
    "a malformed snapshot row must make the whole agenda unavailable"
);
assert.ok(
    observations.logs.some((message) =>
        message.includes("native event snapshot contract violation")
    ),
    "snapshot contract failures must be diagnosable"
);
snapshotManager.destroy();
observations.invalidLocalInstants.clear();
observations.snapshotRows = [];

console.log("EventsManager orchestration behaviour tests passed.");
