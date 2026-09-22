// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned Persian (Solar Hijri) arithmetic.
 *
 * Leap corrections, year starts, month lengths and JDN conversion stay in one
 * private module so the maintained rule set has a single review boundary.
 */

#include "calendar-arithmetic-internal.h"

#include "integer-math.h"

enum
{
    CALENDAR_PLUS_PERSIAN_EPOCH_JDN = 1948320
};

static const gint persian_non_leap_corrections[] = {
    1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799,
    1832, 1865, 1898, 1931, 1964, 1997, 2030, 2059,
    2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224,
    2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356,
    2360, 2389, 2393, 2422, 2426, 2455, 2459, 2488,
    2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
    2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748,
    2752, 2756, 2781, 2785, 2789, 2818, 2822, 2847,
    2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921,
    2946, 2950, 2954, 2979, 2983, 2987
};

static gboolean
persian_is_correction_year(gint64 year)
{
    gsize index;

    if (year < persian_non_leap_corrections[0] ||
        year > persian_non_leap_corrections[
            G_N_ELEMENTS(persian_non_leap_corrections) - 1])
    {
        return FALSE;
    }

    for (index = 0;
         index < G_N_ELEMENTS(persian_non_leap_corrections);
         index++)
    {
        if (year == persian_non_leap_corrections[index])
            return TRUE;
        if (year < persian_non_leap_corrections[index])
            return FALSE;
    }

    return FALSE;
}

static gboolean
persian_is_leap(gint64 year)
{
    if (persian_is_correction_year(year))
        return FALSE;
    if (persian_is_correction_year(
            calendar_plus_i64_subtract_saturating(year, 1)))
    {
        return TRUE;
    }

    return calendar_plus_positive_modulo(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, 25), 11),
        33) < 8;
}

static gint64
persian_first_day_offset(gint64 year)
{
    gint64 result = calendar_plus_i64_multiply_saturating(
        365, calendar_plus_i64_subtract_saturating(year, 1));

    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(8, year), 21),
            33));
    if (year > persian_non_leap_corrections[0] &&
        persian_is_correction_year(
            calendar_plus_i64_subtract_saturating(year, 1)))
    {
        result = calendar_plus_i64_subtract_saturating(result, 1);
    }
    return result;
}

static gint
persian_month_length(gint64 year,
                     gint month)
{
    if (month < 1 || month > 12)
        return 0;
    if (month <= 6)
        return 31;
    if (month <= 11)
        return 30;
    return persian_is_leap(year) ? 30 : 29;
}

static gint64
persian_to_jdn(gint64 year,
               gint month,
               gint day)
{
    static const gint month_offsets[] = {
        0, 0, 31, 62, 93, 124, 155, 186,
        216, 246, 276, 306, 336
    };
    gint64 result;

    if (month < 1 || month > 12)
        return CALENDAR_PLUS_PERSIAN_EPOCH_JDN;

    result = calendar_plus_i64_add_saturating(
        CALENDAR_PLUS_PERSIAN_EPOCH_JDN,
        persian_first_day_offset(year));
    result = calendar_plus_i64_add_saturating(
        result, month_offsets[month]);
    return calendar_plus_i64_add_saturating(result, day - 1);
}

static void
persian_from_jdn(gint64 jdn,
                 CalendarPlusCalendarFields *fields)
{
    static const gint month_offsets[] = {
        0, 0, 31, 62, 93, 124, 155, 186,
        216, 246, 276, 306, 336
    };
    const gint64 days_since_epoch = calendar_plus_i64_subtract_saturating(
        jdn, CALENDAR_PLUS_PERSIAN_EPOCH_JDN);
    gint64 year = calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(
                    33, days_since_epoch),
                3),
            12053),
        1);
    gint64 day_index = calendar_plus_i64_subtract_saturating(
        days_since_epoch, persian_first_day_offset(year));
    gint month;

    if (day_index == 365 && persian_is_correction_year(year))
    {
        year = calendar_plus_i64_add_saturating(year, 1);
        day_index = 0;
    }

    month = day_index < 216 ?
        (gint)calendar_plus_floor_divide(day_index, 31) + 1 :
        (gint)calendar_plus_floor_divide(
            calendar_plus_i64_subtract_saturating(day_index, 6), 30) + 1;

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                day_index, month_offsets[month]),
            1),
        .auxiliary = 0,
        .special = FALSE
    };
}

gint
calendar_plus_arithmetic_persian_month_length(gint64 year,
                                               gint month)
{
    return persian_month_length(year, month);
}

gint64
calendar_plus_arithmetic_persian_fields_to_jdn(gint64 year,
                                                gint month,
                                                gint day)
{
    return persian_to_jdn(year, month, day);
}

void
calendar_plus_arithmetic_persian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    persian_from_jdn(jdn, fields);
}
