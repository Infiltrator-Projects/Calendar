// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/* Native Coptic (Alexandrian 13-month) calendar resource. */

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal_calendar.h>

#include <glib.h>
#include <stdint.h>
#include <string.h>

#include "calendar-arithmetic-internal.h"
#include "calendar-types.h"
#include "icu-calendar.h"
#include "integer-math.h"

#if defined(_WIN32)
#define CALENDAR_RESOURCE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define CALENDAR_RESOURCE_EXPORT __attribute__((visibility("default")))
#else
#define CALENDAR_RESOURCE_EXPORT
#endif

static CalendarDatePart
internal_part(InfiltratrTemporalCalendarDatePart part)
{
    switch (part)
    {
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_DAY:
            return CALENDAR_DATE_PART_DAY;
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_MONTH:
            return CALENDAR_DATE_PART_MONTH;
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_YEAR:
            return CALENDAR_DATE_PART_YEAR;
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_SHORT:
            return CALENDAR_DATE_PART_SHORT;
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL:
            return CALENDAR_DATE_PART_FULL;
        case INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_INVALID:
        default:
            return CALENDAR_DATE_PART_INVALID;
    }
}

static bool
provider_fields(const InfiltratrTemporalCalendarProvider *provider,
                int64_t jdn,
                InfiltratrTemporalCalendarFields *fields)
{
    CalendarCalendarFields native_fields;

    (void)provider;
    if (fields == NULL ||
        !calendar_arithmetic_coptic_fields_from_jdn(
            CALENDAR_CALENDAR_MODE_COPTIC, jdn, &native_fields))
    {
        return false;
    }

    *fields = (InfiltratrTemporalCalendarFields){
        .year = native_fields.year,
        .month = native_fields.month,
        .day = native_fields.day,
        .auxiliary = native_fields.auxiliary,
        .special = native_fields.special
    };
    return true;
}

static int64_t
signed_year(const InfiltratrTemporalCalendarFields *fields)
{
    CalendarCalendarFields native_fields = {
        .year = fields->year,
        .month = fields->month,
        .day = fields->day,
        .auxiliary = fields->auxiliary,
        .special = fields->special
    };

    return calendar_arithmetic_coptic_signed_year_from_fields(
        CALENDAR_CALENDAR_MODE_COPTIC, &native_fields);
}

static void
set_signed_year(int64_t year,
                InfiltratrTemporalCalendarFields *fields)
{
    CalendarCalendarFields native_fields = {
        .month = fields->month,
        .day = fields->day,
        .special = fields->special
    };

    calendar_arithmetic_coptic_set_signed_year(
        CALENDAR_CALENDAR_MODE_COPTIC, year, &native_fields);
    fields->year = native_fields.year;
    fields->auxiliary = native_fields.auxiliary;
}

static int64_t
fields_to_jdn(const InfiltratrTemporalCalendarFields *fields)
{
    return calendar_arithmetic_coptic_fields_to_jdn(
        CALENDAR_CALENDAR_MODE_COPTIC,
        signed_year(fields),
        fields->month,
        fields->day);
}

static bool
provider_format(const InfiltratrTemporalCalendarProvider *provider,
                int64_t jdn,
                InfiltratrTemporalCalendarDatePart part,
                char *buffer,
                size_t capacity,
                size_t *length)
{
    const CalendarDatePart calendar_part = internal_part(part);
    gchar *text;
    size_t text_length;

    (void)provider;
    if (buffer == NULL || capacity == 0U || length == NULL ||
        calendar_part == CALENDAR_DATE_PART_INVALID)
    {
        return false;
    }

    text = calendar_icu_format(
        CALENDAR_CALENDAR_MODE_COPTIC, "coptic", jdn, calendar_part);
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

    infiltratr_copy_string(buffer, capacity, text);
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

    if (result == NULL || !provider_fields(provider, jdn, &fields))
        return false;
    fields.day = 1;
    *result = fields_to_jdn(&fields);
    return true;
}

static bool
provider_add_periods(const InfiltratrTemporalCalendarProvider *provider,
                     int64_t jdn,
                     int32_t amount,
                     int64_t *result)
{
    InfiltratrTemporalCalendarFields fields;
    int64_t year;
    int64_t serial;

    if (result == NULL || !provider_fields(provider, jdn, &fields))
        return false;

    year = signed_year(&fields);
    serial = infiltratr_i64_add_saturating(
        infiltratr_i64_add_saturating(
            infiltratr_i64_multiply_saturating(year, 13),
            fields.month - 1),
        amount);
    year = calendar_floor_divide(serial, 13);
    fields.month = (int32_t)calendar_positive_modulo(serial, 13) + 1;
    set_signed_year(year, &fields);
    fields.day = MIN(
        fields.day,
        calendar_arithmetic_coptic_month_length(year, fields.month));
    *result = fields_to_jdn(&fields);
    return true;
}

static bool
provider_add_years(const InfiltratrTemporalCalendarProvider *provider,
                   int64_t jdn,
                   int32_t amount,
                   int64_t *result)
{
    InfiltratrTemporalCalendarFields fields;
    int64_t year;

    if (result == NULL || !provider_fields(provider, jdn, &fields))
        return false;

    year = infiltratr_i64_add_saturating(signed_year(&fields), amount);
    set_signed_year(year, &fields);
    fields.day = MIN(
        fields.day,
        calendar_arithmetic_coptic_month_length(year, fields.month));
    *result = fields_to_jdn(&fields);
    return true;
}

static const InfiltratrTemporalResourceMetadata coptic_metadata = {
    .struct_size = sizeof(InfiltratrTemporalResourceMetadata),
    .abi_version = INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    .kind = INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
    .id = "coptic",
    .name = "Coptic (Alexandrian 13-month calendar)",
    .supports_seconds = false,
    .requires_latitude = false,
    .requires_longitude = false
};

static const InfiltratrTemporalCalendarProvider coptic_provider = {
    .struct_size = sizeof(InfiltratrTemporalCalendarProvider),
    .abi_version = INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI,
    .calendar_id = "coptic",
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
    return &coptic_metadata;
}

CALENDAR_RESOURCE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void)
{
    return &coptic_provider;
}
