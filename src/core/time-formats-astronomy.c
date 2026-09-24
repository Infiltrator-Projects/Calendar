/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Astronomical, scientific and seasonal clock scheduling.
 *
 * Common owns rendered clock text. Continuous astronomical calculations used
 * to predict the next visible boundary remain here and in time-astronomy.c so
 * Calendar can keep its one-shot panel timer aligned without polling.
 */

#include "time-formats-internal.h"
#include "time-astronomy.h"

#include <math.h>

guint
delay_sidereal_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_local_sidereal_seconds(unix_microseconds, longitude),
        CALENDAR_PLUS_SIDEREAL_RATE,
        show_seconds);
}

guint
delay_solar_provider(gint64 unix_microseconds,
                     gint utc_offset_seconds,
                     gboolean show_seconds,
                     gdouble latitude G_GNUC_UNUSED,
                     gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_apparent_solar_seconds(unix_microseconds, longitude),
        1.0L,
        show_seconds);
}

guint
delay_julian_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY),
        show_seconds ? 100000 : 1000);
}

guint
delay_mean_solar_provider(gint64 unix_microseconds,
                          gint utc_offset_seconds,
                          gboolean show_seconds,
                          gdouble latitude G_GNUC_UNUSED,
                          gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_mean_solar_seconds(unix_microseconds, longitude),
        1.0L,
        show_seconds);
}

guint
delay_modified_julian_provider(gint64 unix_microseconds,
                               gint utc_offset_seconds,
                               gboolean show_seconds,
                               gdouble latitude G_GNUC_UNUSED,
                               gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY),
        show_seconds ? 100000 : 1000);
}

typedef struct
{
    gboolean daylight;
    guint index;
    long double seconds_to_next;
} SeasonalPeriod;

/*
 * Split apparent-solar daytime and nighttime independently. This is the common
 * mathematical primitive behind Roman unequal hours and Edo Japanese toki;
 * each provider supplies its historically appropriate boundary altitude and
 * number of subdivisions.
 */
static gboolean
seasonal_period_at(gint64 unix_microseconds,
                   gdouble latitude,
                   gdouble longitude,
                   gdouble solar_depression_degrees,
                   guint daylight_parts,
                   guint night_parts,
                   SeasonalPeriod *period)
{
    long double dawn;
    long double dusk;
    const long double solar =
        calendar_plus_apparent_solar_seconds(unix_microseconds, longitude);
    long double span;
    long double position;
    long double unit;
    guint parts;

    g_return_val_if_fail(period != NULL, FALSE);

    if (!calendar_plus_solar_day_boundaries(unix_microseconds,
                                            latitude,
                                            solar_depression_degrees,
                                            &dawn,
                                            &dusk))
    {
        return FALSE;
    }

    if (solar >= dawn && solar < dusk)
    {
        period->daylight = TRUE;
        span = dusk - dawn;
        position = solar - dawn;
        parts = daylight_parts;
    }
    else
    {
        period->daylight = FALSE;
        span = SECONDS_PER_DAY - (dusk - dawn);
        position = solar >= dusk ?
            solar - dusk : SECONDS_PER_DAY - dusk + solar;
        parts = night_parts;
    }

    unit = span / parts;
    period->index = MIN((guint)floorl(position / unit), parts - 1);
    period->seconds_to_next =
        (period->index + 1) * unit - position;
    if (period->seconds_to_next <= 0.0L)
        period->seconds_to_next = unit;
    return TRUE;
}

guint
delay_roman_temporal_provider(gint64 unix_microseconds,
                              gint utc_offset_seconds,
                              gboolean show_seconds G_GNUC_UNUSED,
                              gdouble latitude,
                              gdouble longitude)
{
    SeasonalPeriod period;

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            0.833,
                            12,
                            4,
                            &period))
    {
        return 3600000;
    }

    return delay_continuous_microseconds_to_milliseconds(
        period.seconds_to_next * G_USEC_PER_SEC);
}

/* Kansei-calendar dawn/dusk: solar centre 7°21′40″ below the horizon. */
#define JAPANESE_DAWN_DEPRESSION (7.0 + 21.0 / 60.0 + 40.0 / 3600.0)

guint
delay_japanese_temporal_provider(gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds G_GNUC_UNUSED,
                                 gdouble latitude,
                                 gdouble longitude)
{
    SeasonalPeriod period;

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            JAPANESE_DAWN_DEPRESSION,
                            6,
                            6,
                            &period))
    {
        return 3600000;
    }

    return delay_continuous_microseconds_to_milliseconds(
        period.seconds_to_next * G_USEC_PER_SEC);
}


