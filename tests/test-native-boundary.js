#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(
    path.join(__dirname, "..", "src", "cinnamon", "nativeBoundary.js"),
    "utf8"
);
const context = {};
vm.createContext(context);
vm.runInContext(source, context, { filename: "nativeBoundary.js" });
const boundary = context.NativeBoundary;
assert.ok(boundary);
const variant = (value) => ({ deep_unpack: () => value });

const policy = boundary.decodeTemporalPolicy(variant([
    "standard-24", "gregorian", true, false, -36.3, 145.4,
    "system-settings", true,
]));
assert.equal(policy.mode, "standard-24");
assert.equal(policy.calendar, "gregorian");
assert.equal(policy.longitude, 145.4);

const civil = boundary.decodeCivilDate(variant([2026, 10, 11]));
assert.equal(civil.year, 2026);
assert.equal(civil.month, 10);
assert.equal(civil.day, 11);

const grid = boundary.decodeCalendarGrid(variant([[
    "11", 2026, 10, 11, 2, 4, 41,
    true, true, false, true, false, false,
]]));
assert.equal(grid.length, 1);
assert.equal(grid[0].day, 11);
assert.equal(grid[0].column, 4);
assert.equal(grid[0].isToday, true);

const timing = boundary.decodeEventTiming(variant([2, 45, -15]));
assert.equal(timing.state, 2);
assert.equal(timing.secondsUntilStart, 45);
assert.equal(timing.secondsUntilEnd, -15);

const snapshot = boundary.decodeEventSnapshot(variant([
    7,
    [["id", "#112233", "summary", false, false, 1, 2, 0, 0, 3]],
]));
assert.equal(snapshot.revision, 7);
assert.equal(snapshot.events[0].summary, "summary");
assert.equal(snapshot.events[0].endUnix, 2);

assert.throws(() => boundary.decodeTemporalPolicy(variant(["short"])));
assert.throws(() => boundary.decodeCalendarGrid(variant([["short"]])));
assert.throws(() => boundary.decodeEventTiming(variant([1, 2])));
assert.throws(() => boundary.decodeEventSnapshot(null));
