#!/usr/bin/gjs
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

"use strict";

imports.gi.versions.Calendar = "1.0";
const Calendar = imports.gi.Calendar;
const GLib = imports.gi.GLib;
const expectedVersion = GLib.getenv("CALENDAR_EXPECTED_VERSION");
const nativeCalendarResources = ["hebrew", "gregorian", "persian", "coptic"];

// Installed filenames are canonical names, not provider IDs.
function installedModuleCount(directory) {
    if (!GLib.file_test(directory, GLib.FileTest.IS_DIR))
        return 0;
    const dir = GLib.Dir.open(directory, 0);
    let count = 0;
    for (let name = dir.read_name(); name !== null; name = dir.read_name()) {
        if (name.endsWith(".so"))
            count++;
    }
    dir.close();
    return count;
}
const installedCalendars = installedModuleCount("/System/Resources/Calendars");
const installedClocks = installedModuleCount("/System/Resources/Clocks");
requireCondition(installedCalendars === 0 || installedCalendars === 30,
                 "incomplete installed calendar resource set");
requireCondition(installedClocks === 0 || installedClocks === 23,
                 "incomplete installed clock resource set");

function requireCondition(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

requireCondition(
    expectedVersion !== null && Calendar.get_version() === expectedVersion,
    `native version mismatch: ${Calendar.get_version()}`
);
requireCondition(
    Calendar.get_source_id() === `calendar-${expectedVersion}`,
    `unexpected native source id: ${Calendar.get_source_id()}`
);
const buildProfile = Calendar.get_build_profile();
const buildLabel = Calendar.get_build_profile_label();
requireCondition(
    buildProfile === "generic" || buildProfile === "native",
    `unexpected build profile: ${buildProfile}`
);
requireCondition(
    typeof buildLabel === "string" && buildLabel.length > 0,
    "canonical build-profile label is empty"
);

const calendarIds = [
    "iso-week", "julian", "revised-julian",
    "islamic-umalqura", "islamic-civil", "islamic-tbla", "islamic",
    "bahai", "buddhist", "ethiopian", "ethiopic-amete-alem", "chinese",
    "dangi", "indian", "japanese", "minguo", "roman", "byzantine",
    "egyptian-nabonassar", "armenian-traditional", "mayan",
    "french-republican", "swedish-historical", "international-fixed",
    "world", "positivist",
];
for (const id of calendarIds) {
    nativeCalendarResources.push(id);
}

/*
 * Every migrated chronology is resource-only. Installed packages must load the
 * canonical module, while a build-tree smoke on a host without that file must
 * observe no compiled fallback for the same stable ID.
 */
for (const id of nativeCalendarResources) {
    const installed = id === "gregorian" || installedCalendars === 30;
    const provider = Calendar.CalendarSystem.new(id);
    if (installed) {
        requireCondition(
            provider !== null,
            `installed ${id} calendar resource is unavailable`
        );
        requireCondition(
            provider.get_id() === id,
            `${id} calendar provider id mismatch`
        );
        requireCondition(
            provider.format_date(2026, 9, 12, "short").length > 0,
            `${id} calendar resource failed to format`
        );
    } else {
        requireCondition(
            provider === null,
            `${id} calendar remained available without its native resource`
        );
    }
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
        (Calendar.time_mode_from_string(id) !== Calendar.TimeMode.INVALID) ===
            (installedClocks === 23),
        `time provider unavailable: ${id}`
    );
}

const historicalClock = Calendar.SystemClock.new();
historicalClock.start_at_location(
    "roman-temporal", false, false, 0.0, 0.0
);
const romanInstalled = installedClocks === 23;
requireCondition(historicalClock.is_running() === romanInstalled,
                 "historical clock availability disagrees with its resource");
requireCondition(!romanInstalled || historicalClock.get_time().length > 0,
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
        effectiveAuthority === "system-settings",
    "effective temporal authority identifier is invalid"
);
requireCondition(
    effectiveAuthority !== "system-settings" || providerAvailable,
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

requireCondition(
    Calendar.date_same(2026, 8, 8, 2026, 8, 8),
    "native date equality failed"
);
requireCondition(
    !Calendar.date_is_work_day(2026, 8, 8),
    "Saturday was classified as a work day"
);
requireCondition(
    Calendar.date_weekday(2026, 8, 8) === 6,
    "native weekday projection failed"
);

const calendar = Calendar.CalendarSystem.new("gregorian");
const gregorianInstalled = true; // Normal host capability, with or without modules.
requireCondition(
    (calendar !== null) === gregorianInstalled,
    "Gregorian availability does not match its installed resource"
);
if (calendar !== null) {
    requireCondition(calendar.get_id() === "gregorian", "wrong calendar id");
    const [nextDayYear, nextDayMonth, nextDay] = calendar
        .add_days_parts(2026, 8, 8, 1)
        .deep_unpack();
    requireCondition(
        nextDayYear === 2026 && nextDayMonth === 8 && nextDay === 9,
        "native civil-day navigation failed"
    );
    requireCondition(
        calendar.add_days_parts(2147483647, 12, 31, 1) === null,
        "native civil-day navigation crossed the gint year boundary"
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
}

requireCondition(
    Calendar.event_state(100, 200, 150) ===
        Calendar.EventState.PRESENT,
    "event state contract failed"
);
const [state, secondsToStart, secondsToFinish] =
    Calendar.event_timing(100, 200, 150).deep_unpack();
requireCondition(state === Calendar.EventState.PRESENT,
                 "event timing state failed");
requireCondition(secondsToStart === -50 && secondsToFinish === 50,
                 "event timing offsets failed");

const removalStore = Calendar.EventStore.new();
const retainedEvent = new GLib.Variant(
    "(sssbxxx)",
    ["keep", "#112233", "Keep", false, 100, 200, 1]
);
requireCondition(
    removalStore.add_or_update(retainedEvent, 1),
    "event-store removal regression fixture was not indexed"
);
const oversizedRemovalId = "x".repeat(4097);
requireCondition(
    !removalStore.remove(`keep::${oversizedRemovalId}`),
    "oversized removal payload was not rejected atomically"
);
requireCondition(
    removalStore.remove("keep"),
    "malformed removal payload mutated an earlier valid identifier"
);

print("Calendar native typelib contract passed.");
