// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Event normalisation, interval indexing, ordering and snapshots.
 *
 * Inputs and results are ordinary C records. Transport-specific tuple parsing
 * and presentation-specific marshalling belong to adapters. Long events remain
 * one record: two sorted temporal indexes narrow interval queries without
 * expanding an event into one record per covered day.
 */

#include "event-core.h"

#include "julian-day.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/utf8.h>
#include <string.h>

typedef struct
{
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
    gint64 last_update_timestamp;
    guint64 sequence;
    GSequenceIter *start_iter;
    GSequenceIter *end_iter;
} EventRecord;

struct _CalendarEventIndex
{
    /*
     * The hash key is the EventRecord-owned id pointer; the table therefore
     * destroys values only. Replacements use steal-before-free so the key can
     * never outlive, or be freed twice with, its owning record.
     */
    GHashTable *events_by_id;
    /*
     * These sequences contain non-owning EventRecord pointers sorted only by
     * local start/end day. A point or visible-range query can inspect the
     * smaller candidate side instead of walking the complete identity table.
     */
    GSequence *events_by_start;
    GSequence *events_by_end;
    /* Stable insertion order breaks equal-time ties without reordering rows. */
    guint64 next_sequence;
    /* Opaque strictly increasing snapshot token; never wall-clock metadata. */
    gint64 revision;
};

static void
event_record_free(EventRecord *event)
{
    if (event == NULL)
        return;

    g_free(event->id);
    g_free(event->color);
    g_free(event->summary);
    g_free(event);
}

static void
touch_index(CalendarEventIndex *index)
{
    const gint64 now = g_get_monotonic_time();

    /*
     * Preserve a strict change token whenever another gint64 token exists.
     * Common's saturating add also keeps the theoretical terminal value
     * defined instead of relying on signed-overflow behaviour.
     */
    index->revision = now > index->revision ?
        now : infiltratr_i64_add_saturating(index->revision, 1);
}

static gboolean
local_day_start_for_civil(gint year,
                          gint month,
                          gint day,
                          gint64 *result)
{
    gint minute_of_day;

    if (result == NULL ||
        !calendar_gregorian_date_is_valid(year, month, day))
    {
        return FALSE;
    }

    /*
     * Some zones skip midnight and a few historical transitions skip an
     * entire civil date. Search the date itself rather than letting GLib
     * normalize into an adjacent day. Whole-day gaps deliberately return
     * FALSE so callers can preserve the unavailable civil date explicitly.
     */
    for (minute_of_day = 0; minute_of_day < 24 * 60; minute_of_day++)
    {
        g_autoptr(GDateTime) boundary =
            g_date_time_new_local(year,
                                  month,
                                  day,
                                  minute_of_day / 60,
                                  minute_of_day % 60,
                                  0.0);

        if (boundary != NULL &&
            g_date_time_get_year(boundary) == year &&
            g_date_time_get_month(boundary) == month &&
            g_date_time_get_day_of_month(boundary) == day)
        {
            *result = g_date_time_to_unix(boundary);
            return TRUE;
        }
    }

    return FALSE;
}

static gint64
local_day_start(gint64 unix_time)
{
    g_autoptr(GDateTime) instant =
        g_date_time_new_from_unix_local(unix_time);
    gint64 boundary;

    if (instant == NULL)
        return unix_time;

    if (local_day_start_for_civil(g_date_time_get_year(instant),
                                  g_date_time_get_month(instant),
                                  g_date_time_get_day_of_month(instant),
                                  &boundary))
    {
        return boundary;
    }

    /* A wholly skipped civil date has no local boundary to return. */
    return unix_time;
}

