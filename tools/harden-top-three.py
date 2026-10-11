#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot, assertion-driven migration for Calendar 1.0.88.

This helper is intentionally temporary.  It transforms the exact 1.0.87 tree,
adds behavioural tests for the new seams, and is removed by the validation
workflow before the release candidate is committed.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one exact match, found {count}")
    return text.replace(old, new, 1)


def sub_once(text: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"{label}: expected one regex match, found {count}")
    return result


CALENDAR_SERVER_TRANSPORT = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
            null,
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
'''

TEMPORAL_POLICY = r'''// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Effective temporal-policy boundary for the Cinnamon composition root.
 *
 * The native SystemClock publishes one coherent policy snapshot.  This module
 * alone decodes and validates that snapshot, caches one immutable generation
 * and reduces it to the scalar clock configuration needed by presentation.
 */

const CalendarPlus = imports.gi.CalendarPlus;

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
const NativeBoundary = RuntimeSupport.loadLocalModule(
    "nativeBoundary"
).NativeBoundary;

const FALLBACK_POLICY = Object.freeze({
    mode: "standard",
    showSeconds: false,
    locationConfigured: false,
    latitude: 0.0,
    longitude: 0.0,
    calendar: "gregorian",
    authority: "mint-cinnamon",
    providerAvailable: false,
});

function _freezePolicy(decoded, isNativeClockMode) {
    const numericLatitude = Number(decoded.latitude);
    const numericLongitude = Number(decoded.longitude);
    const modeIsValid =
        decoded.mode === "standard" ||
        decoded.mode === "standard-24" ||
        decoded.mode === "standard-12" ||
        isNativeClockMode(decoded.mode);
    const calendarIsValid =
        typeof decoded.calendar === "string" &&
        decoded.calendar.length > 0 &&
        CalendarPlus.CalendarSystem.new(decoded.calendar) !== null;
    const authorityIsValid =
        decoded.authority === "mint-cinnamon" ||
        decoded.authority === "infiltrator-system-settings";

    if (typeof decoded.mode !== "string" || !modeIsValid ||
        !calendarIsValid ||
        !Number.isFinite(numericLatitude) ||
        numericLatitude < -90 || numericLatitude > 90 ||
        !Number.isFinite(numericLongitude) ||
        numericLongitude < -180 || numericLongitude > 180 ||
        !authorityIsValid) {
        throw new Error("invalid effective temporal policy");
    }

    return Object.freeze({
        mode: decoded.mode,
        showSeconds: Boolean(decoded.showSeconds),
        locationConfigured: Boolean(decoded.locationConfigured),
        latitude: numericLatitude,
        longitude: numericLongitude,
        calendar: decoded.calendar,
        authority: decoded.authority,
        providerAvailable: Boolean(decoded.providerAvailable),
    });
}

var TemporalPolicyResolver = class TemporalPolicyResolver {
    constructor(systemClock, isNativeClockMode) {
        this._systemClock = systemClock;
        this._isNativeClockMode = typeof isNativeClockMode === "function"
            ? isNativeClockMode
            : () => false;
        this._cache = null;
    }

    invalidate() {
        this._cache = null;
    }

    get() {
        if (this._cache !== null) {
            return this._cache;
        }
        if (!this._systemClock) {
            this._cache = FALLBACK_POLICY;
            return this._cache;
        }
        try {
            const variant = this._systemClock.get_system_policy();
            if (!variant) {
                throw new Error("missing effective temporal policy");
            }
            this._cache = _freezePolicy(
                NativeBoundary.decodeTemporalPolicy(variant),
                this._isNativeClockMode
            );
        } catch (error) {
            global.logError(error);
            this._cache = FALLBACK_POLICY;
        }
        return this._cache;
    }

    clockConfig({ vertical, use24h, showDate }) {
        const policy = this.get();
        return Object.freeze({
            mode: policy.mode,
            showSeconds: policy.showSeconds,
            locationConfigured: policy.locationConfigured,
            latitude: policy.latitude,
            longitude: policy.longitude,
            vertical: Boolean(vertical),
            use24h: Boolean(use24h),
            showDate: Boolean(showDate),
            primaryCalendar: policy.calendar,
        });
    }
};
'''

POPUP_TEST = r'''#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const root = path.resolve(__dirname, "..");
let nextSignalId = 1;
const timers = new Map();

