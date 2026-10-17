/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Dual-source calendar-provider registry.
 *
 * Calendar identifiers are persisted in system presentation policy, so IDs are
 * stable data rather than implementation details. Compiled providers and
 * trusted native resources are peers in one runtime catalogue. Native resources
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

#include "calendar-arithmetic.h"
#include "calendar-custom.h"
#include "calendar-swedish.h"
#include "icu-calendar.h"

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
    CalendarCalendarMode mode;
    const gchar *icu_keyword;
} BuiltinCalendarProvider;

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

static const BuiltinCalendarProvider *
builtin_provider(const CalendarCalendarProvider *provider)
{
    /* The first-member invariant makes this container cast well-defined. */
    return (const BuiltinCalendarProvider *)provider;
}

static const RuntimeCalendarProvider *
runtime_provider(const CalendarCalendarProvider *provider)
{
    /* RuntimeCalendarProvider likewise embeds the private adapter first. */
    return (const RuntimeCalendarProvider *)provider;
}

static gboolean
icu_fields(const CalendarCalendarProvider *provider,
           gint64 jdn,
           CalendarCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_icu_fields_from_jdn(
        builtin->icu_keyword, jdn, fields);
}

static gchar *
icu_format(const CalendarCalendarProvider *provider,
           gint64 jdn,
           CalendarDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_icu_format(
        builtin->mode, builtin->icu_keyword, jdn, part);
}

static gint64
icu_period_start(const CalendarCalendarProvider *provider,
                 gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_icu_month_start(builtin->icu_keyword, jdn);
}

static gint64
icu_add_periods(const CalendarCalendarProvider *provider,
                gint64 jdn,
                gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_icu_add_months(
        builtin->icu_keyword, jdn, amount);
}

static gint64
icu_add_years(const CalendarCalendarProvider *provider,
              gint64 jdn,
              gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_icu_add_years(
        builtin->icu_keyword, jdn, amount);
}

static gboolean
arithmetic_fields(const CalendarCalendarProvider *provider,
                  gint64 jdn,
                  CalendarCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_arithmetic_fields_from_jdn(
        builtin->mode, jdn, fields);
}

static gint64
arithmetic_period_start(const CalendarCalendarProvider *provider,
                        gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_arithmetic_month_start(builtin->mode, jdn);
}

static gint64
arithmetic_add_periods(const CalendarCalendarProvider *provider,
                       gint64 jdn,
                       gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_arithmetic_add_months(
        builtin->mode, jdn, amount);
}

static gint64
arithmetic_add_years(const CalendarCalendarProvider *provider,
                     gint64 jdn,
                     gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_arithmetic_add_years(
        builtin->mode, jdn, amount);
}

static gboolean
custom_fields(const CalendarCalendarProvider *provider,
              gint64 jdn,
              CalendarCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    calendar_custom_fields_from_jdn(builtin->mode, jdn, fields);
    return TRUE;
}

static gchar *
custom_format(const CalendarCalendarProvider *provider,
              gint64 jdn,
              CalendarDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);
    CalendarCalendarFields fields;

    calendar_custom_fields_from_jdn(builtin->mode, jdn, &fields);
    return calendar_custom_format(builtin->mode, &fields, part);
}

static gint64
custom_period_start(const CalendarCalendarProvider *provider,
                    gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_custom_month_start(builtin->mode, jdn);
}

static gint64
custom_add_periods(const CalendarCalendarProvider *provider,
                   gint64 jdn,
                   gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_custom_add_months(builtin->mode, jdn, amount);
}

static gint64
custom_add_years(const CalendarCalendarProvider *provider,
                 gint64 jdn,
                 gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_custom_add_years(builtin->mode, jdn, amount);
}

static gboolean
swedish_fields(const CalendarCalendarProvider *provider,
               gint64 jdn,
               CalendarCalendarFields *fields)
{
    (void)provider;
    calendar_swedish_fields_from_jdn(jdn, fields);
    return TRUE;
}

static gchar *
swedish_format(const CalendarCalendarProvider *provider,
               gint64 jdn,
               CalendarDatePart part)
{
    CalendarCalendarFields fields;

    (void)provider;
    calendar_swedish_fields_from_jdn(jdn, &fields);
    return calendar_swedish_format(&fields, part);
}

static gint64
swedish_period_start(const CalendarCalendarProvider *provider,
                     gint64 jdn)
{
    (void)provider;
    return calendar_swedish_month_start(jdn);
}

