/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Deterministic property and adversarial tests.
 *
 * These tests complement the small published reference vectors in
 * test-time-formats.c.  They exercise broad ranges, invariants and hostile
 * inputs without introducing network-dependent oracle data.
 */

#include "calendar-core.h"
#include "calendar-system.h"
#include "event-store.h"
#include "julian-day.h"
#include "system-clock.h"
#include "time-formats.h"

#include <math.h>
#include <time.h>

void tzset(void);


static void
test_gregorian_jdn_round_trip(void)
{
    gint year;

    /*
     * Cover four complete Gregorian 400-year cycles and both sides of the
     * common-era boundary.  Stepping by 17 is coprime with common month
     * lengths, so successive samples do not remain on one phase.
     */
    for (year = -400; year <= 1200; year++)
    {
        gint month;

        for (month = 1; month <= 12; month++)
        {
            gint day;

            for (day = 1; day <= 31; day += 17)
            {
                gint returned_year;
                gint returned_month;
                gint returned_day;
                gint64 jdn;

                if (!calendar_plus_gregorian_date_is_valid(year,
                                                            month,
                                                            day))
                {
                    continue;
                }

                jdn = calendar_plus_gregorian_to_jdn(year, month, day);
                calendar_plus_jdn_to_gregorian(jdn,
                                               &returned_year,
                                               &returned_month,
                                               &returned_day);
                g_assert_cmpint(returned_year, ==, year);
                g_assert_cmpint(returned_month, ==, month);
                g_assert_cmpint(returned_day, ==, day);
            }
        }
    }
}

static void
test_all_calendar_grid_properties(void)
{
    gsize calendar_index;

    for (calendar_index = 0;
         calendar_index < calendar_plus_calendar_catalogue_get_count();
         calendar_index++)
    {
        CalendarPlusCalendarDescriptor descriptor = { 0 };
        g_autoptr(CalendarPlusCalendarSystem) calendar = NULL;

        g_assert_true(
            calendar_plus_calendar_catalogue_get(calendar_index, &descriptor));
        calendar = calendar_plus_calendar_system_new(descriptor.id);
        gint year;

        g_assert_nonnull(calendar);
        for (year = 1900; year <= 2100; year += 5)
        {
            gint month;

            for (month = 1; month <= 12; month += 3)
            {
                const gint day = month == 2 ? 28 : 29;
                g_autoptr(GVariant) grid =
                    calendar_plus_calendar_system_build_grid(calendar,
                                                             year,
                                                             month,
                                                             day,
                                                             2026,
                                                             7,
                                                             29,
                                                             year % 7);
                guint selected = 0;
                gsize cell_index;

                g_assert_nonnull(grid);
                g_assert_true(g_variant_is_of_type(
                    grid,
                    G_VARIANT_TYPE("a(siiiiiibbbbbb)")));
                g_assert_cmpuint(g_variant_n_children(grid), ==, 42);

                for (cell_index = 0;
                     cell_index < g_variant_n_children(grid);
                     cell_index++)
                {
                    g_autoptr(GVariant) cell =
                        g_variant_get_child_value(grid, cell_index);
                    gboolean is_selected;
                    gint row;
                    gint column;

                    gint cell_year;
                    gint cell_month;
                    gint cell_day;

                    g_variant_get_child(cell, 1, "i", &cell_year);
                    g_variant_get_child(cell, 2, "i", &cell_month);
                    g_variant_get_child(cell, 3, "i", &cell_day);
                    g_variant_get_child(cell, 4, "i", &row);
                    g_variant_get_child(cell, 5, "i", &column);
                    g_variant_get_child(cell, 9, "b", &is_selected);
                    g_assert_true(calendar_plus_gregorian_date_is_valid(
                        cell_year, cell_month, cell_day));
                    g_assert_cmpint(row, ==,
                                    2 + (gint)(cell_index / 7));
                    g_assert_cmpint(column, ==,
                                    (gint)(cell_index % 7));
                    if (is_selected)
                        selected++;
                }
                g_assert_cmpuint(selected, ==, 1);
            }
        }
    }
}

