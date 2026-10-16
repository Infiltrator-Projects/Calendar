/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Transport-neutral event-source boundary.
 */

#ifndef CALENDAR_EVENT_SOURCE_H
#define CALENDAR_EVENT_SOURCE_H

#include "event-core.h"

G_BEGIN_DECLS

typedef struct _CalendarEventSink CalendarEventSink;
typedef struct _CalendarEventSource CalendarEventSource;

typedef gboolean (*CalendarEventSinkUpsertFunc)(
    gpointer context,
    const CalendarEventInput *input);
typedef gboolean (*CalendarEventSinkRemoveFunc)(gpointer context,
                                                    const gchar *id);
typedef gboolean (*CalendarEventSinkCullFunc)(gpointer context,
                                                  gint64 refresh_timestamp);
typedef void (*CalendarEventSinkClearFunc)(gpointer context);

struct _CalendarEventSink
{
    guint abi_version;
    gpointer context;
    CalendarEventSinkUpsertFunc upsert;
    CalendarEventSinkRemoveFunc remove;
    CalendarEventSinkCullFunc cull;
    CalendarEventSinkClearFunc clear;
};

typedef gboolean (*CalendarEventSourceStartFunc)(
    gpointer context,
    const CalendarEventSink *sink);
typedef void (*CalendarEventSourceStopFunc)(gpointer context);
typedef gboolean (*CalendarEventSourceRangeFunc)(gpointer context,
                                                     gint64 start_unix,
                                                     gint64 end_unix);
typedef gboolean (*CalendarEventSourceOpenFunc)(gpointer context,
                                                    const gchar *event_id);

struct _CalendarEventSource
{
    guint abi_version;
    const gchar *id;
    gpointer context;
    CalendarEventSourceStartFunc start;
    CalendarEventSourceStopFunc stop;
    CalendarEventSourceRangeFunc request_range;
    CalendarEventSourceOpenFunc open_event;
};

enum
{
    CALENDAR_EVENT_SINK_ABI = 1,
    CALENDAR_EVENT_SOURCE_ABI = 1
};

/**
 * calendar_event_index_make_sink:
 * @index: event index that must outlive the source connection
 * @sink: (out): same-thread callback table borrowing @index
 *
 * Sources should copy the table during start(); callbacks remain valid only
 * while @index lives.
 */
void calendar_event_index_make_sink(CalendarEventIndex *index,
                                         CalendarEventSink *sink);
/**
 * calendar_event_source_is_valid:
 * @source: transport-neutral source interface
 *
 * Validates ABI and callbacks required for lifecycle/range requests.
 * request_range() publishes overlapping events through the bound sink;
 * open_event is optional.
 *
 * Returns: whether the source table is safe to call.
 */
gboolean calendar_event_source_is_valid(
    const CalendarEventSource *source);

G_END_DECLS

#endif