guint
calendar_event_day_relation(gint64 start_day_unix,
                                 gint64 end_day_unix,
                                 gint64 comparison_unix)
{
    const gint64 start_day = local_day_start(start_day_unix);
    const gint64 end_day = local_day_start(end_day_unix);
    const gint64 comparison_day = local_day_start(comparison_unix);
    guint relation = CALENDAR_EVENT_DAY_RELATION_NONE;

    if (start_day == comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_STARTS_ON_DAY;
    if (end_day == comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_ENDS_ON_DAY;
    if (start_day < comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_STARTED_BEFORE_DAY;
    if (end_day < comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_ENDED_BEFORE_DAY;
    if (end_day > comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_ENDS_AFTER_DAY;
    if (start_day > comparison_day)
        relation |= CALENDAR_EVENT_DAY_RELATION_STARTED_AFTER_DAY;

    return relation;
}

CalendarEventState
calendar_event_state(gint64 start_unix,
                          gint64 end_unix,
                          gint64 now_unix)
{
    if (end_unix < start_unix)
        return CALENDAR_EVENT_STATE_INVALID;
    if (end_unix < now_unix)
        return CALENDAR_EVENT_STATE_PAST;
    if (start_unix > now_unix)
        return CALENDAR_EVENT_STATE_FUTURE;
    return CALENDAR_EVENT_STATE_PRESENT;
}

CalendarEventTiming
calendar_event_calculate_timing(gint64 start_unix,
                                     gint64 end_unix,
                                     gint64 now_unix)
{
    const CalendarEventTiming result = {
        calendar_event_state(start_unix, end_unix, now_unix),
        infiltratr_i64_subtract_saturating(start_unix, now_unix),
        infiltratr_i64_subtract_saturating(end_unix, now_unix)
    };

    return result;
}

static gboolean
text_is_valid(const gchar *text,
              gsize maximum_bytes,
              gboolean allow_empty)
{
    gsize length;

    if (text == NULL)
        return FALSE;
    length = strlen(text);
    return length <= maximum_bytes && (allow_empty || length > 0) &&
           infiltratr_utf8_validate(text, (size_t)length);
}

static gboolean
unix_time_is_local_datetime(gint64 unix_time)
{
    g_autoptr(GDateTime) value =
        g_date_time_new_from_unix_local(unix_time);

    return value != NULL;
}

static gboolean
color_is_valid(const gchar *color)
{
    gsize index;
    const gsize length = color != NULL ? strlen(color) : 0;

    if (length != 4 && length != 7 && length != 9)
        return FALSE;
    if (color[0] != '#')
        return FALSE;

    for (index = 1; index < length; index++)
    {
        if (!infiltratr_ascii_is_xdigit((unsigned char)color[index]))
            return FALSE;
    }
    return TRUE;
}

gboolean
calendar_event_input_is_valid(const CalendarEventInput *input)
{
    return input != NULL &&
           text_is_valid(input->id,
                         CALENDAR_EVENT_MAX_ID_BYTES,
                         FALSE) &&
           text_is_valid(input->color,
                         CALENDAR_EVENT_MAX_COLOR_BYTES,
                         FALSE) &&
           color_is_valid(input->color) &&
           text_is_valid(input->summary,
                         CALENDAR_EVENT_MAX_SUMMARY_BYTES,
                         TRUE) &&
           unix_time_is_local_datetime(input->start_unix) &&
           unix_time_is_local_datetime(input->end_unix) &&
           input->end_unix >= input->start_unix &&
           (!input->all_day || input->end_unix > input->start_unix);
}

static gint64
event_end_day_unix(const EventRecord *event)
{
    gint64 last_occupied_unix = event->end_unix;

    /*
     * Timed CalendarServer intervals are half-open: [start, end). A positive-
     * duration event ending exactly at 00:00 therefore occupies the preceding
     * civil day, not the day containing its endpoint. All-day records have
     * already had their exclusive following-midnight endpoint normalized by
     * one second during ingestion, while zero-duration timed events remain on
     * the civil day containing their single instant.
     */
    if (!event->all_day && event->end_unix > event->start_unix)
        last_occupied_unix--;

    return local_day_start(last_occupied_unix);
}

static EventRecord *
event_record_from_input(const CalendarEventInput *input)
{
    EventRecord *event;

    if (!calendar_event_input_is_valid(input))
        return NULL;

    event = g_new0(EventRecord, 1);
    event->id = g_strdup(input->id);
    event->color = g_strdup(input->color);
    event->summary = g_strdup(input->summary);
    event->all_day = input->all_day;
    event->start_unix = input->start_unix;
    event->end_unix = input->end_unix;
    event->modified = input->modified;
    event->last_update_timestamp = input->update_timestamp;

    /* The source contract supplies all-day end at following midnight. */
    if (event->all_day)
        event->end_unix--;

    event->start_day_unix = local_day_start(event->start_unix);
    event->end_day_unix = event_end_day_unix(event);
    event->multi_day = event->start_day_unix != event->end_day_unix;
    return event;
}

static gint
compare_start_day(gconstpointer left,
                  gconstpointer right,
                  gpointer user_data)
{
    const EventRecord *a = left;
    const EventRecord *b = right;

    (void)user_data;
    if (a->start_day_unix < b->start_day_unix)
        return -1;
    if (a->start_day_unix > b->start_day_unix)
        return 1;
    return 0;
}

static gint
compare_end_day(gconstpointer left,
                gconstpointer right,
                gpointer user_data)
{
    const EventRecord *a = left;
    const EventRecord *b = right;

    (void)user_data;
    if (a->end_day_unix < b->end_day_unix)
        return -1;
    if (a->end_day_unix > b->end_day_unix)
        return 1;
    return 0;
}

static void
index_record(CalendarEventIndex *index,
             EventRecord *event)
{
    event->start_iter = g_sequence_insert_sorted(index->events_by_start,
                                                  event,
                                                  compare_start_day,
                                                  NULL);
    event->end_iter = g_sequence_insert_sorted(index->events_by_end,
                                                event,
                                                compare_end_day,
                                                NULL);
}

static void
unindex_record(EventRecord *event)
{
    if (event->start_iter != NULL)
    {
        g_sequence_remove(event->start_iter);
        event->start_iter = NULL;
    }
    if (event->end_iter != NULL)
    {
        g_sequence_remove(event->end_iter);
        event->end_iter = NULL;
    }
}

static void
rebuild_temporal_indexes(CalendarEventIndex *index)
{
    GHashTableIter iter;
    gpointer value;

    g_sequence_remove_range(g_sequence_get_begin_iter(index->events_by_start),
                            g_sequence_get_end_iter(index->events_by_start));
    g_sequence_remove_range(g_sequence_get_begin_iter(index->events_by_end),
                            g_sequence_get_end_iter(index->events_by_end));

    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        EventRecord *event = value;

        event->start_iter = NULL;
        event->end_iter = NULL;
        index_record(index, event);
    }
}

CalendarEventIndex *
calendar_event_index_new(void)
{
    CalendarEventIndex *index = g_new0(CalendarEventIndex, 1);

    /*
     * Keys alias EventRecord.id, so no key destroy function is installed.
     * EventRecord owns all strings and is the sole lifetime authority.
     */
    index->events_by_id =
        g_hash_table_new_full(g_str_hash,
                              g_str_equal,
                              NULL,
                              (GDestroyNotify)event_record_free);
    index->events_by_start = g_sequence_new(NULL);
    index->events_by_end = g_sequence_new(NULL);
    index->revision = g_get_monotonic_time();
    return index;
}

void
calendar_event_index_free(CalendarEventIndex *index)
{
    if (index == NULL)
        return;

    g_sequence_free(index->events_by_start);
    g_sequence_free(index->events_by_end);
    index->events_by_start = NULL;
    index->events_by_end = NULL;
    g_hash_table_destroy(index->events_by_id);
    index->events_by_id = NULL;
    g_free(index);
}

gboolean
calendar_event_index_remove(CalendarEventIndex *index,
                                 const gchar *id)
{
    EventRecord *event;

    if (index == NULL || id == NULL)
        return FALSE;

    event = g_hash_table_lookup(index->events_by_id, id);
    if (event == NULL)
        return FALSE;

    unindex_record(event);
    g_hash_table_steal(index->events_by_id, id);
    event_record_free(event);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_event_index_upsert(CalendarEventIndex *index,
                                 const CalendarEventInput *input)
{
    EventRecord *existing;
    EventRecord *replacement;

    if (index == NULL)
        return FALSE;

    replacement = event_record_from_input(input);
    if (replacement == NULL)
        return FALSE;

    existing = g_hash_table_lookup(index->events_by_id, replacement->id);

    /*
     * CalendarServer can finish SetTimeRange before an older asynchronous view
     * has completely stopped emitting. A positive LAST-MODIFIED/CREATED value
     * therefore acts as a monotonic source revision when one is available.
     * Never let a known-older transport record replace newer indexed content.
     */
    if (existing != NULL &&
        existing->modified > 0 &&
        replacement->modified > 0 &&
        replacement->modified < existing->modified)
    {
        event_record_free(replacement);
        return FALSE;
    }

    /*
     * A CalendarServer refresh may resend unchanged records with a new refresh
     * token. Updating only last_update_timestamp preserves culling semantics
     * without incrementing the public revision or rebuilding agenda rows.
     */
    if (existing != NULL &&
        existing->modified == replacement->modified &&
        existing->all_day == replacement->all_day &&
        existing->start_unix == replacement->start_unix &&
        existing->end_unix == replacement->end_unix &&
        infiltratr_string_equal(existing->color, replacement->color) &&
        infiltratr_string_equal(existing->summary, replacement->summary))
    {
        existing->last_update_timestamp = input->update_timestamp;
        event_record_free(replacement);
        return FALSE;
    }

    if (existing != NULL)
    {
        replacement->sequence = existing->sequence;
    }
    else
    {
        index->next_sequence =
            infiltratr_u64_add_saturating(index->next_sequence, 1U);
        replacement->sequence = index->next_sequence;
    }
    if (existing != NULL)
    {
        unindex_record(existing);
        g_hash_table_steal(index->events_by_id, existing->id);
        event_record_free(existing);
    }

    g_hash_table_insert(index->events_by_id,
                        replacement->id,
                        replacement);
    index_record(index, replacement);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_event_index_cull(CalendarEventIndex *index,
                               gint64 minimum_update_timestamp)
{
    g_autoptr(GPtrArray) stale_ids =
        g_ptr_array_new_with_free_func(g_free);
    GHashTableIter iter;
    gpointer value;
    guint item;

    if (index == NULL)
        return FALSE;

    /*
     * Collect ids first instead of removing through the active iterator.
     * Removal deliberately goes through the public mutation path so each
     * actual deletion advances the revision token consistently.
     */
    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        const EventRecord *event = value;

        if (event->last_update_timestamp < minimum_update_timestamp)
            g_ptr_array_add(stale_ids, g_strdup(event->id));
    }

    for (item = 0; item < stale_ids->len; item++)
    {
        const gchar *id = g_ptr_array_index(stale_ids, item);

        calendar_event_index_remove(index, id);
    }
    return stale_ids->len > 0;
}

void
calendar_event_index_clear(CalendarEventIndex *index)
{
    gboolean changed;

    if (index == NULL)
        return;

    changed = g_hash_table_size(index->events_by_id) > 0;
    if (changed)
    {
        g_sequence_remove_range(g_sequence_get_begin_iter(index->events_by_start),
                                g_sequence_get_end_iter(index->events_by_start));
        g_sequence_remove_range(g_sequence_get_begin_iter(index->events_by_end),
                                g_sequence_get_end_iter(index->events_by_end));
        g_hash_table_remove_all(index->events_by_id);
        touch_index(index);
    }
}

gboolean
calendar_event_index_refresh_timezone(CalendarEventIndex *index)
{
    GHashTableIter iter;
    gpointer value;
    gboolean changed = FALSE;

    if (index == NULL)
        return FALSE;

    /*
     * UTC event instants do not change when the timezone does, but their local
     * civil-day membership can. Recompute only the derived day boundaries and
     * rebuild the two non-owning temporal orderings once if anything changed.
     */
    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        EventRecord *event = value;
        const gint64 start_day = local_day_start(event->start_unix);
        const gint64 end_day = event_end_day_unix(event);
        const gboolean multi_day = start_day != end_day;

        if (start_day != event->start_day_unix ||
            end_day != event->end_day_unix ||
            multi_day != event->multi_day)
        {
            event->start_day_unix = start_day;
            event->end_day_unix = end_day;
            event->multi_day = multi_day;
            changed = TRUE;
        }
    }

    if (changed)
    {
        rebuild_temporal_indexes(index);
        touch_index(index);
    }
    return changed;
}

static gint
compare_records(gconstpointer left,
                gconstpointer right)
{
    const EventRecord *a = *(EventRecord *const *)left;
    const EventRecord *b = *(EventRecord *const *)right;

    if (a->start_unix < b->start_unix)
        return -1;
    if (a->start_unix > b->start_unix)
        return 1;
    if (a->end_unix < b->end_unix)
        return -1;
    if (a->end_unix > b->end_unix)
        return 1;
    if (a->sequence < b->sequence)
        return -1;
    if (a->sequence > b->sequence)
        return 1;
    return g_strcmp0(a->id, b->id);
}

static GSequenceIter *
start_day_upper_bound(CalendarEventIndex *index,
                      gint64 day)
{
    EventRecord probe = { .start_day_unix = day };
    GSequenceIter *iter = g_sequence_search(index->events_by_start,
                                            &probe,
                                            compare_start_day,
                                            NULL);

    while (!g_sequence_iter_is_end(iter))
    {
        const EventRecord *event = g_sequence_get(iter);

        if (event->start_day_unix != day)
            break;
        iter = g_sequence_iter_next(iter);
    }
    return iter;
}

static GSequenceIter *
end_day_lower_bound(CalendarEventIndex *index,
                    gint64 day)
{
    EventRecord probe = { .end_day_unix = day };
    GSequenceIter *iter = g_sequence_search(index->events_by_end,
                                            &probe,
                                            compare_end_day,
                                            NULL);

    while (!g_sequence_iter_is_begin(iter))
    {
        GSequenceIter *previous = g_sequence_iter_prev(iter);
        const EventRecord *event = g_sequence_get(previous);

        if (event->end_day_unix != day)
            break;
        iter = previous;
    }
    return iter;
}

static void
append_overlapping_candidates(CalendarEventIndex *index,
                              gint64 first_day,
                              gint64 last_day,
                              GPtrArray *result)
{
    GSequenceIter *start_stop;
    GSequenceIter *end_start;
    GSequenceIter *iter;
    gsize start_count;
    gsize end_count;
    const gsize total = (gsize)g_sequence_get_length(index->events_by_start);

    if (first_day > last_day || total == 0)
        return;

    start_stop = start_day_upper_bound(index, last_day);
    end_start = end_day_lower_bound(index, first_day);
    start_count = (gsize)g_sequence_iter_get_position(start_stop);
    end_count = total - (gsize)g_sequence_iter_get_position(end_start);

    if (start_count <= end_count)
    {
        for (iter = g_sequence_get_begin_iter(index->events_by_start);
             iter != start_stop;
             iter = g_sequence_iter_next(iter))
        {
            EventRecord *event = g_sequence_get(iter);

            if (event->end_day_unix >= first_day)
                g_ptr_array_add(result, event);
        }
        return;
    }

    for (iter = end_start;
         !g_sequence_iter_is_end(iter);
         iter = g_sequence_iter_next(iter))
    {
        EventRecord *event = g_sequence_get(iter);

        if (event->start_day_unix <= last_day)
            g_ptr_array_add(result, event);
    }
}

typedef struct
{
    gboolean today;
    gint64 now_unix;
} EventOrderingContext;

static guint
event_order_group(const EventRecord *event,
                  const EventOrderingContext *context)
{
    if (!context->today)
        return 0;
    if (!event->all_day && event->end_unix < context->now_unix)
        return 0;
    if (event->all_day)
        return 1;
    return 2;
}

static gint
compare_records_for_day(gconstpointer left,
                        gconstpointer right,
                        gpointer user_data)
{
    const EventRecord *a = *(EventRecord *const *)left;
    const EventRecord *b = *(EventRecord *const *)right;
    const EventOrderingContext *context = user_data;
    const guint a_group = event_order_group(a, context);
    const guint b_group = event_order_group(b, context);

    if (a_group < b_group)
        return -1;
    if (a_group > b_group)
        return 1;
    return compare_records(left, right);
}

static void
sort_records_for_day(GPtrArray *records,
                     gint64 requested_day,
                     gint64 now_unix)
{
    EventOrderingContext context = {
        requested_day == local_day_start(now_unix),
        now_unix
    };

    g_ptr_array_sort_with_data(records, compare_records_for_day, &context);
}

static GPtrArray *
ordered_records(CalendarEventIndex *index,
                gint64 local_day_unix,
                gint64 now_unix)
{
    const gint64 requested_day = local_day_start(local_day_unix);
    GPtrArray *ordered = g_ptr_array_new();

    append_overlapping_candidates(index,
                                  requested_day,
                                  requested_day,
                                  ordered);
    /*
     * One ordering authority serves both agenda snapshots and grid colours.
     * Non-today dates use chronological order. Today groups ended timed events,
     * all-day events, then current/future timed events, with deterministic
     * chronological ordering inside each group.
     */
    sort_records_for_day(ordered, requested_day, now_unix);
    return ordered;
}

gchar **
calendar_event_index_colors(CalendarEventIndex *index,
                                 gint64 local_day_unix,
                                 gint64 now_unix)
{
    g_autoptr(GPtrArray) ordered = NULL;
    gchar **colors;
    guint item;

    if (index == NULL)
        return NULL;

    /*
     * Reuse agenda ordering semantics, including today's ended/all-day/current
     * grouping, but copy only colour strings rather than complete event rows.
     */
    ordered = ordered_records(index, local_day_unix, now_unix);
    colors = g_new0(gchar *, (gsize)ordered->len + 1U);
    for (item = 0; item < ordered->len; item++)
    {
        const EventRecord *event = g_ptr_array_index(ordered, item);
        colors[item] = g_strdup(event->color);
    }
    return colors;
}

static gsize
lower_bound_day(const gint64 *days,
                gsize count,
                gint64 target)
{
    gsize low = 0;
    gsize high = count;

    while (low < high)
    {
        const gsize middle = low + (high - low) / 2;

        if (days[middle] < target)
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

static gboolean
build_local_day_range(gint64 first_local_day_unix,
                      gsize day_count,
                      gint64 *days,
                      gboolean *day_valid)
{
    g_autoptr(GDateTime) first = NULL;
    gint64 first_jdn;
    gint first_year;
    gint first_month;
    gint first_day;
    gsize item;

    if (days == NULL || day_valid == NULL || day_count == 0)
        return FALSE;

    first = g_date_time_new_from_unix_local(
        local_day_start(first_local_day_unix));
    if (first == NULL)
        return FALSE;

    first_year = g_date_time_get_year(first);
    first_month = g_date_time_get_month(first);
    first_day = g_date_time_get_day_of_month(first);
    first_jdn = calendar_gregorian_to_jdn(
        first_year, first_month, first_day);

    for (item = 0; item < day_count; item++)
    {
        const gint64 jdn = calendar_i64_add_saturating(
            first_jdn, (gint64)item);
        gint year;
        gint month;
        gint day;

        calendar_jdn_to_gregorian(jdn, &year, &month, &day);
        day_valid[item] =
            local_day_start_for_civil(year, month, day, &days[item]);
    }

    return TRUE;
}

static gchar **
copy_ordered_bucket_colors(GPtrArray *bucket,
                           gint64 requested_day,
                           gint64 now_unix,
                           gsize maximum_colors)
{
    gchar **colors;
    gsize copied = 0;
    guint item;
    const gsize limit = MIN((gsize)bucket->len, maximum_colors);

    colors = g_new0(gchar *, limit + 1U);
    if (limit == 0)
        return colors;

    sort_records_for_day(bucket, requested_day, now_unix);
    for (item = 0; item < bucket->len && copied < limit; item++)
    {
        const EventRecord *event = g_ptr_array_index(bucket, item);
        colors[copied++] = g_strdup(event->color);
    }
    return colors;
}

CalendarEventColorRange *
calendar_event_index_color_range(CalendarEventIndex *index,
                                      gint64 first_local_day_unix,
                                      gsize day_count,
                                      gint64 now_unix,
                                      gsize maximum_colors_per_day)
{
    enum { MAX_RANGE_DAYS = 366 };
    CalendarEventColorRange *range;
    g_autofree gint64 *days = NULL;
    g_autofree gboolean *day_valid = NULL;
    g_autofree gint64 *valid_days = NULL;
    g_autofree gsize *valid_indexes = NULL;
    g_autoptr(GPtrArray) candidates = g_ptr_array_new();
    GPtrArray **buckets;
    gsize valid_count = 0;
    gsize item;

    if (index == NULL || day_count == 0 || day_count > MAX_RANGE_DAYS)
        return NULL;

    days = g_new0(gint64, day_count);
    day_valid = g_new0(gboolean, day_count);
    if (!build_local_day_range(
            first_local_day_unix, day_count, days, day_valid))
    {
        return NULL;
    }

    valid_days = g_new0(gint64, day_count);
    valid_indexes = g_new0(gsize, day_count);
    buckets = g_new0(GPtrArray *, day_count);
    for (item = 0; item < day_count; item++)
    {
        buckets[item] = g_ptr_array_new();
        if (day_valid[item])
        {
            valid_days[valid_count] = days[item];
            valid_indexes[valid_count] = item;
            valid_count++;
        }
    }

    if (valid_count > 0)
    {
        append_overlapping_candidates(index,
                                      valid_days[0],
                                      valid_days[valid_count - 1U],
                                      candidates);
    }

    /*
     * The temporal indexes bound the candidate set first. For each surviving
     * event, binary-search the first relevant day and walk only its intersecting
     * span instead of scanning every indexed event for every visible cell.
     */
    for (item = 0; item < candidates->len; item++)
    {
        EventRecord *event = g_ptr_array_index(candidates, item);
        gsize first;
        gsize day_index;

        first = lower_bound_day(
            valid_days, valid_count, event->start_day_unix);
        if (first >= valid_count ||
            valid_days[first] > event->end_day_unix)
        {
            continue;
        }

        for (day_index = first;
             day_index < valid_count &&
             valid_days[day_index] <= event->end_day_unix;
             day_index++)
        {
            g_ptr_array_add(buckets[valid_indexes[day_index]], event);
        }
    }

    /*
     * These temporary lookup arrays are no longer needed once the candidate
     * events have populated the per-day buckets. Release them before allocating
     * the returned range so ownership stays explicit.
     */
    g_clear_pointer(&valid_days, g_free);
    g_clear_pointer(&valid_indexes, g_free);

    range = g_new0(CalendarEventColorRange, 1);
    range->day_count = day_count;
    range->colors = g_new0(gchar **, day_count);
    for (item = 0; item < day_count; item++)
    {
        range->colors[item] = day_valid[item] ?
            copy_ordered_bucket_colors(
                buckets[item],
                days[item],
                now_unix,
                maximum_colors_per_day) :
            g_new0(gchar *, 1U);
        g_ptr_array_unref(buckets[item]);
    }
    g_free((gpointer)buckets);
    return range;
}

void
calendar_event_color_range_free(CalendarEventColorRange *range)
{
    gsize item;

    if (range == NULL)
        return;
    for (item = 0; item < range->day_count; item++)
        g_strfreev(range->colors[item]);
    g_free((gpointer)range->colors);
    g_free(range);
}

CalendarEventSnapshot *
calendar_event_index_snapshot(CalendarEventIndex *index,
                                   gint64 local_day_unix,
                                   gint64 now_unix)
{
    g_autoptr(GPtrArray) ordered = NULL;
    CalendarEventSnapshot *snapshot;
    guint item;

    if (index == NULL)
        return NULL;

    ordered = ordered_records(index, local_day_unix, now_unix);
    /*
     * Snapshots deep-copy transport-facing text so the caller can retain a
     * view while the mutable index accepts the next CalendarServer burst.
     */
    snapshot = g_new0(CalendarEventSnapshot, 1);
    snapshot->revision = index->revision;
    snapshot->length = ordered->len;
    snapshot->events = g_new0(CalendarEvent, snapshot->length);

    for (item = 0; item < ordered->len; item++)
    {
        const EventRecord *source = g_ptr_array_index(ordered, item);
        CalendarEvent *target = &snapshot->events[item];

        target->id = g_strdup(source->id);
        target->color = g_strdup(source->color);
        target->summary = g_strdup(source->summary);
        target->all_day = source->all_day;
        target->multi_day = source->multi_day;
        target->start_unix = source->start_unix;
        target->end_unix = source->end_unix;
        target->start_day_unix = source->start_day_unix;
        target->end_day_unix = source->end_day_unix;
        target->modified = source->modified;
    }

    return snapshot;
}

void
calendar_event_snapshot_free(CalendarEventSnapshot *snapshot)
{
    gsize item;

    if (snapshot == NULL)
        return;

    for (item = 0; item < snapshot->length; item++)
    {
        g_free(snapshot->events[item].id);
        g_free(snapshot->events[item].color);
        g_free(snapshot->events[item].summary);
    }
    g_free(snapshot->events);
    g_free(snapshot);
}