static void
date_parts(GVariant *parts,
           gint *year,
           gint *month,
           gint *day)
{
    g_assert_nonnull(parts);
    g_assert_true(g_variant_is_of_type(parts, G_VARIANT_TYPE("(iii)")));
    g_variant_get(parts, "(iii)", year, month, day);
    g_assert_true(calendar_plus_gregorian_date_is_valid(*year,
                                                         *month,
                                                         *day));
}

static void
test_all_calendar_typed_navigation(void)
{
    gsize calendar_index;

    for (calendar_index = 0;
         calendar_index < calendar_plus_calendar_catalogue_get_count();
         calendar_index++)
    {
        CalendarPlusCalendarDescriptor descriptor = { 0 };
        g_autoptr(CalendarPlusCalendarSystem) calendar = NULL;

        g_assert_true(
            calendar_plus_calendar_catalogue_get(calendar_index, &descriptor));
        calendar = calendar_plus_calendar_system_new(descriptor.id);
        g_autoptr(GVariant) start =
            calendar_plus_calendar_system_month_start_parts(calendar,
                                                            2026,
                                                            7,
                                                            29);
        gint start_year;
        gint start_month;
        gint start_day;
        gint next_year;
        gint next_month;
        gint next_day;
        gint returned_year;
        gint returned_month;
        gint returned_day;
        g_autoptr(GVariant) next = NULL;
        g_autoptr(GVariant) returned = NULL;

        date_parts(start, &start_year, &start_month, &start_day);
        next = calendar_plus_calendar_system_add_months_parts(calendar,
                                                              start_year,
                                                              start_month,
                                                              start_day,
                                                              1);
        date_parts(next, &next_year, &next_month, &next_day);
        returned = calendar_plus_calendar_system_add_months_parts(calendar,
                                                                  next_year,
                                                                  next_month,
                                                                  next_day,
                                                                  -1);
        date_parts(returned,
                   &returned_year,
                   &returned_month,
                   &returned_day);

        g_assert_true(calendar_plus_date_same(start_year,
                                              start_month,
                                              start_day,
                                              returned_year,
                                              returned_month,
                                              returned_day));

        {
            g_autoptr(GVariant) year_parts =
                calendar_plus_calendar_system_add_years_parts(
                    calendar, 2026, 7, 29, 1);
            g_autofree gchar *year_iso =
                calendar_plus_calendar_system_add_years(
                    calendar, 2026, 7, 29, 1);
            gint typed_year;
            gint typed_month;
            gint typed_day;
            gchar expected[64];

            date_parts(year_parts, &typed_year, &typed_month, &typed_day);
            g_snprintf(expected, sizeof(expected), "%04d-%02d-%02d",
                       typed_year, typed_month, typed_day);
            g_assert_cmpstr(year_iso, ==, expected);
        }
    }
}

static gint64
local_unix(gint year,
           gint month,
           gint day,
           gint hour)
{
    g_autoptr(GDateTime) value =
        g_date_time_new_local(year, month, day, hour, 0, 0.0);

    g_assert_nonnull(value);
    return g_date_time_to_unix(value);
}

static GVariant *
event_variant(const gchar *id,
              gint64 start,
              gint64 end)
{
    return g_variant_ref_sink(
        g_variant_new("(sssbxxx)",
                      id,
                      "#445566",
                      "Long event",
                      FALSE,
                      start,
                      end,
                      (gint64)1));
}

static void
test_unbounded_event_interval(void)
{
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) event =
        event_variant("long",
                      local_unix(2020, 1, 1, 12),
                      local_unix(2030, 1, 1, 12));
    g_autoptr(GVariant) snapshot = NULL;
    g_autoptr(GVariant) rows = NULL;
    gint64 revision;

    g_assert_true(
        calendar_plus_event_store_add_or_update(store, event, 10));
    snapshot = calendar_plus_event_store_get_snapshot(
        store,
        local_unix(2026, 7, 29, 0),
        local_unix(2026, 7, 29, 12));
    g_variant_get(snapshot,
                  "(x@a(sssbbxxxxx))",
                  &revision,
                  &rows);

    g_assert_cmpint(revision, >, 0);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 1);
}