typedef enum
{
    SOLAR_ORIGIN_SUNRISE,
    SOLAR_ORIGIN_SUNSET
} SolarOrigin;

/*
 * Locate the moving origin around an instant. Searching neighbouring UTC dates
 * is necessary because a local solar event can fall on the preceding or
 * following UTC date near the date line. We deliberately stop after two days:
 * when a sunrise/sunset does not occur because of polar day/night, these clock
 * conventions are undefined rather than extrapolated.
 */
static gboolean
solar_origin_window(gint64 unix_microseconds,
                    gdouble latitude,
                    gdouble longitude,
                    SolarOrigin origin,
                    gint64 *previous,
                    gint64 *next)
{
    gint64 best_previous = G_MININT64;
    gint64 best_next = G_MAXINT64;
    gint offset;

    g_return_val_if_fail(previous != NULL, FALSE);

    for (offset = -2; offset <= 2; offset++)
    {
        const gint64 delta = (gint64)offset * MICROSECONDS_PER_DAY;
        gint64 sample;
        gint64 dawn;
        gint64 dusk;
        gint64 candidate;

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

        candidate = origin == SOLAR_ORIGIN_SUNRISE ? dawn : dusk;
        if (candidate <= unix_microseconds && candidate > best_previous)
            best_previous = candidate;
        if (candidate > unix_microseconds && candidate < best_next)
            best_next = candidate;
    }

    if (best_previous == G_MININT64)
        return FALSE;

    *previous = best_previous;
    if (next != NULL)
        *next = best_next;
    return TRUE;
}

static guint
delay_equal_hours_from_solar_origin(gint64 unix_microseconds,
                                    gboolean show_seconds,
                                    gdouble latitude,
                                    gdouble longitude,
                                    SolarOrigin origin)
{
    gint64 start;
    gint64 next;
    gint64 elapsed;
    guint tick_delay;

    if (!solar_origin_window(unix_microseconds,
                             latitude,
                             longitude,
                             origin,
                             &start,
                             &next))
    {
        return 3600000;
    }

    elapsed = unix_microseconds - start;
    tick_delay = delay_for_integer_period(
        elapsed,
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

/*
 * Italian hours (horae ab occasu Solis) are equal hours counted from sunset.
 * Historical local practice sometimes offset the reset from literal sunset;
 * Calendar intentionally uses the unambiguous strict-sunset convention.
 */
guint
delay_italian_hours_provider(gint64 unix_microseconds,
                             gint utc_offset_seconds,
                             gboolean show_seconds,
                             gdouble latitude,
                             gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, latitude, longitude,
        SOLAR_ORIGIN_SUNSET);
}

/*
 * "Babylonian hours" here uses the historical European gnomonic term for
 * equal hours elapsed from sunrise (horae ab ortu Solis). It is not presented
 * as a reconstruction of ancient Mesopotamian civil timekeeping.
 */
guint
delay_babylonian_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude,
                                gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, latitude, longitude,
        SOLAR_ORIGIN_SUNRISE);
}

/*
 * Indian ghaṭī time: sixty ghaṭīs in a mean 24-hour day, with one ghaṭī
 * equal to 24 SI minutes and one vighaṭī to 24 SI seconds. The historical
 * day is presented from computed sunrise; the display resets at the next
 * computed sunrise even when seasonal sunrise drift makes the interval differ
 * slightly from exactly 24 mean hours.
 */
guint
delay_indian_ghati_provider(gint64 unix_microseconds,
                            gint utc_offset_seconds,
                            gboolean show_seconds G_GNUC_UNUSED,
                            gdouble latitude,
                            gdouble longitude)
{
    gint64 start;
    gint64 next;
    guint tick_delay;

    (void)utc_offset_seconds;
    if (!solar_origin_window(unix_microseconds, latitude, longitude,
                             SOLAR_ORIGIN_SUNRISE, &start, &next))
    {
        return 3600000;
    }

    tick_delay = delay_for_integer_period(
        unix_microseconds - start, (gint64)24 * G_USEC_PER_SEC);
    if (next != G_MAXINT64)
    {
        const guint reset_delay =
            delay_continuous_microseconds_to_milliseconds(
                (long double)(next - unix_microseconds));
        return MIN(tick_delay, reset_delay);
    }

    return tick_delay;
}
