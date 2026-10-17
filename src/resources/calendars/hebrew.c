// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native Hebrew calendar resource.
 *
 * This module is intentionally self-contained at the chronology boundary. The
 * 19-year cycle, postponement rules, month tables, navigation and localized ICU
 * presentation all travel with the capability. Calendar loads it through the
 * stable provider ABI; Common discovers its product-neutral metadata. Nothing
 * in Calendar's compiled registry implements Hebrew as a fallback.
 */

#include "calendar-registry.h"
#include "integer-math.h"
#include "julian-day.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal_runtime.h>

#include <glib.h>
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#if defined(_WIN32)
#define CALENDAR_RESOURCE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define CALENDAR_RESOURCE_EXPORT __attribute__((visibility("default")))
#else
#define CALENDAR_RESOURCE_EXPORT
#endif

#define MILLISECONDS_PER_DAY 86400000.0

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

static const UChar utc_zone[] = { 0x0055, 0x0054, 0x0043, 0 };

static gboolean
hebrew_is_leap(gint64 year)
{
    return calendar_positive_modulo(
        calendar_i64_add_saturating(
            calendar_i64_multiply_saturating(year, 12), 17),
        19) >= 12;
}

static gint64
hebrew_start_of_year(gint64 year)
{
    const gint64 months = calendar_floor_divide(
        calendar_i64_subtract_saturating(
            calendar_i64_multiply_saturating(235, year), 234),
        19);
    const gint64 total_fraction = calendar_i64_add_saturating(
        calendar_i64_multiply_saturating(months, HEBREW_MONTH_FRACT),
        HEBREW_BAHARAD);
    const gint64 fraction_days = calendar_floor_divide(
        total_fraction, HEBREW_DAY_PARTS);
    gint64 day = calendar_i64_add_saturating(
        calendar_i64_multiply_saturating(months, 29),
        fraction_days);
    const gint64 fraction = calendar_i64_subtract_saturating(
        total_fraction,
        calendar_i64_multiply_saturating(
            fraction_days, HEBREW_DAY_PARTS));
    gint weekday = (gint)calendar_positive_modulo(day, 7);

    if (weekday == 2 || weekday == 4 || weekday == 6 ||
        (weekday == 0 &&
         fraction >= 21 * HEBREW_HOUR_PARTS + 589 &&
         hebrew_is_leap(calendar_i64_subtract_saturating(year, 1))))
    {
        day = calendar_i64_add_saturating(day, 1);
    }
    else if (weekday == 1 &&
             fraction >= 15 * HEBREW_HOUR_PARTS + 204 &&
             !hebrew_is_leap(year))
    {
        day = calendar_i64_add_saturating(day, 2);
    }

    return day;
}

static gint
hebrew_year_type(gint64 year)
{
    gint64 length = calendar_i64_subtract_saturating(
        hebrew_start_of_year(calendar_i64_add_saturating(year, 1)),
        hebrew_start_of_year(year));

    if (length > 380)
        length = calendar_i64_subtract_saturating(length, 30);

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
        return hebrew_month_length_table[month_index][hebrew_year_type(year)];
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
    gint64 result = calendar_i64_add_saturating(
        hebrew_start_of_year(year), HEBREW_EPOCH_OFFSET);

    result = calendar_i64_add_saturating(
        result, hebrew_month_start_offset(year, month));
    return calendar_i64_add_saturating(result, day - 1);
}

static void
hebrew_from_jdn(gint64 jdn,
                CalendarCalendarFields *fields)
{
    const gint64 d = calendar_i64_subtract_saturating(
        jdn, HEBREW_EPOCH_OFFSET - 1);
    const gint64 approximate_months = calendar_floor_divide(
        calendar_i64_multiply_saturating(d, HEBREW_DAY_PARTS),
        HEBREW_MONTH_PARTS);
    gint64 year = calendar_i64_add_saturating(
        calendar_floor_divide(
            calendar_i64_add_saturating(
                calendar_i64_multiply_saturating(19, approximate_months),
                234),
            235),
        1);
    gint64 day_of_year = calendar_i64_subtract_saturating(
        d, hebrew_start_of_year(year));
    gint month_index = 0;
    gint type;
    gboolean leap;

    while (day_of_year < 1)
    {
        year = calendar_i64_subtract_saturating(year, 1);
        day_of_year = calendar_i64_subtract_saturating(
            d, hebrew_start_of_year(year));
    }

    type = hebrew_year_type(year);
    leap = hebrew_is_leap(year);
    while (month_index < 14 &&
           day_of_year >
               (leap ? hebrew_leap_month_start[month_index][type] :
                       hebrew_month_start[month_index][type]))
    {
        month_index++;
    }
    if (month_index > 0)
        month_index--;

    *fields = (CalendarCalendarFields){
        .year = year,
        .month = month_index + 1,
        .day = (gint)calendar_i64_subtract_saturating(
            day_of_year,
            leap ? hebrew_leap_month_start[month_index][type] :
                   hebrew_month_start[month_index][type]),
        .auxiliary = 0,
        .special = FALSE
    };
}

