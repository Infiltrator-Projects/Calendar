// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Event normalisation, interval indexing, ordering and snapshots.
 *
 * Inputs and results are ordinary C records. Transport-specific tuple parsing
 * and presentation-specific marshalling belong to adapters. Long events remain
 * one record: day membership is an interval query rather than a per-day
 * expansion, so storage is independent of event duration.
 */

#include "event-core.h"

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
} EventRecord;

struct _CalendarPlusEventIndex
{
    /*
     * The hash key is the EventRecord-owned id pointer; the table therefore
     * destroys values only. Replacements use steal-before-free so the key can
     * never outlive, or be freed twice with, its owning record.
     */
    GHashTable *events_by_id;
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
touch_index(CalendarPlusEventIndex *index)
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

static gint64
local_day_start(gint64 unix_time)
{
    g_autoptr(GDateTime) instant =
        g_date_time_new_from_unix_local(unix_time);
    g_autoptr(GDateTime) boundary = NULL;
    gint year;
    gint month;
    gint day;
    gint minute_of_day;

    if (instant == NULL)
        return unix_time;

    year = g_date_time_get_year(instant);
    month = g_date_time_get_month(instant);
    day = g_date_time_get_day_of_month(instant);

    for (minute_of_day = 0; minute_of_day < 24 * 60; minute_of_day++)
    {
        boundary = g_date_time_new_local(year,
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
            return g_date_time_to_unix(boundary);
        }
        if (boundary != NULL)
        {
            g_date_time_unref(boundary);
            boundary = NULL;
        }
    }

    return unix_time;
}

guint
calendar_plus_event_day_relation(gint64 start_day_unix,
                                 gint64 end_day_unix,
                                 gint64 comparison_unix)
{
    const gint64 start_day = local_day_start(start_day_unix);
    const gint64 end_day = local_day_start(end_day_unix);
    const gint64 comparison_day = local_day_start(comparison_unix);
    guint relation = CALENDAR_PLUS_EVENT_DAY_RELATION_NONE;

    if (start_day == comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTS_ON_DAY;
    if (end_day == comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_ON_DAY;
    if (start_day < comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_BEFORE_DAY;
    if (end_day < comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDED_BEFORE_DAY;
    if (end_day > comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_AFTER_DAY;
    if (start_day > comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_AFTER_DAY;

    return relation;
}

CalendarPlusEventState
calendar_plus_event_state(gint64 start_unix,
                          gint64 end_unix,
                          gint64 now_unix)
{
    if (end_unix < start_unix)
        return CALENDAR_PLUS_EVENT_STATE_INVALID;
    if (end_unix < now_unix)
        return CALENDAR_PLUS_EVENT_STATE_PAST;
    if (start_unix > now_unix)
        return CALENDAR_PLUS_EVENT_STATE_FUTURE;
    return CALENDAR_PLUS_EVENT_STATE_PRESENT;
}

CalendarPlusEventTiming
calendar_plus_event_calculate_timing(gint64 start_unix,
                                     gint64 end_unix,
                                     gint64 now_unix)
{
    const CalendarPlusEventTiming result = {
        calendar_plus_event_state(start_unix, end_unix, now_unix),
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
calendar_plus_event_input_is_valid(const CalendarPlusEventInput *input)
{
    return input != NULL &&
           text_is_valid(input->id,
                         CALENDAR_PLUS_EVENT_MAX_ID_BYTES,
                         FALSE) &&
           text_is_valid(input->color,
                         CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES,
                         FALSE) &&
           color_is_valid(input->color) &&
           text_is_valid(input->summary,
                         CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES,
                         TRUE) &&
           unix_time_is_local_datetime(input->start_unix) &&
           unix_time_is_local_datetime(input->end_unix);
}

static EventRecord *
event_record_from_input(const CalendarPlusEventInput *input)
{
    EventRecord *event;

    if (!calendar_plus_event_input_is_valid(input))
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
    if (event->all_day && event->end_unix > G_MININT64)
        event->end_unix--;
    if (event->end_unix < event->start_unix)
        event->end_unix = event->start_unix;

    event->start_day_unix = local_day_start(event->start_unix);
    event->end_day_unix = local_day_start(event->end_unix);
    event->multi_day = event->start_day_unix != event->end_day_unix;
    return event;
}

CalendarPlusEventIndex *
calendar_plus_event_index_new(void)
{
    CalendarPlusEventIndex *index = g_new0(CalendarPlusEventIndex, 1);

    /*
     * Keys alias EventRecord.id, so no key destroy function is installed.
     * EventRecord owns all strings and is the sole lifetime authority.
     */
    index->events_by_id =
        g_hash_table_new_full(g_str_hash,
                              g_str_equal,
                              NULL,
                              (GDestroyNotify)event_record_free);
    index->revision = g_get_monotonic_time();
    return index;
}

void
calendar_plus_event_index_free(CalendarPlusEventIndex *index)
{
    if (index == NULL)
        return;

    g_hash_table_destroy(index->events_by_id);
    index->events_by_id = NULL;
    g_free(index);
}

gboolean
calendar_plus_event_index_remove(CalendarPlusEventIndex *index,
                                 const gchar *id)
{
    EventRecord *event;

    if (index == NULL || id == NULL)
        return FALSE;

    event = g_hash_table_lookup(index->events_by_id, id);
    if (event == NULL)
        return FALSE;

    g_hash_table_steal(index->events_by_id, id);
    event_record_free(event);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_plus_event_index_upsert(CalendarPlusEventIndex *index,
                                 const CalendarPlusEventInput *input)
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
        g_hash_table_steal(index->events_by_id, existing->id);
        event_record_free(existing);
    }

    g_hash_table_insert(index->events_by_id,
                        replacement->id,
                        replacement);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_plus_event_index_cull(CalendarPlusEventIndex *index,
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

        calendar_plus_event_index_remove(index, id);
    }
    return stale_ids->len > 0;
}

void
calendar_plus_event_index_clear(CalendarPlusEventIndex *index)
{
    gboolean changed;

    if (index == NULL)
        return;

    changed = g_hash_table_size(index->events_by_id) > 0;
    g_hash_table_remove_all(index->events_by_id);
    if (changed)
        touch_index(index);
}

gboolean
calendar_plus_event_index_refresh_timezone(CalendarPlusEventIndex *index)
{
    GHashTableIter iter;
    gpointer value;
    gboolean changed = FALSE;

    if (index == NULL)
        return FALSE;

    /*
     * UTC event instants do not change when the timezone does, but their local
     * civil-day membership can. Recompute only the derived day boundaries and
     * expose one revision change if any visible membership changed.
     */
    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        EventRecord *event = value;
        const gint64 start_day = local_day_start(event->start_unix);
        const gint64 end_day = local_day_start(event->end_unix);
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
        touch_index(index);
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

static GPtrArray *
ordered_records(CalendarPlusEventIndex *index,
                gint64 local_day_unix,
                gint64 now_unix)
{
    const gint64 requested_day = local_day_start(local_day_unix);
    GPtrArray *sorted = g_ptr_array_new();
    GPtrArray *ordered;
    GHashTableIter iter;
    gpointer value;
    guint item;

    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        const EventRecord *event = value;

        if (event->start_day_unix <= requested_day &&
            event->end_day_unix >= requested_day)
        {
            g_ptr_array_add(sorted, value);
        }
    }
    /*
     * Base ordering is deterministic for every day. Today's agenda then keeps
     * Cinnamon's useful visual grouping without mutating the stored index:
     * ended timed events, all-day events, then current/future timed events.
     */
    g_ptr_array_sort(sorted, compare_records);

    if (requested_day != local_day_start(now_unix))
        return sorted;

    /* Today's agenda groups ended timed, all-day, then active/future events. */
    ordered = g_ptr_array_new();
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (!event->all_day && event->end_unix < now_unix)
            g_ptr_array_add(ordered, event);
    }
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (event->all_day)
            g_ptr_array_add(ordered, event);
    }
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (!event->all_day && event->end_unix >= now_unix)
            g_ptr_array_add(ordered, event);
    }

    g_ptr_array_unref(sorted);
    return ordered;
}

gchar **
calendar_plus_event_index_colors(CalendarPlusEventIndex *index,
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
                      gint64 *days)
{
    g_autoptr(GDateTime) cursor = NULL;
    g_autoptr(GDateTime) first = NULL;
    gsize item;

    if (days == NULL || day_count == 0)
        return FALSE;

    first = g_date_time_new_from_unix_local(
        local_day_start(first_local_day_unix));
    if (first == NULL)
        return FALSE;

    cursor = first;
    first = NULL;
    for (item = 0; item < day_count; item++)
    {
        g_autoptr(GDateTime) next = NULL;

        days[item] = local_day_start(g_date_time_to_unix(cursor));
        if (item + 1 == day_count)
            break;

        next = g_date_time_add_days(cursor, 1);
        if (next == NULL)
            return FALSE;
        g_date_time_unref(cursor);
        cursor = next;
        next = NULL;
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
    guint pass;
    guint item;
    const gboolean today =
        requested_day == local_day_start(now_unix);
    const gsize limit = MIN((gsize)bucket->len, maximum_colors);

    colors = g_new0(gchar *, limit + 1U);
    if (limit == 0)
        return colors;

    g_ptr_array_sort(bucket, compare_records);
    if (!today)
    {
        for (item = 0; item < bucket->len && copied < limit; item++)
        {
            const EventRecord *event = g_ptr_array_index(bucket, item);
            colors[copied++] = g_strdup(event->color);
        }
        return colors;
    }

    /*
     * Match ordered_records() exactly for today's grid dots: ended timed,
     * all-day, then active/future timed events.
     */
    for (pass = 0; pass < 3 && copied < limit; pass++)
    {
        for (item = 0; item < bucket->len && copied < limit; item++)
        {
            const EventRecord *event = g_ptr_array_index(bucket, item);
            const gboolean include =
                pass == 0 ? (!event->all_day && event->end_unix < now_unix) :
                pass == 1 ? event->all_day :
                            (!event->all_day && event->end_unix >= now_unix);

            if (include)
                colors[copied++] = g_strdup(event->color);
        }
    }
    return colors;
}

CalendarPlusEventColorRange *
calendar_plus_event_index_color_range(CalendarPlusEventIndex *index,
                                      gint64 first_local_day_unix,
                                      gsize day_count,
                                      gint64 now_unix,
                                      gsize maximum_colors_per_day)
{
    enum { MAX_RANGE_DAYS = 366 };
    CalendarPlusEventColorRange *range;
    g_autofree gint64 *days = NULL;
    GPtrArray **buckets;
    GHashTableIter iter;
    gpointer value;
    gsize item;

    if (index == NULL || day_count == 0 || day_count > MAX_RANGE_DAYS)
        return NULL;

    days = g_new0(gint64, day_count);
    if (!build_local_day_range(first_local_day_unix, day_count, days))
        return NULL;

    buckets = g_new0(GPtrArray *, day_count);
    for (item = 0; item < day_count; item++)
        buckets[item] = g_ptr_array_new();

    /*
     * Traverse the hash once.  Binary-search the first relevant day, then walk
     * only the event's intersecting span instead of rescanning every event for
     * every visible calendar cell.
     */
    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        EventRecord *event = value;
        gsize first = lower_bound_day(days, day_count, event->start_day_unix);

        if (first >= day_count || days[first] > event->end_day_unix)
            continue;
        for (item = first;
             item < day_count && days[item] <= event->end_day_unix;
             item++)
        {
            g_ptr_array_add(buckets[item], event);
        }
    }

    range = g_new0(CalendarPlusEventColorRange, 1);
    range->day_count = day_count;
    range->colors = g_new0(gchar **, day_count);
    for (item = 0; item < day_count; item++)
    {
        range->colors[item] = copy_ordered_bucket_colors(
            buckets[item],
            days[item],
            now_unix,
            maximum_colors_per_day);
        g_ptr_array_unref(buckets[item]);
    }
    g_free((gpointer)buckets);
    return range;
}

void
calendar_plus_event_color_range_free(CalendarPlusEventColorRange *range)
{
    gsize item;

    if (range == NULL)
        return;
    for (item = 0; item < range->day_count; item++)
        g_strfreev(range->colors[item]);
    g_free((gpointer)range->colors);
    g_free(range);
}

CalendarPlusEventSnapshot *
calendar_plus_event_index_snapshot(CalendarPlusEventIndex *index,
                                   gint64 local_day_unix,
                                   gint64 now_unix)
{
    g_autoptr(GPtrArray) ordered = NULL;
    CalendarPlusEventSnapshot *snapshot;
    guint item;

    if (index == NULL)
        return NULL;

    ordered = ordered_records(index, local_day_unix, now_unix);
    /*
     * Snapshots deep-copy transport-facing text so the caller can retain a
     * view while the mutable index accepts the next CalendarServer burst.
     */
    snapshot = g_new0(CalendarPlusEventSnapshot, 1);
    snapshot->revision = index->revision;
    snapshot->length = ordered->len;
    snapshot->events = g_new0(CalendarPlusEvent, snapshot->length);

    for (item = 0; item < ordered->len; item++)
    {
        const EventRecord *source = g_ptr_array_index(ordered, item);
        CalendarPlusEvent *target = &snapshot->events[item];

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
calendar_plus_event_snapshot_free(CalendarPlusEventSnapshot *snapshot)
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
