/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Platform-neutral live-clock engine.
 */

#ifndef CALENDAR_CLOCK_ENGINE_H
#define CALENDAR_CLOCK_ENGINE_H

#include "time-formats.h"

G_BEGIN_DECLS

typedef struct _CalendarClockEngine CalendarClockEngine;
/* Zero is reserved for scheduling failure/no active timer. */
typedef guintptr CalendarClockTimer;
typedef void (*CalendarClockTimerFunc)(gpointer user_data);
typedef void (*CalendarClockTickFunc)(gpointer user_data);

typedef gint64 (*CalendarClockNowFunc)(gpointer context);
typedef gint (*CalendarClockUtcOffsetFunc)(gpointer context,
                                              gint64 unix_microseconds);
typedef CalendarClockTimer (*CalendarClockScheduleFunc)(
    gpointer context,
    guint delay_milliseconds,
    CalendarClockTimerFunc callback,
    gpointer callback_data);
typedef void (*CalendarClockCancelFunc)(gpointer context,
                                           CalendarClockTimer timer);

typedef struct
{
    guint abi_version;
    gpointer context;
    /* Microseconds since 1970-01-01 00:00:00 UTC. */
    CalendarClockNowFunc now;
    /* Local UTC offset in seconds, including daylight saving at the instant. */
    CalendarClockUtcOffsetFunc utc_offset;
} CalendarClockTimeSource;

typedef struct
{
    guint abi_version;
    gpointer context;
    /*
     * Arms one callback after at least @delay_milliseconds and returns a
     * non-zero handle.  It is a one-shot: the callback must run at most once.
     */
    CalendarClockScheduleFunc schedule_once;
    /* Prevents a pending callback for @timer; never called with zero. */
    CalendarClockCancelFunc cancel;
} CalendarClockScheduler;

typedef struct
{
    CalendarTimeMode mode;
    gboolean show_seconds;
    gboolean vertical;
    /* Degrees north; ignored by modes that do not require latitude. */
    gdouble latitude;
    /* Degrees east of Greenwich; ignored by modes that do not require it. */
    gdouble longitude;
} CalendarClockConfig;

enum
{
    CALENDAR_CLOCK_TIME_SOURCE_ABI = 1,
    CALENDAR_CLOCK_SCHEDULER_ABI = 1
};

/**
 * calendar_clock_interfaces_are_valid:
 * @time_source: time-source interface table
 * @scheduler: one-shot scheduler interface table
 *
 * Returns: whether both ABI versions and required callbacks are valid.
 */
gboolean calendar_clock_interfaces_are_valid(
    const CalendarClockTimeSource *time_source,
    const CalendarClockScheduler *scheduler);
/**
 * calendar_clock_engine_new:
 * @time_source: valid time-source interface
 * @scheduler: valid one-shot scheduler interface
 * @tick: callback after each visible transition
 * @tick_data: borrowed callback context
 *
 * The engine copies both tables but borrows their contexts and @tick_data.
 *
 * Returns: (transfer full) (nullable): engine, or %NULL for invalid interfaces
 */
CalendarClockEngine *calendar_clock_engine_new(
    const CalendarClockTimeSource *time_source,
    const CalendarClockScheduler *scheduler,
    CalendarClockTickFunc tick,
    gpointer tick_data);
/** Accepts %NULL and cancels any armed one-shot before release. */
void calendar_clock_engine_free(CalendarClockEngine *engine);

/**
 * calendar_clock_engine_start:
 * @engine: clock engine
 * @config: display/scheduling configuration
 *
 * Coordinates required by the selected mode must be finite and inside the
 * physical latitude/longitude ranges (-90..90 and -180..180). Values for
 * coordinates ignored by the selected mode do not affect that mode.
 *
 * Returns: %TRUE only when the first timer was successfully armed.
 */
gboolean calendar_clock_engine_start(
    CalendarClockEngine *engine,
    const CalendarClockConfig *config);
/** Cancels the armed one-shot and makes the engine idle. */
void calendar_clock_engine_stop(CalendarClockEngine *engine);

/** Returns: whether the engine currently owns a non-zero timer handle. */
gboolean calendar_clock_engine_is_running(
    const CalendarClockEngine *engine);
/**
 * calendar_clock_engine_format:
 * @engine: clock engine
 *
 * Returns: (transfer full): allocated current native-clock text, or empty
 * text while the engine is idle or its selected configuration was rejected
 */
gchar *calendar_clock_engine_format(
    const CalendarClockEngine *engine);

G_END_DECLS

#endif
