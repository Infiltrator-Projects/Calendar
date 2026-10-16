// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned Coptic and Ethiopic arithmetic.
 *
 * The shared 13-month leap cycle, Coptic/Ethiopic epochs and Amete
 * Mihret/Amete Alem year mapping stay together. Keeping year-era mapping in
 * the same private module as conversion prevents navigation from acquiring a
 * second interpretation of the chronology.
 */

#include "calendar-arithmetic-internal.h"

#include "integer-math.h"
#include "julian-day.h"

enum
{
    CALENDAR_COPTIC_EPOCH_JDN = 1825030,
    CALENDAR_ETHIOPIC_EPOCH_JDN = 1724221
};

#define ETHIOPIC_AMETE_ALEM_OFFSET G_GINT64_CONSTANT(5500)

gint64
calendar_arithmetic_coptic_signed_year_from_fields(
    CalendarCalendarMode mode,
    const CalendarCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_CALENDAR_MODE_COPTIC:
            return fields->auxiliary == 0 ?
                calendar_i64_subtract_saturating(1, fields->year) :
                fields->year;
        case CALENDAR_CALENDAR_MODE_ETHIOPIAN:
            return fields->auxiliary == 0 ?
                calendar_i64_subtract_saturating(
                    fields->year, ETHIOPIC_AMETE_ALEM_OFFSET) :
                fields->year;
        case CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_i64_subtract_saturating(
                fields->year, ETHIOPIC_AMETE_ALEM_OFFSET);
        default:
            return fields->year;
    }
}

void
calendar_arithmetic_coptic_set_signed_year(
    CalendarCalendarMode mode,
    gint64 signed_year,
    CalendarCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_CALENDAR_MODE_COPTIC:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year =
                    calendar_i64_subtract_saturating(1, signed_year);
                fields->auxiliary = 0;
            }
            break;
        case CALENDAR_CALENDAR_MODE_ETHIOPIAN:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year = calendar_i64_add_saturating(
                    signed_year, ETHIOPIC_AMETE_ALEM_OFFSET);
                fields->auxiliary = 0;
            }
            break;
        case CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            fields->year = calendar_i64_add_saturating(
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
coptic_style_is_leap(gint64 year)
{
    return calendar_positive_modulo(year, 4) == 3;
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
    gint64 result = calendar_i64_subtract_saturating(epoch, 1);

    result = calendar_i64_add_saturating(
        result,
        calendar_i64_multiply_saturating(
            365, calendar_i64_subtract_saturating(year, 1)));
    result = calendar_i64_add_saturating(
        result, calendar_floor_divide(year, 4));
    result = calendar_i64_add_saturating(
        result,
        calendar_i64_multiply_saturating(30, month - 1));
    return calendar_i64_add_saturating(result, day);
}

static gint64
coptic_style_year_from_jdn(gint64 jdn,
                           gint64 epoch)
{
    return calendar_floor_divide(
        calendar_i64_add_saturating(
            calendar_i64_multiply_saturating(
                4, calendar_i64_subtract_saturating(jdn, epoch)),
            1463),
        1461);
}

static void
coptic_style_from_jdn(CalendarCalendarMode mode,
                      gint64 jdn,
                      gint64 epoch,
                      CalendarCalendarFields *fields)
{
    const gint64 year = coptic_style_year_from_jdn(jdn, epoch);
    const gint64 start = coptic_style_to_jdn(year, 1, 1, epoch);
    const gint month = (gint)calendar_i64_add_saturating(
        calendar_floor_divide(
            calendar_i64_subtract_saturating(jdn, start), 30),
        1);

    *fields = (CalendarCalendarFields){
        .month = month,
        .day = (gint)calendar_i64_add_saturating(
            calendar_i64_subtract_saturating(
                jdn, coptic_style_to_jdn(year, month, 1, epoch)),
            1),
        .special = FALSE
    };
    calendar_arithmetic_coptic_set_signed_year(mode, year, fields);
}


gint
calendar_arithmetic_coptic_month_length(gint64 year, gint month)
{
    return coptic_style_month_length(year, month);
}

gint64
calendar_arithmetic_coptic_fields_to_jdn(
    CalendarCalendarMode mode, gint64 year, gint month, gint day)
{
    switch (mode)
    {
        case CALENDAR_CALENDAR_MODE_COPTIC:
            return coptic_style_to_jdn(
                year, month, day, CALENDAR_COPTIC_EPOCH_JDN);
        case CALENDAR_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return coptic_style_to_jdn(
                year, month, day, CALENDAR_ETHIOPIC_EPOCH_JDN);
        default:
            return CALENDAR_UNIX_EPOCH_JDN;
    }
}

gboolean
calendar_arithmetic_coptic_fields_from_jdn(
    CalendarCalendarMode mode,
    gint64 jdn,
    CalendarCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_CALENDAR_MODE_COPTIC:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_COPTIC_EPOCH_JDN, fields);
            return TRUE;
        case CALENDAR_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_ETHIOPIC_EPOCH_JDN, fields);
            return TRUE;
        default:
            return FALSE;
    }
}
