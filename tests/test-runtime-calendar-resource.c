// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#include "calendar-registry.h"
#include "julian-day.h"

#include <infiltratr/temporal.h>

#include <glib.h>

static void
assert_runtime_metadata(const gchar *calendar_id,
                        const gchar *expected_name)
{
    const CalendarCalendarProvider *provider =
        calendar_calendar_provider_from_id(calendar_id);
    const InfiltratrTemporalCalendarInfo *info;

    g_assert_nonnull(provider);
    g_assert_cmpstr(provider->calendar_id, ==, calendar_id);
    g_assert_cmpuint(provider->abi_version, ==,
                     CALENDAR_CALENDAR_PROVIDER_ABI);
    info = calendar_calendar_provider_info(provider);
    g_assert_nonnull(info);
    g_assert_cmpstr(info->id, ==, calendar_id);
    g_assert_cmpstr(info->name, ==, expected_name);
}

static void
test_runtime_calendar_resource(void)
{
    const gsize builtin_count = infiltratr_temporal_calendar_count();
    const CalendarCalendarProvider *provider;
    const InfiltratrTemporalCalendarInfo *info;
    CalendarCalendarFields fields;
    gchar *formatted;
    gint64 jdn;

    /*
     * Migrated chronologies must be absent from Common's compiled base
     * catalogue. Their installed executable resources are the sole authority.
     */
    g_assert_null(infiltratr_temporal_calendar_find("hebrew"));
    g_assert_nonnull(infiltratr_temporal_calendar_find("gregorian"));
    g_assert_null(infiltratr_temporal_calendar_find("persian"));
    g_assert_null(infiltratr_temporal_calendar_find("coptic"));
    g_assert_cmpuint(builtin_count, ==, 1U);
    g_assert_cmpuint(calendar_calendar_provider_get_count(), ==,
                     31U);

    provider = calendar_calendar_provider_from_id("fixture-calendar");
    g_assert_nonnull(provider);
    g_assert_cmpstr(provider->calendar_id, ==, "fixture-calendar");
    g_assert_cmpuint(provider->abi_version, ==,
                     CALENDAR_CALENDAR_PROVIDER_ABI);

    info = calendar_calendar_provider_info(provider);
    g_assert_nonnull(info);
    g_assert_cmpstr(info->id, ==, "fixture-calendar");
    g_assert_cmpstr(info->name, ==, "Fixture Calendar");

    g_assert_true(provider->fields_from_jdn(provider, 12345, &fields));
    g_assert_cmpint(fields.month, ==, 1);
    g_assert_cmpint(fields.day, ==, 1);

    formatted = provider->format(
        provider, 12345, CALENDAR_DATE_PART_FULL);
    g_assert_cmpstr(formatted, ==, "Fixture Calendar Date");
    g_free(formatted);

    g_assert_cmpint(provider->period_start(provider, 1000), ==, 1000);
    g_assert_cmpint(provider->add_periods(provider, 1000, 2), ==, 1002);
    g_assert_cmpint(provider->add_years(provider, 1000, 2), ==, 1730);

    /* Hebrew remains the reference production runtime chronology. */
    assert_runtime_metadata(
        "hebrew", "Hebrew Calendar");
    provider = calendar_calendar_provider_from_id("hebrew");
    jdn = calendar_gregorian_to_jdn(2026, 9, 12);
    g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 5787);
    g_assert_cmpint(fields.month, ==, 1);
    g_assert_cmpint(fields.day, ==, 1);
    g_assert_cmpint(provider->period_start(provider, jdn), ==, jdn);

    /* Gregorian now has exactly the same resource-only ownership model. */
    assert_runtime_metadata("gregorian", "Gregorian Calendar");
    provider = calendar_calendar_provider_from_id("gregorian");
    g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 2026);
    g_assert_cmpint(fields.month, ==, 9);
    g_assert_cmpint(fields.day, ==, 12);
    g_assert_cmpint(fields.auxiliary, ==, 1);
    g_assert_cmpint(provider->period_start(provider, jdn), ==,
                    calendar_gregorian_to_jdn(2026, 9, 1));

    /*
     * Persian is resource-only: its verified Nowruz epoch is independent of
     * the removed Calendar-core arithmetic copy. The old direct arithmetic
     * entry point must reject it instead of manufacturing a fallback.
     */
    assert_runtime_metadata(
        "persian", "Persian Solar Hijri Calendar");
    provider = calendar_calendar_provider_from_id("persian");
    g_assert_true(provider->fields_from_jdn(
        provider, calendar_gregorian_to_jdn(2026, 3, 21), &fields));
    g_assert_cmpint(fields.year, ==, 1405);
    g_assert_cmpint(fields.month, ==, 1);
    g_assert_cmpint(fields.day, ==, 1);
    g_assert_cmpint(fields.auxiliary, ==, 0);
    g_assert_false(fields.special);

    /* A fixed Coptic reference vector through the installed provider ABI. */
    assert_runtime_metadata(
        "coptic", "Coptic Calendar");
    provider = calendar_calendar_provider_from_id("coptic");
    g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 1743);
    g_assert_cmpint(fields.month, ==, 1);
    g_assert_cmpint(fields.day, ==, 2);
    g_assert_cmpint(fields.auxiliary, ==, 1);
    g_assert_false(fields.special);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(
        "/calendar/runtime-resource/dual-source",
        test_runtime_calendar_resource);
    return g_test_run();
}