static gint64
swedish_add_periods(const CalendarCalendarProvider *provider,
                    gint64 jdn,
                    gint amount)
{
    (void)provider;
    return calendar_swedish_add_months(jdn, amount);
}

static gint64
swedish_add_years(const CalendarCalendarProvider *provider,
                  gint64 jdn,
                  gint amount)
{
    (void)provider;
    return calendar_swedish_add_years(jdn, amount);
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

    (void)infiltratr_temporal_calendar_period_start(
        runtime->module, jdn, &result);
    return result;
}

static gint64
runtime_add_periods(const CalendarCalendarProvider *provider,
                    gint64 jdn,
                    gint amount)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    int64_t result = jdn;

    (void)infiltratr_temporal_calendar_add_periods(
        runtime->module, jdn, (int32_t)amount, &result);
    return result;
}

static gint64
runtime_add_years(const CalendarCalendarProvider *provider,
                  gint64 jdn,
                  gint amount)
{
    const RuntimeCalendarProvider *runtime = runtime_provider(provider);
    int64_t result = jdn;

    (void)infiltratr_temporal_calendar_add_years(
        runtime->module, jdn, (int32_t)amount, &result);
    return result;
}

#define PROVIDER_INTERFACE(mode_, id_, fields_, format_, start_, add_, years_) \
    { \
        CALENDAR_CALENDAR_PROVIDER_ABI, \
        id_, \
        CALENDAR_CALENDAR_MODE_##mode_, \
        fields_, format_, start_, add_, years_ \
    }

#define ICU_PROVIDER(mode_, id_, keyword_) \
    [CALENDAR_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           icu_fields, icu_format, icu_period_start, \
                           icu_add_periods, icu_add_years), \
        CALENDAR_CALENDAR_MODE_##mode_, keyword_ \
    }

#define ARITHMETIC_PROVIDER(mode_, id_, keyword_) \
    [CALENDAR_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           arithmetic_fields, icu_format, \
                           arithmetic_period_start, arithmetic_add_periods, \
                           arithmetic_add_years), \
        CALENDAR_CALENDAR_MODE_##mode_, keyword_ \
    }

#define CUSTOM_PROVIDER(mode_, id_) \
    [CALENDAR_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           custom_fields, custom_format, custom_period_start, \
                           custom_add_periods, custom_add_years), \
        CALENDAR_CALENDAR_MODE_##mode_, NULL \
    }

#define SWEDISH_PROVIDER(mode_, id_) \
    [CALENDAR_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           swedish_fields, swedish_format, swedish_period_start, \
                           swedish_add_periods, swedish_add_years), \
        CALENDAR_CALENDAR_MODE_##mode_, NULL \
    }

/*
 * Local compiled implementation slots are keyed explicitly by stable calendar
 * ID. Native filesystem providers have no entry in this array. Common owns
 * descriptive catalogue metadata, but catalogue completeness and array ordering
 * are deliberately not runtime prerequisites for chronology.
 */
static const BuiltinCalendarProvider providers[] = {
    ARITHMETIC_PROVIDER(GREGORIAN, "gregorian", "gregorian"),
    CUSTOM_PROVIDER(ISO_WEEK, "iso-week"),
    CUSTOM_PROVIDER(JULIAN, "julian"),
    CUSTOM_PROVIDER(REVISED_JULIAN, "revised-julian"),
    ARITHMETIC_PROVIDER(ISLAMIC_UMM_AL_QURA, "islamic-umalqura", "islamic-umalqura"),
    ARITHMETIC_PROVIDER(ISLAMIC_CIVIL, "islamic-civil", "islamic-civil"),
    ARITHMETIC_PROVIDER(ISLAMIC_TBLA, "islamic-tbla", "islamic-tbla"),
    ICU_PROVIDER(ISLAMIC, "islamic", "islamic"),
    ARITHMETIC_PROVIDER(PERSIAN, "persian", "persian"),
    CUSTOM_PROVIDER(BAHAI, "bahai"),
    ARITHMETIC_PROVIDER(BUDDHIST, "buddhist", "buddhist"),
    ARITHMETIC_PROVIDER(COPTIC, "coptic", "coptic"),
    ARITHMETIC_PROVIDER(ETHIOPIAN, "ethiopian", "ethiopic"),
    ARITHMETIC_PROVIDER(ETHIOPIC_AMETE_ALEM, "ethiopic-amete-alem", "ethiopic-amete-alem"),
    ICU_PROVIDER(CHINESE, "chinese", "chinese"),
    ICU_PROVIDER(DANGI, "dangi", "dangi"),
    ARITHMETIC_PROVIDER(INDIAN, "indian", "indian"),
    ARITHMETIC_PROVIDER(JAPANESE, "japanese", "japanese"),
    ARITHMETIC_PROVIDER(MINGUO, "minguo", "roc"),
    CUSTOM_PROVIDER(ROMAN, "roman"),
    CUSTOM_PROVIDER(BYZANTINE, "byzantine"),
    CUSTOM_PROVIDER(EGYPTIAN_NABONASSAR, "egyptian-nabonassar"),
    CUSTOM_PROVIDER(ARMENIAN_TRADITIONAL, "armenian-traditional"),
    CUSTOM_PROVIDER(MAYAN, "mayan"),
    CUSTOM_PROVIDER(FRENCH_REPUBLICAN, "french-republican"),
    SWEDISH_PROVIDER(SWEDISH_HISTORICAL, "swedish-historical"),
    CUSTOM_PROVIDER(INTERNATIONAL_FIXED, "international-fixed"),
    CUSTOM_PROVIDER(WORLD, "world"),
    CUSTOM_PROVIDER(POSITIVIST, "positivist")
};

