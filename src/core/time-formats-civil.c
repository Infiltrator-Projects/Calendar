/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Civil and discrete native clock scheduling.
 *
 * Only unmigrated clocks retain Calendar-owned scheduling here. Fixed-unit
 * decimal, Internet, Unix, hexadecimal and binary scheduling is provided by
 * their independent system resources through Common's executable clock ABI.
 */

#include "time-formats-internal.h"

#include <infiltratr/temporal.h>

/* The first five fixed-unit modes now live in /System/Resources/Clocks. */

guint
delay_chinese_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    const gint64 shifted =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC;

    (void)longitude;
    return delay_for_integer_period(
        shifted,
        (gint64)(show_seconds ? 1 : 2) * SECONDS_PER_HOUR * G_USEC_PER_SEC);
}