static void
test_malformed_event_variants(void)
{
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) too_short =
        g_variant_ref_sink(g_variant_new("(ss)", "id", "colour"));
    g_autoptr(GVariant) wrong_types =
        g_variant_ref_sink(
            g_variant_new("(sssssss)",
                          "id",
                          "colour",
                          "summary",
                          "not-bool",
                          "not-start",
                          "not-end",
                          "not-modified"));
    g_autoptr(GVariant) empty_id =
        g_variant_ref_sink(
            g_variant_new("(sssbxxx)",
                          "",
                          "#000000",
                          "summary",
                          FALSE,
                          (gint64)0,
                          (gint64)1,
                          (gint64)1));
    g_autofree gchar *oversized_id = g_strnfill(4097, 'i');
    g_autofree gchar *oversized_summary = g_strnfill(65537, 's');
    g_autoptr(GVariant) bad_color =
        g_variant_ref_sink(
            g_variant_new("(sssbxxx)",
                          "bad-color",
                          "rgb(1,2,3)",
                          "summary",
                          FALSE,
                          (gint64)0,
                          (gint64)1,
                          (gint64)1));
    g_autoptr(GVariant) huge_id =
        g_variant_ref_sink(
            g_variant_new("(sssbxxx)",
                          oversized_id,
                          "#000000",
                          "summary",
                          FALSE,
                          (gint64)0,
                          (gint64)1,
                          (gint64)1));
    g_autoptr(GVariant) huge_summary =
        g_variant_ref_sink(
            g_variant_new("(sssbxxx)",
                          "huge-summary",
                          "#000000",
                          oversized_summary,
                          FALSE,
                          (gint64)0,
                          (gint64)1,
                          (gint64)1));

    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           NULL,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           too_short,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           wrong_types,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           empty_id,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           bad_color,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           huge_id,
                                                           1));
    g_assert_false(calendar_plus_event_store_add_or_update(store,
                                                           huge_summary,
                                                           1));
}

static void
test_event_store_clear_contract(void)
{
    const gint64 day = local_unix(2026, 7, 29, 0);
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) event =
        event_variant("clear-me",
                      local_unix(2026, 7, 29, 9),
                      local_unix(2026, 7, 29, 10));
    g_autoptr(GVariant) snapshot = NULL;
    g_autoptr(GVariant) rows = NULL;
    g_auto(GStrv) colors = NULL;
    gint64 revision = 0;

    g_assert_true(calendar_plus_event_store_add_or_update(store, event, 1));
    snapshot = calendar_plus_event_store_get_snapshot(
        store, day, local_unix(2026, 7, 29, 9));
    g_variant_get(snapshot, "(x@a(sssbbxxxxx))", &revision, &rows);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 1);

    calendar_plus_event_store_clear(store);
    g_clear_pointer(&snapshot, g_variant_unref);
    g_clear_pointer(&rows, g_variant_unref);
    snapshot = calendar_plus_event_store_get_snapshot(
        store, day, local_unix(2026, 7, 29, 9));
    g_variant_get(snapshot, "(x@a(sssbbxxxxx))", &revision, &rows);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 0);

    colors = calendar_plus_event_store_get_colors(
        store, day, local_unix(2026, 7, 29, 9));
    g_assert_nonnull(colors);
    g_assert_cmpuint(g_strv_length(colors), ==, 0);

    /* Clearing an already-empty store is deliberately harmless. */
    calendar_plus_event_store_clear(store);
}

static void
test_event_timezone_refresh(void)
{
    const gchar *original_timezone = g_getenv("TZ");
    g_autofree gchar *saved_timezone = g_strdup(original_timezone);
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GDateTime) start = NULL;
    g_autoptr(GVariant) event = NULL;
    gint64 start_unix;

    g_assert_true(g_setenv("TZ", "UTC", TRUE));
    tzset();
    start = g_date_time_new_utc(2026, 7, 29, 0, 30, 0.0);
    g_assert_nonnull(start);
    start_unix = g_date_time_to_unix(start);
    event = event_variant("timezone", start_unix, start_unix + 3600);
    g_assert_true(calendar_plus_event_store_add_or_update(store, event, 1));
    g_assert_false(calendar_plus_event_store_refresh_timezone(store));

    g_assert_true(g_setenv("TZ", "America/New_York", TRUE));
    tzset();
    g_assert_true(calendar_plus_event_store_refresh_timezone(store));
    g_assert_false(calendar_plus_event_store_refresh_timezone(store));

    if (saved_timezone != NULL)
        g_assert_true(g_setenv("TZ", saved_timezone, TRUE));
    else
        g_unsetenv("TZ");
    tzset();
}

