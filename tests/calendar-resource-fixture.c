// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#include "calendar-registry.h"

#include <infiltratr/temporal_runtime.h>

#if defined(_WIN32)
#define CALENDAR_FIXTURE_EXPORT __declspec(dllexport)
#else
#define CALENDAR_FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

static gboolean fixture_fields(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    CalendarCalendarFields *fields)
{
    (void)provider;
    if (fields == NULL)
        return FALSE;

    fields->year = 9000 + (jdn % 100);
    fields->month = 1;
    fields->day = 1;
    fields->auxiliary = 0;
    fields->special = FALSE;
    return TRUE;
}

static gchar *fixture_format(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    CalendarDatePart part)
{
    (void)provider;
    (void)jdn;
    (void)part;
    return g_strdup("Fixture Calendar Date");
}

static gint64 fixture_period_start(
    const CalendarCalendarProvider *provider,
    gint64 jdn)
{
    (void)provider;
    return jdn;
}

static gint64 fixture_add_periods(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    gint amount)
{
    (void)provider;
    return jdn + amount;
}

static gint64 fixture_add_years(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    gint amount)
{
    (void)provider;
    return jdn + ((gint64)amount * 365);
}

CALENDAR_FIXTURE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
CALENDAR_FIXTURE_EXPORT const CalendarCalendarProvider *
calendar_calendar_provider_v3(void);

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

CALENDAR_FIXTURE_EXPORT const CalendarCalendarProvider *
calendar_calendar_provider_v3(void)
{
    static const CalendarCalendarProvider provider = {
        CALENDAR_CALENDAR_PROVIDER_ABI,
        "fixture-calendar",
        0U,
        fixture_fields,
        fixture_format,
        fixture_period_start,
        fixture_add_periods,
        fixture_add_years
    };
    return &provider;
}
