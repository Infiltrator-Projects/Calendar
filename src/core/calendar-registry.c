/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Built-in calendar-provider registry.
 *
 * Calendar identifiers are persisted in Cinnamon settings, so IDs are stable
 * data rather than implementation details.  Each provider exposes the same
 * backend-neutral operation table.  CalendarSystem resolves that interface
 * once and never branches on ICU versus native implementation afterwards.
 *
 * BuiltinCalendarProvider embeds the generic interface first. The built-in
 * adapters can therefore recover their private enum or ICU keyword while the
 * interface itself remains independent of that private state.
 */

#include "calendar-registry.h"

#include "calendar-arithmetic.h"
#include "calendar-custom.h"
#include "calendar-swedish.h"
#include "icu-calendar.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>

typedef struct
{
    CalendarPlusCalendarProvider interface;
    CalendarPlusCalendarMode mode;
    const gchar *icu_keyword;
} BuiltinCalendarProvider;

static const BuiltinCalendarProvider *
builtin_provider(const CalendarPlusCalendarProvider *provider)
{
    /* The first-member invariant makes this container cast well-defined. */
    return (const BuiltinCalendarProvider *)provider;
}

static gboolean
icu_fields(const CalendarPlusCalendarProvider *provider,
           gint64 jdn,
           CalendarPlusCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_fields_from_jdn(
        builtin->icu_keyword, jdn, fields);
}

static gchar *
icu_format(const CalendarPlusCalendarProvider *provider,
           gint64 jdn,
           CalendarPlusDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_format(
        builtin->mode, builtin->icu_keyword, jdn, part);
}

static gint64
icu_period_start(const CalendarPlusCalendarProvider *provider,
                 gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_month_start(builtin->icu_keyword, jdn);
}

static gint64
icu_add_periods(const CalendarPlusCalendarProvider *provider,
                gint64 jdn,
                gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_add_months(
        builtin->icu_keyword, jdn, amount);
}

static gint64
icu_add_years(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_add_years(
        builtin->icu_keyword, jdn, amount);
}

static gboolean
arithmetic_fields(const CalendarPlusCalendarProvider *provider,
                  gint64 jdn,
                  CalendarPlusCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_arithmetic_fields_from_jdn(
        builtin->mode, jdn, fields);
}

static gint64
arithmetic_period_start(const CalendarPlusCalendarProvider *provider,
                        gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_arithmetic_month_start(builtin->mode, jdn);
}

static gint64
arithmetic_add_periods(const CalendarPlusCalendarProvider *provider,
                       gint64 jdn,
                       gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_arithmetic_add_months(
        builtin->mode, jdn, amount);
}

static gint64
arithmetic_add_years(const CalendarPlusCalendarProvider *provider,
                     gint64 jdn,
                     gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_arithmetic_add_years(
        builtin->mode, jdn, amount);
}

static gboolean
custom_fields(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              CalendarPlusCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    calendar_plus_custom_fields_from_jdn(builtin->mode, jdn, fields);
    return TRUE;
}

static gchar *
custom_format(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              CalendarPlusDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);
    CalendarPlusCalendarFields fields;

    calendar_plus_custom_fields_from_jdn(builtin->mode, jdn, &fields);
    return calendar_plus_custom_format(builtin->mode, &fields, part);
}

static gint64
custom_period_start(const CalendarPlusCalendarProvider *provider,
                    gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_month_start(builtin->mode, jdn);
}

static gint64
custom_add_periods(const CalendarPlusCalendarProvider *provider,
                   gint64 jdn,
                   gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_add_months(builtin->mode, jdn, amount);
}

static gint64
custom_add_years(const CalendarPlusCalendarProvider *provider,
                 gint64 jdn,
                 gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_add_years(builtin->mode, jdn, amount);
}

static gboolean
swedish_fields(const CalendarPlusCalendarProvider *provider,
               gint64 jdn,
               CalendarPlusCalendarFields *fields)
{
    (void)provider;
    calendar_plus_swedish_fields_from_jdn(jdn, fields);
    return TRUE;
}

static gchar *
swedish_format(const CalendarPlusCalendarProvider *provider,
               gint64 jdn,
               CalendarPlusDatePart part)
{
    CalendarPlusCalendarFields fields;

    (void)provider;
    calendar_plus_swedish_fields_from_jdn(jdn, &fields);
    return calendar_plus_swedish_format(&fields, part);
}

static gint64
swedish_period_start(const CalendarPlusCalendarProvider *provider,
                     gint64 jdn)
{
    (void)provider;
    return calendar_plus_swedish_month_start(jdn);
}

static gint64
swedish_add_periods(const CalendarPlusCalendarProvider *provider,
                    gint64 jdn,
                    gint amount)
{
    (void)provider;
    return calendar_plus_swedish_add_months(jdn, amount);
}

static gint64
swedish_add_years(const CalendarPlusCalendarProvider *provider,
                  gint64 jdn,
                  gint amount)
{
    (void)provider;
    return calendar_plus_swedish_add_years(jdn, amount);
}

