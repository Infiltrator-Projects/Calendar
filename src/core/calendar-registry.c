/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Built-in calendar-provider registry.
 *
 * Calendar identifiers are persisted in Cinnamon settings, so IDs are stable
 * data rather than implementation details. Each provider exposes the same
 * backend-neutral operation table. CalendarSystem resolves that interface once
 * and never branches on ICU versus native implementation afterwards.
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

#define PROVIDER_INTERFACE(mode_, id_, fields_, format_, start_, add_, years_) \
    { \
        CALENDAR_PLUS_CALENDAR_PROVIDER_ABI, \
        id_, \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, \
        fields_, format_, start_, add_, years_ \
    }

#define ICU_PROVIDER(mode_, id_, keyword_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           icu_fields, icu_format, icu_period_start, \
                           icu_add_periods, icu_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, keyword_ \
    }

#define ARITHMETIC_PROVIDER(mode_, id_, keyword_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           arithmetic_fields, icu_format, \
                           arithmetic_period_start, arithmetic_add_periods, \
                           arithmetic_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, keyword_ \
    }

#define CUSTOM_PROVIDER(mode_, id_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           custom_fields, custom_format, custom_period_start, \
                           custom_add_periods, custom_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

#define SWEDISH_PROVIDER(mode_, id_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(mode_, id_, \
                           swedish_fields, swedish_format, swedish_period_start, \
                           swedish_add_periods, swedish_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

/*
 * Local implementation slots are keyed explicitly by stable calendar ID.
 * Common owns descriptive catalogue metadata, but catalogue completeness and
 * array ordering are deliberately not runtime prerequisites for chronology.
 * An independently valid Calendar provider must continue to work even if a
 * mismatched Common build lacks metadata for some other provider.
 */
static const BuiltinCalendarProvider providers[] = {
    ARITHMETIC_PROVIDER(GREGORIAN, "gregorian", "gregorian"),
    CUSTOM_PROVIDER(ISO_WEEK, "iso-week"),
    CUSTOM_PROVIDER(JULIAN, "julian"),
    CUSTOM_PROVIDER(REVISED_JULIAN, "revised-julian"),
    ARITHMETIC_PROVIDER(HEBREW, "hebrew", "hebrew"),
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

G_STATIC_ASSERT(G_N_ELEMENTS(providers) == CALENDAR_PLUS_CALENDAR_MODE_COUNT);

static const CalendarPlusCalendarProvider *
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

gsize
calendar_plus_calendar_provider_get_count(void)
{
    return G_N_ELEMENTS(providers);
}

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_at(gsize index)
{
    if (index >= G_N_ELEMENTS(providers))
        return NULL;

    return &providers[index].interface;
}

const InfiltratrTemporalCalendarInfo *
calendar_plus_calendar_provider_info(
    const CalendarPlusCalendarProvider *provider)
{
    if (provider == NULL ||
        provider->abi_version != CALENDAR_PLUS_CALENDAR_PROVIDER_ABI ||
        provider->calendar_id == NULL)
    {
        return NULL;
    }

    return infiltratr_temporal_calendar_find(provider->calendar_id);
}

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_from_id(const gchar *calendar_id)
{
    return provider_from_id_unchecked(calendar_id);
}
