/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 * Scheduling for historical Nürnberg and the separate local reconstruction.
 */
#include "time-formats-internal.h"
#include "time-astronomy.h"
#include <math.h>

guint
delay_nuremberg_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude G_GNUC_UNUSED,
                                gdouble longitude G_GNUC_UNUSED)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_apparent_solar_seconds(unix_microseconds, 11.0767),
        1.0L, show_seconds);
}

typedef enum { NUR_SUNRISE, NUR_SUNSET } NurBoundary;

static gboolean
boundary_window(gint64 instant, gdouble latitude, gdouble longitude,
                gint64 *previous, gint64 *next)
{
    gint64 best_previous = G_MININT64;
    gint64 best_next = G_MAXINT64;
    gint offset;

    g_return_val_if_fail(previous != NULL, FALSE);
    for (offset = -2; offset <= 2; offset++) {
        const gint64 delta = (gint64)offset * MICROSECONDS_PER_DAY;
        gint64 sample, dawn, dusk;
        if (!infiltratr_i64_add_checked(instant, delta, &sample) ||
            !calendar_plus_solar_boundary_instants(
                sample, latitude, longitude, 0.833, &dawn, &dusk))
            continue;
        const gint64 candidates[2] = { dawn, dusk };
        for (guint i = 0; i < 2; i++) {
            if (candidates[i] <= instant && candidates[i] > best_previous)
                best_previous = candidates[i];
            if (candidates[i] > instant && candidates[i] < best_next)
                best_next = candidates[i];
        }
    }
    if (best_previous == G_MININT64)
        return FALSE;
    *previous = best_previous;
    if (next != NULL) *next = best_next;
    return TRUE;
}

guint
delay_nuremberg_solar_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude,
                                gdouble longitude)
{
    gint64 start, next;
    guint tick_delay;
    (void)utc_offset_seconds;
    if (!boundary_window(unix_microseconds, latitude, longitude, &start, &next))
        return 3600000U;
    tick_delay = delay_for_integer_period(
        unix_microseconds - start,
        (show_seconds ? 1 : SECONDS_PER_MINUTE) * (gint64)G_USEC_PER_SEC);
    if (next != G_MAXINT64)
        return MIN(tick_delay,
                   delay_continuous_microseconds_to_milliseconds(
                       (long double)(next - unix_microseconds)));
    return tick_delay;
}