static void
hebrew_add_months_to_fields(CalendarCalendarFields *fields,
                            gint amount)
{
    gint64 year = fields->year;
    gint month = fields->month - 1;
    gint remaining = amount;

    if (remaining >= 235 || remaining <= -235)
    {
        const gint cycles = remaining / 235;
        year = calendar_i64_add_saturating(
            year, calendar_i64_multiply_saturating(cycles, 19));
        remaining -= cycles * 235;
    }

    while (remaining > 0)
    {
        month++;
        if (month > 12)
        {
            month = 0;
            year = calendar_i64_add_saturating(year, 1);
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
            year = calendar_i64_subtract_saturating(year, 1);
            month = 12;
        }
        if (month == 5 && !hebrew_is_leap(year))
            month--;
        remaining++;
    }

    fields->year = year;
    fields->month = month + 1;
    fields->day = MIN(fields->day, hebrew_month_length(year, fields->month));
}

static gboolean
make_icu_locale(gchar *locale,
                gint32 locale_capacity)
{
    UErrorCode status = U_ZERO_ERROR;
    const gchar *language;

    if (locale == NULL || locale_capacity <= 0)
        return FALSE;

    language = g_get_language_names()[0];
    if (infiltratr_string_equal(language, "C") ||
        infiltratr_string_equal(language, "POSIX"))
    {
        language = "en_US";
    }
    infiltratr_copy_string(locale, (size_t)locale_capacity, language);
    uloc_setKeywordValue("calendar",
                         "hebrew",
                         locale,
                         locale_capacity,
                         &status);
    return U_SUCCESS(status);
}

static UCalendar *
open_hebrew_calendar(UErrorCode *status)
{
    gchar locale[ULOC_FULLNAME_CAPACITY];

    if (!make_icu_locale(locale, G_N_ELEMENTS(locale)))
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    return ucal_open(utc_zone, 3, locale, UCAL_DEFAULT, status);
}

static UDate
jdn_to_udate(gint64 jdn)
{
    return (UDate)(jdn - CALENDAR_UNIX_EPOCH_JDN) *
               MILLISECONDS_PER_DAY +
           MILLISECONDS_PER_DAY / 2.0;
}

