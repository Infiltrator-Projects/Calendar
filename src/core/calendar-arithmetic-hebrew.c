// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned Hebrew arithmetic.
 *
 * The 19-year cycle, postponement rules, month tables and family-specific
 * month navigation stay together so conversion and navigation cannot drift.
 */

#include "calendar-arithmetic-internal.h"

#include "integer-math.h"

enum
{
    HEBREW_HOUR_PARTS = 1080,
    HEBREW_DAY_PARTS = 24 * HEBREW_HOUR_PARTS,
    HEBREW_MONTH_FRACT = 12 * HEBREW_HOUR_PARTS + 793,
    HEBREW_MONTH_PARTS = 29 * HEBREW_DAY_PARTS + HEBREW_MONTH_FRACT,
    HEBREW_BAHARAD = 11 * HEBREW_HOUR_PARTS + 204,
    HEBREW_EPOCH_OFFSET = 347998
};

static const gint hebrew_month_length_table[13][3] = {
    { 30, 30, 30 }, { 29, 29, 30 }, { 29, 30, 30 },
    { 29, 29, 29 }, { 30, 30, 30 }, { 30, 30, 30 },
    { 29, 29, 29 }, { 30, 30, 30 }, { 29, 29, 29 },
    { 30, 30, 30 }, { 29, 29, 29 }, { 30, 30, 30 },
    { 29, 29, 29 }
};

static const gint hebrew_month_start[14][3] = {
    { 0, 0, 0 }, { 30, 30, 30 }, { 59, 59, 60 },
    { 88, 89, 90 }, { 117, 118, 119 }, { 147, 148, 149 },
    { 147, 148, 149 }, { 176, 177, 178 }, { 206, 207, 208 },
    { 235, 236, 237 }, { 265, 266, 267 }, { 294, 295, 296 },
    { 324, 325, 326 }, { 353, 354, 355 }
};

static const gint hebrew_leap_month_start[14][3] = {
    { 0, 0, 0 }, { 30, 30, 30 }, { 59, 59, 60 },
    { 88, 89, 90 }, { 117, 118, 119 }, { 147, 148, 149 },
    { 177, 178, 179 }, { 206, 207, 208 }, { 236, 237, 238 },
    { 265, 266, 267 }, { 295, 296, 297 }, { 324, 325, 326 },
    { 354, 355, 356 }, { 383, 384, 385 }
};

static gboolean
hebrew_is_leap(gint64 year)
{
    return calendar_plus_positive_modulo(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, 12), 17),
        19) >= 12;
}

static gint64
hebrew_start_of_year(gint64 year)
{
    const gint64 months = calendar_plus_floor_divide(
        calendar_plus_i64_subtract_saturating(
            calendar_plus_i64_multiply_saturating(235, year), 234),
        19);
    const gint64 total_fraction = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(
            months, HEBREW_MONTH_FRACT),
        HEBREW_BAHARAD);
    const gint64 fraction_days = calendar_plus_floor_divide(
        total_fraction, HEBREW_DAY_PARTS);
    gint64 day = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(months, 29),
        fraction_days);
    const gint64 fraction = calendar_plus_i64_subtract_saturating(
        total_fraction,
        calendar_plus_i64_multiply_saturating(
            fraction_days, HEBREW_DAY_PARTS));
    gint weekday = (gint)calendar_plus_positive_modulo(day, 7);

    if (weekday == 2 || weekday == 4 || weekday == 6 ||
        (weekday == 0 &&
         fraction >= 21 * HEBREW_HOUR_PARTS + 589 &&
         hebrew_is_leap(
             calendar_plus_i64_subtract_saturating(year, 1))))
    {
        day = calendar_plus_i64_add_saturating(day, 1);
    }
    else if (weekday == 1 &&
             fraction >= 15 * HEBREW_HOUR_PARTS + 204 &&
             !hebrew_is_leap(year))
    {
        day = calendar_plus_i64_add_saturating(day, 2);
    }

    return day;
}

static gint
hebrew_year_type(gint64 year)
{
    gint64 length = calendar_plus_i64_subtract_saturating(
        hebrew_start_of_year(
            calendar_plus_i64_add_saturating(year, 1)),
        hebrew_start_of_year(year));

    if (length > 380)
        length = calendar_plus_i64_subtract_saturating(length, 30);

    if (length == 353)
        return 0;
    if (length == 355)
        return 2;
    return 1;
}

