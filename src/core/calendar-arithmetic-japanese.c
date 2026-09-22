// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned Japanese imperial-era arithmetic.
 *
 * Modern era boundaries, era-year conversion and Gregorian/JDN mapping stay
 * together so host ICU era-table changes cannot alter Calendar's native
 * arithmetic behaviour. Locale-sensitive presentation remains outside this
 * private module.
 */

#include "calendar-arithmetic-internal.h"

#include "integer-math.h"
#include "julian-day.h"

typedef struct
{
    gint code;
    gint start_year;
    gint start_month;
    gint start_day;
} JapaneseEra;

/*
 * CLDR's modern Japanese era boundary data. CLDR 49 / ICU 79 removes the
 * pre-Meiji era sequence, so Calendar owns the stable modern boundary that
 * current ICU itself converges on instead of inheriting version-dependent
 * historical-era tables from the host.
 */
static const JapaneseEra japanese_eras[] = {
    { 232, 1868, 10, 23 }, /* Meiji */
    { 233, 1912, 7, 30 },  /* Taisho */
    { 234, 1926, 12, 25 }, /* Showa */
    { 235, 1989, 1, 8 },   /* Heisei */
    { 236, 2019, 5, 1 }    /* Reiwa */
};

static gboolean
japanese_date_on_or_after(gint year,
                          gint month,
                          gint day,
                          const JapaneseEra *era)
{
    if (year != era->start_year)
        return year > era->start_year;
    if (month != era->start_month)
        return month > era->start_month;
    return day >= era->start_day;
}

static const JapaneseEra *
japanese_era_by_code(gint64 code)
{
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        if (code == japanese_eras[index].code)
            return &japanese_eras[index];
    }
    return NULL;
}

gint64
calendar_plus_arithmetic_japanese_signed_year_from_fields(const CalendarPlusCalendarFields *fields)
{
    const JapaneseEra *era;

    if (fields->auxiliary == 0)
        return calendar_plus_i64_subtract_saturating(1, fields->year);
    if (fields->auxiliary == 1)
        return fields->year;

    era = japanese_era_by_code(fields->auxiliary);
    if (era == NULL)
        return fields->year;

    return calendar_plus_i64_add_saturating(
        era->start_year,
        calendar_plus_i64_subtract_saturating(fields->year, 1));
}

void
calendar_plus_arithmetic_japanese_set_year_for_gregorian(gint64 gregorian_year,
                                CalendarPlusCalendarFields *fields)
{
    gsize index;

    if (gregorian_year < 1)
    {
        fields->year =
            calendar_plus_i64_subtract_saturating(1, gregorian_year);
        fields->auxiliary = 0;
        return;
    }

    fields->year = gregorian_year;
    fields->auxiliary = 1;
    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        if (gregorian_year < japanese_eras[index].start_year)
            break;
        fields->year = calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                gregorian_year, japanese_eras[index].start_year),
            1);
        fields->auxiliary = japanese_eras[index].code;
    }
}

void
calendar_plus_arithmetic_japanese_fields_from_jdn(gint64 jdn,
                  CalendarPlusCalendarFields *fields)
{
    gint year;
    gint month;
    gint day;
    gsize index;

    calendar_plus_jdn_to_gregorian(jdn, &year, &month, &day);
    fields->year = year >= 1 ? year :
        calendar_plus_i64_subtract_saturating(1, year);
    fields->month = month;
    fields->day = day;
    fields->auxiliary = year >= 1 ? 1 : 0;
    fields->special = FALSE;

    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        const JapaneseEra *era = &japanese_eras[index];

        if (!japanese_date_on_or_after(year, month, day, era))
            break;

        fields->year = calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(year, era->start_year),
            1);
        fields->auxiliary = era->code;
    }
}

