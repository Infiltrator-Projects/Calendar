#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, value: str) -> None:
    (ROOT / path).write_text(value, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    value = text(path)
    count = value.count(old)
    assert count == 1, f"{path}: expected one exact match, found {count}"
    write(path, value.replace(old, new, 1))


def regex_once(path: str, pattern: str, replacement: str) -> None:
    value = text(path)
    updated, count = re.subn(pattern, replacement, value, count=1, flags=re.DOTALL)
    assert count == 1, f"{path}: expected one regex match, found {count}: {pattern}"
    write(path, updated)


# 1. CalendarServer tuple ownership: only the native adapter knows tuple positions.
old_add = '''gboolean
calendar_plus_event_store_add_or_update(CalendarPlusEventStore *self,
                                        GVariant *event_data,
                                        gint64 update_timestamp)
{
    g_autoptr(GVariant) tuple = NULL;
    g_autoptr(GVariant) id_value = NULL;
    g_autoptr(GVariant) color_value = NULL;
    g_autoptr(GVariant) summary_value = NULL;
    g_autoptr(GVariant) all_day_value = NULL;
    g_autoptr(GVariant) start_value = NULL;
    g_autoptr(GVariant) end_value = NULL;
    g_autoptr(GVariant) modified_value = NULL;
    g_autofree gchar *id = NULL;
    g_autofree gchar *color = NULL;
    g_autofree gchar *summary = NULL;
    CalendarPlusEventInput input;

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    if (event_data == NULL)
        return FALSE;

    tuple = unwrap_variant(event_data);
    if (g_variant_classify(tuple) != G_VARIANT_CLASS_TUPLE ||
        g_variant_n_children(tuple) < 7)
    {
        return FALSE;
    }

    id_value = event_child(tuple, 0);
    color_value = event_child(tuple, 1);
    summary_value = event_child(tuple, 2);
    all_day_value = event_child(tuple, 3);
    start_value = event_child(tuple, 4);
    end_value = event_child(tuple, 5);
    modified_value = event_child(tuple, 6);

    id = variant_dup_string_limited(id_value,
                                    CALENDAR_PLUS_EVENT_MAX_ID_BYTES,
                                    FALSE);
    color = variant_dup_string_limited(color_value,
                                       CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES,
                                       FALSE);
    summary = variant_dup_string_limited(
        summary_value, CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES, TRUE);

    input.id = id;
    input.color = color;
    input.summary = summary;
    input.update_timestamp = update_timestamp;
    if (id == NULL || color == NULL || summary == NULL ||
        !g_variant_is_of_type(all_day_value, G_VARIANT_TYPE_BOOLEAN) ||
        !variant_get_int64(start_value, &input.start_unix) ||
        !variant_get_int64(end_value, &input.end_unix) ||
        !variant_get_int64(modified_value, &input.modified))
    {
        return FALSE;
    }

    input.all_day = g_variant_get_boolean(all_day_value);
    return calendar_plus_event_index_upsert(
        calendar_plus_event_store_get_index(self), &input);
}
'''
new_add = '''static gboolean
event_input_from_variant(GVariant *event_data,
                         gint64 update_timestamp,
                         CalendarPlusEventInput *input,
                         gchar **id,
                         gchar **color,
                         gchar **summary)
{
    g_autoptr(GVariant) tuple = NULL;
    g_autoptr(GVariant) id_value = NULL;
    g_autoptr(GVariant) color_value = NULL;
    g_autoptr(GVariant) summary_value = NULL;
    g_autoptr(GVariant) all_day_value = NULL;
    g_autoptr(GVariant) start_value = NULL;
    g_autoptr(GVariant) end_value = NULL;
    g_autoptr(GVariant) modified_value = NULL;

    if (event_data == NULL || input == NULL || id == NULL || color == NULL ||
        summary == NULL)
    {
        return FALSE;
    }

    tuple = unwrap_variant(event_data);
    if (g_variant_classify(tuple) != G_VARIANT_CLASS_TUPLE ||
        g_variant_n_children(tuple) < 7)
    {
        return FALSE;
    }

    id_value = event_child(tuple, 0);
    color_value = event_child(tuple, 1);
    summary_value = event_child(tuple, 2);
    all_day_value = event_child(tuple, 3);
    start_value = event_child(tuple, 4);
    end_value = event_child(tuple, 5);
    modified_value = event_child(tuple, 6);

    *id = variant_dup_string_limited(id_value,
                                     CALENDAR_PLUS_EVENT_MAX_ID_BYTES,
                                     FALSE);
    *color = variant_dup_string_limited(color_value,
                                        CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES,
                                        FALSE);
    *summary = variant_dup_string_limited(
        summary_value, CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES, TRUE);

    input->id = *id;
    input->color = *color;
    input->summary = *summary;
    input->update_timestamp = update_timestamp;
    if (*id == NULL || *color == NULL || *summary == NULL ||
        !g_variant_is_of_type(all_day_value, G_VARIANT_TYPE_BOOLEAN) ||
        !variant_get_int64(start_value, &input->start_unix) ||
        !variant_get_int64(end_value, &input->end_unix) ||
        !variant_get_int64(modified_value, &input->modified))
    {
        return FALSE;
    }

    input->all_day = g_variant_get_boolean(all_day_value);
    return TRUE;
}

static gboolean
event_input_overlaps_range(const CalendarPlusEventInput *input,
                           gint64 range_start_unix,
                           gint64 range_end_unix)
{
    if (input == NULL || range_end_unix < range_start_unix)
        return FALSE;

    if (input->all_day)
    {
        return input->start_unix <= range_end_unix &&
               input->end_unix > range_start_unix;
    }
    return input->start_unix <= range_end_unix &&
           input->end_unix >= range_start_unix;
}

gboolean
calendar_plus_event_store_add_or_update(CalendarPlusEventStore *self,
                                        GVariant *event_data,
                                        gint64 update_timestamp)
{
    g_autofree gchar *id = NULL;
    g_autofree gchar *color = NULL;
    g_autofree gchar *summary = NULL;
    CalendarPlusEventInput input = { 0 };

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    if (!event_input_from_variant(event_data,
                                  update_timestamp,
                                  &input,
                                  &id,
                                  &color,
                                  &summary))
    {
        return FALSE;
    }

    return calendar_plus_event_index_upsert(
        calendar_plus_event_store_get_index(self), &input);
}

gboolean
calendar_plus_event_store_add_or_update_in_range(
    CalendarPlusEventStore *self,
    GVariant *event_data,
    gint64 update_timestamp,
    gint64 range_start_unix,
    gint64 range_end_unix)
{
    g_autofree gchar *id = NULL;
    g_autofree gchar *color = NULL;
    g_autofree gchar *summary = NULL;
    CalendarPlusEventInput input = { 0 };

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    if (!event_input_from_variant(event_data,
                                  update_timestamp,
                                  &input,
                                  &id,
                                  &color,
                                  &summary) ||
        !event_input_overlaps_range(&input,
                                    range_start_unix,
                                    range_end_unix))
    {
        return FALSE;
    }

    return calendar_plus_event_index_upsert(
        calendar_plus_event_store_get_index(self), &input);
}
'''
replace_once("src/adapters/event-gvariant-adapter.c", old_add, new_add)

header_anchor = '''gboolean calendar_plus_event_store_add_or_update(
    CalendarPlusEventStore *self,
    GVariant *event_data,
    gint64 update_timestamp);

'''
header_add = header_anchor + '''/**
 * calendar_plus_event_store_add_or_update_in_range:
 * @self: an event store
 * @event_data: a Cinnamon CalendarServer event tuple
 * @update_timestamp: monotonic timestamp for the current server refresh
 * @range_start_unix: inclusive first accepted Unix instant
 * @range_end_unix: inclusive last accepted Unix instant
 *
 * Parses the external CalendarServer tuple once in the native transport
 * adapter, rejects rows that cannot overlap the newest requested range and
 * then applies the same normalisation as calendar_plus_event_store_add_or_update().
 * JavaScript therefore never depends on CalendarServer tuple positions.
 * All-day CalendarServer end points remain exclusive; timed end points are
 * inclusive.
 *
 * Returns: %TRUE when an admitted row changed visible event data
 */
gboolean calendar_plus_event_store_add_or_update_in_range(
    CalendarPlusEventStore *self,
    GVariant *event_data,
    gint64 update_timestamp,
    gint64 range_start_unix,
    gint64 range_end_unix);

'''
replace_once("src/adapters/event-store.h", header_anchor, header_add)

replace_once(
    "src/abi/calendar-plus.map",
    '''CALENDAR_PLUS_2.9 {\n    global:\n        calendar_plus_format_date_v1;\n} CALENDAR_PLUS_2.8;''',
    '''CALENDAR_PLUS_2.9 {\n    global:\n        calendar_plus_format_date_v1;\n} CALENDAR_PLUS_2.8;\n\nCALENDAR_PLUS_2.10 {\n    global:\n        calendar_plus_event_store_add_or_update_in_range;\n} CALENDAR_PLUS_2.9;'''
)

baseline = text("tests/abi-baseline.txt")
needle = "calendar_plus_event_store_add_or_update@CALENDAR_PLUS_1.0\n"
assert needle in baseline
baseline = baseline.replace(
    needle,
    needle + "calendar_plus_event_store_add_or_update_in_range@CALENDAR_PLUS_2.10\n",
    1,
)
write("tests/abi-baseline.txt", baseline)

# Named native boundary objects remove positional tuple knowledge from presentation.
native_boundary = '''// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Named JavaScript boundary for native GVariant contracts.
 *
 * Only this module knows positional native tuple layouts. Presentation and
 * controller modules consume named objects so native ABI evolution cannot
 * scatter field-number assumptions through the Cinnamon surface.
 */

function _unpack(variant, contract) {
    if (variant === null || typeof variant.deep_unpack !== "function") {
        throw new Error(`${contract}: missing GVariant value`);
    }
    return variant.deep_unpack();
}

function _array(value, length, contract) {
    if (!Array.isArray(value) || value.length !== length) {
        throw new Error(`${contract}: unexpected native record shape`);
    }
    return value;
}

var NativeBoundary = Object.freeze({
    decodeCivilDate(variant) {
        const row = _array(_unpack(variant, "civil date"), 3, "civil date");
        return Object.freeze({ year: row[0], month: row[1], day: row[2] });
    },

    decodeTemporalPolicy(variant) {
        const row = _array(
            _unpack(variant, "temporal policy"),
            8,
            "temporal policy"
        );
        return Object.freeze({
            mode: row[0],
            calendar: row[1],
            showSeconds: row[2],
            locationConfigured: row[3],
            latitude: row[4],
            longitude: row[5],
            authority: row[6],
            providerAvailable: row[7],
        });
    },

    decodeCalendarGrid(variant) {
        const rows = _unpack(variant, "calendar grid");
        if (!Array.isArray(rows)) {
            throw new Error("calendar grid: expected row array");
        }
        return rows.map((value) => {
            const row = _array(value, 13, "calendar grid row");
            return Object.freeze({
                dayLabel: row[0],
                year: row[1],
                month: row[2],
                day: row[3],
                row: row[4],
                column: row[5],
                weekNumber: row[6],
                isWorkDay: row[7],
                isToday: row[8],
                isSelected: row[9],
                isCurrentPeriod: row[10],
                isTopRow: row[11],
                isLeftEdge: row[12],
            });
        });
    },

    decodeEventSnapshot(variant) {
        const snapshot = _array(
            _unpack(variant, "event snapshot"),
            2,
            "event snapshot"
        );
        if (!Array.isArray(snapshot[1])) {
            throw new Error("event snapshot: expected event row array");
        }
        return Object.freeze({
            revision: snapshot[0],
            events: snapshot[1].map((value) => {
                const row = _array(value, 10, "event snapshot row");
                return Object.freeze({
                    id: row[0],
                    color: row[1],
                    summary: row[2],
                    allDay: row[3],
                    multiDay: row[4],
                    startUnix: row[5],
                    endUnix: row[6],
                    startDayUnix: row[7],
                    endDayUnix: row[8],
                    modified: row[9],
                });
            }),
        });
    },
});
'''
write("src/cinnamon/nativeBoundary.js", native_boundary)

# eventManager: native admission + named snapshot decoder.
replace_once(
    "src/cinnamon/eventManager.js",
    '''const EventRangeState = RuntimeSupport.loadLocalModule(\n    "eventRangeState"\n).EventRangeState;\n''',
    '''const EventRangeState = RuntimeSupport.loadLocalModule(\n    "eventRangeState"\n).EventRangeState;\nconst NativeBoundary = RuntimeSupport.loadLocalModule(\n    "nativeBoundary"\n).NativeBoundary;\n'''
)
regex_once(
    "src/cinnamon/eventManager.js",
    r'function _eventVariantOverlapsRange\(eventVariant, rangeStart, rangeEnd\) \{.*?\n\}\n\nclass EventRecord',
    'class EventRecord'
)
regex_once(
    "src/cinnamon/eventManager.js",
    r'class EventRecord \{\n    constructor\(row\) \{.*?\n    \}\n\n    relation_to_day',
    '''class EventRecord {
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

    relation_to_day'''
)
regex_once(
    "src/cinnamon/eventManager.js",
    r'class EventSnapshot \{\n    constructor\(variant\) \{.*?\n    \}\n\n    get_event_list',
    '''class EventSnapshot {
    constructor(variant) {
        const decoded = NativeBoundary.decodeEventSnapshot(variant);
        this.revision = decoded.revision;
        this.events = [];
        for (const row of decoded.events) {
            try {
                this.events.push(new EventRecord(row));
            } catch (error) {
                global.logError(error);
            }
        }
    }

    get_event_list'''
)
old_ingest = '''        let changed = false;
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
'''
new_ingest = '''        let changed = false;
        for (const eventVariant of payload.unpack()) {
            /*
             * CalendarServer has no request-generation token on event signals.
             * The native transport adapter owns tuple parsing and range
             * admission together so JavaScript never learns tuple positions.
             */
            changed = this.event_store.add_or_update_in_range(
                eventVariant,
                this.last_update_timestamp,
                rangeStart,
                rangeEnd
            ) || changed;
        }
'''
replace_once("src/cinnamon/eventManager.js", old_ingest, new_ingest)

# Calendar view receives resolved scalar policy and a narrow event source.
replace_once(
    "src/cinnamon/calendar.js",
    '''const RuntimeSupport = _loadRuntimeSupport();\nconst SignalBag = RuntimeSupport.SignalBag;\n''',
    '''const RuntimeSupport = _loadRuntimeSupport();\nconst SignalBag = RuntimeSupport.SignalBag;\nconst NativeBoundary = RuntimeSupport.loadLocalModule(\n    "nativeBoundary"\n).NativeBoundary;\n'''
)
replace_once(
    "src/cinnamon/calendar.js",
    '''function _localDateFromVariant(parts) {\n    if (parts === null) {\n        return null;\n    }\n    const [year, month, day] = parts.deep_unpack();\n    return _localDate(year, month, day);\n}\n''',
    '''function _localDateFromVariant(parts) {\n    if (parts === null) {\n        return null;\n    }\n    const civil = NativeBoundary.decodeCivilDate(parts);\n    return _localDate(civil.year, civil.month, civil.day);\n}\n'''
)
old_constructor = '''var Calendar = class Calendar {
    constructor(settings, events_manager, desktop_settings) {
        this.settings = settings;
        this.events_manager = events_manager;
        this.desktop_settings = desktop_settings;
        if (!this.desktop_settings) {
            throw new Error("Calendar: desktop settings are required.");
        }
        this._destroyed = false;
        this._update_id = 0;
        this._set_date_idle_id = 0;
        this._weekStart = Cinnamon.util_get_week_start();
        this._digitWidth = NaN;
        this._calendarSystemId = "gregorian";
        this._calendarSystem = CalendarPlus.CalendarSystem.new("gregorian");
        this._selectedDate = new Date();
        this._pendingDate = null;
        this._focusAfterUpdate = null;
        this.events_enabled = false;

        this._actorSignals = new SignalBag();
        this._eventSignals = new SignalBag();
        this._desktopSignals = new SignalBag();

        this.actor = new St.Table({
            homogeneous: false,
            style_class: "calendar",
            reactive: true,
        });

        this.settings.bindWithObject(
            this,
            "show-week-numbers",
            "show_week_numbers",
            this._onSettingsChange
        );

        this._desktopSignals.connect(
            this.desktop_settings,
            `changed::${FIRST_WEEKDAY_KEY}`,
            () => this._onSettingsChange(null, FIRST_WEEKDAY_KEY)
        );

'''
new_constructor = '''var Calendar = class Calendar {
    constructor(event_source, weekStart, showWeekNumbers) {
        if (!event_source) {
            throw new Error("Calendar: event source is required.");
        }
        this.event_source = event_source;
        this._destroyed = false;
        this._update_id = 0;
        this._set_date_idle_id = 0;
        this._weekStart = Number.isInteger(weekStart) && weekStart >= 0 && weekStart <= 6
            ? weekStart
            : 0;
        this.show_week_numbers = Boolean(showWeekNumbers);
        this._digitWidth = NaN;
        this._calendarSystemId = "gregorian";
        this._calendarSystem = CalendarPlus.CalendarSystem.new("gregorian");
        this._selectedDate = new Date();
        this._pendingDate = null;
        this._focusAfterUpdate = null;
        this.events_enabled = false;

        this._actorSignals = new SignalBag();
        this._eventSignals = new SignalBag();

        this.actor = new St.Table({
            homogeneous: false,
            style_class: "calendar",
            reactive: true,
        });

'''
replace_once("src/cinnamon/calendar.js", old_constructor, new_constructor)
# event source renaming is intentionally local to this presentation module.
calendar_source = text("src/cinnamon/calendar.js").replace("this.events_manager", "this.event_source")
calendar_source = calendar_source.replace("events_manager, signal", "event_source, signal")
write("src/cinnamon/calendar.js", calendar_source)
regex_once(
    "src/cinnamon/calendar.js",
    r'    _onSettingsChange\(object, key\) \{.*?\n    \}\n\n    refreshEventAvailability',
    '''    setShowWeekNumbers(showWeekNumbers) {
        if (this._destroyed) {
            return;
        }
        const next = Boolean(showWeekNumbers);
        if (next === this.show_week_numbers) {
            return;
        }
        this.show_week_numbers = next;
        this._buildHeader();
        this._update(false);
    }

    setWeekStart(weekStart) {
        if (this._destroyed || !Number.isInteger(weekStart) ||
            weekStart < 0 || weekStart > 6 || weekStart === this._weekStart) {
            return;
        }
        this._weekStart = weekStart;
        this._buildHeader();
        this._update(false);
    }

    refreshEventAvailability'''
)
replace_once(
    "src/cinnamon/calendar.js",
    "        const records = model.deep_unpack();\n",
    "        const records = NativeBoundary.decodeCalendarGrid(model);\n"
)
replace_once(
    "src/cinnamon/calendar.js",
    '''                    record[1], record[2], record[3]\n''',
    '''                    record.year, record.month, record.day\n'''
)
# There are two endpoint loops using the same positional expression.
value = text("src/cinnamon/calendar.js")
value = value.replace("                    record[1], record[2], record[3]\n", "                    record.year, record.month, record.day\n")
write("src/cinnamon/calendar.js", value)
regex_once(
    "src/cinnamon/calendar.js",
    r'    _addGridCell\(record, eventColors\) \{\n        const \[.*?\n        \] = record;',
    '''    _addGridCell(record, eventColors) {
        const {
            dayLabel,
            year,
            month,
            day,
            row,
            column,
            weekNumber,
            isWorkDay,
            isToday,
            isSelected,
            isCurrentPeriod,
            isTopRow,
            isLeftEdge,
        } = record;'''
)
replace_once("src/cinnamon/calendar.js", "        this._desktopSignals.disconnectAll();\n", "")
replace_once("src/cinnamon/calendar.js", "        this.desktop_settings = null;\n", "")
replace_once("src/cinnamon/calendar.js", "        this.settings = null;\n", "")

# Event list owns a scalar render preference rather than Gio.Settings.
replace_once(
    "src/cinnamon/eventView.js",
    '''var EventList = class EventList {\n    constructor(desktop_settings) {\n        if (!desktop_settings) {\n            throw new Error("EventList: desktop settings are required.");\n        }\n        this.desktop_settings = desktop_settings;\n''',
    '''var EventList = class EventList {\n    constructor(use24h) {\n        this._use24h = Boolean(use24h);\n'''
)
replace_once(
    "src/cinnamon/eventView.js",
    '''    refresh_variations() {\n''',
    '''    set_use_24h(use24h) {\n        const next = Boolean(use24h);\n        if (next !== this._use24h) {\n            this._use24h = next;\n            this._current_event_cache_key = null;\n        }\n    }\n\n    refresh_variations() {\n'''
)
replace_once(
    "src/cinnamon/eventView.js",
    '''        const use24h = this.desktop_settings.get_boolean("clock-use-24h");\n''',
    '''        const use24h = this._use24h;\n'''
)
replace_once("src/cinnamon/eventView.js", "        this.desktop_settings = null;\n", "")

# panelClock consumes resolved scalar desktop policy.
panel = text("src/cinnamon/panelClock.js")
panel = panel.replace("function uses24HourClock(mode, desktopSettings) {", "function uses24HourClock(mode, use24h) {")
panel = panel.replace('    return desktopSettings.get_boolean("clock-use-24h");', "    return Boolean(use24h);")
panel = panel.replace('        config.desktopSettings.get_boolean("clock-show-date");', "        Boolean(config.showDate);")
panel = panel.replace("uses24HourClock(config.mode, config.desktopSettings)", "uses24HourClock(config.mode, config.use24h)")
panel = panel.replace('        !config.desktopSettings.get_boolean("clock-show-date")) {', "        !Boolean(config.showDate)) {")
write("src/cinnamon/panelClock.js", panel)

# Applet is the composition root and sole owner of live applet/desktop settings.
replace_once(
    "src/cinnamon/applet.js",
    '''const CalendarPlus = imports.gi.CalendarPlus;\nconst CinnamonDesktop = imports.gi.CinnamonDesktop;\n''',
    '''const CalendarPlus = imports.gi.CalendarPlus;\nconst Cinnamon = imports.gi.Cinnamon;\nconst CinnamonDesktop = imports.gi.CinnamonDesktop;\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''const RuntimeSupport = _loadRuntimeSupport();\nconst SignalBag = RuntimeSupport.SignalBag;\n''',
    '''const RuntimeSupport = _loadRuntimeSupport();\nconst SignalBag = RuntimeSupport.SignalBag;\nconst NativeBoundary = RuntimeSupport.loadLocalModule(\n    "nativeBoundary"\n).NativeBoundary;\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''        this.show_events = true;\n        this.theme_mode = "system";\n''',
    '''        this.show_events = true;\n        this.show_week_numbers = false;\n        this.theme_mode = "system";\n'''
)
old_build = '''        this.event_list = new PopupShell.EventList(this.desktop_settings);
        this.events_manager = new EventManager.EventsManager();
        this._calendarEventSource = new PopupShell.CalendarEventSource(
            this.events_manager
        );
        this._calendar = new Calendar.Calendar(
            this.settings,
            this._calendarEventSource,
            this.desktop_settings
        );
'''
new_build = '''        this.show_week_numbers = Boolean(
            this.settings.getValue("show-week-numbers")
        );
        this.event_list = new PopupShell.EventList(this._desktopUses24h());
        this.events_manager = new EventManager.EventsManager();
        this._calendarEventSource = new PopupShell.CalendarEventSource(
            this.events_manager
        );
        this._calendar = new Calendar.Calendar(
            this._calendarEventSource,
            Cinnamon.util_get_week_start(),
            this.show_week_numbers
        );
'''
replace_once("src/cinnamon/applet.js", old_build, new_build)
replace_once(
    "src/cinnamon/applet.js",
    '''        this.settings.bind(\n            "theme-mode",\n''',
    '''        this.settings.bind(\n            "show-week-numbers",\n            "show_week_numbers",\n            this._onShowWeekNumbersChanged\n        );\n        this.settings.bind(\n            "theme-mode",\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''    _onThemeModeChanged() {\n''',
    '''    _onShowWeekNumbersChanged() {\n        if (!this._destroyed && this._calendar) {\n            this._calendar.setShowWeekNumbers(this.show_week_numbers);\n        }\n    }\n\n    _onWeekStartChanged() {\n        if (!this._destroyed && this._calendar) {\n            this._calendar.setWeekStart(Cinnamon.util_get_week_start());\n        }\n    }\n\n    _onThemeModeChanged() {\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''        this._resetLabelWidth();\n        this._configureWallClock();\n''',
    '''        this._resetLabelWidth();\n        this.event_list.set_use_24h(this._desktopUses24h());\n        this._configureWallClock();\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''        this._signals.connect(\n            this.desktop_settings,\n            "changed::clock-use-24h",\n''',
    '''        this._signals.connect(\n            this.desktop_settings,\n            "changed::first-day-of-week",\n            () => this._onWeekStartChanged()\n        );\n        this._signals.connect(\n            this.desktop_settings,\n            "changed::clock-use-24h",\n'''
)
replace_once(
    "src/cinnamon/applet.js",
    '''    _systemThemeName() {\n''',
    '''    _desktopUses24h() {\n        try {\n            return this.desktop_settings &&\n                this.desktop_settings.get_boolean("clock-use-24h");\n        } catch (error) {\n            return false;\n        }\n    }\n\n    _desktopShowsDate() {\n        try {\n            return this.desktop_settings &&\n                this.desktop_settings.get_boolean("clock-show-date");\n        } catch (error) {\n            return false;\n        }\n    }\n\n    _systemThemeName() {\n'''
)
old_unpack = '''            const [
                mode,
                calendar,
                showSeconds,
                locationConfigured,
                latitude,
                longitude,
                authority,
                providerAvailable,
            ] = variant.deep_unpack();

            const numericLatitude = Number(latitude);
            const numericLongitude = Number(longitude);
'''
new_unpack = '''            const decoded = NativeBoundary.decodeTemporalPolicy(variant);
            const {
                mode,
                calendar,
                showSeconds,
                locationConfigured,
                latitude,
                longitude,
                authority,
                providerAvailable,
            } = decoded;

            const numericLatitude = Number(latitude);
            const numericLongitude = Number(longitude);
'''
replace_once("src/cinnamon/applet.js", old_unpack, new_unpack)
replace_once(
    "src/cinnamon/applet.js",
    '''            vertical: this._isVerticalPanel(),\n            desktopSettings: this.desktop_settings,\n            primaryCalendar: temporal.calendar,\n''',
    '''            vertical: this._isVerticalPanel(),\n            use24h: this._desktopUses24h(),\n            showDate: this._desktopShowsDate(),\n            primaryCalendar: temporal.calendar,\n'''
)

# Tests for the named boundary and dependency direction.
native_test = '''#!/usr/bin/env node
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

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
    "infiltrator-system-settings", true,
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

const snapshot = boundary.decodeEventSnapshot(variant([
    7,
    [["id", "#112233", "summary", false, false, 1, 2, 0, 0, 3]],
]));
assert.equal(snapshot.revision, 7);
assert.equal(snapshot.events[0].summary, "summary");
assert.equal(snapshot.events[0].endUnix, 2);

assert.throws(() => boundary.decodeTemporalPolicy(variant(["short"])));
assert.throws(() => boundary.decodeCalendarGrid(variant([["short"]])));
assert.throws(() => boundary.decodeEventSnapshot(null));
'''
write("tests/test-native-boundary.js", native_test)
replace_once(
    "Makefile",
    '''\tpython3 tests/test-popup-lifecycle.py\n\tnode tests/test-js-runtime.js\n''',
    '''\tpython3 tests/test-popup-lifecycle.py\n\tnode tests/test-native-boundary.js\n\tnode tests/test-js-runtime.js\n'''
)

# Add native admission regression coverage.
properties_anchor = '''static void
test_event_color_range(void)
'''
properties_test = '''static void
test_event_store_range_admission(void)
{
    const gint64 range_start = local_unix(2026, 7, 29, 0);
    const gint64 range_end = local_unix(2026, 7, 29, 23) + 3599;
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) inside =
        event_variant("inside",
                      local_unix(2026, 7, 29, 9),
                      local_unix(2026, 7, 29, 10));
    g_autoptr(GVariant) outside =
        event_variant("outside",
                      local_unix(2026, 7, 30, 9),
                      local_unix(2026, 7, 30, 10));
    g_autoptr(GVariant) all_day_touching = g_variant_ref_sink(
        g_variant_new("(sssbxxx)",
                      "all-day-touching", "#445566", "summary", TRUE,
                      local_unix(2026, 7, 28, 0),
                      range_start,
                      (gint64)1));
    g_autoptr(GVariant) snapshot = NULL;
    g_autoptr(GVariant) rows = NULL;
    gint64 revision = 0;

    g_assert_true(calendar_plus_event_store_add_or_update_in_range(
        store, inside, 10, range_start, range_end));
    g_assert_false(calendar_plus_event_store_add_or_update_in_range(
        store, outside, 10, range_start, range_end));
    g_assert_false(calendar_plus_event_store_add_or_update_in_range(
        store, all_day_touching, 10, range_start, range_end));
    g_assert_false(calendar_plus_event_store_add_or_update_in_range(
        store, inside, 10, range_end, range_start));

    snapshot = calendar_plus_event_store_get_snapshot(
        store, range_start, local_unix(2026, 7, 29, 12));
    g_variant_get(snapshot, "(x@a(sssbbxxxxx))", &revision, &rows);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 1);
}

'''
replace_once("tests/test-properties.c", properties_anchor, properties_test + properties_anchor)
replace_once(
    "tests/test-properties.c",
    '''    g_test_add_func("/properties/event-color-range",\n                    test_event_color_range);\n''',
    '''    g_test_add_func("/properties/event-color-range",\n                    test_event_color_range);\n    g_test_add_func("/properties/event-range-admission",\n                    test_event_store_range_admission);\n'''
)

# Strengthen architecture checks around the refactor.
replace_once(
    "tests/test-settings.py",
    '''    event_range_state = read("eventRangeState.js")\n    event_port = read("calendarEventSource.js")\n''',
    '''    event_range_state = read("eventRangeState.js")\n    event_port = read("calendarEventSource.js")\n    native_boundary = read("nativeBoundary.js")\n'''
)
replace_once(
    "tests/test-settings.py",
    '''    assert {"calendar", "eventManager", "panelClock", "popupShell"}.issubset(\n        loaded_modules(applet)\n    )\n''',
    '''    assert {\n        "calendar", "eventManager", "nativeBoundary", "panelClock", "popupShell"\n    }.issubset(loaded_modules(applet))\n'''
)
old_agenda_test = '''    # Agenda presentation needs the desktop clock preference, not the complete
    # Calendar applet settings object.
    assert re.search(r"class EventList\\s*\\{\\s*constructor\\s*\\(\\s*desktop_settings\\s*\\)", event_view)
    forbid_tokens(js_code(event_view), "this.settings", "constructor(settings,")
    assert re.search(
        r"new\\s+PopupShell\\.EventList\\s*\\(\\s*this\\.desktop_settings\\s*\\)", applet
    )
'''
new_agenda_test = '''    # Lower presentation modules consume resolved scalar preferences rather
    # than live applet/desktop settings objects.
    assert re.search(r"class EventList\\s*\\{\\s*constructor\\s*\\(\\s*use24h\\s*\\)", event_view)
    require_tokens(event_view, "set_use_24h(use24h)", "this._use24h")
    forbid_tokens(js_code(event_view), "this.settings", "this.desktop_settings")
    forbid_tokens(js_code(calendar), "this.settings", "this.desktop_settings", "bindWithObject")
    forbid_tokens(js_code(panel_clock), "desktopSettings", "get_boolean(\\\"clock-use-24h\\\")")
    require_tokens(calendar, "setShowWeekNumbers(showWeekNumbers)", "setWeekStart(weekStart)")
'''
replace_once("tests/test-settings.py", old_agenda_test, new_agenda_test)
replace_once(
    "tests/test-settings.py",
    '''    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "event_store")\n''',
    '''    forbid_tokens(js_code(event_port), "CalendarServer", "Gio.", "bus_watch", "event_store")\n    forbid_tokens(js_code(event_manager), "eventVariant.deep_unpack", "row[3]", "row[4]", "row[5]")\n    require_tokens(event_manager, "add_or_update_in_range(", "NativeBoundary.decodeEventSnapshot")\n'''
)
replace_once(
    "tests/test-settings.py",
    '''    forbid_tokens(\n        js_code(calendar),\n        "_addCivilDays", "_gregorianWeekday", "while (cellsPlaced < 42)"\n    )\n''',
    '''    forbid_tokens(\n        js_code(calendar),\n        "_addCivilDays", "_gregorianWeekday", "while (cellsPlaced < 42)",\n        "model.deep_unpack", "record[1]", "record[2]", "record[3]"\n    )\n    require_tokens(calendar, "NativeBoundary.decodeCalendarGrid", "NativeBoundary.decodeCivilDate")\n    require_tokens(native_boundary, "decodeTemporalPolicy", "decodeCalendarGrid", "decodeEventSnapshot")\n    forbid_tokens(js_code(applet), "variant.deep_unpack")\n'''
)

# Keep architecture documentation truthful.
replace_once(
    "docs/ARCHITECTURE.md",
    '''Cinnamon owns panel integration, settings presentation, lifecycle and CalendarServer transport mechanics. Calendar now owns deterministic arithmetic/navigation for the fixed-rule calendar families it can validate directly.''',
    '''Cinnamon owns panel integration, settings presentation, lifecycle and CalendarServer transport mechanics. Live applet and desktop settings terminate at the composition root; lower presentation modules receive resolved scalar policy rather than settings objects. A dedicated JavaScript native-boundary decoder is the sole presentation-side owner of positional GVariant output layouts, while raw CalendarServer input tuples are interpreted only by the native event adapter. Calendar now owns deterministic arithmetic/navigation for the fixed-rule calendar families it can validate directly.'''
)

# Patch release metadata.
replace_once("Makefile", "VERSION := 1.0.86", "VERSION := 1.0.87")
replace_once("src/cinnamon/metadata.json", '    "version": "1.0.86",', '    "version": "1.0.87",')
replace_once("README.md", "**Stable release:** 1.0.86\\", "**Stable release:** 1.0.87\\")
replace_once(
    "CHANGELOG.md",
    '''## Unreleased\n\nNo unreleased changes.\n\n## 1.0.86 - 2026-10-10\n''',
    '''## Unreleased\n\nNo unreleased changes.\n\n## 1.0.87 - 2026-10-11\n\n- Make the native event adapter the sole owner of CalendarServer tuple decoding and visible-range admission; Cinnamon JavaScript no longer depends on external tuple positions.\n- Terminate live applet and desktop settings at the composition root. Calendar, agenda and panel-clock modules receive only the scalar policy they require.\n- Add a named native-boundary decoder for civil dates, temporal policy, calendar-grid rows and event snapshots so positional GVariant layouts no longer leak through presentation code.\n- Add native admission, JavaScript boundary-shape and architecture regressions for the decoupled contracts.\n\n## 1.0.86 - 2026-10-10\n'''
)
replace_once(
    "debian/changelog",
    '''infiltrator-calendar (1.0.86) unstable; urgency=medium\n''',
    '''infiltrator-calendar (1.0.87) unstable; urgency=medium\n\n  * Make native code the sole owner of CalendarServer tuple parsing/range admission.\n  * Keep live settings objects at the applet composition root.\n  * Decode positional native output once into named JavaScript boundary objects.\n  * Add regression coverage for the new ownership boundaries.\n\n -- Shannon Smith <noreply@the-infiltratr.com>  Sun, 11 Oct 2026 21:20:00 +1100\n\ninfiltrator-calendar (1.0.86) unstable; urgency=medium\n'''
)

# Ensure this one-shot helper cannot accidentally become product source.
print("Boundary decoupling transformations applied successfully.")
