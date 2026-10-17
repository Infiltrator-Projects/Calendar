// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#include <infiltratr/temporal_calendar.h>

#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#define CALENDAR_FIXTURE_EXPORT __declspec(dllexport)
#else
#define CALENDAR_FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

static bool fixture_fields(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn,
    InfiltratrTemporalCalendarFields *fields)
{
    (void)provider;
    if (fields == NULL)
        return false;

    fields->year = INT64_C(9000) + (jdn % INT64_C(100));
    fields->month = 1;
    fields->day = 1;
    fields->auxiliary = 0;
    fields->special = false;
    return true;
}

static bool fixture_format(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn,
    InfiltratrTemporalCalendarDatePart part,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    static const char text[] = "Fixture Calendar Date";
    const size_t text_length = sizeof(text) - 1U;

    (void)provider;
    (void)jdn;
    (void)part;
    if (buffer == NULL || length == NULL || capacity <= text_length)
        return false;
    memcpy(buffer, text, sizeof(text));
    *length = text_length;
    return true;
}

static bool fixture_period_start(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn,
    int64_t *result)
{
    (void)provider;
    if (result == NULL)
        return false;
    *result = jdn;
    return true;
}

static bool fixture_add_periods(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn,
    int32_t amount,
    int64_t *result)
{
    (void)provider;
    if (result == NULL)
        return false;
    *result = jdn + amount;
    return true;
}

static bool fixture_add_years(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn,
    int32_t amount,
    int64_t *result)
{
    (void)provider;
    if (result == NULL)
        return false;
    *result = jdn + ((int64_t)amount * INT64_C(365));
    return true;
}

CALENDAR_FIXTURE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
CALENDAR_FIXTURE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void);

CALENDAR_FIXTURE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void)
{
    static const InfiltratrTemporalResourceMetadata metadata = {
        sizeof(InfiltratrTemporalResourceMetadata),
        INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
        INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
        "fixture-calendar",
        "Fixture Calendar",
        false,
        false,
        false
    };
    return &metadata;
}

CALENDAR_FIXTURE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void)
{
    static const InfiltratrTemporalCalendarProvider provider = {
        sizeof(InfiltratrTemporalCalendarProvider),
        INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI,
        "fixture-calendar",
        fixture_fields,
        fixture_format,
        fixture_period_start,
        fixture_add_periods,
        fixture_add_years
    };
    return &provider;
}
