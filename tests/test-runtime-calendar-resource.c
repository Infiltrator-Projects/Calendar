// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#include "calendar-registry.h"

#include <infiltratr/temporal.h>

#include <glib.h>

static void test_runtime_calendar_resource(void)
{
    const gsize builtin_count = infiltratr_temporal_calendar_count();
    const CalendarCalendarProvider *provider;
    const InfiltratrTemporalCalendarInfo *info;
    CalendarCalendarFields fields;
    gchar *formatted;

    g_assert_cmpuint(calendar_calendar_provider_get_count(), ==,
                     builtin_count + 1U);

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

    /* Existing compiled capability remains authoritative and unchanged. */
    g_assert_nonnull(calendar_calendar_provider_from_id("gregorian"));
    g_assert_cmpstr(
        calendar_calendar_provider_info(
            calendar_calendar_provider_from_id("gregorian"))->id,
        ==,
        "gregorian");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(
        "/calendar/runtime-resource/dual-source",
        test_runtime_calendar_resource);
    return g_test_run();
}
