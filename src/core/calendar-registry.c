/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Resource-only calendar-provider registry.
 *
 * Calendar identifiers are persisted in system presentation policy, so IDs are
 * stable data rather than implementation details. Trusted native resources
 * are the sole executable catalogue. Native resources
 * are discovered from the InfiltratorOS Calendar resource directory and are
 * admitted only after Common validates both their generic metadata and their
 * executable chronology ABI. Calendar then adapts that system-wide ABI into its
 * private dispatch interface; filesystem resources never depend on a Calendar-
 * application-specific loader contract.
 *
 * A calendar moved to native resources is absent when its module is absent; the
 * registry never manufactures a compiled fallback for it.
 */

#include "calendar-registry.h"


#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_calendar.h>
#include <infiltratr/temporal_runtime.h>

#ifndef CALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY
#define CALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY \
    INFILTRATR_TEMPORAL_NATIVE_CALENDAR_DIRECTORY
#endif

typedef struct
{
    CalendarCalendarProvider interface;
    InfiltratrTemporalCalendarModule *module;
    gchar *id;
    gchar *name;
    InfiltratrTemporalCalendarInfo info;
} RuntimeCalendarProvider;

static GPtrArray *runtime_providers;
static GOnce runtime_registry_once = G_ONCE_INIT;

static const RuntimeCalendarProvider *
runtime_provider(const CalendarCalendarProvider *provider)
{
    /* RuntimeCalendarProvider likewise embeds the private adapter first. */
    return (const RuntimeCalendarProvider *)provider;
}

static InfiltratrTemporalCalendarDatePart
runtime_date_part(CalendarDatePart part)
{
    switch (part)
    {
        case CALENDAR_DATE_PART_DAY:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_DAY;
        case CALENDAR_DATE_PART_MONTH:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_MONTH;
        case CALENDAR_DATE_PART_YEAR:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_YEAR;
        case CALENDAR_DATE_PART_SHORT:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_SHORT;
        case CALENDAR_DATE_PART_FULL:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL;
        case CALENDAR_DATE_PART_INVALID:
        default:
            return INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_INVALID;
    }
}

static gboolean
runtime_fields(const CalendarCalendarProvider *provider,
               gint64 jdn,
               CalendarCalendarFields *fields)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    InfiltratrTemporalCalendarFields common_fields;

    if (fields == NULL || runtime->module == NULL ||
        !infiltratr_temporal_calendar_fields_from_jdn(
            runtime->module, jdn, &common_fields))
    {
        return FALSE;
    }

    *fields = (CalendarCalendarFields){
        .year = common_fields.year,
        .month = common_fields.month,
        .day = common_fields.day,
        .auxiliary = common_fields.auxiliary,
        .special = common_fields.special
    };
    return TRUE;
}

static gchar *
runtime_format(const CalendarCalendarProvider *provider,
               gint64 jdn,
               CalendarDatePart part)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    gchar text[INFILTRATR_TEMPORAL_CALENDAR_TEXT_CAPACITY];
    size_t length = 0U;

    if (runtime->module == NULL ||
        !infiltratr_temporal_calendar_format(
            runtime->module,
            jdn,
            runtime_date_part(part),
            text,
            sizeof(text),
            &length))
    {
        return g_strdup("");
    }
    return g_strndup(text, length);
}

static gint64
runtime_period_start(const CalendarCalendarProvider *provider,
                     gint64 jdn)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    int64_t result = jdn;

    return infiltratr_temporal_calendar_period_start(
        runtime->module, jdn, &result) ? result : G_MAXINT64;
}

static gint64
runtime_add_periods(const CalendarCalendarProvider *provider,
                    gint64 jdn,
                    gint amount)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    int64_t result = jdn;

    return infiltratr_temporal_calendar_add_periods(
        runtime->module, jdn, (int32_t)amount, &result) ? result : G_MAXINT64;
}

static gint64
runtime_add_years(const CalendarCalendarProvider *provider,
                  gint64 jdn,
                  gint amount)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    int64_t result = jdn;

    return infiltratr_temporal_calendar_add_years(
        runtime->module, jdn, (int32_t)amount, &result) ? result : G_MAXINT64;
}

static RuntimeCalendarProvider *
runtime_provider_record_from_id(const gchar *calendar_id)
{
    guint index;

    if (runtime_providers == NULL || calendar_id == NULL)
        return NULL;

    for (index = 0; index < runtime_providers->len; index++)
    {
        RuntimeCalendarProvider *record =
            g_ptr_array_index(runtime_providers, index);
        if (record != NULL &&
            infiltratr_string_equal(record->id, calendar_id))
        {
            return record;
        }
    }
    return NULL;
}