static gchar *
utf16_to_utf8(const UChar *source,
              gint32 source_length)
{
    UErrorCode status = U_ZERO_ERROR;
    gint32 required = 0;
    gchar *result;
    size_t capacity = 0U;

    u_strToUTF8(NULL, 0, &required, source, source_length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
        return g_strdup("");

    status = U_ZERO_ERROR;
    if (required < 0 ||
        !infiltratr_size_add_checked((size_t)required, 1U, &capacity) ||
        capacity > (size_t)G_MAXINT32)
    {
        return g_strdup("");
    }

    result = g_malloc(capacity);
    u_strToUTF8(result,
                (gint32)capacity,
                NULL,
                source,
                source_length,
                &status);
    if (U_FAILURE(status))
    {
        g_free(result);
        return g_strdup("");
    }
    return result;
}

static const gchar *
pattern_for_part(CalendarDatePart part)
{
    if (part == CALENDAR_DATE_PART_DAY)
        return "d";
    if (part == CALENDAR_DATE_PART_MONTH)
        return "LLLL";
    if (part == CALENDAR_DATE_PART_YEAR)
        return "y";
    if (part == CALENDAR_DATE_PART_FULL)
        return "EEEE, d MMMM y G";
    return "d MMMM y G";
}

static gchar *
hebrew_format_jdn(gint64 jdn,
                  CalendarDatePart part)
{
    UErrorCode status = U_ZERO_ERROR;
    gchar locale[ULOC_FULLNAME_CAPACITY];
    UChar pattern_utf16[128];
    gint32 pattern_length = 0;
    UDateFormat *formatter;
    UCalendar *calendar;
    UChar stack_buffer[256];
    UChar *output = stack_buffer;
    gint32 capacity = G_N_ELEMENTS(stack_buffer);
    gint32 length;
    gchar *result;
    const gchar *pattern = pattern_for_part(part);

    if (!make_icu_locale(locale, G_N_ELEMENTS(locale)))
        return g_strdup("");

    u_strFromUTF8(pattern_utf16,
                  G_N_ELEMENTS(pattern_utf16),
                  &pattern_length,
                  pattern,
                  -1,
                  &status);
    if (U_FAILURE(status))
        return g_strdup("");

    formatter = udat_open(UDAT_PATTERN,
                          UDAT_PATTERN,
                          locale,
                          utc_zone,
                          3,
                          pattern_utf16,
                          pattern_length,
                          &status);
    if (U_FAILURE(status) || formatter == NULL)
        return g_strdup("");

    calendar = open_hebrew_calendar(&status);
    if (U_FAILURE(status) || calendar == NULL)
    {
        udat_close(formatter);
        return g_strdup("");
    }
    udat_setCalendar(formatter, calendar);
    ucal_close(calendar);

    length = udat_format(formatter,
                         jdn_to_udate(jdn),
                         output,
                         capacity,
                         NULL,
                         &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
    {
        size_t element_count = 0U;
        size_t allocation_bytes = 0U;

        status = U_ZERO_ERROR;
        if (length < 0 ||
            !infiltratr_size_add_checked(
                (size_t)length, 1U, &element_count) ||
            element_count > (size_t)G_MAXINT32 ||
            !infiltratr_size_multiply_checked(
                element_count, sizeof(UChar), &allocation_bytes))
        {
            udat_close(formatter);
            return g_strdup("");
        }

        capacity = (gint32)element_count;
        output = g_malloc(allocation_bytes);
        length = udat_format(formatter,
                             jdn_to_udate(jdn),
                             output,
                             capacity,
                             NULL,
                             &status);
    }

    result = U_FAILURE(status) ? g_strdup("") :
             utf16_to_utf8(output, length);
    if (output != stack_buffer)
        g_free(output);
    udat_close(formatter);
    return result;
}

static gboolean
provider_fields(const CalendarCalendarProvider *provider,
                gint64 jdn,
                CalendarCalendarFields *fields)
{
    (void)provider;
    if (fields == NULL)
        return FALSE;
    hebrew_from_jdn(jdn, fields);
    return TRUE;
}

static gchar *
provider_format(const CalendarCalendarProvider *provider,
                gint64 jdn,
                CalendarDatePart part)
{
    (void)provider;
    return hebrew_format_jdn(jdn, part);
}

static gint64
provider_period_start(const CalendarCalendarProvider *provider,
                      gint64 jdn)
{
    CalendarCalendarFields fields;

    (void)provider;
    hebrew_from_jdn(jdn, &fields);
    return hebrew_to_jdn(fields.year, fields.month, 1);
}

static gint64
provider_add_periods(const CalendarCalendarProvider *provider,
                     gint64 jdn,
                     gint amount)
{
    CalendarCalendarFields fields;

    (void)provider;
    hebrew_from_jdn(jdn, &fields);
    hebrew_add_months_to_fields(&fields, amount);
    return hebrew_to_jdn(fields.year, fields.month, fields.day);
}

static gint64
provider_add_years(const CalendarCalendarProvider *provider,
                   gint64 jdn,
                   gint amount)
{
    CalendarCalendarFields fields;

    (void)provider;
    hebrew_from_jdn(jdn, &fields);
    fields.year = calendar_i64_add_saturating(fields.year, amount);
    if (fields.month == 6 && !hebrew_is_leap(fields.year))
        fields.month = 7;
    fields.day = MIN(fields.day,
                     hebrew_month_length(fields.year, fields.month));
    return hebrew_to_jdn(fields.year, fields.month, fields.day);
}

static const InfiltratrTemporalResourceMetadata hebrew_metadata = {
    .struct_size = sizeof(InfiltratrTemporalResourceMetadata),
    .abi_version = INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    .kind = INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
    .id = "hebrew",
    .name = "Hebrew (fixed arithmetic calendar)",
    .supports_seconds = false,
    .requires_latitude = false,
    .requires_longitude = false
};

static const CalendarCalendarProvider hebrew_provider = {
    .abi_version = CALENDAR_CALENDAR_PROVIDER_ABI,
    .calendar_id = "hebrew",
    .common_index = G_MAXSIZE,
    .fields_from_jdn = provider_fields,
    .format = provider_format,
    .period_start = provider_period_start,
    .add_periods = provider_add_periods,
    .add_years = provider_add_years
};

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
CALENDAR_RESOURCE_EXPORT const CalendarCalendarProvider *
calendar_calendar_provider_v3(void);

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void)
{
    return &hebrew_metadata;
}

CALENDAR_RESOURCE_EXPORT const CalendarCalendarProvider *
calendar_calendar_provider_v3(void)
{
    return &hebrew_provider;
}
