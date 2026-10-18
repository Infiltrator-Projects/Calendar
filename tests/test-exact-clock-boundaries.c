// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-specific integration tests for the exact timing primitives supplied
 * by Libraries. These tests exercise the boundary semantics through the
 * Calendar public clock functions rather than retesting Common in isolation.
 */

#include "calendar-core.h"
#include "julian-day.h"
#include "time-formats.h"

#include <glib.h>
#include <string.h>

static void
test_decimal_exact_boundaries(void)
{
    g_autofree gchar *before = calendar_format_time(
        CALENDAR_TIME_MODE_DECIMAL, 863999, 0, TRUE, FALSE, 0.0);
    g_autofree gchar *at = calendar_format_time(
        CALENDAR_TIME_MODE_DECIMAL, 864000, 0, TRUE, FALSE, 0.0);

    g_assert_cmpstr(before, ==, "0:00:00");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_DECIMAL,
                         863999, 0, TRUE, 0.0),
                     ==, 1);

    g_assert_cmpstr(at, ==, "0:00:01");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_DECIMAL,
                         864000, 0, TRUE, 0.0),
                     ==, 864);
}

static void
test_hexadecimal_rational_boundary(void)
{
    /*
     * One hexadecimal tick is exactly 86,400,000,000 / 65,536
     * microseconds = 1,318,359.375 microseconds. The display must stay at 0000
     * for the final whole microsecond before that boundary, then advance at the
     * first representable whole-microsecond instant after it. The one-shot
     * delay is rounded upward and therefore can never fire early.
     */
    g_autofree gchar *before = calendar_format_time(
        CALENDAR_TIME_MODE_HEXADECIMAL,
        1318359, 0, FALSE, FALSE, 0.0);
    g_autofree gchar *after = calendar_format_time(
        CALENDAR_TIME_MODE_HEXADECIMAL,
        1318360, 0, FALSE, FALSE, 0.0);

    g_assert_cmpstr(before, ==, "0000");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_HEXADECIMAL,
                         1318359, 0, FALSE, 0.0),
                     ==, 1);

    g_assert_cmpstr(after, ==, "0001");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_HEXADECIMAL,
                         1318360, 0, FALSE, 0.0),
                     ==, 1319);
}

static void
test_signed_unix_boundaries(void)
{
    g_autofree gchar *negative = calendar_format_time(
        CALENDAR_TIME_MODE_UNIX, -1, 0, FALSE, FALSE, 0.0);
    g_autofree gchar *zero = calendar_format_time(
        CALENDAR_TIME_MODE_UNIX, 0, 0, FALSE, FALSE, 0.0);

    g_assert_cmpstr(negative, ==, "-1");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_UNIX,
                         -1, 0, FALSE, 0.0),
                     ==, 1);

    g_assert_cmpstr(zero, ==, "0");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
                         CALENDAR_TIME_MODE_UNIX,
                         0, 0, FALSE, 0.0),
                     ==, 1000);
}

static void
test_extreme_instant_normalisation(void)
{
    /*
     * The clock API accepts the complete gint64 instant and gint UTC-offset
     * domains. Reducing each operand to a civil-day phase before addition keeps
     * these extrema defined under UBSan rather than overflowing signed integers.
     */
    const gint64 instants[] = { G_MININT64, G_MAXINT64 };
    const gint offsets[] = { G_MININT, G_MAXINT };
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(instants); index++)
    {
        g_autofree gchar *decimal = calendar_format_time(
            CALENDAR_TIME_MODE_DECIMAL,
            instants[index], offsets[index], TRUE, FALSE, 0.0);
        g_autofree gchar *internet = calendar_format_time(
            CALENDAR_TIME_MODE_INTERNET,
            instants[index], offsets[index], TRUE, FALSE, 0.0);

        g_assert_nonnull(decimal);
        g_assert_cmpstr(decimal, !=, "");
        g_assert_nonnull(internet);
        g_assert_true(g_str_has_prefix(internet, "@"));
        g_assert_cmpuint(calendar_time_delay_to_next_tick(
                             CALENDAR_TIME_MODE_DECIMAL,
                             instants[index], offsets[index], TRUE, 0.0),
                         >, 0);
        g_assert_cmpuint(calendar_time_delay_to_next_tick(
                             CALENDAR_TIME_MODE_CHINESE,
                             instants[index], offsets[index], FALSE, 0.0),
                         >, 0);
    }
}









static void
test_extreme_navigation_is_bounded(void)
{
    CalendarCalendarEngine *gregorian =
        calendar_calendar_engine_new("gregorian");
    CalendarCalendarEngine *iso =
        calendar_calendar_engine_new("iso-week");
    const CalendarDate maximum = { G_MAXINT, 12, 31 };
    const CalendarDate minimum = { G_MININT, 1, 1 };
    CalendarDate result = { 0 };

    g_assert_nonnull(gregorian);
    g_assert_nonnull(iso);
    g_assert_false(calendar_calendar_engine_add_years(
        gregorian, &maximum, 1, &result));
    g_assert_false(calendar_calendar_engine_add_years(
        gregorian, &minimum, -1, &result));
    g_assert_false(calendar_calendar_engine_add_years(
        iso, &maximum, 1, &result));

    calendar_calendar_engine_free(iso);
    calendar_calendar_engine_free(gregorian);
}



