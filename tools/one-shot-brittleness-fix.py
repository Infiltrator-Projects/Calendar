#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot guarded refactor for Calendar's remaining brittle boundaries."""

from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise SystemExit(
            f"{path}: expected one occurrence, found {count}: {old[:100]!r}"
        )
    write(path, text.replace(old, new, 1))


def replace_all(path: str, old: str, new: str, expected: int | None = None) -> int:
    text = read(path)
    count = text.count(old)
    if count == 0 or (expected is not None and count != expected):
        raise SystemExit(
            f"{path}: unexpected occurrence count {count} for {old[:100]!r}"
        )
    write(path, text.replace(old, new))
    return count


# 1. Isolate range request state from CalendarServer transport.
write(
    "src/cinnamon/eventRangeState.js",
    """// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * CalendarServer visible-range request state.
 *
 * This object deliberately contains no D-Bus, event-store or presentation
 * code. It makes request-generation, admission and retry transitions atomic
 * so EventsManager cannot accidentally update one flag without the others.
 */

var EventRangeState = class EventRangeState {
    constructor() {
        this._generation = 0;
        this._pending = false;
        this._succeeded = false;
        this._acceptingEvents = false;
        this._queuedForce = false;
        this._retryAttempt = 0;
    }

    get generation() { return this._generation; }
    get pending() { return this._pending; }
    get succeeded() { return this._succeeded; }
    get acceptingEvents() { return this._acceptingEvents; }
    get queuedForce() { return this._queuedForce; }

    beginRequest() {
        if (this._pending) {
            return 0;
        }
        this._generation += 1;
        this._pending = true;
        this._succeeded = false;
        this._acceptingEvents = false;
        return this._generation;
    }

    isCurrent(generation) {
        return generation === this._generation;
    }

    finishRequest(generation, succeeded) {
        if (!this.isCurrent(generation)) {
            return false;
        }
        this._pending = false;
        this._succeeded = Boolean(succeeded);
        this._acceptingEvents = false;
        return true;
    }

    acceptCompletedRequest() {
        this._acceptingEvents = !this._pending && this._succeeded;
        return this._acceptingEvents;
    }

    closeAdmission() {
        this._acceptingEvents = false;
    }

    invalidate() {
        this._acceptingEvents = false;
        this._succeeded = false;
        return this._pending;
    }

    needsRetry() {
        return !this._pending && !this._succeeded;
    }

    queueForce(force = true) {
        this._queuedForce = this._queuedForce || Boolean(force);
    }

    consumeQueuedForce() {
        const queued = this._queuedForce;
        this._queuedForce = false;
        return queued;
    }

    clearQueuedForce() {
        this._queuedForce = false;
    }

    nextRetryDelay() {
        const delay = Math.min(60, Math.pow(2, Math.min(this._retryAttempt, 5)));
        this._retryAttempt += 1;
        return delay;
    }

    resetRetry() {
        this._retryAttempt = 0;
    }

    resetForServerLoss() {
        this._generation += 1;
        this._pending = false;
        this._succeeded = false;
        this._acceptingEvents = false;
        this._queuedForce = false;
        this._retryAttempt = 0;
    }

    destroy() {
        this.resetForServerLoss();
    }
};
""",
)

replace_once(
    "src/cinnamon/eventManager.js",
    "const sameInstant = RuntimeSupport.sameInstant;\n",
    "const sameInstant = RuntimeSupport.sameInstant;\n"
    "const EventRangeState = RuntimeSupport.loadLocalModule(\n"
    "    \"eventRangeState\"\n"
    ").EventRangeState;\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        this._range_retry_timer_id = 0;\n"
    "        this._range_retry_attempt = 0;\n",
    "        this._range_retry_timer_id = 0;\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        this._range_request_generation = 0;\n"
    "        this._range_request_pending = false;\n"
    "        this._range_request_succeeded = false;\n"
    "        this._range_accepting_events = false;\n"
    "        this._queued_range_force = false;\n",
    "        this._rangeState = new EventRangeState();\n",
)
replace_all(
    "src/cinnamon/eventManager.js",
    "        this._range_request_generation += 1;\n"
    "        this._range_request_pending = false;\n"
    "        this._range_request_succeeded = false;\n"
    "        this._range_accepting_events = false;\n"
    "        this._queued_range_force = false;\n",
    "        this._rangeState.resetForServerLoss();\n",
    expected=2,
)

