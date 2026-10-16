/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Platform-neutral event index and snapshot contract.
 */

#ifndef CALENDAR_EVENT_CORE_H
#define CALENDAR_EVENT_CORE_H

#include "event-types.h"

G_BEGIN_DECLS

enum
{
    CALENDAR_EVENT_MAX_ID_BYTES = 4096,
    CALENDAR_EVENT_MAX_COLOR_BYTES = 9,
    CALENDAR_EVENT_MAX_SUMMARY_BYTES = 65536
};

typedef struct _CalendarEventIndex CalendarEventIndex;

/*
 * Borrowed transport input. start_unix and end_unix are Unix seconds. Timed
 * end points are inclusive. For all-day events, end_unix is the exclusive
 * following-midnight endpoint; the index converts it to an inclusive final
 * second exactly once. modified is source metadata; update_timestamp is the
 * caller's refresh token used only for culling.
 */
typedef struct
{
    const gchar *id;
    const gchar *color;
    const gchar *summary;
    gboolean all_day;
    gint64 start_unix;
    gint64 end_unix;
    gint64 modified;
    gint64 update_timestamp;
} CalendarEventInput;

typedef struct
{
    /* All strings are owned by the containing snapshot. */
    gchar *id;
    gchar *color;
    gchar *summary;
    gboolean all_day;
    gboolean multi_day;
    gint64 start_unix;
    gint64 end_unix;
    gint64 start_day_unix;
    gint64 end_day_unix;
    gint64 modified;
} CalendarEvent;

typedef struct
{
    /* Opaque change token; compare for equality rather than interpreting it. */
    gint64 revision;
    gsize length;
    /* Owned array of @length deep-copied records. */
    CalendarEvent *events;
} CalendarEventSnapshot;

typedef struct
{
    CalendarEventState state;
    gint64 seconds_until_start;
    gint64 seconds_until_end;
} CalendarEventTiming;

typedef struct
{
    gsize day_count;
    /* One NULL-terminated colour vector per requested local civil day. */
    gchar ***colors;
} CalendarEventColorRange;


/** Returns: (transfer full): new single-owner-thread event index. */
CalendarEventIndex *calendar_event_index_new(void);

/** Accepts %NULL; no cross-thread synchronisation is implied. */
void calendar_event_index_free(CalendarEventIndex *index);

/** Returns: whether input satisfies size, UTF-8 and colour rules. */
gboolean calendar_event_input_is_valid(
    const CalendarEventInput *input);
/** Inserts/replaces one event; unchanged refreshes do not advance revision. */
gboolean calendar_event_index_upsert(
    CalendarEventIndex *index,
    const CalendarEventInput *input);
/** Removes one id and advances revision only when it existed. */
gboolean calendar_event_index_remove(CalendarEventIndex *index,
                                          const gchar *id);
/** Removes records older than the caller's completed refresh token. */
gboolean calendar_event_index_cull(CalendarEventIndex *index,
                                        gint64 minimum_update_timestamp);
/** Clears all records, advancing revision only when state changed. */
void calendar_event_index_clear(CalendarEventIndex *index);

/** Recomputes derived local-day membership after a timezone change. */
gboolean calendar_event_index_refresh_timezone(
    CalendarEventIndex *index);

/**
 * calendar_event_index_colors:
 * @index: event index
 * @local_day_unix: any Unix instant on the requested local day
 * @now_unix: current Unix seconds used for today's ordering
 *
 * Returns: (transfer full) (array zero-terminated=1): stable ordered colours
 * for events intersecting the requested local day.
 */
gchar **calendar_event_index_colors(CalendarEventIndex *index,
                                         gint64 local_day_unix,
                                         gint64 now_unix);


/**
 * Builds bounded colour vectors for consecutive local civil days in one index
 * scan.  Day stepping uses GLib civil-day arithmetic rather than 86,400-second
 * increments so DST and offset transitions cannot shift bucket identity.
 */
CalendarEventColorRange *calendar_event_index_color_range(
    CalendarEventIndex *index,
    gint64 first_local_day_unix,
    gsize day_count,
    gint64 now_unix,
    gsize maximum_colors_per_day);
void calendar_event_color_range_free(CalendarEventColorRange *range);

/**
 * calendar_event_index_snapshot:
 * @index: event index
 * @local_day_unix: any Unix instant on the requested local day
 * @now_unix: current Unix seconds used for today's agenda grouping
 *
 * Returns: (transfer full) (nullable): deep-copied immutable snapshot
 */
CalendarEventSnapshot *calendar_event_index_snapshot(
    CalendarEventIndex *index,
    gint64 local_day_unix,
    gint64 now_unix);
/** Accepts %NULL and frees every deep-copied snapshot string. */
void calendar_event_snapshot_free(CalendarEventSnapshot *snapshot);

/** Returns a bitmask describing an event interval relative to one local day. */
guint calendar_event_day_relation(gint64 start_day_unix,
                                       gint64 end_day_unix,
                                       gint64 comparison_unix);
/** Returns past/present/future, or INVALID for an inverted range. */
CalendarEventState calendar_event_state(gint64 start_unix,
                                                  gint64 end_unix,
                                                  gint64 now_unix);
/** Returns state plus saturating signed seconds to start and end. */
CalendarEventTiming calendar_event_calculate_timing(
    gint64 start_unix,
    gint64 end_unix,
    gint64 now_unix);

G_END_DECLS

#endif
