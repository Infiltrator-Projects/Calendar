// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native Hebrew calendar resource.
 *
 * The chronology and its presentation travel as one system resource. The
 * module exports Common's generic temporal metadata plus Common's executable
 * calendar-provider ABI; it has no dependency on Calendar's private registry
 * ABI. Any Common consumer can therefore discover and execute this installed
 * capability from /System/Resources/Calendars.
 */

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal_calendar.h>

#include <glib.h>
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#define CALENDAR_RESOURCE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define CALENDAR_RESOURCE_EXPORT __attribute__((visibility("default")))
#else
#define CALENDAR_RESOURCE_EXPORT
#endif

#define MILLISECONDS_PER_DAY 86400000.0
#define HEBREW_UNIX_EPOCH_JDN INT64_C(2440588)

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

static int64_t
floor_divide(int64_t value,
             int64_t divisor)
{
    int64_t quotient = 0;

    return infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL) ?
        quotient : 0;
}

static int64_t
positive_modulo(int64_t value,
                int64_t modulus)
{
    int64_t remainder = 0;

    return infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder) ?
        remainder : 0;
}

static gboolean
hebrew_is_leap(int64_t year)
{
    return positive_modulo(
        infiltratr_i64_add_saturating(
            infiltratr_i64_multiply_saturating(year, 12), 17),
        19) >= 12;
}

static int64_t
hebrew_start_of_year(int64_t year)
{
    const int64_t months = floor_divide(
        infiltratr_i64_subtract_saturating(
            infiltratr_i64_multiply_saturating(235, year), 234),
        19);
    const int64_t total_fraction = infiltratr_i64_add_saturating(
        infiltratr_i64_multiply_saturating(months, HEBREW_MONTH_FRACT),
        HEBREW_BAHARAD);
    const int64_t fraction_days = floor_divide(
        total_fraction, HEBREW_DAY_PARTS);
    int64_t day = infiltratr_i64_add_saturating(
        infiltratr_i64_multiply_saturating(months, 29),
        fraction_days);
    const int64_t fraction = infiltratr_i64_subtract_saturating(
        total_fraction,
        infiltratr_i64_multiply_saturating(
            fraction_days, HEBREW_DAY_PARTS));
    gint weekday = (gint)positive_modulo(day, 7);

    if (weekday == 2 || weekday == 4 || weekday == 6 ||
        (weekday == 0 &&
         fraction >= 21 * HEBREW_HOUR_PARTS + 589 &&
         hebrew_is_leap(infiltratr_i64_subtract_saturating(year, 1))))
    {
        day = infiltratr_i64_add_saturating(day, 1);
    }
    else if (weekday == 1 &&
             fraction >= 15 * HEBREW_HOUR_PARTS + 204 &&
             !hebrew_is_leap(year))
    {
        day = infiltratr_i64_add_saturating(day, 2);
    }

    return day;
}

static gint
hebrew_year_type(int64_t year)
{
    int64_t length = infiltratr_i64_subtract_saturating(
        hebrew_start_of_year(infiltratr_i64_add_saturating(year, 1)),
        hebrew_start_of_year(year));

    if (length > 380)
        length = infiltratr_i64_subtract_saturating(length, 30);

    if (length == 353)
        return 0;
    if (length == 355)
        return 2;
    return 1;
}

