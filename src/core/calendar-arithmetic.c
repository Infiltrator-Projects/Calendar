// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned arithmetic calendar engines.
 *
 * These rules intentionally replace ICU only where the calendar itself is a
 * deterministic arithmetic system. ICU remains the locale-formatting authority
 * for these providers until Calendar also owns the corresponding locale data.
 *
 * Epochs and arithmetic follow the same civil-date/JDN convention used by the
 * rest of Calendar: an integral JDN identifies a civil date at midnight.
 */

#include "calendar-arithmetic.h"

#include "julian-day.h"

enum
{
    CALENDAR_PLUS_COPTIC_EPOCH_JDN = 1825030,
    CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN = 1724221,
    CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN = 1948440,
    CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN = 1948439,
    CALENDAR_PLUS_PERSIAN_EPOCH_JDN = 1948320
};

#define ETHIOPIC_AMETE_ALEM_OFFSET G_GINT64_CONSTANT(5500)
#define INDIAN_GREGORIAN_OFFSET G_GINT64_CONSTANT(78)

static gboolean
arithmetic_mode_supported(CalendarPlusCalendarMode mode)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return TRUE;
        default:
            return FALSE;
    }
}

static gint64
signed_year_from_fields(CalendarPlusCalendarMode mode,
                        const CalendarPlusCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(1, fields->year) :
                fields->year;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(
                    fields->year, ETHIOPIC_AMETE_ALEM_OFFSET) :
                fields->year;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_i64_subtract_saturating(
                fields->year, ETHIOPIC_AMETE_ALEM_OFFSET);

        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
            return calendar_plus_i64_subtract_saturating(fields->year, 543);

        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(
                    1912, fields->year) :
                calendar_plus_i64_add_saturating(
                    fields->year, 1911);

        default:
            return fields->year;
    }
}

static void
set_signed_year(CalendarPlusCalendarMode mode,
                gint64 signed_year,
                CalendarPlusCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year =
                    calendar_plus_i64_subtract_saturating(1, signed_year);
                fields->auxiliary = 0;
            }
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year = calendar_plus_i64_add_saturating(
                    signed_year, ETHIOPIC_AMETE_ALEM_OFFSET);
                fields->auxiliary = 0;
            }
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            fields->year = calendar_plus_i64_add_saturating(
                signed_year, ETHIOPIC_AMETE_ALEM_OFFSET);
            fields->auxiliary = 0;
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
            fields->year = calendar_plus_i64_add_saturating(
                signed_year, 543);
            fields->auxiliary = 0;
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            if (signed_year >= 1912)
            {
                fields->year = calendar_plus_i64_subtract_saturating(
                    signed_year, 1911);
                fields->auxiliary = 1;
            }
            else
            {
                fields->year = calendar_plus_i64_subtract_saturating(
                    1912, signed_year);
                fields->auxiliary = 0;
            }
            break;

        default:
            fields->year = signed_year;
            fields->auxiliary = 0;
            break;
    }
}

static gboolean
islamic_is_leap(gint64 year)
{
    const gint64 numerator = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(11, year), 14);

    return calendar_plus_positive_modulo(numerator, 30) < 11;
}

static gint
islamic_month_length(gint64 year,
                     gint month)
{
    if (month < 1 || month > 12)
        return 0;
    if (month == 12 && islamic_is_leap(year))
        return 30;
    return (month % 2) == 1 ? 30 : 29;
}

static gint64
islamic_to_jdn(gint64 year,
               gint month,
               gint day,
               gint64 epoch)
{
    const gint64 month_index = month - 1;
    const gint64 month_days = calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(59, month_index), 1),
        2);
    gint64 result = calendar_plus_i64_subtract_saturating(epoch, 1);

    result = calendar_plus_i64_add_saturating(result, day);
    result = calendar_plus_i64_add_saturating(result, month_days);
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(
            354, calendar_plus_i64_subtract_saturating(year, 1)));
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                3, calendar_plus_i64_multiply_saturating(11, year)),
            30));
    return result;
}

static void
islamic_from_jdn(gint64 jdn,
                 gint64 epoch,
                 CalendarPlusCalendarFields *fields)
{
    const gint64 year = calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(
                30, calendar_plus_i64_subtract_saturating(jdn, epoch)),
            10646),
        10631);
    gint month = 1;

    while (month < 12 &&
           jdn >= islamic_to_jdn(year, month + 1, 1, epoch))
    {
        month++;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                jdn, islamic_to_jdn(year, month, 1, epoch)),
            1),
        .auxiliary = 0,
        .special = FALSE
    };
}

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