class Actor {
    constructor() {
        this.reactive = true;
        this.visible = true;
        this.opacity = 255;
        this.size = null;
    }
    is_finalized() { return false; }
    hide() { this.visible = false; }
    set_size(width, height) { this.size = [width, height]; }
    remove_all_transitions() {}
    get_preferred_size() { return [0, 0, 320, 500]; }
}

class BasePopupMenu {
    constructor() {
        this.actor = new Actor();
        this.isOpen = false;
        this.passEvents = false;
        this._signals = new Map();
    }
    connect(signal, callback) {
        const id = nextSignalId++;
        this._signals.set(id, [signal, callback]);
        return id;
    }
    _emit(signal, ...args) {
        for (const [name, callback] of this._signals.values()) {
            if (name === signal) callback(this, ...args);
        }
    }
    open() {
        this.isOpen = true;
        this.actor.visible = true;
        this._emit("open-state-changed", true);
    }
    close() {
        this.isOpen = false;
        this.actor.visible = false;
        this._emit("open-state-changed", false);
        this._emit("menu-animated-closed");
    }
    setOrientation() {}
    _calculatePosition() { return [10, 20]; }
    destroy() {}
}

const context = {
    console,
    global: {
        logError() {},
        workspace_manager: {
            get_active_workspace() {
                return {
                    get_work_area_for_monitor() {
                        return { x: 100, width: 1400 };
                    },
                };
            },
        },
    },
    imports: {
        ui: {
            applet: { AppletPopupMenu: BasePopupMenu },
            main: {
                layoutManager: {
                    findMonitorForActor() { return { index: 0, x: 0, width: 1920 }; },
                },
            },
        },
        gi: {
            GLib: {
                PRIORITY_DEFAULT: 0,
                SOURCE_REMOVE: false,
                timeout_add(priority, delay, callback) {
                    const id = nextSignalId++;
                    timers.set(id, callback);
                    return id;
                },
                source_remove(id) { timers.delete(id); },
            },
            St: { Side: { TOP: 0, BOTTOM: 1, LEFT: 2, RIGHT: 3 } },
        },
    },
};
vm.createContext(context);
const source = fs.readFileSync(
    path.join(root, "src", "cinnamon", "popupMenu.js"), "utf8"
);
vm.runInContext(
    `${source}\nglobalThis.__CalendarPopupMenu = CalendarPopupMenu;`,
    context,
    { filename: "popupMenu.js" }
);
const CalendarPopupMenu = context.__CalendarPopupMenu;
const launcher = { locationLabel: "right", actor: {} };
const menu = new CalendarPopupMenu(launcher, context.imports.gi.St.Side.TOP);

assert.equal(menu.actor.reactive, false, "closed popup must not intercept input");
menu.open(false);
assert.equal(menu.actor.reactive, true, "opening must restore popup input");
menu.setEventPassthrough(true);
assert.equal(menu.passEvents, true, "event passthrough is allowed only while open");
menu.close(false);
assert.equal(menu.passEvents, false, "closing must revoke event passthrough");
assert.equal(menu.actor.reactive, false, "closing must revoke popup input");
assert.equal(menu.actor.visible, false, "completed close must hide the actor");
menu.setEventPassthrough(true);
assert.equal(menu.passEvents, false, "a closed popup must stay non-interactive");
assert.deepEqual(
    Array.from(menu._calculatePosition()),
    [1180, 20],
    "right-edge placement must use the active workspace work area"
);
menu.destroy();
console.log("Popup lifecycle behaviour tests passed.");
'''

TEMPORAL_POLICY_TEST = r'''#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");