static gint
hebrew_month_length(int64_t year,
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
hebrew_month_start_offset(int64_t year,
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

static int64_t
hebrew_to_jdn(int64_t year,
              gint month,
              gint day)
{
    int64_t result = infiltratr_i64_add_saturating(
        hebrew_start_of_year(year), HEBREW_EPOCH_OFFSET);

    result = infiltratr_i64_add_saturating(
        result, hebrew_month_start_offset(year, month));
    return infiltratr_i64_add_saturating(result, day - 1);
}

static void
hebrew_from_jdn(int64_t jdn,
                InfiltratrTemporalCalendarFields *fields)
{
    const int64_t d = infiltratr_i64_subtract_saturating(
        jdn, HEBREW_EPOCH_OFFSET - 1);
    const int64_t approximate_months = floor_divide(
        infiltratr_i64_multiply_saturating(d, HEBREW_DAY_PARTS),
        HEBREW_MONTH_PARTS);
    int64_t year = infiltratr_i64_add_saturating(
        floor_divide(
            infiltratr_i64_add_saturating(
                infiltratr_i64_multiply_saturating(19, approximate_months),
                234),
            235),
        1);
    int64_t day_of_year = infiltratr_i64_subtract_saturating(
        d, hebrew_start_of_year(year));
    gint month_index = 0;
    gint type;
    gboolean leap;

    while (day_of_year < 1)
    {
        year = infiltratr_i64_subtract_saturating(year, 1);
        day_of_year = infiltratr_i64_subtract_saturating(
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

    *fields = (InfiltratrTemporalCalendarFields){
        .year = year,
        .month = month_index + 1,
        .day = (int32_t)infiltratr_i64_subtract_saturating(
            day_of_year,
            leap ? hebrew_leap_month_start[month_index][type] :
                   hebrew_month_start[month_index][type]),
        .auxiliary = 0,
        .special = false
    };
}

static void
hebrew_add_months_to_fields(InfiltratrTemporalCalendarFields *fields,
                            int32_t amount)
{
    int64_t year = fields->year;
    gint month = fields->month - 1;
    int32_t remaining = amount;

    if (remaining >= 235 || remaining <= -235)
    {
        const int32_t cycles = remaining / 235;
        year = infiltratr_i64_add_saturating(
            year, infiltratr_i64_multiply_saturating(cycles, 19));
        remaining -= cycles * 235;
    }

    while (remaining > 0)
    {
        month++;
        if (month > 12)
        {
            month = 0;
            year = infiltratr_i64_add_saturating(year, 1);
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
            year = infiltratr_i64_subtract_saturating(year, 1);
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
jdn_to_udate(int64_t jdn)
{
    return (UDate)(jdn - HEBREW_UNIX_EPOCH_JDN) *
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
pattern_for_part(InfiltratrTemporalCalendarDatePart part)
{
    if (part == INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_DAY)
        return "d";
    if (part == INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_MONTH)
        return "LLLL";
    if (part == INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_YEAR)
        return "y";
    if (part == INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL)
        return "EEEE, d MMMM y G";
    return "d MMMM y G";
}

static gchar *
hebrew_format_jdn(int64_t jdn,
                  InfiltratrTemporalCalendarDatePart part)
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

static bool
provider_fields(const InfiltratrTemporalCalendarProvider *provider,
                int64_t jdn,
                InfiltratrTemporalCalendarFields *fields)
{
    (void)provider;
    if (fields == NULL)
        return false;
    hebrew_from_jdn(jdn, fields);
    return true;
}

static bool
provider_format(const InfiltratrTemporalCalendarProvider *provider,
                int64_t jdn,
                InfiltratrTemporalCalendarDatePart part,
                char *buffer,
                size_t capacity,
                size_t *length)
{
    gchar *text;
    size_t text_length;

    (void)provider;
    if (buffer == NULL || capacity == 0U || length == NULL ||
        part <= INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_INVALID ||
        part > INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL)
    {
        return false;
    }

    text = hebrew_format_jdn(jdn, part);
    if (text == NULL || text[0] == '\0')
    {
        g_free(text);
        return false;
    }
    text_length = strlen(text);
    if (text_length >= capacity)
    {
        g_free(text);
        return false;
    }

    memcpy(buffer, text, text_length + 1U);
    *length = text_length;
    g_free(text);
    return true;
}

static bool
provider_period_start(const InfiltratrTemporalCalendarProvider *provider,
                      int64_t jdn,
                      int64_t *result)
{
    InfiltratrTemporalCalendarFields fields;

    (void)provider;
    if (result == NULL)
        return false;
    hebrew_from_jdn(jdn, &fields);
    *result = hebrew_to_jdn(fields.year, fields.month, 1);
    return true;
}

static bool
provider_add_periods(const InfiltratrTemporalCalendarProvider *provider,
                     int64_t jdn,
                     int32_t amount,
                     int64_t *result)
{
    InfiltratrTemporalCalendarFields fields;

    (void)provider;
    if (result == NULL)
        return false;
    hebrew_from_jdn(jdn, &fields);
    hebrew_add_months_to_fields(&fields, amount);
    *result = hebrew_to_jdn(fields.year, fields.month, fields.day);
    return true;
}

static bool
provider_add_years(const InfiltratrTemporalCalendarProvider *provider,
                   int64_t jdn,
                   int32_t amount,
                   int64_t *result)
{
    InfiltratrTemporalCalendarFields fields;

    (void)provider;
    if (result == NULL)
        return false;
    hebrew_from_jdn(jdn, &fields);
    fields.year = infiltratr_i64_add_saturating(fields.year, amount);
    if (fields.month == 6 && !hebrew_is_leap(fields.year))
        fields.month = 7;
    fields.day = MIN(fields.day,
                     hebrew_month_length(fields.year, fields.month));
    *result = hebrew_to_jdn(fields.year, fields.month, fields.day);
    return true;
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

static const InfiltratrTemporalCalendarProvider hebrew_provider = {
    .struct_size = sizeof(InfiltratrTemporalCalendarProvider),
    .abi_version = INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI,
    .calendar_id = "hebrew",
    .fields_from_jdn = provider_fields,
    .format = provider_format,
    .period_start = provider_period_start,
    .add_periods = provider_add_periods,
    .add_years = provider_add_years
};

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void);

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void)
{
    return &hebrew_metadata;
}

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void)
{
    return &hebrew_provider;
}