static void
test_typed_date_api_and_validation(void)
{
    g_autoptr(CalendarPlusCalendarSystem) calendar =
        calendar_plus_calendar_system_new("gregorian");
    g_autofree gchar *typed =
        calendar_plus_calendar_system_format_date_part(
            calendar,
            2026,
            7,
            29,
            CALENDAR_PLUS_DATE_PART_SHORT);
    g_autofree gchar *legacy =
        calendar_plus_calendar_system_format_date(calendar,
                                                  2026,
                                                  7,
                                                  29,
                                                  "short");
    g_autofree gchar *bad_day =
        calendar_plus_calendar_system_format_date_part(
            calendar,
            2026,
            2,
            30,
            CALENDAR_PLUS_DATE_PART_SHORT);
    g_autofree gchar *bad_part =
        calendar_plus_calendar_system_format_date_part(
            calendar,
            2026,
            7,
            29,
            CALENDAR_PLUS_DATE_PART_INVALID);

    g_assert_cmpstr(typed, ==, legacy);
    g_assert_cmpstr(bad_day, ==, "");
    g_assert_cmpstr(bad_part, ==, "");
    g_assert_null(calendar_plus_calendar_system_build_grid(calendar,
                                                           2026,
                                                           2,
                                                           30,
                                                           2026,
                                                           7,
                                                           29,
                                                           1));
}

static void
test_time_registry_contract(void)
{
    const gsize count = calendar_plus_time_mode_get_count();
    gsize index;

    g_assert_cmpuint(count, ==,
                     (gsize)CALENDAR_PLUS_TIME_MODE_NUREMBERG_HOURS);

    for (index = 0; index < count; index++)
    {
        const CalendarPlusTimeMode mode =
            calendar_plus_time_mode_get_at(index);
        const gchar *id = calendar_plus_time_mode_get_id(mode);
        const gchar *name = calendar_plus_time_mode_get_name(mode);
        gsize other;

        g_assert_cmpint(mode, !=, CALENDAR_PLUS_TIME_MODE_INVALID);
        g_assert_nonnull(id);
        g_assert_cmpuint(strlen(id), >, 0U);
        g_assert_nonnull(name);
        g_assert_cmpuint(strlen(name), >, 0U);
        g_assert_cmpint(calendar_plus_time_mode_from_string(id), ==, mode);

        for (other = index + 1; other < count; other++)
        {
            const CalendarPlusTimeMode other_mode =
                calendar_plus_time_mode_get_at(other);
            g_assert_cmpstr(id, !=,
                calendar_plus_time_mode_get_id(other_mode));
        }
    }

    g_assert_cmpint(calendar_plus_time_mode_get_at(count), ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(calendar_plus_time_mode_from_string(NULL), ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(calendar_plus_time_mode_from_string("missing"), ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);

    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_SIDEREAL));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_SOLAR));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_MEAN_SOLAR));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_ROMAN_TEMPORAL));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_ITALIAN_HOURS));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_BABYLONIAN_HOURS));
    g_assert_true(calendar_plus_time_mode_requires_longitude(
        CALENDAR_PLUS_TIME_MODE_NUREMBERG_HOURS));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_ROMAN_TEMPORAL));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_ITALIAN_HOURS));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_BABYLONIAN_HOURS));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_INDIAN_GHATI));
    g_assert_true(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_NUREMBERG_HOURS));
    g_assert_false(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_CHINESE_KE));
    g_assert_false(calendar_plus_time_mode_requires_latitude(
        CALENDAR_PLUS_TIME_MODE_SOLAR));
    g_assert_cmpint(
        calendar_plus_time_mode_from_string("roman-temporal@-36.3800"),
        ==,
        CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_false(calendar_plus_time_mode_supports_seconds(
        CALENDAR_PLUS_TIME_MODE_UNIX));
    g_assert_false(calendar_plus_time_mode_supports_seconds(
        CALENDAR_PLUS_TIME_MODE_CHINESE));
    g_assert_false(calendar_plus_time_mode_supports_seconds(
        CALENDAR_PLUS_TIME_MODE_ROMAN_TEMPORAL));
    g_assert_false(calendar_plus_time_mode_supports_seconds(
        CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL));
    g_assert_null(calendar_plus_time_mode_get_id(
        CALENDAR_PLUS_TIME_MODE_INVALID));
    g_assert_null(calendar_plus_time_mode_get_name(
        CALENDAR_PLUS_TIME_MODE_INVALID));
}

