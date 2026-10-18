// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Run against a deliberately empty native clock directory. All extracted
 * clock modes must fail closed even when Common still knows their metadata.
 * This executable links the real Calendar core and Common: the test cannot
 * pass by substituting mock formatters for missing system .so resources.
 */
#include "time-formats.h"

#include <glib.h>

static void test_extracted_clocks_require_installed_modules(void)
{
    static const struct {
        CalendarTimeMode mode;
        const char *id;
    } extracted[] = {
        { CALENDAR_TIME_MODE_ITALIAN_HOURS, "italian-hours" },
        { CALENDAR_TIME_MODE_BABYLONIAN_HOURS, "babylonian-hours" },
        { CALENDAR_TIME_MODE_SOLAR, "solar" },
        { CALENDAR_TIME_MODE_CHINESE, "chinese-time" },
        { CALENDAR_TIME_MODE_SIDEREAL, "sidereal" },
        { CALENDAR_TIME_MODE_DECIMAL, "decimal" },
        { CALENDAR_TIME_MODE_INTERNET, "internet" },
        { CALENDAR_TIME_MODE_UNIX, "unix" },
        { CALENDAR_TIME_MODE_HEXADECIMAL, "hexadecimal" },
        { CALENDAR_TIME_MODE_BINARY, "binary" },
        { CALENDAR_TIME_MODE_JULIAN, "julian" },
        { CALENDAR_TIME_MODE_MODIFIED_JULIAN, "modified-julian" },
        { CALENDAR_TIME_MODE_CHINESE_KE, "chinese-ke" },
        { CALENDAR_TIME_MODE_MEAN_SOLAR, "mean-solar" },
        { CALENDAR_TIME_MODE_STANDARD_12, "standard-12" },
        { CALENDAR_TIME_MODE_STANDARD_24, "standard-24" }
    };

    for (gsize i = 0; i < G_N_ELEMENTS(extracted); ++i)
    {
        g_autofree gchar *text = calendar_format_time_at_location(
            extracted[i].mode, G_GINT64_CONSTANT(1792256400000000),
            39600, TRUE, FALSE, -36.0, 145.0);

        g_assert_nonnull(text);
        g_assert_cmpstr(text, ==, "");
        g_assert_cmpint(calendar_time_mode_from_string(extracted[i].id),
                        ==, CALENDAR_TIME_MODE_INVALID);
        /* Retry is permitted, but not a synthetic clock calculation. */
        g_assert_cmpuint(calendar_time_delay_to_next_tick_at_location(
            extracted[i].mode, 0, 0, TRUE, -36.0, 145.0), ==, 1000U);
    }

    /* A missing migrated resource must not disable an unmigrated clock. */
    g_assert_cmpint(calendar_time_mode_from_string("roman-temporal"),
                    ==, CALENDAR_TIME_MODE_ROMAN_TEMPORAL);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/clock-resource/absent-migrated-modules",
                    test_extracted_clocks_require_installed_modules);
    return g_test_run();
}
