/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Civil and discrete native clock scheduling.
 *
 * Common renders the visible clock text. These callbacks retain Calendar's
 * exact next-boundary scheduling so the Cinnamon panel wakes only when the
 * Common-rendered value can change.
 */

#include "time-formats-internal.h"

#include <infiltratr/temporal.h>

guint
delay_decimal_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? DECIMAL_SECONDS_PER_DAY : 1000);
}

/* @000 is midnight at UTC+01:00; the host's civil timezone is not involved. */
static gint64
internet_microseconds(gint64 unix_microseconds)
{
    const gint64 instant_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);

    return positive_modulo(
        instant_phase + (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC,
        MICROSECONDS_PER_DAY);
}

guint
delay_internet_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        internet_microseconds(unix_microseconds),
        show_seconds ? INTERNET_BEATS_PER_DAY * 100 : INTERNET_BEATS_PER_DAY);
}

guint
delay_unix_provider(gint64 unix_microseconds,
                    gint utc_offset_seconds,
                    gboolean show_seconds,
                    gdouble latitude G_GNUC_UNUSED,
                    gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)longitude;
    return delay_for_integer_period(unix_microseconds, G_USEC_PER_SEC);
}

guint
delay_hexadecimal_provider(gint64 unix_microseconds,
                           gint utc_offset_seconds,
                           gboolean show_seconds,
                           gdouble latitude G_GNUC_UNUSED,
                           gdouble longitude)
{
    (void)show_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        HEX_TICKS_PER_DAY);
}

guint
delay_binary_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? SECONDS_PER_DAY : MINUTES_PER_HOUR * HOURS_PER_DAY);
}

guint
delay_chinese_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds G_GNUC_UNUSED,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    const gint64 shifted =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC;

    (void)longitude;
    return delay_for_integer_period(
        shifted,
        (gint64)2 * SECONDS_PER_HOUR * G_USEC_PER_SEC);
}


/*
 * A documented Han-era convention divided one civil day into one hundred kè.
 * Calendar exposes that exact equal partition without implying that the
 * convention was uniform across every Chinese dynasty.
 */
guint
delay_chinese_ke_provider(gint64 unix_microseconds,
                          gint utc_offset_seconds,
                          gboolean show_seconds G_GNUC_UNUSED,
                          gdouble latitude G_GNUC_UNUSED,
                          gdouble longitude G_GNUC_UNUSED)
{
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        100);
}
