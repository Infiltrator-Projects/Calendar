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
    CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN = 1948439
};

#define ETHIOPIC_AMETE_ALEM_OFFSET G_GINT64_CONSTANT(5500)
#define INDIAN_GREGORIAN_OFFSET G_GINT64_CONSTANT(78)

static gboolean
arithmetic_mode_supported(CalendarPlusCalendarMode mode)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
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

    year = calendar_plus_i64_add_saturating(
        signed_year_from_fields(mode, &fields), amount);
    set_signed_year(mode, year, &fields);
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}