static gboolean
coptic_style_is_leap(gint64 year)
{
    return calendar_plus_positive_modulo(year, 4) == 3;
}

static gint
coptic_style_month_length(gint64 year,
                          gint month)
{
    if (month < 1 || month > 13)
        return 0;
    if (month <= 12)
        return 30;
    return coptic_style_is_leap(year) ? 6 : 5;
}

static gint64
coptic_style_to_jdn(gint64 year,
                    gint month,
                    gint day,
                    gint64 epoch)
{
    gint64 result = calendar_plus_i64_subtract_saturating(epoch, 1);

    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(
            365, calendar_plus_i64_subtract_saturating(year, 1)));
    result = calendar_plus_i64_add_saturating(
        result, calendar_plus_floor_divide(year, 4));
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(30, month - 1));
    return calendar_plus_i64_add_saturating(result, day);
}

static gint64
coptic_style_year_from_jdn(gint64 jdn,
                           gint64 epoch)
{
    return calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(
                4, calendar_plus_i64_subtract_saturating(jdn, epoch)),
            1463),
        1461);
}

static void
coptic_style_from_jdn(CalendarPlusCalendarMode mode,
                      gint64 jdn,
                      gint64 epoch,
                      CalendarPlusCalendarFields *fields)
{
    const gint64 year = coptic_style_year_from_jdn(jdn, epoch);
    const gint64 start = coptic_style_to_jdn(year, 1, 1, epoch);
    const gint month = (gint)calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_subtract_saturating(jdn, start), 30),
        1);

    *fields = (CalendarPlusCalendarFields){
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                jdn, coptic_style_to_jdn(year, month, 1, epoch)),
            1),
        .special = FALSE
    };
    set_signed_year(mode, year, fields);
}

static gint64
indian_year_start_jdn(gint64 gregorian_year)
{
    return calendar_plus_gregorian_to_jdn(
        gregorian_year,
        3,
        calendar_plus_gregorian_is_leap(gregorian_year) ? 21 : 22);
}

static gint
indian_month_length_for_year(gint64 saka_year,
                             gint month)
{
    const gint64 gregorian_year = calendar_plus_i64_add_saturating(
        saka_year, INDIAN_GREGORIAN_OFFSET);

    if (month < 1 || month > 12)
        return 0;
    if (month == 1)
        return calendar_plus_gregorian_is_leap(gregorian_year) ? 31 : 30;
    if (month <= 6)
        return 31;
    return 30;
}

static gint64
indian_to_jdn(gint64 saka_year,
              gint month,
              gint day)
{
    const gint64 gregorian_year = calendar_plus_i64_add_saturating(
        saka_year, INDIAN_GREGORIAN_OFFSET);
    const gint chaitra =
        calendar_plus_gregorian_is_leap(gregorian_year) ? 31 : 30;
    gint64 offset;

    if (month <= 1)
    {
        offset = day - 1;
    }
    else if (month <= 6)
    {
        offset = calendar_plus_i64_add_saturating(
            chaitra,
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(31, month - 2),
                day - 1));
    }
    else
    {
        offset = calendar_plus_i64_add_saturating(
            chaitra + 5 * 31,
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(30, month - 7),
                day - 1));
    }

    return calendar_plus_i64_add_saturating(
        indian_year_start_jdn(gregorian_year), offset);
}

static void
indian_from_jdn(gint64 jdn,
                CalendarPlusCalendarFields *fields)
{
    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint64 start_year;
    gint64 start;
    gint64 saka_year;
    gint64 day_index;
    gint month;
    gint day;
    gint chaitra;

    calendar_plus_jdn_to_gregorian(
        jdn, &gregorian_year, &gregorian_month, &gregorian_day);
    (void)gregorian_month;
    (void)gregorian_day;

    start_year = gregorian_year;
    start = indian_year_start_jdn(start_year);
    if (jdn < start)
    {
        start_year = calendar_plus_i64_subtract_saturating(start_year, 1);
        start = indian_year_start_jdn(start_year);
    }

    saka_year = calendar_plus_i64_subtract_saturating(
        start_year, INDIAN_GREGORIAN_OFFSET);
    day_index = calendar_plus_i64_subtract_saturating(jdn, start);
    chaitra = calendar_plus_gregorian_is_leap(start_year) ? 31 : 30;

    if (day_index < chaitra)
    {
        month = 1;
        day = (gint)day_index + 1;
    }
    else if (day_index < chaitra + 5 * 31)
    {
        const gint64 remaining =
            calendar_plus_i64_subtract_saturating(day_index, chaitra);

        month = (gint)calendar_plus_floor_divide(remaining, 31) + 2;
        day = (gint)calendar_plus_positive_modulo(remaining, 31) + 1;
    }
    else
    {
        const gint64 remaining = calendar_plus_i64_subtract_saturating(
            day_index, chaitra + 5 * 31);

        month = (gint)calendar_plus_floor_divide(remaining, 30) + 7;
        day = (gint)calendar_plus_positive_modulo(remaining, 30) + 1;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = saka_year,
        .month = month,
        .day = day,
        .auxiliary = 0,
        .special = FALSE
    };
}