static RuntimeCalendarProvider *
runtime_provider_record_from_interface(
    const CalendarCalendarProvider *provider)
{
    guint index;

    if (runtime_providers == NULL || provider == NULL)
        return NULL;

    for (index = 0; index < runtime_providers->len; index++)
    {
        RuntimeCalendarProvider *record =
            g_ptr_array_index(runtime_providers, index);
        if (record != NULL && &record->interface == provider)
            return record;
    }
    return NULL;
}

static bool
load_runtime_provider(
    const InfiltratrTemporalResourceMetadata *metadata,
    const char *module_path,
    void *user_data)
{
    InfiltratrTemporalCalendarModule *module;
    const InfiltratrTemporalResourceMetadata *loaded_metadata;
    RuntimeCalendarProvider *record;

    (void)user_data;
    if (metadata == NULL ||
        runtime_provider_record_from_id(metadata->id) != NULL)
    {
        /* The first validated native ID wins. */
        return true;
    }

    module = module_path != NULL
        ? infiltratr_temporal_calendar_module_open(module_path)
        : infiltratr_temporal_calendar_module_open_system();
    if (module == NULL)
        return true;

    loaded_metadata = infiltratr_temporal_calendar_module_metadata(module);
    if (loaded_metadata == NULL ||
        !infiltratr_string_equal(loaded_metadata->id, metadata->id))
    {
        infiltratr_temporal_calendar_module_free(module);
        return true;
    }

    record = g_new0(RuntimeCalendarProvider, 1);
    record->module = module;
    record->id = g_strdup(loaded_metadata->id);
    record->name = g_strdup(loaded_metadata->name);
    record->info.id = record->id;
    record->info.name = record->name;
    record->interface = (CalendarCalendarProvider){
        .abi_version = CALENDAR_CALENDAR_PROVIDER_ABI,
        .calendar_id = record->id,
        .common_index = G_MAXSIZE,
        .fields_from_jdn = runtime_fields,
        .format = runtime_format,
        .period_start = runtime_period_start,
        .add_periods = runtime_add_periods,
        .add_years = runtime_add_years
    };
    g_ptr_array_add(runtime_providers, record);
    return true;
}

static gpointer
initialise_runtime_registry(gpointer user_data)
{
    size_t ignored = 0U;

    (void)user_data;
    runtime_providers = g_ptr_array_new();
    /*
     * A missing directory is an empty optional catalogue. A directory I/O
     * error likewise leaves the catalogue empty. Individual
     * malformed modules are ignored by Common before or during behaviour load.
     */
    (void)infiltratr_temporal_runtime_scan_directory(
        CALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY,
        INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
        load_runtime_provider,
        NULL,
        &ignored);
    if (runtime_provider_record_from_id("gregorian") == NULL)
    {
        /* Common delegates the normal calendar to host services. This adapter
         * contains no custom calendar implementation or substitute engine. */
        InfiltratrTemporalCalendarModule *system =
            infiltratr_temporal_calendar_module_open_system();
        if (system != NULL)
        {
            (void)load_runtime_provider(
                infiltratr_temporal_calendar_module_metadata(system), NULL, NULL);
            infiltratr_temporal_calendar_module_free(system);
        }
    }
    return runtime_providers;
}

static void
ensure_runtime_registry(void)
{
    (void)g_once(&runtime_registry_once, initialise_runtime_registry, NULL);
}

gsize
calendar_calendar_provider_get_count(void)
{
    ensure_runtime_registry();
    return runtime_providers != NULL ? (gsize)runtime_providers->len : 0U;
}

const CalendarCalendarProvider *
calendar_calendar_provider_at(gsize index)
{
    ensure_runtime_registry();
    if (runtime_providers == NULL || index >= (gsize)runtime_providers->len)
        return NULL;

    return &((RuntimeCalendarProvider *)
        g_ptr_array_index(runtime_providers, (guint)index))->interface;
}

const InfiltratrTemporalCalendarInfo *
calendar_calendar_provider_info(
    const CalendarCalendarProvider *provider)
{
    RuntimeCalendarProvider *runtime_record;

    if (provider == NULL ||
        provider->abi_version != CALENDAR_CALENDAR_PROVIDER_ABI ||
        provider->calendar_id == NULL)
    {
        return NULL;
    }

    ensure_runtime_registry();
    runtime_record = runtime_provider_record_from_interface(provider);
    return runtime_record != NULL ? &runtime_record->info : NULL;
}

const CalendarCalendarProvider *
calendar_calendar_provider_from_id(const gchar *calendar_id)
{
    RuntimeCalendarProvider *runtime_record;

    ensure_runtime_registry();
    runtime_record = runtime_provider_record_from_id(calendar_id);
    return runtime_record != NULL ? &runtime_record->interface : NULL;
}
