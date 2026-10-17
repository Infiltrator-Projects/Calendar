#!/usr/bin/env node
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
            Calendar: {
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
    "system-settings", true];
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
    "system-settings", true];
policy = resolver.get();
assert.equal(policy.mode, "roman", "native clock modes use the injected catalogue predicate");
resolver.invalidate();
row = ["standard", "gregorian", false, true, 999, 145.4,
    "system-settings", true];
policy = resolver.get();
assert.equal(policy.mode, "standard");
assert.equal(policy.calendar, "gregorian");
assert.equal(policy.authority, "mint-cinnamon");
assert.ok(context.global.errors.length > 0, "invalid policy must be reported before fallback");

const eventViewSource = fs.readFileSync(
    path.join(root, "src", "cinnamon", "eventView.js"), "utf8"
);
const appletSource = fs.readFileSync(
    path.join(root, "src", "cinnamon", "applet.js"), "utf8"
);
assert.match(
    eventViewSource,
    /format_date_part\([\s\S]*NativeCalendar\.DatePart\.FULL/,
    "agenda selected-date header must format non-Gregorian dates through the active provider"
);
assert.match(
    eventViewSource,
    /set_calendar_system\(calendarSystem\)/,
    "agenda view must accept the active calendar system from the composition root"
);
assert.match(
    appletSource,
    /new PopupShell\.EventList\(\s*this\._desktopUses24h\(\),\s*this\._calendar_system\s*\)/,
    "agenda must receive the initial active calendar system"
);
assert.match(
    appletSource,
    /this\.event_list\.set_calendar_system\(this\._calendar_system\)/,
    "calendar policy changes must refresh the agenda header calendar system"
);
console.log("Temporal policy behaviour tests passed.");