G_STATIC_ASSERT(G_N_ELEMENTS(providers) == CALENDAR_CALENDAR_MODE_COUNT);

static const CalendarCalendarProvider *
provider_from_id_unchecked(const gchar *calendar_id)
{
    gsize index;

    if (calendar_id == NULL)
        return NULL;

    for (index = 0; index < G_N_ELEMENTS(providers); index++)
    {
        if (infiltratr_string_equal(
                calendar_id, providers[index].interface.calendar_id))
        {
            return &providers[index].interface;
        }
    }
    return NULL;
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
    if (metadata == NULL || module_path == NULL ||
        provider_from_id_unchecked(metadata->id) != NULL ||
        runtime_provider_record_from_id(metadata->id) != NULL)
    {
        /* Existing compiled capability and first validated native ID win. */
        return true;
    }

    module = infiltratr_temporal_calendar_module_open(module_path);
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
     * error likewise cannot remove the compiled capability set. Individual
     * malformed modules are ignored by Common before or during behaviour load.
     */
    (void)infiltratr_temporal_runtime_scan_directory(
        CALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY,
        INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
        load_runtime_provider,
        NULL,
        &ignored);
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
    return G_N_ELEMENTS(providers) +
        (runtime_providers != NULL ? (gsize)runtime_providers->len : 0U);
}

const CalendarCalendarProvider *
calendar_calendar_provider_at(gsize index)
{
    ensure_runtime_registry();
    if (index < G_N_ELEMENTS(providers))
        return &providers[index].interface;

    index -= G_N_ELEMENTS(providers);
    if (runtime_providers == NULL || index >= (gsize)runtime_providers->len)
        return NULL;

    return &((RuntimeCalendarProvider *)
        g_ptr_array_index(runtime_providers, (guint)index))->interface;
}

const InfiltratrTemporalCalendarInfo *
calendar_calendar_provider_info(
    const CalendarCalendarProvider *provider)
{
    const InfiltratrTemporalCalendarInfo *builtin_info;
    RuntimeCalendarProvider *runtime_record;

    if (provider == NULL ||
        provider->abi_version != CALENDAR_CALENDAR_PROVIDER_ABI ||
        provider->calendar_id == NULL)
    {
        return NULL;
    }

    builtin_info = infiltratr_temporal_calendar_find(provider->calendar_id);
    if (builtin_info != NULL &&
        provider_from_id_unchecked(provider->calendar_id) == provider)
    {
        return builtin_info;
    }

    ensure_runtime_registry();
    runtime_record = runtime_provider_record_from_interface(provider);
    return runtime_record != NULL ? &runtime_record->info : NULL;
}

const CalendarCalendarProvider *
calendar_calendar_provider_from_id(const gchar *calendar_id)
{
    const CalendarCalendarProvider *builtin;
    RuntimeCalendarProvider *runtime_record;

    builtin = provider_from_id_unchecked(calendar_id);
    if (builtin != NULL)
        return builtin;

    ensure_runtime_registry();
    runtime_record = runtime_provider_record_from_id(calendar_id);
    return runtime_record != NULL ? &runtime_record->interface : NULL;
}
