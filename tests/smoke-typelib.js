#!/usr/bin/gjs
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

"use strict";

imports.gi.versions.CalendarPlus = "1.0";
const CalendarPlus = imports.gi.CalendarPlus;
const GLib = imports.gi.GLib;
const expectedVersion = GLib.getenv("CALENDAR_PLUS_EXPECTED_VERSION");

function requireCondition(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

requireCondition(
    expectedVersion !== null && CalendarPlus.get_version() === expectedVersion,
    `native version mismatch: ${CalendarPlus.get_version()}`
);
requireCondition(
    CalendarPlus.get_source_id() === `calendar-plus-${expectedVersion}`,
    `unexpected native source id: ${CalendarPlus.get_source_id()}`
);
const buildProfile = CalendarPlus.get_build_profile();
const buildLabel = CalendarPlus.get_build_profile_label();
requireCondition(
    buildProfile === "generic" || buildProfile === "native",
    `unexpected build profile: ${buildProfile}`
);
requireCondition(
    typeof buildLabel === "string" && buildLabel.length > 0,
    "canonical build-profile label is empty"
);


const calendarIds = [
    "gregorian", "iso-week", "julian", "revised-julian", "hebrew",
    "islamic-umalqura", "islamic-civil", "islamic-tbla", "islamic",
    "persian", "bahai", "buddhist", "coptic", "ethiopian",
    "ethiopic-amete-alem", "chinese", "dangi", "indian", "japanese",
    "minguo", "roman", "byzantine", "egyptian-nabonassar",
    "armenian-traditional", "mayan", "french-republican",
    "swedish-historical", "international-fixed", "world", "positivist",
];
for (const id of calendarIds) {
    const provider = CalendarPlus.CalendarSystem.new(id);
    requireCondition(provider !== null, `calendar provider unavailable: ${id}`);
    requireCondition(provider.get_id() === id, `calendar provider id mismatch: ${id}`);
    requireCondition(
        provider.format_date(2026, 8, 11, "short").length > 0,
        `calendar provider failed to format: ${id}`
    );
}

const timeModeIds = [
    "decimal", "internet", "unix", "hexadecimal",
    "binary", "sidereal", "solar", "julian", "mean-solar",
    "modified-julian", "chinese-time", "roman-temporal",
    "japanese-temporal", "italian-hours", "babylonian-hours",
    "indian-ghati", "chinese-ke", "nuremberg-hours",
    "babylonian-ancient", "nuremberg-solar",
];
for (const id of timeModeIds) {
    requireCondition(
        CalendarPlus.time_mode_from_string(id) !== CalendarPlus.TimeMode.INVALID,
        `time provider unavailable: ${id}`
    );
}

const historicalClock = CalendarPlus.SystemClock.new();
historicalClock.start_at_location(
    "roman-temporal", false, false, 0.0, 0.0
);
requireCondition(historicalClock.is_running(),
                 "historical clock did not start");
requireCondition(historicalClock.get_time().length > 0,
                 "historical clock produced no text");
historicalClock.stop();

const effectivePolicy = historicalClock.get_system_policy().deep_unpack();
requireCondition(
    effectivePolicy.length === 8,
    "effective temporal policy snapshot has the wrong shape"
);
const [
    effectiveMode,
    effectiveCalendar,
    effectiveSeconds,
    effectiveLocationConfigured,
    effectiveLatitude,
    effectiveLongitude,
    effectiveAuthority,
    providerAvailable,
] = effectivePolicy;
requireCondition(
    effectiveMode.length > 0 && effectiveCalendar.length > 0,
    "effective temporal policy identifiers are empty"
);
requireCondition(
    typeof effectiveSeconds === "boolean" &&
        typeof effectiveLocationConfigured === "boolean" &&
        Number.isFinite(effectiveLatitude) &&
        Number.isFinite(effectiveLongitude),
    "effective temporal policy has invalid scalar fields"
);
requireCondition(
    effectiveAuthority === "mint-cinnamon" ||
        effectiveAuthority === "infiltrator-system-settings",
    "effective temporal authority identifier is invalid"
);
requireCondition(
    effectiveAuthority !== "infiltrator-system-settings" || providerAvailable,
    "Infiltrator authority cannot be active without its provider"
);
if (!providerAvailable) {
    requireCondition(
        effectiveMode === "standard" && effectiveCalendar === "gregorian",
        "provider absence must resolve to conventional Mint/Gregorian policy"
    );
}
requireCondition(
    historicalClock.get_system_mode() === effectiveMode,
    "system-mode compatibility getter disagrees with policy snapshot"
);
requireCondition(
    historicalClock.get_system_calendar() === effectiveCalendar,
    "system-calendar compatibility getter disagrees with policy snapshot"
);
requireCondition(
    historicalClock.get_system_show_seconds() === effectiveSeconds,
    "system-seconds compatibility getter disagrees with policy snapshot"
);
requireCondition(
    historicalClock.get_system_location_configured() ===
        effectiveLocationConfigured,
    "system-location compatibility getter disagrees with policy snapshot"
);
requireCondition(
    historicalClock.get_system_latitude() === effectiveLatitude &&
        historicalClock.get_system_longitude() === effectiveLongitude,
    "system-coordinate compatibility getters disagree with policy snapshot"
);

const calendar = CalendarPlus.CalendarSystem.new("gregorian");
requireCondition(calendar !== null, "Gregorian CalendarSystem unavailable");
requireCondition(calendar.get_id() === "gregorian", "wrong calendar id");
requireCondition(
    CalendarPlus.date_same(2026, 8, 8, 2026, 8, 8),
    "native date equality failed"
);
requireCondition(
    !CalendarPlus.date_is_work_day(2026, 8, 8),
    "Saturday was classified as a work day"
);

const [year, month, day] = calendar
    .add_months_parts(2026, 8, 8, 1)
    .deep_unpack();
requireCondition(
    year === 2026 && month === 9 && day === 8,
    "typed calendar navigation failed"
);
const [startYear, startMonth, startDay] = calendar
    .month_start_parts(2026, 8, 8)
    .deep_unpack();
requireCondition(
    startYear === 2026 && startMonth === 8 && startDay === 1,
    "typed calendar period-start navigation failed"
);
const [nextYear, nextYearMonth, nextYearDay] = calendar
    .add_years_parts(2026, 8, 8, 1)
    .deep_unpack();
requireCondition(
    nextYear === 2027 && nextYearMonth === 8 && nextYearDay === 8,
    "typed calendar year navigation failed"
);

const grid = calendar
    .build_grid(2026, 8, 8, 2026, 8, 8, 1)
    .deep_unpack();
requireCondition(grid.length === 42, "calendar grid is not 42 cells");
requireCondition(
    grid.filter((cell) => cell[9]).length === 1,
    "calendar grid does not contain exactly one selected cell"
);

requireCondition(
    CalendarPlus.event_state(100, 200, 150) ===
        CalendarPlus.EventState.PRESENT,
    "event state contract failed"
);
const [state, secondsToStart, secondsToFinish] =
    CalendarPlus.event_timing(100, 200, 150).deep_unpack();
requireCondition(state === CalendarPlus.EventState.PRESENT,
                 "event timing state failed");
requireCondition(secondsToStart === -50 && secondsToFinish === 50,
                 "event timing offsets failed");

print("Calendar native typelib contract passed.");