static gint
periods_per_year(CalendarPlusCalendarMode mode)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return 13;
        default:
            return 12;
    }
}

static gint
month_length(CalendarPlusCalendarMode mode,
             const CalendarPlusCalendarFields *fields)
{
    const gint64 year = signed_year_from_fields(mode, fields);

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            return calendar_plus_gregorian_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return coptic_style_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return indian_month_length_for_year(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return persian_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return hebrew_month_length(year, fields->month);

        default:
            return 0;
    }
}

static gint64
fields_to_jdn(CalendarPlusCalendarMode mode,
              const CalendarPlusCalendarFields *fields)
{
    const gint64 year = signed_year_from_fields(mode, fields);

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            return calendar_plus_gregorian_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
            return islamic_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            return coptic_style_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_COPTIC_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return coptic_style_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return indian_to_jdn(year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return persian_to_jdn(year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return hebrew_to_jdn(year, fields->month, fields->day);

        default:
            return CALENDAR_PLUS_UNIX_EPOCH_JDN;
    }
}

gboolean
calendar_plus_arithmetic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint year;
    gint month;
    gint day;

    g_return_val_if_fail(fields != NULL, FALSE);
    *fields = (CalendarPlusCalendarFields){ 0 };

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            calendar_plus_jdn_to_gregorian(jdn, &year, &month, &day);
            fields->month = month;
            fields->day = day;
            fields->special = FALSE;
            set_signed_year(mode, year, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
            islamic_from_jdn(
                jdn, CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            islamic_from_jdn(
                jdn, CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_PLUS_COPTIC_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            indian_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            persian_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            hebrew_from_jdn(jdn, fields);
            return TRUE;

        default:
            return FALSE;
    }
}

gint64
calendar_plus_arithmetic_month_start(
    CalendarPlusCalendarMode mode,
    gint64 jdn)
{
    CalendarPlusCalendarFields fields;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    fields.day = 1;
    return fields_to_jdn(mode, &fields);
}

gint64
calendar_plus_arithmetic_add_months(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount)
{
    CalendarPlusCalendarFields fields;
    gint64 year;
    gint64 serial;
    gint period_count;
    gint target_month;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    if (mode == CALENDAR_PLUS_CALENDAR_MODE_HEBREW)
    {
        hebrew_add_months_to_fields(&fields, amount);
        return fields_to_jdn(mode, &fields);
    }

    year = signed_year_from_fields(mode, &fields);
    period_count = periods_per_year(mode);
    serial = calendar_plus_i64_add_saturating(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, period_count),
            fields.month - 1),
        amount);
    year = calendar_plus_floor_divide(serial, period_count);
    target_month =
        (gint)calendar_plus_positive_modulo(serial, period_count) + 1;

    set_signed_year(mode, year, &fields);
    fields.month = target_month;
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}

gint64
calendar_plus_arithmetic_add_years(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount)
{
    CalendarPlusCalendarFields fields;
    gint64 year;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    if (mode == CALENDAR_PLUS_CALENDAR_MODE_HEBREW)
    {
        fields.year = calendar_plus_i64_add_saturating(
            fields.year, amount);
        if (fields.month == 6 && !hebrew_is_leap(fields.year))
            fields.month = 7;
        fields.day = MIN(
            fields.day, hebrew_month_length(fields.year, fields.month));
        return fields_to_jdn(mode, &fields);
    }

    year = calendar_plus_i64_add_saturating(
        signed_year_from_fields(mode, &fields), amount);
    set_signed_year(mode, year, &fields);
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}