for old, new in (
    ("this._range_request_pending", "this._rangeState.pending"),
    ("this._range_request_succeeded", "this._rangeState.succeeded"),
    ("this._range_accepting_events", "this._rangeState.acceptingEvents"),
    ("this._queued_range_force", "this._rangeState.queuedForce"),
    ("this._range_request_generation", "this._rangeState.generation"),
):
    replace_all("src/cinnamon/eventManager.js", old, new)
replace_all(
    "src/cinnamon/eventManager.js",
    "this._range_retry_attempt = 0;",
    "this._rangeState.resetRetry();",
)

replace_once(
    "src/cinnamon/eventManager.js",
    "        const delay = Math.min(\n"
    "            60,\n"
    "            Math.pow(2, Math.min(this._range_retry_attempt, 5))\n"
    "        );\n"
    "        this._range_retry_attempt += 1;\n",
    "        const delay = this._rangeState.nextRetryDelay();\n",
)
replace_all(
    "src/cinnamon/eventManager.js",
    "this._rangeState.acceptingEvents = false;",
    "this._rangeState.closeAdmission();",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        this._rangeState.closeAdmission();\n"
    "        this._rangeState.succeeded = false;\n"
    "        if (clearStore) {\n",
    "        const requestPending = this._rangeState.invalidate();\n"
    "        if (clearStore) {\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        if (this._rangeState.pending) {\n"
    "            this._rangeState.queuedForce = true;\n"
    "        } else if (this.current_range_start !== null &&\n",
    "        if (requestPending) {\n"
    "            this._rangeState.queueForce(true);\n"
    "        } else if (this.current_range_start !== null &&\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        const requestGeneration = ++this._rangeState.generation;\n"
    "        this._rangeState.pending = true;\n"
    "        this._rangeState.succeeded = false;\n",
    "        const requestGeneration = this._rangeState.beginRequest();\n"
    "        if (requestGeneration === 0) {\n"
    "            return;\n"
    "        }\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "                        requestGeneration === this._rangeState.generation) {\n",
    "                        this._rangeState.isCurrent(requestGeneration)) {\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "                    requestGeneration !== this._rangeState.generation) {\n",
    "                    !this._rangeState.isCurrent(requestGeneration)) {\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "                this._rangeState.pending = false;\n"
    "                this._rangeState.succeeded = succeeded;\n\n",
    "                if (!this._rangeState.finishRequest(\n"
    "                        requestGeneration, succeeded)) {\n"
    "                    return;\n"
    "                }\n\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "                if (desiredChanged || this._rangeState.queuedForce) {\n"
    "                    const queuedForce = this._rangeState.queuedForce;\n"
    "                    this._rangeState.queuedForce = false;\n",
    "                if (desiredChanged || this._rangeState.queuedForce) {\n"
    "                    const queuedForce =\n"
    "                        this._rangeState.consumeQueuedForce();\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "                    this._rangeState.acceptingEvents = true;\n",
    "                    this._rangeState.acceptCompletedRequest();\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "            this._rangeState.queuedForce =\n"
    "                this._rangeState.queuedForce || Boolean(force) || changed;\n",
    "            this._rangeState.queueForce(Boolean(force) || changed);\n",
)
replace_once(
    "src/cinnamon/eventManager.js",
    "        this._rangeState.queuedForce = false;\n"
    "        /*\n"
    "         * Every actual CalendarServer request owns a fresh local generation.\n",
    "        this._rangeState.clearQueuedForce();\n"
    "        /*\n"
    "         * Every actual CalendarServer request owns a fresh local generation.\n",
)
# The second resetForServerLoss is the destroy path; give it the stronger semantic name.
replace_once(
    "src/cinnamon/eventManager.js",
    "        this._destroyed = true;\n"
    "        this._inited = false;\n"
    "        this._calendar_server_generation += 1;\n"
    "        this._rangeState.resetForServerLoss();\n",
    "        this._destroyed = true;\n"
    "        this._inited = false;\n"
    "        this._calendar_server_generation += 1;\n"
    "        this._rangeState.destroy();\n",
)

# Opaque revisions are identity tokens, not timestamps.
replace_once(
    "src/cinnamon/eventManager.js",
    "        this.timestamp = revision;\n",
    "        this.revision = revision;\n",
)
replace_once(
    "src/cinnamon/eventView.js",
    "${snapshot.timestamp}:",
    "${snapshot.revision}:",
)

# 2. Native civil-day arithmetic and weekday policy.
replace_once(
    "src/core/calendar-core.c",
    "#include <infiltratr/temporal.h>\n",
    "#include <infiltratr/arithmetic.h>\n#include <infiltratr/temporal.h>\n",
)
replace_once(
    "src/core/calendar-core.c",
    "    return calendar_plus_date_is_valid(&left) &&\n"
    "           calendar_plus_date_is_valid(&right) &&\n"
    "           year_a == year_b && month_a == month_b && day_a == day_b;\n"
    "}\n\n",
    "    return calendar_plus_date_is_valid(&left) &&\n"
    "           calendar_plus_date_is_valid(&right) &&\n"
    "           year_a == year_b && month_a == month_b && day_a == day_b;\n"
    "}\n\n"
    "gint\n"
    "calendar_plus_date_weekday(gint year,\n"
    "                           gint month,\n"
    "                           gint day)\n"
    "{\n"
    "    const CalendarPlusDate date = { year, month, day };\n\n"
    "    if (!calendar_plus_date_is_valid(&date))\n"
    "        return -1;\n\n"
    "    return calendar_plus_iso_weekday(\n"
    "        calendar_plus_gregorian_to_jdn(year, month, day)) % 7;\n"
    "}\n\n"
    "gboolean\n"
    "calendar_plus_date_add_days(const CalendarPlusDate *date,\n"
    "                            gint amount,\n"
    "                            CalendarPlusDate *result)\n"
    "{\n"
    "    gint64 jdn;\n"
    "    gint64 shifted;\n\n"
    "    return result != NULL &&\n"
    "           date_to_jdn(date, &jdn) &&\n"
    "           infiltratr_i64_add_checked(jdn, (gint64)amount, &shifted) &&\n"
    "           date_from_jdn(shifted, result);\n"
    "}\n\n",
)
replace_once(
    "src/core/calendar-core.h",
    "/**\n * calendar_plus_date_is_work_day:\n",
    "/** Returns: Sunday=0 through Saturday=6, or -1 for invalid input. */\n"
    "gint calendar_plus_date_weekday(gint year, gint month, gint day);\n"
    "/** Adds signed Gregorian civil days without passing through local time. */\n"
    "gboolean calendar_plus_date_add_days(const CalendarPlusDate *date,\n"
    "                                     gint amount,\n"
    "                                     CalendarPlusDate *result);\n"
    "/**\n * calendar_plus_date_is_work_day:\n",
)
replace_once(
    "src/adapters/calendar-gvariant-adapter.c",
    "static gboolean\nperiod_start_operation(",
    "static gboolean\n"
    "add_days_operation(const CalendarPlusCalendarEngine *engine,\n"
    "                   const CalendarPlusDate *date,\n"
    "                   gint amount,\n"
    "                   CalendarPlusDate *result)\n"
    "{\n"
    "    (void)engine;\n"
    "    return calendar_plus_date_add_days(date, amount, result);\n"
    "}\n\n"
    "static gboolean\nperiod_start_operation(",
)
replace_once(
    "src/adapters/calendar-gvariant-adapter.c",
    "GVariant *\ncalendar_plus_calendar_system_add_months_parts(\n",
    "GVariant *\n"
    "calendar_plus_calendar_system_add_days_parts(\n"
    "    CalendarPlusCalendarSystem *self,\n"
    "    gint gregorian_year,\n"
    "    gint gregorian_month,\n"
    "    gint gregorian_day,\n"
    "    gint amount)\n"
    "{\n"
    "    return navigate_to_variant(self,\n"
    "                               gregorian_year,\n"
    "                               gregorian_month,\n"
    "                               gregorian_day,\n"
    "                               amount,\n"
    "                               add_days_operation);\n"
    "}\n\n"
    "GVariant *\ncalendar_plus_calendar_system_add_months_parts(\n",
)
replace_once(
    "src/adapters/calendar-system.h",
    "/**\n * calendar_plus_date_is_work_day:\n",
    "/**\n"
    " * calendar_plus_date_weekday:\n"
    " * @year: proleptic Gregorian year\n"
    " * @month: Gregorian month\n"
    " * @day: Gregorian day\n"
    " *\n"
    " * Returns: Sunday=0 through Saturday=6, or -1 for invalid input\n"
    " */\n"
    "gint calendar_plus_date_weekday(gint year, gint month, gint day);\n\n"
    "/**\n * calendar_plus_date_is_work_day:\n",
)
replace_once(
    "src/adapters/calendar-system.h",
    "/**\n * calendar_plus_calendar_system_add_months_parts:\n",
    "/**\n"
    " * calendar_plus_calendar_system_add_days_parts:\n"
    " * @self: a calendar converter\n"
    " * @gregorian_year: proleptic Gregorian year\n"
    " * @gregorian_month: Gregorian month from 1 through 12\n"
    " * @gregorian_day: Gregorian day of month\n"
    " * @amount: signed number of absolute Gregorian civil days\n"
    " *\n"
    " * Returns: (transfer full) (nullable): an `(iii)` Gregorian year/month/day\n"
    " */\n"
    "GVariant *calendar_plus_calendar_system_add_days_parts(\n"
    "    CalendarPlusCalendarSystem *self,\n"
    "    gint gregorian_year,\n"
    "    gint gregorian_month,\n"
    "    gint gregorian_day,\n"
    "    gint amount);\n\n"
    "/**\n * calendar_plus_calendar_system_add_months_parts:\n",
)

calendar = read("src/cinnamon/calendar.js")
calendar, count = re.subn(
    r"function _gregorianWeekday\(year, month, day\) \{.*?\n\}\n\n",
    "",
    calendar,
    count=1,
    flags=re.S,
)
if count != 1:
    raise SystemExit("calendar.js: failed to remove _gregorianWeekday")
old_day = (
    "    getDay() { return _gregorianWeekday(this._year, this._month, this._day); }"
)
if old_day not in calendar:
    raise SystemExit("calendar.js: CivilDate weekday contract changed")
calendar = calendar.replace(
    old_day,
    "    getDay() { return CalendarPlus.date_weekday(this._year, this._month, this._day); }",
    1,
)
calendar, count = re.subn(
    r"function _addCivilDays\(date, delta\) \{.*?\n\}\n\nfunction _representableLocalDate",
    "function _representableLocalDate",
    calendar,
    count=1,
    flags=re.S,
)
if count != 1:
    raise SystemExit("calendar.js: failed to remove _addCivilDays")
old = """    _dateByDays(date, delta) {
        /*
         * Day-key navigation is pure civil arithmetic. It therefore works for
         * timezone-skipped dates and for the full native gint year domain
         * without passing through JavaScript Date milliseconds.
         */
        return _addCivilDays(date, delta);
    }
"""
new = """    _dateByDays(date, delta) {
        /* Native civil arithmetic preserves skipped local dates and gint bounds. */
        if (!Number.isInteger(delta) || !this._calendarSystem) {
            return null;
        }
        return _localDateFromVariant(
            this._calendarSystem.add_days_parts(..._dateFields(date), delta)
        );
    }
"""
if old not in calendar:
    raise SystemExit("calendar.js: _dateByDays contract changed unexpectedly")
calendar = calendar.replace(old, new, 1)
write("src/cinnamon/calendar.js", calendar)

# ABI contract additions.
replace_once(
    "src/abi/calendar-plus.map",
    "CALENDAR_PLUS_2.7 {\n"
    "    global:\n"
    "        calendar_plus_event_store_get_color_range;\n"
    "} CALENDAR_PLUS_2.6;\n",
    "CALENDAR_PLUS_2.7 {\n"
    "    global:\n"
    "        calendar_plus_event_store_get_color_range;\n"
    "} CALENDAR_PLUS_2.6;\n\n"
    "CALENDAR_PLUS_2.8 {\n"
    "    global:\n"
    "        calendar_plus_calendar_system_add_days_parts;\n"
    "        calendar_plus_date_add_days;\n"
    "        calendar_plus_date_weekday;\n"
    "} CALENDAR_PLUS_2.7;\n",
)
baseline = read("tests/abi-baseline.txt").splitlines()
header = baseline[0]
symbols = set(baseline[1:])
symbols.update(
    {
        "calendar_plus_calendar_system_add_days_parts@CALENDAR_PLUS_2.8",
        "calendar_plus_date_add_days@CALENDAR_PLUS_2.8",
        "calendar_plus_date_weekday@CALENDAR_PLUS_2.8",
    }
)
write("tests/abi-baseline.txt", header + "\n" + "\n".join(sorted(symbols)) + "\n")

# Runtime harness loads the actual extracted state object.
replace_once(
    "tests/test-js-runtime.js",
    "                return {\n                    midnight(value) {\n",
    "                return {\n"
    "                    loadLocalModule(moduleName) {\n"
    "                        if (moduleName === \"eventRangeState\") {\n"
    "                            return context.__eventRangeStateModule;\n"
    "                        }\n"
    "                        throw new Error(`unexpected local module ${moduleName}`);\n"
    "                    },\n"
    "                    midnight(value) {\n",
)
replace_once(
    "tests/test-js-runtime.js",
    "    vm.createContext(context);\n"
    "    const source = fs.readFileSync(\n"
    "        path.join(root, \"src\", \"cinnamon\", \"eventManager.js\"),\n",
    "    vm.createContext(context);\n"
    "    const rangeStateSource = fs.readFileSync(\n"
    "        path.join(root, \"src\", \"cinnamon\", \"eventRangeState.js\"),\n"
    "        \"utf8\"\n"
    "    );\n"
    "    vm.runInContext(\n"
    "        `${rangeStateSource}\\nglobalThis.__EventRangeState = EventRangeState;`,\n"
    "        context,\n"
    "        { filename: \"eventRangeState.js\" }\n"
    "    );\n"
    "    context.__eventRangeStateModule = {\n"
    "        EventRangeState: context.__EventRangeState,\n"
    "    };\n"
    "    const source = fs.readFileSync(\n"
    "        path.join(root, \"src\", \"cinnamon\", \"eventManager.js\"),\n",
)
replace_all(
    "tests/test-js-runtime.js",
    "manager._range_accepting_events = true;",
    "manager._rangeState.finishRequest(manager._rangeState.beginRequest(), true);\n"
    "    manager._rangeState.acceptCompletedRequest();",
    expected=4,
)
for old, new in (
    ("manager._range_accepting_events", "manager._rangeState.acceptingEvents"),
    ("manager._range_request_succeeded", "manager._rangeState.succeeded"),
    ("manager._queued_range_force", "manager._rangeState.queuedForce"),
):
    replace_all("tests/test-js-runtime.js", old, new)
replace_once(
    "tests/test-js-runtime.js",
    "manager._range_request_pending = true;",
    "manager._rangeState.beginRequest();",
)

# Calendar harness: remove tests of the deleted JS algorithm and verify delegation.
whole = read("tests/test-js-runtime.js")
marker = "function evaluateCalendar() {"
if marker not in whole:
    raise SystemExit("test-js-runtime.js: evaluateCalendar missing")
before, calendar_tests = whole.split(marker, 1)
old_date_same = (
    "                    date_same(yearA, monthA, dayA, yearB, monthB, dayB) {"
)
if old_date_same not in calendar_tests:
    raise SystemExit("test-js-runtime.js: CalendarPlus date stub changed")
calendar_tests = calendar_tests.replace(
    old_date_same,
    "                    date_weekday(year, month, day) {\n"
    "                        const value = new Date(0);\n"
    "                        value.setUTCHours(12, 0, 0, 0);\n"
    "                        value.setUTCFullYear(year, month - 1, day);\n"
    "                        return value.getUTCDay();\n"
    "                    },\n"
    + old_date_same,
    1,
)
calendar_tests = calendar_tests.replace(
    "${source}\\nglobalThis.__Calendar = Calendar;\\nglobalThis.__localDate = _localDate;\\nglobalThis.__representableLocalDate = _representableLocalDate;\\nglobalThis.__addCivilDays = _addCivilDays;",
    "${source}\\nglobalThis.__Calendar = Calendar;\\nglobalThis.__localDate = _localDate;\\nglobalThis.__representableLocalDate = _representableLocalDate;",
    1,
)
calendar_tests = calendar_tests.replace(
    "        addCivilDays: context.__addCivilDays,\n", "", 1
)
calendar_tests = calendar_tests.replace(
    "        const { localDate, representableLocalDate, addCivilDays } =\n"
    "            evaluateCalendar();",
    "        const { localDate, representableLocalDate } = evaluateCalendar();",
    1,
)
extreme_block = """        const extremeNext = addCivilDays(extreme, 1);
        assert.notEqual(extremeNext, null);
        assert.equal(extremeNext.getFullYear(), 2147483647);
        assert.equal(extremeNext.getMonth() + 1, 12);
        assert.equal(extremeNext.getDate(), 31);
        assert.ok(extremeNext.getDay() >= 0 && extremeNext.getDay() <= 6);
        assert.equal(
            addCivilDays(extremeNext, 1),
            null,
            "keyboard day navigation must stop cleanly at the native year limit"
        );
"""
if extreme_block not in calendar_tests:
    raise SystemExit("test-js-runtime.js: old addCivilDays boundary block missing")
calendar_tests = calendar_tests.replace(extreme_block, "", 1)
navigation_test = """
function testCalendarDayNavigationDelegatesToNative() {
    const { Calendar } = evaluateCalendar();
    const calendar = Object.create(Calendar.prototype);
    let call = null;
    calendar._calendarSystem = {
        add_days_parts(year, month, day, amount) {
            call = [year, month, day, amount];
            if (amount === 2) {
                return null;
            }
            return {
                deep_unpack() { return [year, month, day + amount]; },
            };
        },
    };
    const source = new Date(2026, 7, 8, 12, 0, 0);
    const next = calendar._dateByDays(source, 1);
    assert.deepEqual(call, [2026, 8, 8, 1]);
    assert.equal(next.getFullYear(), 2026);
    assert.equal(next.getMonth() + 1, 8);
    assert.equal(next.getDate(), 9);
    assert.equal(calendar._dateByDays(source, 2), null);
}

"""
insert_before = "function testCalendarRejectsUnsupportedProviderDate() {"
if insert_before not in calendar_tests:
    raise SystemExit("test-js-runtime.js: Calendar insertion point missing")
calendar_tests = calendar_tests.replace(
    insert_before, navigation_test + insert_before, 1
)
call = "testCalendarRejectsNormalizedCivilDates();\n"
if call not in calendar_tests:
    raise SystemExit("test-js-runtime.js: Calendar test call missing")
calendar_tests = calendar_tests.replace(
    call, call + "testCalendarDayNavigationDelegatesToNative();\n", 1
)
write("tests/test-js-runtime.js", before + marker + calendar_tests)

# Real typelib smoke covers ordinary navigation, weekday and signed-gint boundary.
replace_once(
    "tests/smoke-typelib.js",
    "requireCondition(\n"
    "    !CalendarPlus.date_is_work_day(2026, 8, 8),\n"
    "    \"Saturday was classified as a work day\"\n"
    ");\n\n",
    "requireCondition(\n"
    "    !CalendarPlus.date_is_work_day(2026, 8, 8),\n"
    "    \"Saturday was classified as a work day\"\n"
    ");\n"
    "requireCondition(\n"
    "    CalendarPlus.date_weekday(2026, 8, 8) === 6,\n"
    "    \"native weekday projection failed\"\n"
    ");\n"
    "const [nextDayYear, nextDayMonth, nextDay] = calendar\n"
    "    .add_days_parts(2026, 8, 8, 1)\n"
    "    .deep_unpack();\n"
    "requireCondition(\n"
    "    nextDayYear === 2026 && nextDayMonth === 8 && nextDay === 9,\n"
    "    \"native civil-day navigation failed\"\n"
    ");\n"
    "requireCondition(\n"
    "    calendar.add_days_parts(2147483647, 12, 31, 1) === null,\n"
    "    \"native civil-day navigation crossed the gint year boundary\"\n"
    ");\n\n",
)

# Architecture contracts ban regressions back to the old coupling.
replace_once(
    "tests/test-settings.py",
    "    event_manager = read(\"eventManager.js\")\n",
    "    event_manager = read(\"eventManager.js\")\n"
    "    event_range_state = read(\"eventRangeState.js\")\n",
)
replace_once(
    "tests/test-settings.py",
    "    assert \"this._calendarSystem.add_months_parts(\" in calendar\n",
    "    assert \"this._calendarSystem.add_days_parts(\" in calendar\n"
    "    assert \"this._calendarSystem.add_months_parts(\" in calendar\n",
)
replace_once(
    "tests/test-settings.py",
    "    assert \"CalendarPlus.date_same(\" in calendar\n",
    "    assert \"CalendarPlus.date_same(\" in calendar\n"
    "    assert \"CalendarPlus.date_weekday(\" in calendar\n"
    "    assert \"_addCivilDays\" not in calendar\n"
    "    assert \"_gregorianWeekday\" not in calendar\n",
)
replace_once(
    "tests/test-settings.py",
    "    assert \"this.event_store.refresh_timezone()\" in event_manager\n",
    "    assert \"this.event_store.refresh_timezone()\" in event_manager\n"
    "    assert \"new EventRangeState()\" in event_manager\n"
    "    for legacy_flag in (\n"
    "        \"_range_request_generation\", \"_range_request_pending\",\n"
    "        \"_range_request_succeeded\", \"_range_accepting_events\",\n"
    "        \"_queued_range_force\", \"_range_retry_attempt\",\n"
    "    ):\n"
    "        assert legacy_flag not in event_manager\n"
    "    for transition in (\n"
    "        \"beginRequest()\", \"finishRequest(\", \"invalidate()\",\n"
    "        \"acceptCompletedRequest()\", \"nextRetryDelay()\",\n"
    "        \"resetForServerLoss()\",\n"
    "    ):\n"
    "        assert transition in event_range_state\n",
)

replace_once(
    "docs/ARCHITECTURE.md",
    "This design keeps CalendarServer lifecycle and transport behaviour out of portable event semantics while allowing the Cinnamon presentation layer to refresh safely from coherent Calendar-owned state.\n",
    "This design keeps CalendarServer lifecycle and transport behaviour out of portable event semantics while allowing the Cinnamon presentation layer to refresh safely from coherent Calendar-owned state. Cinnamon event transport also isolates visible-range request generation, admission, retry and queued-force transitions in a dedicated state object so CalendarServer callbacks cannot mutate an accidental constellation of controller flags.\n",
)

subprocess.run(["python3", "tools/update-runtime-hashes.py"], cwd=ROOT, check=True)

# Final invariants before the normal project gate runs.
manager = read("src/cinnamon/eventManager.js")
for forbidden in (
    "_range_request_generation",
    "_range_request_pending",
    "_range_request_succeeded",
    "_range_accepting_events",
    "_queued_range_force",
    "_range_retry_attempt",
):
    if forbidden in manager:
        raise SystemExit(f"legacy range flag survived: {forbidden}")
calendar = read("src/cinnamon/calendar.js")
if "_addCivilDays" in calendar or "_gregorianWeekday" in calendar:
    raise SystemExit("duplicate JS civil arithmetic survived")
if "snapshot.timestamp" in read("src/cinnamon/eventView.js"):
    raise SystemExit("opaque event revision is still named timestamp")

print("Calendar brittleness refactor staged successfully")