static gint
hebrew_month_length(gint64 year,
                    gint month)
{
    const gint month_index = month - 1;

    if (month < 1 || month > 13)
        return 0;
    if (month_index == 5 && !hebrew_is_leap(year))
        return 0;
    if (month_index == 1 || month_index == 2)
        return hebrew_month_length_table[month_index][
            hebrew_year_type(year)];
    return hebrew_month_length_table[month_index][0];
}

static gint
hebrew_month_start_offset(gint64 year,
                          gint month)
{
    const gint month_index = month - 1;
    const gint type = hebrew_year_type(year);

    if (month < 1 || month > 13)
        return 0;
    return hebrew_is_leap(year) ?
        hebrew_leap_month_start[month_index][type] :
        hebrew_month_start[month_index][type];
}

static gint64
hebrew_to_jdn(gint64 year,
              gint month,
              gint day)
{
    gint64 result = calendar_plus_i64_add_saturating(
        hebrew_start_of_year(year), HEBREW_EPOCH_OFFSET);

    result = calendar_plus_i64_add_saturating(
        result, hebrew_month_start_offset(year, month));
    return calendar_plus_i64_add_saturating(result, day - 1);
}

static void
hebrew_from_jdn(gint64 jdn,
                CalendarPlusCalendarFields *fields)
{
    const gint64 d = calendar_plus_i64_subtract_saturating(
        jdn, HEBREW_EPOCH_OFFSET - 1);
    const gint64 approximate_months = calendar_plus_floor_divide(
        calendar_plus_i64_multiply_saturating(
            d, HEBREW_DAY_PARTS),
        HEBREW_MONTH_PARTS);
    gint64 year = calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(
                    19, approximate_months),
                234),
            235),
        1);
    gint64 day_of_year = calendar_plus_i64_subtract_saturating(
        d, hebrew_start_of_year(year));
    gint month_index = 0;
    gint type;
    gboolean leap;

    while (day_of_year < 1)
    {
        year = calendar_plus_i64_subtract_saturating(year, 1);
        day_of_year = calendar_plus_i64_subtract_saturating(
            d, hebrew_start_of_year(year));
    }

    type = hebrew_year_type(year);
    leap = hebrew_is_leap(year);
    while (month_index < 14 &&
           day_of_year >
               (leap ?
                    hebrew_leap_month_start[month_index][type] :
                    hebrew_month_start[month_index][type]))
    {
        month_index++;
    }
    if (month_index > 0)
        month_index--;

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month_index + 1,
        .day = (gint)calendar_plus_i64_subtract_saturating(
            day_of_year,
            leap ?
                hebrew_leap_month_start[month_index][type] :
                hebrew_month_start[month_index][type]),
        .auxiliary = 0,
        .special = FALSE
    };
}

static void
hebrew_add_months_to_fields(CalendarPlusCalendarFields *fields,
                            gint amount)
{
    gint64 year = fields->year;
    gint month = fields->month - 1;
    gint remaining = amount;

    if (remaining >= 235 || remaining <= -235)
    {
        const gint cycles = remaining / 235;

        year = calendar_plus_i64_add_saturating(
            year, calendar_plus_i64_multiply_saturating(cycles, 19));
        remaining -= cycles * 235;
    }

    while (remaining > 0)
    {
        month++;
        if (month > 12)
        {
            month = 0;
            year = calendar_plus_i64_add_saturating(year, 1);
        }
        if (month == 5 && !hebrew_is_leap(year))
            month++;
        remaining--;
    }

    while (remaining < 0)
    {
        month--;
        if (month < 0)
        {
            year = calendar_plus_i64_subtract_saturating(year, 1);
            month = 12;
        }
        if (month == 5 && !hebrew_is_leap(year))
            month--;
        remaining++;
    }

    fields->year = year;
    fields->month = month + 1;
    fields->day = MIN(
        fields->day, hebrew_month_length(year, fields->month));
}

gboolean
calendar_plus_arithmetic_hebrew_is_leap(gint64 year)
{
    return hebrew_is_leap(year);
}

gint
calendar_plus_arithmetic_hebrew_month_length(gint64 year,
                                              gint month)
{
    return hebrew_month_length(year, month);
}

gint64
calendar_plus_arithmetic_hebrew_fields_to_jdn(gint64 year,
                                               gint month,
                                               gint day)
{
    return hebrew_to_jdn(year, month, day);
}

void
calendar_plus_arithmetic_hebrew_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    hebrew_from_jdn(jdn, fields);
}

void
calendar_plus_arithmetic_hebrew_add_months_to_fields(
    CalendarPlusCalendarFields *fields,
    gint amount)
{
    hebrew_add_months_to_fields(fields, amount);
}