static void
test_native_historical_precision_boundaries(void)
{
    const gint64 before = 3600LL * G_USEC_PER_SEC - 1;
    g_autofree gchar *initial = calendar_format_time(
        CALENDAR_TIME_MODE_CHINESE, before, 0, TRUE, FALSE, 0.0);
    g_autofree gchar *next = calendar_format_time(
        CALENDAR_TIME_MODE_CHINESE, before + 1, 0, TRUE, FALSE, 0.0);
    g_assert_cmpstr(initial, ==, "Zǐ, second half");
    g_assert_cmpstr(next, ==, "Chǒu, first half");
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
        CALENDAR_TIME_MODE_CHINESE, before, 0, TRUE, 0.0), ==, 1);

    /* The half-shi boundary changes fine text but not the double-hour label. */
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
        CALENDAR_TIME_MODE_CHINESE, 23 * 3600LL * G_USEC_PER_SEC,
        0, TRUE, 0.0), ==, 3600000);
    g_assert_cmpuint(calendar_time_delay_to_next_tick(
        CALENDAR_TIME_MODE_CHINESE, 23 * 3600LL * G_USEC_PER_SEC,
        0, FALSE, 0.0), ==, 7200000);
    const gint64 midday = (946728000LL + 1200) * G_USEC_PER_SEC;
    const guint fine = calendar_time_delay_to_next_tick_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, midday, 0, TRUE, 0.0, 0.0);
    const guint coarse = calendar_time_delay_to_next_tick_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, midday, 0, FALSE, 0.0, 0.0);
    g_autofree gchar *fine_text = calendar_format_time_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, midday, 0, TRUE, FALSE, 0.0, 0.0);
    g_assert_nonnull(strstr(fine_text, "twelfth"));
    g_assert_cmpuint(fine, >, 0);
    g_assert_cmpuint(fine, <, coarse);
    /* A solar-phase estimate alone can wake before Common's label changes.
     * The scheduled millisecond must be the first actual displayed change. */
    const gint64 boundary = midday + (gint64)fine * 1000;
    g_autofree gchar *before_tick = calendar_format_time_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, boundary - 1000,
        0, TRUE, FALSE, 0.0, 0.0);
    g_autofree gchar *at_tick = calendar_format_time_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, boundary,
        0, TRUE, FALSE, 0.0, 0.0);
    g_assert_cmpstr(before_tick, ==, fine_text);
    g_assert_cmpstr(at_tick, !=, fine_text);
    const gint64 night = 946684800LL * G_USEC_PER_SEC;
    g_assert_cmpuint(calendar_time_delay_to_next_tick_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, night, 0, TRUE, 0.0, 0.0),
        ==, calendar_time_delay_to_next_tick_at_location(
        CALENDAR_TIME_MODE_ROMAN_TEMPORAL, night, 0, FALSE, 0.0, 0.0));
}

static void
test_date_renderer_runtime_contract(void)
{
    char buffer[512] = "keep";
    size_t length = 99;

    g_assert_true(calendar_format_date_v1("roman", 2026, 7, 29,
        "short", buffer, sizeof(buffer), &length));
    g_assert_cmpstr(buffer, ==, "a.d. XVII Kal. Aug., MMDCCLXXIX A.U.C.");
    g_assert_cmpuint(length, ==, strlen(buffer));
    strcpy(buffer, "keep");
    length = 99;
    g_assert_false(calendar_format_date_v1("roman", 2026, 7, 29,
        "short", buffer, 5, &length));
    g_assert_false(calendar_format_date_v1("roman", 2026, 2, 30,
        "short", buffer, sizeof(buffer), &length));
    g_assert_false(calendar_format_date_v1("unknown", 2026, 7, 29,
        "short", buffer, sizeof(buffer), &length));
    g_assert_false(calendar_format_date_v1("roman", 2026, 7, 29,
        "invalid", buffer, sizeof(buffer), &length));
    g_assert_cmpstr(buffer, ==, "keep");
    g_assert_cmpuint(length, ==, 99);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_assert_true(g_setenv("LANGUAGE", "en_US", TRUE));
    g_test_add_func("/exact-clock/decimal-boundaries",
                    test_decimal_exact_boundaries);
    g_test_add_func("/exact-clock/hex-rational-boundary",
                    test_hexadecimal_rational_boundary);
    g_test_add_func("/exact-clock/unix-signed-boundaries",
                    test_signed_unix_boundaries);
    g_test_add_func("/exact-clock/extreme-instant-normalisation",
                    test_extreme_instant_normalisation);
    g_test_add_func("/date-domain/extreme-navigation-bounded",
                    test_extreme_navigation_is_bounded);
    g_test_add_func("/exact-clock/native-historical-precision",
                    test_native_historical_precision_boundaries);
    g_test_add_func("/date-domain/runtime-formatter-contract",
                    test_date_renderer_runtime_contract);
    return g_test_run();
}