function decodeTemporalPolicy(variant) {
    const row = variant.deep_unpack();
    return {
        mode: row[0], calendar: row[1], showSeconds: row[2],
        locationConfigured: row[3], latitude: row[4], longitude: row[5],
        authority: row[6], providerAvailable: row[7],
    };
}
const context = {
    console,
    global: { errors: [], logError(error) { this.errors.push(String(error)); } },
    require(name) {
        assert.equal(name, "./runtimeSupport");
        return {
            loadLocalModule(moduleName) {
                assert.equal(moduleName, "nativeBoundary");
                return { NativeBoundary: { decodeTemporalPolicy } };
            },
        };
    },
    imports: {
        ui: {},
        gi: {
            CalendarPlus: {
                CalendarSystem: {
                    new(id) { return ["gregorian", "chinese"].includes(id) ? {} : null; },
                },
            },
        },
    },
};
vm.createContext(context);
const source = fs.readFileSync(
    path.join(root, "src", "cinnamon", "temporalPolicy.js"), "utf8"
);
vm.runInContext(
    `${source}\nglobalThis.__TemporalPolicyResolver = TemporalPolicyResolver;`,
    context,
    { filename: "temporalPolicy.js" }
);
const Resolver = context.__TemporalPolicyResolver;
let reads = 0;
let row = ["standard-24", "chinese", true, true, -36.3, 145.4,
    "infiltrator-system-settings", true];
const clock = { get_system_policy() { reads += 1; return { deep_unpack: () => row }; } };
const resolver = new Resolver(clock, (mode) => mode === "roman");
let policy = resolver.get();
assert.equal(policy.mode, "standard-24");
assert.equal(policy.calendar, "chinese");
assert.equal(policy.longitude, 145.4);
assert.equal(reads, 1);
assert.equal(resolver.get(), policy, "one policy generation must be cached atomically");
assert.equal(reads, 1);
const config = resolver.clockConfig({ vertical: true, use24h: false, showDate: true });
assert.equal(config.primaryCalendar, "chinese");
assert.equal(config.vertical, true);
resolver.invalidate();
row = ["roman", "gregorian", false, true, -36.3, 145.4,
    "infiltrator-system-settings", true];
policy = resolver.get();
assert.equal(policy.mode, "roman", "native clock modes use the injected catalogue predicate");
resolver.invalidate();
row = ["standard", "gregorian", false, true, 999, 145.4,
    "infiltrator-system-settings", true];
