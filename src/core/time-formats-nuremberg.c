/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Nuremberg historical hours.
 *
 * Surviving Nuremberg instruments carry separate equal-hour counts for the
 * daylight and night portions of the civil day.  The count begins again at
 * sunrise and again at sunset, so noon may be hour 8 near midsummer but only
 * hour 4 near midwinter. Common renders the visible Nuremberg value; Calendar
 * retains the boundary calculation solely to schedule the next exact panel
 * update. This is distinct from Italian hours (one sunset-to-sunset count)
 * and Babylonian hours (one sunrise-to-sunrise count).
 *
 * Model choice: Calendar interprets the historical equal-hour description as
 * elapsed ordinary hours reset at the same computed 0.833-degree sunrise and
 * sunset boundaries used by the other solar-origin providers. Surviving
 * instruments and local practice were not a single numerical refraction model;
 * this is therefore a deterministic reconstruction rule, not a claim that
 * every historical Nuremberg clock used identical boundaries.
 */

#include "time-formats-internal.h"
#include "time-astronomy.h"

#include <math.h>

typedef enum
{
    NUREMBERG_BOUNDARY_SUNRISE,
    NUREMBERG_BOUNDARY_SUNSET
} NurembergBoundary;

static gboolean
nuremberg_boundary_window(gint64 unix_microseconds,
                          gdouble latitude,
                          gdouble longitude,
                          gint64 *previous,
                          gint64 *next,
                          NurembergBoundary *previous_kind)
{
    gint64 best_previous = G_MININT64;
    gint64 best_next = G_MAXINT64;
    NurembergBoundary best_kind = NUREMBERG_BOUNDARY_SUNRISE;
    gint offset;

    g_return_val_if_fail(previous != NULL, FALSE);
    g_return_val_if_fail(previous_kind != NULL, FALSE);

    for (offset = -2; offset <= 2; offset++)
    {
        const gint64 delta = (gint64)offset * MICROSECONDS_PER_DAY;
        gint64 sample;
        gint64 dawn;
        gint64 dusk;
        gint64 candidates[2];
        NurembergBoundary kinds[2] = {
            NUREMBERG_BOUNDARY_SUNRISE,
            NUREMBERG_BOUNDARY_SUNSET
        };
        guint index;

        if (!infiltratr_i64_add_checked(unix_microseconds, delta, &sample))
            continue;
        if (!calendar_plus_solar_boundary_instants(sample,
                                                   latitude,
                                                   longitude,
                                                   0.833,
                                                   &dawn,
                                                   &dusk))
        {
            continue;
        }

        candidates[0] = dawn;
        candidates[1] = dusk;
        for (index = 0; index < G_N_ELEMENTS(candidates); index++)
        {
            const gint64 candidate = candidates[index];

            if (candidate <= unix_microseconds && candidate > best_previous)
            {
                best_previous = candidate;
                best_kind = kinds[index];
            }
            if (candidate > unix_microseconds && candidate < best_next)
                best_next = candidate;
        }
    }

    if (best_previous == G_MININT64)
        return FALSE;

    *previous = best_previous;
    *previous_kind = best_kind;
    if (next != NULL)
        *next = best_next;
    return TRUE;
}

guint
delay_nuremberg_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude,
                                gdouble longitude)
{
    gint64 start;
    gint64 next;
    NurembergBoundary boundary;
    guint tick_delay;

    (void)utc_offset_seconds;
    if (!nuremberg_boundary_window(unix_microseconds,
                                   latitude,
                                   longitude,
                                   &start,
                                   &next,
                                   &boundary))
    {
        return 3600000;
    }

    (void)boundary;
    tick_delay = delay_for_integer_period(
        unix_microseconds - start,
        (show_seconds ? 1 : SECONDS_PER_MINUTE) * (gint64)G_USEC_PER_SEC);

    if (next != G_MAXINT64)
    {
        const guint reset_delay =
            delay_continuous_microseconds_to_milliseconds(
                (long double)(next - unix_microseconds));
        return MIN(tick_delay, reset_delay);
    }

    return tick_delay;
}