static void
test_all_time_modes_runtime_contract(void)
{
    const gint64 normal_instant =
        G_GINT64_CONSTANT(1788177600) * G_USEC_PER_SEC;
    const gint64 extremes[] = { G_MININT64, G_MAXINT64 };
    const gsize count = calendar_plus_time_mode_get_count();
    gsize index;

    for (index = 0; index < count; index++)
    {
        const CalendarPlusTimeMode mode =
            calendar_plus_time_mode_get_at(index);
        const gchar *id = calendar_plus_time_mode_get_id(mode);
        guint show_seconds;
        guint vertical;
        gsize extreme_index;

        for (show_seconds = 0; show_seconds <= 1; show_seconds++)
        {
            for (vertical = 0; vertical <= 1; vertical++)
            {
                g_autofree gchar *formatted =
                    calendar_plus_format_time_at_location(
                        mode,
                        normal_instant,
                        10 * 60 * 60,
                        show_seconds != 0,
                        vertical != 0,
                        -36.3833,
                        145.4000);
                const guint delay =
                    calendar_plus_time_delay_to_next_tick_at_location(
                        mode,
                        normal_instant,
                        10 * 60 * 60,
                        show_seconds != 0,
                        -36.3833,
                        145.4000);

                g_assert_nonnull(formatted);
                g_assert_cmpuint(strlen(formatted), >, 0U);
                g_assert_cmpuint(delay, >, 0U);
            }
        }

        if (calendar_plus_time_mode_requires_latitude(mode))
        {
            g_autofree gchar *missing_latitude =
                calendar_plus_format_time_at_location(
                    mode, normal_instant, 10 * 60 * 60,
                    FALSE, FALSE, NAN, 145.4000);
            g_assert_cmpstr(missing_latitude, ==, "");
            g_assert_cmpuint(
                calendar_plus_time_delay_to_next_tick_at_location(
                    mode, normal_instant, 10 * 60 * 60,
                    FALSE, NAN, 145.4000),
                ==, 3600000U);
        }

        if (calendar_plus_time_mode_requires_longitude(mode))
        {
            g_autofree gchar *missing_longitude =
                calendar_plus_format_time_at_location(
                    mode, normal_instant, 10 * 60 * 60,
                    FALSE, FALSE, -36.3833, NAN);
            g_assert_cmpstr(missing_longitude, ==, "");
            g_assert_cmpuint(
                calendar_plus_time_delay_to_next_tick_at_location(
                    mode, normal_instant, 10 * 60 * 60,
                    FALSE, -36.3833, NAN),
                ==, 3600000U);
        }

        for (extreme_index = 0;
             extreme_index < G_N_ELEMENTS(extremes);
             extreme_index++)
        {
            g_autofree gchar *extreme =
                calendar_plus_format_time_at_location(
                    mode,
                    extremes[extreme_index],
                    G_MAXINT,
                    TRUE,
                    FALSE,
                    -36.3833,
                    145.4000);
            const guint delay =
                calendar_plus_time_delay_to_next_tick_at_location(
                    mode,
                    extremes[extreme_index],
                    G_MAXINT,
                    TRUE,
                    -36.3833,
                    145.4000);

            g_assert_nonnull(extreme);
            g_assert_cmpuint(delay, >, 0U);
        }

        g_test_message("validated complete runtime contract for %s", id);
    }

    {
        g_autofree gchar *invalid =
            calendar_plus_format_time_at_location(
                CALENDAR_PLUS_TIME_MODE_INVALID,
                normal_instant, 0, FALSE, FALSE, 0.0, 0.0);
        g_assert_cmpstr(invalid, ==, "");
        g_assert_cmpuint(
            calendar_plus_time_delay_to_next_tick_at_location(
                CALENDAR_PLUS_TIME_MODE_INVALID,
                normal_instant, 0, FALSE, 0.0, 0.0),
            ==, 1000U);
    }
}

