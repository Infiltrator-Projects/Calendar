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
const observations = {
    watches: 0, unwatches: 0, proxyRequests: 0, cancels: 0,
    busAppeared: null, busVanished: null, proxyCallback: null,
    proxyStartError: null, rangeCalls: [], rangeCallbacks: [], disconnects: 0,
    errors: [],
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
class Cancellable {
    constructor() { this.cancelled = false; }
    cancel() {
        this.cancelled = true;
        observations.cancels += 1;
    }
}
function makeServer(status = 2, rangeStartError = null) {
    const server = { status, signals: new Map() };
    let startError = rangeStartError;
    server.connect = (signal, callback) => {
        const id = nextId++;
        server.signals.set(id, [signal, callback]);
        return id;
    };
    server.disconnect = (id) => { server.signals.delete(id); observations.disconnects += 1; };
    server.emitSignal = (signal, ...args) => {
        for (const [name, callback] of server.signals.values()) {
            if (name === signal) callback(server, ...args);
        }
    };
    server.call_set_time_range = (start, end, force, cancellable, callback) => {
        if (startError !== null) {
            const error = startError;
            startError = null;
            throw error;
        }
        observations.rangeCalls.push([start, end, force, cancellable]);
        observations.rangeCallbacks.push(callback);
    };
    server.call_set_time_range_finish = (result) => {
        if (result && result.error) throw result.error;
    };
    return server;
}
const context = {
    console,
    require(name) {
        assert.equal(name, "./runtimeSupport");
        return { SignalBag };
    },
    global: { logError(error) { observations.errors.push(String(error)); } },
    imports: {
        ui: {},
        signals: { addSignalMethods },
        mainloop: {
            timeout_add_seconds(delay, callback) {
                const id = nextId++;
                timeouts.set(id, callback);
                return id;
            },
            source_remove(id) { timeouts.delete(id); },
        },
        gi: {
            GLib: { SOURCE_REMOVE: false },
            Gio: {
                Cancellable,
                BusType: { SESSION: 0 },
                BusNameWatcherFlags: { NONE: 0 },
                DBusProxyFlags: { NONE: 0 },
                bus_watch_name(type, name, flags, appeared, vanished) {
                    observations.watches += 1;
                    observations.busAppeared = appeared;
                    observations.busVanished = vanished;
                    return nextId++;
                },
                bus_unwatch_name() { observations.unwatches += 1; },
            },
            Cinnamon: {
                CalendarServerProxy: {
                    new_for_bus(type, flags, name, objectPath, cancellable, callback) {
                        observations.proxyRequests += 1;
                        if (observations.proxyStartError !== null) {
                            const error = observations.proxyStartError;
                            observations.proxyStartError = null;
                            throw error;
                        }
                        observations.proxyCallback = callback;
                    },
                    new_for_bus_finish(result) {
                        if (result.error) throw result.error;
                        return result.server;
                    },
                },
            },
        },
    },
};
vm.createContext(context);
const source = fs.readFileSync(
    path.join(root, "src", "cinnamon", "calendarServerTransport.js"), "utf8"
);
vm.runInContext(
    `${source}\nglobalThis.__Transport = CalendarServerTransport;` +
    `\nglobalThis.__NO_CAL = STATUS_NO_CALENDARS;`,
    context,
    { filename: "calendarServerTransport.js" }
);
const Transport = context.__Transport;
const transport = new Transport();
let ready = 0;
let unavailable = [];
transport.connect("ready", () => { ready += 1; });
transport.connect("unavailable", (owner, expectedEmpty) => unavailable.push(expectedEmpty));
transport.start();
transport.start();
assert.equal(observations.watches, 1, "service watch must be single-owner");
assert.equal(observations.proxyRequests, 1, "concurrent connect attempts must coalesce");
const server = makeServer(2);
observations.proxyCallback(null, { server });
assert.equal(ready, 1);
assert.equal(transport.is_ready(), true);
assert.equal(transport.status, 2);
assert.equal(transport.request_range(10, 20, true, (succeeded, current) => {
    assert.equal(succeeded, true);
    assert.equal(current, true);
}), true);
assert.deepEqual(
    observations.rangeCalls[0].slice(0, 3),
    [10, 20, true]
);
assert.ok(
    observations.rangeCalls[0][3] instanceof Cancellable,
    "range requests must share the transport-owned cancellable"
);
observations.rangeCallbacks[0](server, {});
observations.busVanished();
assert.equal(transport.is_ready(), false);
assert.deepEqual(unavailable, [false]);
assert.equal(timeouts.size, 1, "unexpected loss must schedule bounded reconnect");
const reconnect = timeouts.values().next().value;
timeouts.clear();
reconnect();
assert.equal(observations.proxyRequests, 2);
const emptyServer = makeServer(context.__NO_CAL);
observations.proxyCallback(null, { server: emptyServer });
observations.busVanished();
assert.equal(unavailable.at(-1), true, "authoritative empty shutdown is not a failure");
assert.equal(timeouts.size, 0, "authoritative empty shutdown must not poll reconnect");
transport.destroy();
transport.destroy();
assert.equal(observations.unwatches, 1);
assert.equal(observations.cancels, 1);
assert.equal(
    observations.rangeCalls[0][3].cancelled,
    true,
    "destroy must cancel an outstanding range request"
);

{
    const requestsBefore = observations.proxyRequests;
    const errorsBefore = observations.errors.length;
    observations.proxyStartError = new Error("proxy start failed");
    const failingTransport = new Transport();
    let synchronousFailures = 0;
    failingTransport.connect("unavailable", (owner, expectedEmpty) => {
        assert.equal(expectedEmpty, false);
        synchronousFailures += 1;
    });
    failingTransport.start();
    assert.equal(synchronousFailures, 1, "synchronous proxy start failure must be observable");
    assert.equal(observations.proxyRequests, requestsBefore + 1);
    assert.equal(timeouts.size, 1, "synchronous proxy start failure must schedule reconnect");
    assert.equal(observations.errors.length, errorsBefore + 1);
    assert.match(observations.errors.at(-1), /proxy start failed/);

    const retry = timeouts.values().next().value;
    timeouts.clear();
    retry();
    assert.equal(
        observations.proxyRequests,
        requestsBefore + 2,
        "failed proxy initiation must clear connecting state for retry"
    );
    failingTransport.destroy();
}

{
    const errorsBefore = observations.errors.length;
    const rangeTransport = new Transport();
    rangeTransport.start();
    const rangeServer = makeServer(2, new Error("range start failed"));
    observations.proxyCallback(null, { server: rangeServer });
    let callbackCalls = 0;
    assert.equal(
        rangeTransport.request_range(30, 40, false, () => { callbackCalls += 1; }),
        false,
        "synchronous range-call initiation failure must return false"
    );
    assert.equal(callbackCalls, 0, "a request that never started must not synthesize completion");
    assert.equal(observations.errors.length, errorsBefore + 1);
    assert.match(observations.errors.at(-1), /range start failed/);

    assert.equal(
        rangeTransport.request_range(30, 40, false, () => { callbackCalls += 1; }),
        true,
        "transport must remain usable after a synchronous range start failure"
    );
    observations.rangeCallbacks.at(-1)(rangeServer, {});
    assert.equal(callbackCalls, 1);
    rangeTransport.destroy();
}

function evaluateTimezoneMonitor({ directFails = false, fallbackFails = false, directConnectFails = false } = {}) {
    const timezoneObservations = {
        directMonitors: 0,
        directoryMonitors: 0,
        monitorCancels: 0,
        cancellableCancels: 0,
        disconnects: 0,
        errors: [],
    };

    class TimezoneCancellable {
        cancel() { timezoneObservations.cancellableCancels += 1; }
    }

    function makeMonitor(connectFails = false) {
        const signals = new Map();
        return {
            connect(signal, callback) {
                if (connectFails) throw new Error("monitor connect failed");
                const id = nextId++;
                signals.set(id, [signal, callback]);
                return id;
            },
            disconnect(id) {
                signals.delete(id);
                timezoneObservations.disconnects += 1;
            },
            cancel() { timezoneObservations.monitorCancels += 1; },
            emitChanged(file = null, otherFile = null) {
                for (const [signal, callback] of signals.values()) {
                    if (signal === "changed") {
                        callback(this, file, otherFile, 0);
                    }
                }
            },
        };
    }

    const directMonitor = makeMonitor(directConnectFails);
    const directoryMonitor = makeMonitor();
    const fakeFile = (value) => ({
        get_basename() { return path.basename(value); },
        monitor_file() {
            timezoneObservations.directMonitors += 1;
            if (directFails) throw new Error("direct monitor failed");
            return directMonitor;
        },
        monitor_directory() {
            timezoneObservations.directoryMonitors += 1;
            if (fallbackFails) throw new Error("directory monitor failed");
            return directoryMonitor;
        },
    });

    const timezoneContext = {
        console,
        global: {
            logError(error) { timezoneObservations.errors.push(String(error)); },
        },
        imports: {
            signals: { addSignalMethods },
            gi: {
                Gio: {
                    Cancellable: TimezoneCancellable,
                    FileMonitorFlags: { NONE: 0 },
                    File: { new_for_path: fakeFile },
                },
            },
        },
    };
    vm.createContext(timezoneContext);
    const timezoneSource = fs.readFileSync(
        path.join(root, "src", "cinnamon", "timezoneMonitor.js"), "utf8"
    );
    vm.runInContext(
        `${timezoneSource}\nglobalThis.__TimezoneMonitor = TimezoneMonitor;`,
        timezoneContext,
        { filename: "timezoneMonitor.js" }
    );

    return {
        TimezoneMonitor: timezoneContext.__TimezoneMonitor,
        observations: timezoneObservations,
        directMonitor,
        directoryMonitor,
        file(name) { return fakeFile(`/etc/${name}`); },
    };
}

{
    const fixture = evaluateTimezoneMonitor();
    const timezoneMonitor = new fixture.TimezoneMonitor();
    let changed = 0;
    timezoneMonitor.connect("changed", () => { changed += 1; });
    timezoneMonitor.start();
    timezoneMonitor.start();
    assert.equal(fixture.observations.directMonitors, 1);
    assert.equal(fixture.observations.directoryMonitors, 0);
    fixture.directMonitor.emitChanged();
    assert.equal(changed, 1);
    timezoneMonitor.destroy();
    timezoneMonitor.destroy();
    assert.equal(fixture.observations.disconnects, 1);
    assert.equal(fixture.observations.monitorCancels, 1);
    assert.equal(fixture.observations.cancellableCancels, 1);
}

{
    const fixture = evaluateTimezoneMonitor({ directFails: true });
    const timezoneMonitor = new fixture.TimezoneMonitor();
    let changed = 0;
    timezoneMonitor.connect("changed", () => { changed += 1; });
    timezoneMonitor.start();
    assert.equal(fixture.observations.directMonitors, 1);
    assert.equal(fixture.observations.directoryMonitors, 1);
    fixture.directoryMonitor.emitChanged(fixture.file("passwd"));
    assert.equal(changed, 0, "fallback must ignore unrelated /etc changes");
    fixture.directoryMonitor.emitChanged(fixture.file("localtime"));
    assert.equal(changed, 1, "fallback must observe /etc/localtime replacement");
    fixture.directoryMonitor.emitChanged(null, fixture.file("localtime"));
    assert.equal(changed, 2, "fallback must observe localtime move targets");
    timezoneMonitor.destroy();
}

{
    const fixture = evaluateTimezoneMonitor({ directConnectFails: true });
    const timezoneMonitor = new fixture.TimezoneMonitor();
    let changed = 0;
    timezoneMonitor.connect("changed", () => { changed += 1; });
    timezoneMonitor.start();
    assert.equal(fixture.observations.directMonitors, 1);
    assert.equal(fixture.observations.directoryMonitors, 1);
    assert.equal(
        fixture.observations.monitorCancels,
        1,
        "a direct monitor whose signal hookup fails must be cancelled"
    );
    fixture.directoryMonitor.emitChanged(fixture.file("localtime"));
    assert.equal(changed, 1);
    timezoneMonitor.destroy();
    assert.equal(fixture.observations.monitorCancels, 2);
}

{
    const fixture = evaluateTimezoneMonitor({
        directFails: true,
        fallbackFails: true,
    });
    const timezoneMonitor = new fixture.TimezoneMonitor();
    timezoneMonitor.start();
    assert.equal(fixture.observations.errors.length, 1);
    assert.match(
        fixture.observations.errors[0],
        /fallback failed/,
        "total monitoring failure must remain observable"
    );
    timezoneMonitor.destroy();
}

console.log("CalendarServer transport and timezone monitor behaviour tests passed.");