#define PROVIDER_INTERFACE(mode_, fields_, format_, start_, add_, years_) \
    { \
        CALENDAR_PLUS_CALENDAR_PROVIDER_ABI, \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, \
        fields_, format_, start_, add_, years_ \
    }

#define ICU_PROVIDER(mode_, keyword_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, \
                           icu_fields, icu_format, icu_period_start, \
                           icu_add_periods, icu_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, keyword_ \
    }

#define ARITHMETIC_PROVIDER(mode_, keyword_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, \
                           arithmetic_fields, icu_format, \
                           arithmetic_period_start, arithmetic_add_periods, \
                           arithmetic_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, keyword_ \
    }

#define CUSTOM_PROVIDER(mode_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, \
                           custom_fields, custom_format, custom_period_start, \
                           custom_add_periods, custom_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

#define SWEDISH_PROVIDER(mode_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, \
                           swedish_fields, swedish_format, swedish_period_start, \
                           swedish_add_periods, swedish_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

/*
 * This table intentionally follows Common's catalogue order exactly.
 * Calendar owns only the backend implementation choice and ICU keyword.
 * Persisted identifiers and English presentation names are read from Common.
 */
static const BuiltinCalendarProvider providers[] = {
    ARITHMETIC_PROVIDER(GREGORIAN, "gregorian"),
    CUSTOM_PROVIDER(ISO_WEEK),
    CUSTOM_PROVIDER(JULIAN),
    CUSTOM_PROVIDER(REVISED_JULIAN),
    ARITHMETIC_PROVIDER(HEBREW, "hebrew"),
    ARITHMETIC_PROVIDER(ISLAMIC_UMM_AL_QURA, "islamic-umalqura"),
    ARITHMETIC_PROVIDER(ISLAMIC_CIVIL, "islamic-civil"),
    ARITHMETIC_PROVIDER(ISLAMIC_TBLA, "islamic-tbla"),
    ICU_PROVIDER(ISLAMIC, "islamic"),
    ARITHMETIC_PROVIDER(PERSIAN, "persian"),
    CUSTOM_PROVIDER(BAHAI),
    ARITHMETIC_PROVIDER(BUDDHIST, "buddhist"),
    ARITHMETIC_PROVIDER(COPTIC, "coptic"),
    ARITHMETIC_PROVIDER(ETHIOPIAN, "ethiopic"),
    ARITHMETIC_PROVIDER(ETHIOPIC_AMETE_ALEM, "ethiopic-amete-alem"),
    ICU_PROVIDER(CHINESE, "chinese"),
    ICU_PROVIDER(DANGI, "dangi"),
    ARITHMETIC_PROVIDER(INDIAN, "indian"),
    ARITHMETIC_PROVIDER(JAPANESE, "japanese"),
    ARITHMETIC_PROVIDER(MINGUO, "roc"),
    CUSTOM_PROVIDER(ROMAN),
    CUSTOM_PROVIDER(BYZANTINE),
    CUSTOM_PROVIDER(EGYPTIAN_NABONASSAR),
    CUSTOM_PROVIDER(ARMENIAN_TRADITIONAL),
    CUSTOM_PROVIDER(MAYAN),
    CUSTOM_PROVIDER(FRENCH_REPUBLICAN),
    SWEDISH_PROVIDER(SWEDISH_HISTORICAL),
    CUSTOM_PROVIDER(INTERNATIONAL_FIXED),
    CUSTOM_PROVIDER(WORLD),
    CUSTOM_PROVIDER(POSITIVIST)
};

G_STATIC_ASSERT(G_N_ELEMENTS(providers) == CALENDAR_PLUS_CALENDAR_MODE_COUNT);

gsize
calendar_plus_calendar_provider_get_count(void)
{
    const gsize common_count = infiltratr_temporal_calendar_count();

    return common_count == G_N_ELEMENTS(providers) ? common_count : 0;
}

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_at(gsize index)
{
    if (index >= calendar_plus_calendar_provider_get_count() ||
        providers[index].interface.common_index != index)
    {
        return NULL;
    }

    return &providers[index].interface;
}

const InfiltratrTemporalCalendarInfo *
calendar_plus_calendar_provider_info(
    const CalendarPlusCalendarProvider *provider)
{
    if (provider == NULL ||
        provider->abi_version != CALENDAR_PLUS_CALENDAR_PROVIDER_ABI ||
        provider->common_index >= calendar_plus_calendar_provider_get_count())
    {
        return NULL;
    }

    return infiltratr_temporal_calendar_at(provider->common_index);
}

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_from_id(const gchar *calendar_id)
{
    gsize index;

    if (calendar_id == NULL)
        return NULL;

    for (index = 0; index < calendar_plus_calendar_provider_get_count(); index++)
    {
        const CalendarPlusCalendarProvider *provider =
            calendar_plus_calendar_provider_at(index);
        const InfiltratrTemporalCalendarInfo *info =
            calendar_plus_calendar_provider_info(provider);

        if (info != NULL && infiltratr_string_equal(calendar_id, info->id))
            return provider;
    }

    return NULL;
}