static void
test_system_clock_policy_accessors(void)
{
    g_autoptr(CalendarPlusSystemClock) clock =
        calendar_plus_system_clock_new();
    g_autoptr(GVariant) policy =
        calendar_plus_system_clock_get_system_policy(clock);
    const gchar *snapshot_mode = NULL;
    const gchar *snapshot_calendar = NULL;
    const gchar *authority = NULL;
    gboolean snapshot_seconds = FALSE;
    gboolean snapshot_location = FALSE;
    gboolean provider_available = FALSE;
    gdouble snapshot_latitude = 0.0;
    gdouble snapshot_longitude = 0.0;
    g_autofree gchar *mode = NULL;
    g_autofree gchar *calendar = NULL;

    g_assert_nonnull(policy);
    g_assert_true(g_variant_is_of_type(
        policy, G_VARIANT_TYPE("(ssbbddsb)")));
    g_variant_get(policy, "(&s&sbbdd&sb)",
                  &snapshot_mode,
                  &snapshot_calendar,
                  &snapshot_seconds,
                  &snapshot_location,
                  &snapshot_latitude,
                  &snapshot_longitude,
                  &authority,
                  &provider_available);

    mode = calendar_plus_system_clock_get_system_mode(clock);
    calendar = calendar_plus_system_clock_get_system_calendar(clock);

    g_assert_cmpstr(mode, ==, snapshot_mode);
    g_assert_cmpstr(calendar, ==, snapshot_calendar);
    g_assert_cmpint(calendar_plus_system_clock_get_system_show_seconds(clock),
                    ==, snapshot_seconds);
    g_assert_cmpint(
        calendar_plus_system_clock_get_system_location_configured(clock),
        ==, snapshot_location);
    g_assert_cmpfloat(
        calendar_plus_system_clock_get_system_latitude(clock),
        ==, snapshot_latitude);
    g_assert_cmpfloat(
        calendar_plus_system_clock_get_system_longitude(clock),
        ==, snapshot_longitude);
    g_assert_true(isfinite(snapshot_latitude));
    g_assert_true(isfinite(snapshot_longitude));
    g_assert_true(
        g_str_equal(authority, "mint-cinnamon") ||
        g_str_equal(authority, "infiltrator-system-settings"));
    if (g_str_equal(authority, "infiltrator-system-settings"))
        g_assert_true(provider_available);
}

int
main(int argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/properties/gregorian-jdn-round-trip",
                    test_gregorian_jdn_round_trip);
    g_test_add_func("/properties/all-calendar-grids",
                    test_all_calendar_grid_properties);
    g_test_add_func("/properties/all-calendar-navigation",
                    test_all_calendar_typed_navigation);
    g_test_add_func("/properties/unbounded-event-interval",
                    test_unbounded_event_interval);
    g_test_add_func("/properties/event-store-clear",
                    test_event_store_clear_contract);
    g_test_add_func("/properties/malformed-event-variants",
                    test_malformed_event_variants);
    g_test_add_func("/properties/event-timezone-refresh",
                    test_event_timezone_refresh);
    g_test_add_func("/properties/typed-date-api",
                    test_typed_date_api_and_validation);
    g_test_add_func("/properties/time-registry",
                    test_time_registry_contract);
    g_test_add_func("/properties/all-time-modes-runtime",
                    test_all_time_modes_runtime_contract);
    g_test_add_func("/properties/system-policy-accessors",
                    test_system_clock_policy_accessors);
    return g_test_run();
}