policy = resolver.get();
assert.equal(policy.mode, "standard");
assert.equal(policy.calendar, "gregorian");
assert.equal(policy.authority, "mint-cinnamon");
assert.ok(context.global.errors.length > 0, "invalid policy must be reported before fallback");
console.log("Temporal policy behaviour tests passed.");
'''

TRANSPORT_TEST = r'''#!/usr/bin/env node
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
    rangeCalls: [], rangeCallbacks: [], disconnects: 0,
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
class Cancellable { cancel() { observations.cancels += 1; } }
function makeServer(status = 2) {
    const server = { status, signals: new Map() };
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
        observations.rangeCalls.push([start, end, force]);
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
    global: { logError() {} },
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
assert.deepEqual(observations.rangeCalls[0], [10, 20, true]);
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
console.log("CalendarServer transport behaviour tests passed.");
'''

EVENT_MANAGER_TEST = r'''#!/usr/bin/env node
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
const observations = {
    clears: 0, addCalls: [], rangeRequests: [], timezoneCancels: 0,
    timezoneDisconnects: 0, transportDestroys: 0,
};
function addSignalMethods(prototype) {
    prototype.connect = function(signal, callback) {
        if (!this.__signals) this.__signals = new Map();
        const id = nextId++;
        this.__signals.set(id, [signal, callback]);
        return id;
    };
    prototype.disconnect = function(id) { if (this.__signals) this.__signals.delete(id); };
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
class FakeDateTime {
    constructor(unix) { this.unix = unix; }
    to_unix() { return this.unix; }
    get_year() { return new Date(this.unix * 1000).getUTCFullYear(); }
    get_month() { return new Date(this.unix * 1000).getUTCMonth() + 1; }
    get_day_of_month() { return new Date(this.unix * 1000).getUTCDate(); }
    add_days(days) { return new FakeDateTime(this.unix + days * 86400); }
}
const DateTime = {
    new_local(y, m, d, h, min, sec) {
        return new FakeDateTime(Date.UTC(y, m - 1, d, h, min, sec) / 1000);
    },
    new_utc(y, m, d, h, min, sec) {
        return new FakeDateTime(Date.UTC(y, m - 1, d, h, min, sec) / 1000);
    },
    new_from_unix_local(value) { return new FakeDateTime(value); },
    new_now_local() { return new FakeDateTime(0); },
};
class TimezoneMonitor {
    connect() { return 77; }
    disconnect() { observations.timezoneDisconnects += 1; }
    cancel() { observations.timezoneCancels += 1; }
}
class FakeTransport {
    constructor() {
        this.status = 0;
        this.ready = false;
        this.requests = [];
        addSignalMethods(Object.getPrototypeOf(this));
    }
    is_ready() { return this.ready; }
    start() {}
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
    global: { logError() {} },
    require(name) {
        assert.equal(name, "./runtimeSupport");
        return {
            SignalBag,
            midnight(value) {
                return DateTime.new_local(value.get_year(), value.get_month(), value.get_day_of_month(), 0, 0, 0);
            },
            sameInstant(a, b) { return a !== null && b !== null && a.to_unix() === b.to_unix(); },
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
                throw new Error(`unexpected module ${moduleName}`);
            },
        };
    },
    imports: {
        ui: {},
        signals: { addSignalMethods },
        mainloop: {
            timeout_add_seconds(delay, callback) { const id = nextId++; timeouts.set(id, callback); return id; },
            idle_add(callback) { const id = nextId++; idles.set(id, callback); return id; },
            source_remove(id) { timeouts.delete(id); idles.delete(id); },
        },
        gi: {
            Gio: {
                Cancellable: class { cancel() {} },
                File: { new_for_path() { return { monitor_file() { return new TimezoneMonitor(); } }; } },
                FileMonitorFlags: { NONE: 0 },
            },
            GLib: {
                DateTime,
                SOURCE_REMOVE: false,
                get_monotonic_time() { return 123456; },
            },
            CalendarPlus: {
                event_day_relation() { return 0; },
                event_timing() { return { deep_unpack() { return [0, 0, 0]; } }; },
                EventStore: {
                    new() {
                        return {
                            clear() { observations.clears += 1; },
                            refresh_timezone() {},
                            add_or_update_in_range(event, timestamp, start, end) {
                                observations.addCalls.push([event, timestamp, start, end]);
                                return true;
                            },
                            get_snapshot() { return { deep_unpack() { return [1, []]; } }; },
                            get_color_range() { return { deep_unpack() { return []; } }; },
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
assert.equal(observations.rangeRequests[0][1] - observations.rangeRequests[0][0], 42 * 86400 - 1);
transport.complete(0, true);
manager.set_visible_range(first, last, false);
assert.equal(observations.rangeRequests.length, 1, "successful identical range must be stable");
manager.set_visible_range(first, last, true);
assert.equal(observations.rangeRequests.length, 2, "forced refresh must cross the transport port");
transport.complete(1, true);
manager._ingestEvents({ unpack() { return [{ opaque: true }]; } });
assert.equal(observations.addCalls.length, 1, "admitted rows must go directly to the native store boundary");
manager.set_visible_range(first, last, true);
transport.complete(2, false);
assert.equal(timeouts.size, 1, "range failure must retain bounded retry behaviour");
const retry = timeouts.values().next().value;
timeouts.clear();
retry();
assert.equal(observations.rangeRequests.length, 4);
const presented = [];
manager.emit = (signal, ...args) => presented.push([signal, ...args]);
transport.emit("unavailable", false);
assert.equal(manager._rangeState.acceptingEvents, false);
assert.ok(observations.clears > 0, "transport loss must invalidate native cached rows");
assert.ok(presented.some(([signal]) => signal === "has-calendars-changed"));
transport.status = 1;
assert.equal(manager.should_show_event_pane(), false, "authoritative empty status collapses the agenda");
manager.destroy();
manager.destroy();
assert.equal(observations.transportDestroys, 1);
assert.equal(observations.timezoneDisconnects, 1);
assert.equal(observations.timezoneCancels, 1);
console.log("EventsManager orchestration behaviour tests passed.");
'''


def patch_event_manager() -> None:
    path = "src/cinnamon/eventManager.js"
    text = read(path)
    text = replace_once(text, "const Cinnamon = imports.gi.Cinnamon;\n", "", "remove CalendarServer GI import")
    text = replace_once(
        text,
        'const STATUS_UNKNOWN = 0;\nconst STATUS_NO_CALENDARS = 1;\nconst CALENDAR_SERVER_BUS_NAME = "org.cinnamon.CalendarServer";\n',
        "",
        "remove transport constants",
    )
    marker = '''const NativeBoundary = RuntimeSupport.loadLocalModule(
    "nativeBoundary"
).NativeBoundary;
'''
    addition = marker + '''const CalendarServerTransportModule = RuntimeSupport.loadLocalModule(
    "calendarServerTransport"
);
const CalendarServerTransport = CalendarServerTransportModule.CalendarServerTransport;
const STATUS_UNKNOWN = CalendarServerTransportModule.STATUS_UNKNOWN;
const STATUS_NO_CALENDARS = CalendarServerTransportModule.STATUS_NO_CALENDARS;
'''
    text = replace_once(text, marker, addition, "load transport boundary")
    text = replace_once(text, "    constructor() {\n", "    constructor(transport = null) {\n", "inject transport")
    old = '''        this._enabled = true;
        this._destroyed = false;
        this._inited = false;
        this._bus_watch_id = 0;
        this._calendar_server = null;
        this._calendar_server_connecting = false;
        this._calendar_server_generation = 0;
        this._reconnect_timer_id = 0;
        this._range_retry_timer_id = 0;
        this._serverSignals = new SignalBag();
        this._cancellable = new Gio.Cancellable();
        this._cached_state = STATUS_UNKNOWN;
        this._reload_id = 0;
'''
    new = '''        this._enabled = true;
        this._destroyed = false;
        this._transport = transport || new CalendarServerTransport();
        this._transportSignals = new SignalBag();
        this._range_retry_timer_id = 0;
        this._timezoneCancellable = new Gio.Cancellable();
        this._reload_id = 0;
'''
    text = replace_once(text, old, new, "replace transport-owned constructor state")
    text = replace_once(
        text,
        "        this._timezone_monitor_signal_id = 0;\n        this._startTimezoneMonitor();\n",
        "        this._timezone_monitor_signal_id = 0;\n        this._wireTransport();\n        this._startTimezoneMonitor();\n",
        "wire transport",
    )
    methods = r'''
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

    start_events() {
        if (!this._destroyed && this._transport !== null) {
            this._transport.start();
        }
    }

    _transportReady() {
        if (!this._destroyed) {
            this.emit("events-manager-ready");
        }
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

    _cancelRangeRetry() {'''
    text = sub_once(
        text,
        r'\n    start_events\(\) \{.*?\n    _cancelRangeRetry\(\) \{',
        methods,
        "extract CalendarServer transport lifecycle",
    )
    text = text.replace("this._cancellable\n            );", "this._timezoneCancellable\n            );")
    if "this._cancellable" in text:
        # Remaining cancellable ownership must be removed by destroy rewrite below.
        pass
    text = replace_once(
        text,
        "                    this._calendar_server !== null &&\n",
        "                    this._transport !== null && this._transport.is_ready() &&\n",
        "retry through transport readiness",
    )
    text = sub_once(
        text,
        r'\n    _statusChanged\(\) \{.*?\n    _requestVisibleRange\(first, last, force, clearStore\) \{',
        r'''
    _statusChanged(next) {
        if (this._destroyed || next === STATUS_UNKNOWN) {
            return;
        }
        this.queue_reload(true);
        this.emit("has-calendars-changed");
    }

    _requestVisibleRange(first, last, force, clearStore) {''',
        "consume transport status signal",
    )
    request_method = r'''
    _requestVisibleRange(first, last, force, clearStore) {
        if (this._destroyed || this._transport === null ||
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
                if (this._destroyed || !currentTransport ||
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

    set_visible_range'''
    text = sub_once(
        text,
        r'\n    _requestVisibleRange\(first, last, force, clearStore\) \{.*?\n    \}\n\n    set_visible_range',
        request_method,
        "route range requests through transport",
    )
    text = sub_once(
        text,
        r'\n    should_show_event_pane\(\) \{.*?\n    is_active\(\) \{.*?\n    \}',
        r'''
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
    }''',
        "derive activity from transport port",
    )
    destroy = r'''
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
        if (this._timezoneCancellable !== null) {
            try {
                this._timezoneCancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
            this._timezoneCancellable = null;
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
Signals'''
    text = sub_once(
        text,
        r'\n    destroy\(\) \{.*?\n    \}\n\};\nSignals',
        destroy,
        "make manager teardown transport-agnostic",
    )
    forbidden = (
        "CalendarServerProxy", "bus_watch_name", "_calendar_server",
        "_calendar_server_connecting", "_calendar_server_generation",
        "_serverSignals", "_reconnect_timer_id",
    )
    for token in forbidden:
        if token in text:
            raise RuntimeError(f"eventManager transport coupling remains: {token}")
    write(path, text)


def patch_applet() -> None:
    path = "src/cinnamon/applet.js"
    text = read(path)
    old = '''const NativeBoundary = RuntimeSupport.loadLocalModule(
    "nativeBoundary"
).NativeBoundary;
/* PopupShell owns RuntimeSupport.loadLocalModule("eventView") and the smaller presentation leaf modules. */
'''
    new = '''const TemporalPolicy = RuntimeSupport.loadLocalModule("temporalPolicy");
/* PopupShell owns RuntimeSupport.loadLocalModule("eventView") and the smaller presentation leaf modules. */
'''
    text = replace_once(text, old, new, "replace direct native-boundary dependency")
    text = replace_once(text, "        this._temporalPolicyCache = null;\n", "        this._temporalPolicy = null;\n", "policy resolver state")
    text = replace_once(
        text,
        "        this.system_clock = CalendarPlus.SystemClock.new();\n",
        "        this.system_clock = CalendarPlus.SystemClock.new();\n        this._temporalPolicy = new TemporalPolicy.TemporalPolicyResolver(\n            this.system_clock,\n            PanelClock.isNativeClockMode\n        );\n",
        "construct policy resolver",
    )
    text = replace_once(
        text,
        "                this._temporalPolicyCache = null;\n                this._onSettingsChanged();\n",
        "                this._temporalPolicy.invalidate();\n                this._onSettingsChanged();\n",
        "invalidate policy generation",
    )
    text = sub_once(
        text,
        r'\n    _systemTemporalPolicy\(\) \{.*?\n    _syncCalendarSystem\(\) \{',
        r'''
    _syncCalendarSystem() {''',
        "move policy decoding out of applet",
    )
    text = text.replace("this._systemTemporalPolicy()", "this._temporalPolicy.get()")
    text = sub_once(
        text,
        r'\n    _clockConfig\(\) \{.*?\n    \}\n\n    _syncSystemClock',
        r'''
    _clockConfig() {
        return this._temporalPolicy.clockConfig({
            vertical: this._isVerticalPanel(),
            use24h: this._desktopUses24h(),
            showDate: this._desktopShowsDate(),
        });
    }

    _syncSystemClock''',
        "reduce clock configuration at policy boundary",
    )
    text = replace_once(
        text,
        "        this._temporalPolicyCache = null;\n",
        "        this._temporalPolicy = null;\n",
        "destroy policy resolver",
    )
    for token in ("NativeBoundary", "decodeTemporalPolicy", "providerAvailable: false"):
        if token in text:
            raise RuntimeError(f"applet still owns temporal policy decoding: {token}")
    write(path, text)


def patch_architecture_tests() -> None:
    path = "tests/test-settings.py"
    text = read(path)
    text = replace_once(
        text,
        '    event_manager = read("eventManager.js")\n    event_range_state = read("eventRangeState.js")\n',
        '    event_manager = read("eventManager.js")\n    event_transport = read("calendarServerTransport.js")\n    event_range_state = read("eventRangeState.js")\n',
        "load event transport test subject",
    )
    text = replace_once(
        text,
        '    native_boundary = read("nativeBoundary.js")\n    panel_clock = read("panelClock.js")\n',
        '    native_boundary = read("nativeBoundary.js")\n    temporal_policy = read("temporalPolicy.js")\n    panel_clock = read("panelClock.js")\n',
        "load temporal policy test subject",
    )
    text = replace_once(
        text,
        '''    assert {
        "calendar", "eventManager", "nativeBoundary", "panelClock", "popupShell"
    }.issubset(loaded_modules(applet))
''',
        '''    assert {
        "calendar", "eventManager", "panelClock", "popupShell", "temporalPolicy"
    }.issubset(loaded_modules(applet))
    assert "nativeBoundary" not in loaded_modules(applet)
''',
        "composition-root module contract",
    )
    anchor = '''    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "event_store")
    forbid_tokens(js_code(event_manager), "eventVariant.deep_unpack", "row[3]", "row[4]", "row[5]")
    require_tokens(event_manager, "add_or_update_in_range(", "NativeBoundary.decodeEventSnapshot")
'''
    replacement = '''    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "event_store")
    forbid_tokens(
        js_code(event_manager),
        "CalendarServerProxy", "bus_watch_name", "call_set_time_range(",
        "_calendar_server", "eventVariant.deep_unpack", "row[3]", "row[4]", "row[5]",
    )
    require_tokens(
        event_transport,
        "CalendarServerProxy", "bus_watch_name", "request_range(",
        'emit("events-added-or-updated"', 'emit("unavailable"',
    )
    require_tokens(event_manager, "CalendarServerTransport", "add_or_update_in_range(", "NativeBoundary.decodeEventSnapshot")
'''
    text = replace_once(text, anchor, replacement, "transport ownership checks")
    old = '''    require_tokens(
        applet,
        "CalendarPlus.SystemClock.new()",
        "get_system_policy()",
        "this._calendar.setCalendarSystem(temporal.calendar)",
    )
'''
    new = '''    require_tokens(
        applet,
        "CalendarPlus.SystemClock.new()",
        "TemporalPolicy.TemporalPolicyResolver",
        "this._calendar.setCalendarSystem(temporal.calendar)",
    )
    forbid_tokens(js_code(applet), "get_system_policy()", "decodeTemporalPolicy", "deep_unpack")
    require_tokens(
        temporal_policy,
        "get_system_policy()", "NativeBoundary.decodeTemporalPolicy",
        '"mint-cinnamon"', '"infiltrator-system-settings"',
    )
'''
    text = replace_once(text, old, new, "temporal policy ownership checks")
    write(path, text)


def patch_runtime_tests() -> None:
    path = "tests/test-js-runtime.js"
    text = read(path)
    old = '''testEventListCacheIdentity();
testVisibleEventRange();
testVisibleEventRangeFailureRecovery();
testSuccessfulEmptyRangeReplacesPreviousGeneration();
testRangeAdmissionDelegatesOpaqueRowsToNativeBoundary();
testRemovalSignalsInvalidateInsteadOfDeletingNewGeneration();
testTimezoneReloadPreservesCivilRange();
testCalendarSetChangeClosesAdmission();
testServerLossClearsPresentedAgenda();
testExpectedNoCalendarShutdownStaysAuthoritative();
testBatchedEventColorBridge();
testEventsManagerPresentationState();
testEventsManagerPreservesPreEpochSelection();
testEventsManagerLifecycle();
testEventsManagerReconnect();
'''
    new = '''testEventListCacheIdentity();
/* EventsManager and CalendarServer transport are exercised through injected
 * behavioural ports in test-event-manager.js and test-calendar-server-transport.js.
 * Keeping those tests separate prevents private transport layout from becoming
 * part of the presentation runtime contract again. */
'''
    text = replace_once(text, old, new, "remove private EventsManager test coupling")
    write(path, text)


def patch_makefile() -> None:
    path = "Makefile"
    text = read(path)
    text = replace_once(text, "VERSION := 1.0.87", "VERSION := 1.0.88", "version")
    old = '''\tpython3 tests/test-settings.py
\tpython3 tests/test-popup-lifecycle.py
\tnode tests/test-native-boundary.js
\tnode tests/test-js-runtime.js
'''
    new = '''\tpython3 tests/test-settings.py
\tnode tests/test-popup-menu.js
\tnode tests/test-native-boundary.js
\tnode tests/test-temporal-policy.js
\tnode tests/test-calendar-server-transport.js
\tnode tests/test-event-manager.js
\tnode tests/test-js-runtime.js
'''
    text = replace_once(text, old, new, "behavioural JS validation set")
    write(path, text)


def patch_release_metadata() -> None:
    metadata_path = ROOT / "src/cinnamon/metadata.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if metadata.get("version") != "1.0.87":
        raise RuntimeError("unexpected Calendar metadata version")
    metadata["version"] = "1.0.88"
    metadata_path.write_text(json.dumps(metadata, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")

    readme = read("README.md")
    readme = replace_once(readme, "**Stable release:** 1.0.87", "**Stable release:** 1.0.88", "README release")
    write("README.md", readme)

    changelog = read("CHANGELOG.md")
    old = "## Unreleased\n\nNo unreleased changes.\n\n## 1.0.87 - 2026-10-11\n"
    new = """## Unreleased

No unreleased changes.

## 1.0.88 - 2026-10-11

- Extract CalendarServer discovery, reconnect and SetTimeRange transport from EventsManager; event/cache semantics now consume a narrow transport port.
- Move effective temporal-policy decoding, validation and caching out of applet.js into a dedicated boundary so the applet remains a composition root.
- Replace source-layout popup assertions and private EventsManager transport tests with behavioural module tests; architecture checks now enforce ownership rather than implementation spelling.

## 1.0.87 - 2026-10-11
"""
    changelog = replace_once(changelog, old, new, "changelog 1.0.88")
    write("CHANGELOG.md", changelog)

    debian = read("debian/changelog")
    entry = """infiltrator-calendar (1.0.88) unstable; urgency=medium

  * Extract CalendarServer transport from the event/cache controller.
  * Isolate effective temporal-policy decoding from the applet composition root.
  * Replace implementation-text lifecycle tests with behavioural boundary tests.

 -- Shannon Smith <noreply@the-infiltratr.com>  Sun, 11 Oct 2026 22:45:00 +1100

"""
    if not debian.startswith("infiltrator-calendar (1.0.87)"):
        raise RuntimeError("unexpected Debian changelog head")
    write("debian/changelog", entry + debian)

    architecture = read("docs/ARCHITECTURE.md")
    old_arch = "Cinnamon event transport also isolates visible-range request generation, admission, retry and queued-force transitions in a dedicated state object so CalendarServer callbacks cannot mutate an accidental constellation of controller flags."
    new_arch = "Cinnamon event transport isolates CalendarServer discovery, reconnect and range-call mechanics behind a dedicated transport object. EventsManager consumes that narrow port and owns event/cache semantics, while visible-range request generation, admission, retry and queued-force transitions remain in a dedicated state object. CalendarServer callbacks therefore cannot mutate an accidental constellation of controller flags."
    architecture = replace_once(architecture, old_arch, new_arch, "event architecture")
    old_policy = "Live applet and desktop settings terminate at the composition root; lower presentation modules receive resolved scalar policy rather than settings objects."
    new_policy = "Live applet and desktop settings terminate at the composition root; a dedicated temporal-policy resolver decodes and validates the native effective snapshot, and lower presentation modules receive only resolved scalar policy rather than settings objects."
    architecture = replace_once(architecture, old_policy, new_policy, "policy architecture")
    write("docs/ARCHITECTURE.md", architecture)


def main() -> None:
    write("src/cinnamon/calendarServerTransport.js", CALENDAR_SERVER_TRANSPORT)
    write("src/cinnamon/temporalPolicy.js", TEMPORAL_POLICY)
    write("tests/test-popup-menu.js", POPUP_TEST)
    write("tests/test-temporal-policy.js", TEMPORAL_POLICY_TEST)
    write("tests/test-calendar-server-transport.js", TRANSPORT_TEST)
    write("tests/test-event-manager.js", EVENT_MANAGER_TEST)
    popup_old = ROOT / "tests/test-popup-lifecycle.py"
    if not popup_old.exists():
        raise RuntimeError("expected brittle popup lifecycle test is missing")
    popup_old.unlink()
    patch_event_manager()
    patch_applet()
    patch_architecture_tests()
    patch_runtime_tests()
    patch_makefile()
    patch_release_metadata()


if __name__ == "__main__":
    main()
