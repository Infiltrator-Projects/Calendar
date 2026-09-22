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
 *
 * Invariants:
 * - this module owns rules and numeric continuation, never locale presentation;
 * - mode-specific month length, fields↔JDN conversion and month/year navigation
 *   share the same helpers so navigation cannot silently use a different leap
 *   rule from formatting;
 * - fixed-range tables such as Umm al-Qura reject navigation beyond their
 *   evidenced range instead of extrapolating invented dates;
 * - intermediate year/day arithmetic uses the checked/saturating Common
 *   primitives before narrowing back to public field widths.
 *
 * Family-specific conversion and rule sets stay together in private sibling
 * modules. The coordinator owns only cross-family dispatch, simple
 * Gregorian-derived era mapping and generic navigation so review boundaries
 * follow chronology ownership rather than file size.
 */

#include "calendar-arithmetic.h"
#include "calendar-arithmetic-internal.h"

#include "julian-day.h"

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
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
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
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(1, fields->year) :
                fields->year;

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_arithmetic_coptic_signed_year_from_fields(
                mode, fields);

        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
            return calendar_plus_i64_subtract_saturating(fields->year, 543);

        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(
                    1912, fields->year) :
                calendar_plus_i64_add_saturating(
                    fields->year, 1911);

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return calendar_plus_arithmetic_japanese_signed_year_from_fields(\n                fields);

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

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            calendar_plus_arithmetic_coptic_set_signed_year(
                mode, signed_year, fields);
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

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            calendar_plus_arithmetic_japanese_set_year_for_gregorian(\n                signed_year, fields);
            break;

        default:
            fields->year = signed_year;
            fields->auxiliary = 0;
            break;
    }
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
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return calendar_plus_gregorian_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return calendar_plus_arithmetic_islamic_month_length(
                mode, year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_arithmetic_coptic_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return calendar_plus_arithmetic_indian_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return calendar_plus_arithmetic_persian_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return calendar_plus_arithmetic_hebrew_month_length(
                year, fields->month);

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
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return calendar_plus_gregorian_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return calendar_plus_arithmetic_islamic_fields_to_jdn(
                mode, year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_arithmetic_coptic_fields_to_jdn(
                mode, year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return calendar_plus_arithmetic_indian_fields_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return calendar_plus_arithmetic_persian_fields_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return calendar_plus_arithmetic_hebrew_fields_to_jdn(
                year, fields->month, fields->day);

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

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            calendar_plus_arithmetic_japanese_fields_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return calendar_plus_arithmetic_islamic_fields_from_jdn(
                mode, jdn, fields);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_arithmetic_coptic_fields_from_jdn(
                mode, jdn, fields);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            calendar_plus_arithmetic_indian_fields_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            calendar_plus_arithmetic_persian_fields_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            calendar_plus_arithmetic_hebrew_fields_from_jdn(jdn, fields);
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
        calendar_plus_arithmetic_hebrew_add_months_to_fields(
            &fields, amount);
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
    if (mode == CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA &&
        !calendar_plus_arithmetic_islamic_year_supported(mode, year))
    {
        return jdn;
    }
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
        if (fields.month == 6 &&
            !calendar_plus_arithmetic_hebrew_is_leap(fields.year))
            fields.month = 7;
        fields.day = MIN(
            fields.day,
            calendar_plus_arithmetic_hebrew_month_length(
                fields.year, fields.month));
        return fields_to_jdn(mode, &fields);
    }

    year = calendar_plus_i64_add_saturating(
        signed_year_from_fields(mode, &fields), amount);
    if (mode == CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA &&
        !calendar_plus_arithmetic_islamic_year_supported(mode, year))
    {
        return jdn;
    }
    set_signed_year(mode, year, &fields);
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}
