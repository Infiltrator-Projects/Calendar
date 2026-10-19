/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Adapter-free contract tests.  This binary links CORE_SOURCES only: passing
 * proves that calendar, event and live-clock behaviour does not require
 * GObject, GVariant, GJS or a GLib main loop.
 */

#include "calendar-core.h"
#include "calendar-registry.h"
#include "clock-engine.h"
#include "event-core.h"
#include "event-source.h"
#include "julian-day.h"

#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>
#include <math.h>


#define CALENDAR_FIELD_ANY G_MININT

typedef struct
{
    const gchar *id;
    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint64 year;
    gint month;
    gint day;
} CalendarReferenceVector;

static void
assert_calendar_field(gint64 actual,
                      gint64 expected,
                      const gchar *id,
                      const gchar *field)
{
    if (expected == CALENDAR_FIELD_ANY)
        return;

    if (actual != expected)
    {
        g_error("calendar reference mismatch for %s %s: "
                "got %" G_GINT64_FORMAT ", expected %" G_GINT64_FORMAT,
                id,
                field,
                actual,
                expected);
    }
}

static void
test_calendar_reference_vectors(void)
{
    /*
     * Stable civil/epoch anchors exercise every registered provider without
     * relying on locale-sensitive month names.  Most vectors are epoch/new-
     * year identities; Chinese year numbering is intentionally left open
     * because ICU exposes the sexagenary-cycle counter separately from the
     * native year name used for display.
     */
    static const CalendarReferenceVector vectors[] = {
        { "gregorian", 2000, 1, 1, 2000, 1, 1 },
        { "julian", 2000, 1, 14, 2000, 1, 1 },
        { "iso-week", 2021, 1, 4, 2021, 1, 1 },
        { "hebrew", 2026, 9, 12, 5787, 1, 1 },
        { "islamic", 622, 7, 18, 1, 1, 1 },
        { "islamic-civil", 622, 7, 19, 1, 1, 1 },
        { "islamic-umalqura", 1882, 11, 12, 1300, 1, 1 },
        { "persian", 2026, 3, 21, 1405, 1, 1 },
        { "chinese", 2026, 2, 17, CALENDAR_FIELD_ANY, 1, 1 },
        { "indian", 2026, 3, 22, 1948, 1, 1 },
        { "coptic", 2026, 9, 11, 1743, 1, 1 },
        { "ethiopian", 2026, 9, 11, 2019, 1, 1 },
        { "buddhist", 2026, 1, 1, 2569, 1, 1 },
        { "japanese", 2019, 5, 1, 1, 5, 1 },
        { "minguo", 1912, 1, 1, 1, 1, 1 },
        { "french-republican", 1792, 9, 22, 1, 1, 1 },
        { "roman", 2000, 1, 14, 2000, 1, 1 },
        { "mayan", 2012, 12, 21, 5200, 0, 0 },
        { "bahai", 1844, 3, 21, 1, 1, 1 },
        { "international-fixed", 2026, 1, 1, 2026, 1, 1 },
        { "world", 2026, 1, 1, 2026, 1, 1 },
        { "positivist", 2026, 1, 1, 238, 1, 1 },
        { "revised-julian", 2000, 1, 1, 2000, 1, 1 },
        { "byzantine", 2026, 9, 14, 7535, 9, 1 },
        { "egyptian-nabonassar", -746, 2, 18, 1, 1, 1 },
        { "dangi", 2026, 2, 17, CALENDAR_FIELD_ANY, 1, 1 },
        { "ethiopic-amete-alem", 2026, 9, 11, 7519, 1, 1 },
        { "islamic-tbla", 622, 7, 18, 1, 1, 1 },
        { "armenian-traditional", 552, 7, 13, 1, 1, 1 },
        { "swedish-historical", 1700, 3, 11, 1700, 3, 1 }
    };
    gsize index;

    /*
     * Registry order is not a contract: installed native resources are
     * discovered separately from compiled providers, and Common's metadata
     * catalogue is keyed by stable ID rather than positional index.
     */
    g_assert_cmpuint(G_N_ELEMENTS(vectors), ==,
                     calendar_calendar_provider_get_count());

    for (index = 0; index < calendar_calendar_provider_get_count(); index++)
    {
        const CalendarCalendarProvider *provider =
            calendar_calendar_provider_at(index);
        const InfiltratrTemporalCalendarInfo *info;

        g_assert_nonnull(provider);
        info = calendar_calendar_provider_info(provider);
        g_assert_nonnull(info);
        g_assert_cmpstr(info->id, ==, provider->calendar_id);
        g_assert_true(calendar_calendar_provider_from_id(info->id) == provider);
    }

    for (index = 0; index < G_N_ELEMENTS(vectors); index++)
    {
        const CalendarCalendarProvider *provider =
            calendar_calendar_provider_from_id(vectors[index].id);
        const InfiltratrTemporalCalendarInfo *info =
            calendar_calendar_provider_info(provider);
        const gboolean resource =
            g_str_equal(vectors[index].id, "hebrew") ||
            g_str_equal(vectors[index].id, "gregorian") ||
            g_str_equal(vectors[index].id, "persian") ||
            g_str_equal(vectors[index].id, "coptic");

        g_assert_nonnull(provider);
        g_assert_nonnull(info);
        g_assert_cmpstr(info->id, ==, vectors[index].id);
        if (resource)
            g_assert_cmpuint(provider->common_index, ==, G_MAXSIZE);
    }

    {
        const CalendarCalendarProvider *hebrew =
            calendar_calendar_provider_from_id("hebrew");
        const InfiltratrTemporalCalendarInfo *info =
            calendar_calendar_provider_info(hebrew);
        g_assert_nonnull(info);
        g_assert_cmpstr(info->name, ==, "Hebrew Calendar");
    }

    for (index = 0; index < G_N_ELEMENTS(vectors); index++)
    {
        const CalendarReferenceVector *vector = &vectors[index];
        const CalendarCalendarProvider *provider =
            calendar_calendar_provider_from_id(vector->id);
        const gint64 jdn = calendar_gregorian_to_jdn(
            vector->gregorian_year,
            vector->gregorian_month,
            vector->gregorian_day);
        CalendarCalendarFields fields = { 0 };

        g_assert_nonnull(provider);
        g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
        assert_calendar_field(fields.year, vector->year, vector->id, "year");
        assert_calendar_field(fields.month, vector->month, vector->id, "month");
        assert_calendar_field(fields.day, vector->day, vector->id, "day");
        g_assert_false(fields.special);
    }
}


static void
test_ui_language_calendar_presentation(void)
{
    g_autofree gchar *saved_language = g_strdup(g_getenv("LANGUAGE"));
    CalendarCalendarEngine *engine = calendar_calendar_engine_new("chinese");
    const CalendarDate date = { 2026, 10, 10 };
    char bridge_text[512];

    g_assert_nonnull(engine);
    g_assert_true(g_setenv("LANGUAGE", "en_AU", TRUE));
    g_autofree gchar *english = calendar_calendar_engine_format_date(
        engine, &date, CALENDAR_DATE_PART_SHORT);
    g_assert_cmpstr(english, ==, "1 Ninth Month bing-wu");
    g_assert_true(calendar_format_date_v1("chinese", 2026, 10, 10,
        "short", bridge_text, sizeof(bridge_text), NULL));
    g_assert_cmpstr(bridge_text, ==, english);

    g_assert_true(g_setenv("LANGUAGE", "zh_TW", TRUE));
    g_autofree gchar *chinese = calendar_calendar_engine_format_date(
        engine, &date, CALENDAR_DATE_PART_SHORT);
    g_assert_nonnull(strstr(chinese, "九月"));
    g_assert_nonnull(strstr(chinese, "丙午"));
    g_assert_null(strstr(chinese, "2026"));
    g_assert_null(strstr(chinese, "Ninth"));
    g_assert_true(calendar_format_date_v1("chinese", 2026, 10, 10,
        "short", bridge_text, sizeof(bridge_text), NULL));
    g_assert_cmpstr(bridge_text, ==, chinese);

    if (saved_language != NULL)
        g_assert_true(g_setenv("LANGUAGE", saved_language, TRUE));
    else
        g_unsetenv("LANGUAGE");
    calendar_calendar_engine_free(engine);
}

static void
test_native_cyclic_year_presentation(void)
{
    g_autofree gchar *saved_language = g_strdup(g_getenv("LANGUAGE"));
    const gchar *calendars[] = { "chinese", "dangi" };
    const CalendarDate before_new_year = { 2026, 1, 1 };
    const CalendarDate new_year = { 2026, 2, 17 };
    const CalendarDate date = { 2026, 10, 10 };
    const CalendarDatePart parts[] = {
        CALENDAR_DATE_PART_SHORT,
        CALENDAR_DATE_PART_FULL,
        CALENDAR_DATE_PART_YEAR
    };
    char bridge[512];

    for (gsize i = 0; i < G_N_ELEMENTS(calendars); i++)
    {
        CalendarCalendarEngine *engine =
            calendar_calendar_engine_new(calendars[i]);
        g_assert_nonnull(engine);
        g_assert_true(g_setenv("LANGUAGE", "en_AU", TRUE));
        for (gsize p = 0; p < G_N_ELEMENTS(parts); p++)
        {
            g_autofree gchar *text = calendar_calendar_engine_format_date(
                engine, &date, parts[p]);
            g_assert_nonnull(strstr(text, "bing-wu"));
            g_assert_null(strstr(text, "2026"));
            g_assert_null(strstr(text, "43"));
        }
        g_autofree gchar *before = calendar_calendar_engine_format_date(
            engine, &before_new_year, CALENDAR_DATE_PART_YEAR);
        g_autofree gchar *after = calendar_calendar_engine_format_date(
            engine, &new_year, CALENDAR_DATE_PART_YEAR);
        g_assert_cmpstr(before, ==, "yi-si");
        g_assert_cmpstr(after, ==, "bing-wu");
        g_assert_true(calendar_format_date_v1(calendars[i], 2026, 10, 10,
            "full", bridge, sizeof(bridge), NULL));
        g_assert_nonnull(strstr(bridge, "bing-wu"));
        g_assert_null(strstr(bridge, "2026"));

        g_assert_true(g_setenv("LANGUAGE", "zh_TW", TRUE));
        g_autofree gchar *chinese = calendar_calendar_engine_format_date(
            engine, &date, CALENDAR_DATE_PART_YEAR);
        g_assert_cmpstr(chinese, ==, "丙午");
        g_assert_true(g_setenv("LANGUAGE", "ko_KR", TRUE));
        g_autofree gchar *korean = calendar_calendar_engine_format_date(
            engine, &date, CALENDAR_DATE_PART_YEAR);
        g_assert_cmpstr(korean, ==, "병오");
        calendar_calendar_engine_free(engine);
    }
    if (saved_language != NULL)
        g_assert_true(g_setenv("LANGUAGE", saved_language, TRUE));
    else
        g_unsetenv("LANGUAGE");
}

static void
test_historical_calendar_edge_vectors(void)
{
    const CalendarCalendarProvider *roman =
        calendar_calendar_provider_from_id("roman");
    const CalendarCalendarProvider *french =
        calendar_calendar_provider_from_id("french-republican");
    const CalendarCalendarProvider *byzantine =
        calendar_calendar_provider_from_id("byzantine");
    CalendarCalendarFields fields = { 0 };
    g_autofree gchar *text = NULL;
    gint64 jdn;

    g_assert_nonnull(roman);
    g_assert_nonnull(french);
    g_assert_nonnull(byzantine);

    jdn = G_GINT64_CONSTANT(2460378) /* Julian 2024-02-24 */;
    g_assert_true(roman->fields_from_jdn(roman, jdn, &fields));
    text = roman->format(roman, jdn, CALENDAR_DATE_PART_FULL);
    g_assert_nonnull(text);
    g_assert_nonnull(strstr(text, "bis VI Kal. Mar"));
    g_clear_pointer(&text, g_free);

    jdn = calendar_gregorian_to_jdn(1795, 9, 22);
    g_assert_true(french->fields_from_jdn(french, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 3);
    g_assert_cmpint(fields.month, ==, 13);
    g_assert_cmpint(fields.day, ==, 6);
    jdn = calendar_gregorian_to_jdn(1799, 9, 22);
    g_assert_true(french->fields_from_jdn(french, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 7);
    g_assert_cmpint(fields.month, ==, 13);
    g_assert_cmpint(fields.day, ==, 6);
    jdn = calendar_gregorian_to_jdn(1803, 9, 23);
    g_assert_true(french->fields_from_jdn(french, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 11);
    g_assert_cmpint(fields.month, ==, 13);
    g_assert_cmpint(fields.day, ==, 6);

    jdn = G_GINT64_CONSTANT(2461297) /* Julian 2026-08-31 */;
    g_assert_true(byzantine->fields_from_jdn(byzantine, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 7534);
    jdn = G_GINT64_CONSTANT(2461298) /* Julian 2026-09-01 */;
    g_assert_true(byzantine->fields_from_jdn(byzantine, jdn, &fields));
    g_assert_cmpint(fields.year, ==, 7535);
}

static void
test_gregorian_proleptic_cutover(void)
{
    static const CalendarReferenceVector vectors[] = {
        { "gregorian", 1500, 3, 1, 1500, 3, 1 },
        { "gregorian", 1582, 10, 4, 1582, 10, 4 },
        { "gregorian", 1582, 10, 10, 1582, 10, 10 },
        { "gregorian", 1582, 10, 15, 1582, 10, 15 }
    };
    const CalendarCalendarProvider *provider =
        calendar_calendar_provider_from_id("gregorian");
    CalendarCalendarEngine *engine =
        calendar_calendar_engine_new("gregorian");
    const CalendarDate selected = { 1582, 10, 10 };
    const CalendarDate today = { 2026, 9, 19 };
    CalendarDate navigated = { 0 };
    CalendarCalendarGrid grid = { 0 };
    gsize index;
    gboolean found_selected = FALSE;

    g_assert_nonnull(provider);
    g_assert_nonnull(engine);

    for (index = 0; index < G_N_ELEMENTS(vectors); index++)
    {
        const CalendarReferenceVector *vector = &vectors[index];
        const gint64 jdn = calendar_gregorian_to_jdn(
            vector->gregorian_year,
            vector->gregorian_month,
            vector->gregorian_day);
        CalendarCalendarFields fields = { 0 };

        g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
        g_assert_cmpint(fields.year, ==, vector->year);
        g_assert_cmpint(fields.month, ==, vector->month);
        g_assert_cmpint(fields.day, ==, vector->day);
    }

    g_assert_true(calendar_calendar_engine_period_start(
        engine, &selected, &navigated));
    g_assert_true(calendar_date_same(
        navigated.year, navigated.month, navigated.day,
        1582, 10, 1));

    g_assert_true(calendar_calendar_engine_add_periods(
        engine, &selected, 1, &navigated));
    g_assert_true(calendar_date_same(
        navigated.year, navigated.month, navigated.day,
        1582, 11, 10));

    g_assert_true(calendar_calendar_engine_add_years(
        engine, &selected, -82, &navigated));
    g_assert_true(calendar_date_same(
        navigated.year, navigated.month, navigated.day,
        1500, 10, 10));

    g_assert_true(calendar_calendar_engine_build_grid(
        engine, &selected, &today, 0, &grid));
    for (index = 0;
         index < CALENDAR_CALENDAR_GRID_CELLS;
         index++)
    {
        const CalendarCalendarCell *cell = &grid.cells[index];

        if (!cell->is_selected)
            continue;

        found_selected = TRUE;
        g_assert_true(calendar_date_same(
            cell->date.year, cell->date.month, cell->date.day,
            1582, 10, 10));
        g_assert_cmpstr(cell->day_label, ==, "10");
        break;
    }
    g_assert_true(found_selected);
    calendar_calendar_grid_clear(&grid);
    calendar_calendar_engine_free(engine);
}

typedef struct
{
    gint64 now;
    gint utc_offset;
    CalendarClockTimer next_timer;
    CalendarClockTimerFunc callback;
    gpointer callback_data;
    guint last_delay;
    guint schedules;
    guint cancellations;
    guint ticks;
    CalendarClockEngine *engine;
    gboolean reconfigure_on_tick;
    CalendarClockConfig next_config;
} FakeClock;

static gint64
fake_now(gpointer context)
{
    return ((FakeClock *)context)->now;
}

static gint
fake_utc_offset(gpointer context,
                gint64 unix_microseconds)
{
    (void)unix_microseconds;
    return ((FakeClock *)context)->utc_offset;
}

static CalendarClockTimer
fake_schedule(gpointer context,
              guint delay,
              CalendarClockTimerFunc callback,
              gpointer callback_data)
{
    FakeClock *clock = context;

    clock->last_delay = delay;
    clock->callback = callback;
    clock->callback_data = callback_data;
    clock->schedules++;
    clock->next_timer++;
    if (clock->next_timer == (CalendarClockTimer)0)
        clock->next_timer++;
    return clock->next_timer;
}

static void
fake_cancel(gpointer context,
            CalendarClockTimer timer)
{
    FakeClock *clock = context;

    g_assert_cmpuint(timer, ==, clock->next_timer);
    clock->cancellations++;
    clock->callback = NULL;
    clock->callback_data = NULL;
}

static void
fake_tick(gpointer context)
{
    FakeClock *clock = context;

    clock->ticks++;
    if (clock->reconfigure_on_tick)
    {
        clock->reconfigure_on_tick = FALSE;
        g_assert_true(calendar_clock_engine_start(clock->engine,
                                                       &clock->next_config));
    }
}

static void
fake_fire(FakeClock *clock)
{
    CalendarClockTimerFunc callback = clock->callback;
    gpointer callback_data = clock->callback_data;

    g_assert_nonnull(callback);
    clock->callback = NULL;
    clock->callback_data = NULL;
    callback(callback_data);
}

static void
test_bounded_calendar_navigation_failure(void)
{
    CalendarCalendarEngine *engine =
        calendar_calendar_engine_new("islamic-umalqura");
    const CalendarDate first_supported = { 1882, 11, 12 };
    const CalendarDate outside_supported = { 1800, 1, 1 };
    CalendarDate result = { 123, 4, 5 };

    g_assert_nonnull(engine);
    g_assert_false(calendar_calendar_engine_add_periods(
        engine, &first_supported, -1, &result));
    g_assert_cmpint(result.year, ==, 123);
    g_assert_cmpint(result.month, ==, 4);
    g_assert_cmpint(result.day, ==, 5);
    g_assert_false(calendar_calendar_engine_add_years(
        engine, &first_supported, -1, &result));
    g_assert_false(calendar_calendar_engine_period_start(
        engine, &outside_supported, &result));

    calendar_calendar_engine_free(engine);
}

static void
test_clock_interfaces(void)
{
    FakeClock clock = {
        .now = 12 * (gint64)G_USEC_PER_SEC * 60 * 60
    };
    const CalendarClockTimeSource time_source = {
        CALENDAR_CLOCK_TIME_SOURCE_ABI,
        &clock,
        fake_now,
        fake_utc_offset
    };
    const CalendarClockScheduler scheduler = {
        CALENDAR_CLOCK_SCHEDULER_ABI,
        &clock,
        fake_schedule,
        fake_cancel
    };
    CalendarClockConfig config = {
        .mode = CALENDAR_TIME_MODE_DECIMAL,
        .show_seconds = FALSE,
        .vertical = FALSE,
        .latitude = 0.0,
        .longitude = 0.0
    };
    CalendarClockEngine *engine =
        calendar_clock_engine_new(&time_source,
                                       &scheduler,
                                       fake_tick,
                                       &clock);
    g_autofree gchar *formatted = NULL;

    g_assert_nonnull(engine);
    clock.engine = engine;
    g_assert_true(calendar_clock_engine_start(engine, &config));
    g_assert_true(calendar_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.schedules, ==, 1);
    g_assert_cmpuint(clock.last_delay, >, 0);
    formatted = calendar_clock_engine_format(engine);
    g_assert_cmpstr(formatted, ==, "5:00");

    clock.next_config = config;
    clock.next_config.show_seconds = TRUE;
    clock.reconfigure_on_tick = TRUE;
    fake_fire(&clock);
    g_assert_cmpuint(clock.ticks, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 2);
    g_assert_cmpuint(clock.cancellations, ==, 0);

    config = clock.next_config;
    g_assert_true(calendar_clock_engine_start(engine, &config));
    g_assert_cmpuint(clock.schedules, ==, 2);
    config.vertical = TRUE;
    g_assert_true(calendar_clock_engine_start(engine, &config));
    g_assert_cmpuint(clock.cancellations, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 3);

    calendar_clock_engine_stop(engine);
    g_assert_false(calendar_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.cancellations, ==, 2);

    config.mode = CALENDAR_TIME_MODE_NUREMBERG_SOLAR;
    config.latitude = NAN;
    config.longitude = 11.0767;
    g_assert_false(calendar_clock_engine_start(engine, &config));
    g_assert_false(calendar_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.schedules, ==, 3);

    config.mode = CALENDAR_TIME_MODE_ROMAN_TEMPORAL;
    config.latitude = 91.0;
    config.longitude = 11.0767;
    g_assert_false(calendar_clock_engine_start(engine, &config));
    config.latitude = 49.4521;
    config.longitude = 181.0;
    g_assert_false(calendar_clock_engine_start(engine, &config));
    g_assert_false(calendar_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.schedules, ==, 3);

    calendar_clock_engine_free(engine);
}

typedef struct
{
    CalendarClockEngine *engine;
    guint ticks;
} DestroyingTick;

static void
destroying_tick(gpointer context)
{
    DestroyingTick *state = context;

    state->ticks++;
    calendar_clock_engine_free(state->engine);
    state->engine = NULL;
}

static void
test_clock_destroy_during_tick(void)
{
    FakeClock clock = { .now = (gint64)G_USEC_PER_SEC };
    const CalendarClockTimeSource time_source = {
        CALENDAR_CLOCK_TIME_SOURCE_ABI,
        &clock,
        fake_now,
        fake_utc_offset
    };
    const CalendarClockScheduler scheduler = {
        CALENDAR_CLOCK_SCHEDULER_ABI,
        &clock,
        fake_schedule,
        fake_cancel
    };
    const CalendarClockConfig config = {
        .mode = CALENDAR_TIME_MODE_UNIX,
        .show_seconds = FALSE,
        .vertical = FALSE,
        .latitude = 0.0,
        .longitude = 0.0
    };
    DestroyingTick state = { NULL, 0 };

    state.engine = calendar_clock_engine_new(&time_source,
                                                  &scheduler,
                                                  destroying_tick,
                                                  &state);
    g_assert_nonnull(state.engine);
    g_assert_true(calendar_clock_engine_start(state.engine, &config));
    fake_fire(&clock);
    g_assert_null(state.engine);
    g_assert_cmpuint(state.ticks, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 1);
}

static void
test_calendar_records(void)
{
    CalendarCalendarEngine *engine;
    const CalendarDate selected = { 2026, 7, 29 };
    const CalendarDate today = { 2026, 7, 29 };
    CalendarDate start = { 0 };
    CalendarCalendarGrid grid = { 0 };
    CalendarCalendarDescriptor descriptor = { 0 };
    guint selected_count = 0;
    guint item;

    g_assert_cmpuint(calendar_calendar_catalogue_get_count(), ==, 30);
    for (item = 0; item < calendar_calendar_catalogue_get_count(); item++)
    {
        g_assert_true(calendar_calendar_catalogue_get(item, &descriptor));
        g_assert_nonnull(descriptor.id);
        g_assert_nonnull(descriptor.name);
        g_assert_cmpstr(descriptor.id, !=, "");
        g_assert_cmpstr(descriptor.name, !=, "");
    }
    g_assert_false(calendar_calendar_catalogue_get(
        calendar_calendar_catalogue_get_count(), &descriptor));

    engine = calendar_calendar_engine_new("gregorian");
    g_assert_nonnull(engine);
    g_assert_true(calendar_calendar_engine_period_start(engine,
                                                             &selected,
                                                             &start));
    g_assert_cmpint(start.year, ==, 2026);
    g_assert_cmpint(start.month, ==, 7);
    g_assert_cmpint(start.day, ==, 1);
    g_assert_true(calendar_calendar_engine_build_grid(engine,
                                                           &selected,
                                                           &today,
                                                           0,
                                                           &grid));
    for (item = 0; item < CALENDAR_CALENDAR_GRID_CELLS; item++)
    {
        const CalendarCalendarCell *cell = &grid.cells[item];

        g_assert_nonnull(cell->day_label);
        g_assert_cmpint(cell->row, ==, 2 + (gint)(item / 7));
        g_assert_cmpint(cell->column, ==, (gint)(item % 7));
        if (cell->is_selected)
            selected_count++;
    }
    g_assert_cmpuint(selected_count, ==, 1);
    calendar_calendar_grid_clear(&grid);
    calendar_calendar_engine_free(engine);
}

typedef struct
{
    CalendarEventSink sink;
    gboolean started;
    gboolean stopped;
} FakeEventSource;

static gboolean
fake_source_start(gpointer context,
                  const CalendarEventSink *sink)
{
    FakeEventSource *source = context;

    g_assert_nonnull(sink);
    g_assert_cmpuint(sink->abi_version, ==, CALENDAR_EVENT_SINK_ABI);
    source->sink = *sink;
    source->started = TRUE;
    return TRUE;
}

static void
fake_source_stop(gpointer context)
{
    ((FakeEventSource *)context)->stopped = TRUE;
}

static gboolean
fake_source_range(gpointer context,
                  gint64 start_unix,
                  gint64 end_unix)
{
    FakeEventSource *source = context;
    const CalendarEventInput input = {
        "portable-event",
        "#123456",
        "Portable source",
        FALSE,
        start_unix + 3600,
        end_unix - 3600,
        1,
        10
    };

    return source->sink.upsert(source->sink.context, &input);
}

static gboolean
fake_source_open(gpointer context,
                 const gchar *event_id)
{
    (void)context;
    return g_strcmp0(event_id, "portable-event") == 0;
}

static void
test_event_source_and_snapshot(void)
{
    const gint64 day = 1785283200;
    FakeEventSource state = { { 0 }, FALSE, FALSE };
    CalendarEventIndex *index = calendar_event_index_new();
    CalendarEventSink sink;
    CalendarEventSource source = {
        CALENDAR_EVENT_SOURCE_ABI,
        "test-source",
        &state,
        fake_source_start,
        fake_source_stop,
        fake_source_range,
        fake_source_open
    };
    CalendarEventSnapshot *snapshot;

    g_assert_nonnull(index);
    calendar_event_index_make_sink(index, &sink);
    g_assert_true(calendar_event_source_is_valid(&source));
    g_assert_true(source.start(source.context, &sink));
    g_assert_true(source.request_range(source.context, day, day + 86400));
    g_assert_true(source.open_event(source.context, "portable-event"));

    snapshot = calendar_event_index_snapshot(index,
                                                  day,
                                                  day + 43200);
    g_assert_nonnull(snapshot);
    g_assert_cmpuint(snapshot->length, ==, 1);
    g_assert_cmpstr(snapshot->events[0].id, ==, "portable-event");
    g_assert_cmpstr(snapshot->events[0].summary, ==, "Portable source");
    calendar_event_snapshot_free(snapshot);

    source.stop(source.context);
    g_assert_true(state.started);
    g_assert_true(state.stopped);
    calendar_event_index_free(index);
}

static void
test_event_index_scale(void)
{
    enum { EVENT_COUNT = 10000 };
    g_autoptr(GDateTime) midday =
        g_date_time_new_local(2026, 9, 20, 12, 0, 0.0);
    CalendarEventIndex *index = calendar_event_index_new();
    CalendarEventSnapshot *snapshot;
    const gint64 day = g_date_time_to_unix(midday);
    gdouble elapsed;
    guint item;

    g_assert_nonnull(midday);
    g_assert_nonnull(index);

    g_test_timer_start();
    for (item = 0; item < EVENT_COUNT; item++)
    {
        gchar id[48];
        CalendarEventInput input = {
            id,
            "#00ADEF",
            "Scale event",
            FALSE,
            day + (gint64)(item % 21600),
            day + (gint64)(item % 21600) + 60,
            (gint64)item,
            1
        };

        g_snprintf(id, sizeof(id), "scale-%u", item);
        g_assert_true(calendar_event_index_upsert(index, &input));
    }

    snapshot = calendar_event_index_snapshot(index, day, day);
    g_assert_nonnull(snapshot);
    g_assert_cmpuint(snapshot->length, ==, EVENT_COUNT);
    elapsed = g_test_timer_elapsed();
    g_test_message("10k event upsert + snapshot: %.3f s", elapsed);

    /*
     * Regression ceiling, not a benchmark contest. Ten thousand same-day
     * records is deliberately extreme for a panel calendar, and five seconds
     * leaves sanitizer/CI headroom while catching an accidental quadratic path.
     */
    g_assert_cmpfloat(elapsed, <, 5.0);

    calendar_event_snapshot_free(snapshot);
    calendar_event_index_free(index);
}







static void
test_nuremberg_clock_boundaries(void)
{
    const CalendarTimeMode historical =
        calendar_time_mode_from_string("nuremberg-hours");
    const CalendarTimeMode local =
        calendar_time_mode_from_string("nuremberg-solar");
    const CalendarTimeMode ancient =
        calendar_time_mode_from_string("babylonian-ancient");
    const gint64 sample = G_GINT64_CONSTANT(1782043200000000);
    g_autofree gchar *text = NULL;

    g_assert_cmpint(historical, ==, CALENDAR_TIME_MODE_NUREMBERG_HOURS);
    g_assert_false(calendar_time_mode_requires_latitude(historical));
    g_assert_false(calendar_time_mode_requires_longitude(historical));
    text = calendar_format_time_at_location(
        historical, sample, 7200, FALSE, FALSE, NAN, NAN);
    g_assert_true(strstr(text, "NUR-D") != NULL || strstr(text, "NUR-N") != NULL);
    g_clear_pointer(&text, g_free);
    {
        g_autoptr(GDateTime) before_wendetag =
            g_date_time_new_utc(2026, 7, 19, 23, 59, 59.0);
        gint64 near_midnight;
        guint delay;

        g_assert_nonnull(before_wendetag);
        near_midnight =
            calendar_i64_add_saturating(
                calendar_i64_multiply_saturating(
                    g_date_time_to_unix(before_wendetag), G_USEC_PER_SEC),
                900000);
        delay = calendar_time_delay_to_next_tick_at_location(
            historical, near_midnight, 0, FALSE, NAN, NAN);
        g_assert_cmpuint(delay, <=, 100U);
    }

    g_assert_cmpint(local, ==, CALENDAR_TIME_MODE_NUREMBERG_SOLAR);
    g_assert_true(calendar_time_mode_requires_latitude(local));
    text = calendar_format_time_at_location(
        local, sample, 7200, FALSE, FALSE, 49.4521, 11.0767);
    g_assert_nonnull(strstr(text, "NUR-L"));
    g_clear_pointer(&text, g_free);

    g_assert_cmpint(ancient, ==, CALENDAR_TIME_MODE_BABYLONIAN_ANCIENT);
    text = calendar_format_time_at_location(
        ancient, sample, 7200, FALSE, FALSE, 49.4521, 11.0767);
    g_assert_nonnull(strstr(text, "bēru"));
    g_assert_nonnull(strstr(text, "UŠ"));
}

static void
test_time_catalogue(void)
{
    gsize item;

    g_assert_cmpuint(infiltratr_temporal_clock_mode_count(), >=, 3);
    g_assert_cmpuint(calendar_time_mode_get_count(), ==,
                     infiltratr_temporal_clock_mode_count() - 3U);

    g_assert_cmpint(calendar_time_mode_from_string("standard"), ==,
                    CALENDAR_TIME_MODE_INVALID);
    /* Explicit civil choices now have independent native resources, but
     * remain outside the 21 alternative clock catalogue. */
    g_assert_cmpint(calendar_time_mode_from_string("standard-24"), ==,
                    CALENDAR_TIME_MODE_STANDARD_24);
    g_assert_cmpint(calendar_time_mode_from_string("standard-12"), ==,
                    CALENDAR_TIME_MODE_STANDARD_12);
    g_assert_cmpuint(calendar_time_mode_get_count(), ==, 21U);
    g_assert_cmpint(calendar_time_mode_get_at(21U), ==,
                    CALENDAR_TIME_MODE_INVALID);

    for (item = 0; item < calendar_time_mode_get_count(); item++)
    {
        const CalendarTimeMode mode =
            calendar_time_mode_get_at(item);
        const gchar *id = calendar_time_mode_get_id(mode);
        const InfiltratrTemporalClockModeInfo *info;

        g_assert_cmpint(mode, !=, CALENDAR_TIME_MODE_INVALID);
        g_assert_nonnull(id);
        info = infiltratr_temporal_clock_mode_find(id);
        g_assert_nonnull(info);
        /* Stable IDs/capabilities belong to Common's policy catalogue,
         * but the actual human name belongs to the installed module. */
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open_id(id);
        g_assert_nonnull(module);
        const InfiltratrTemporalResourceMetadata *metadata =
            infiltratr_temporal_clock_module_metadata(module);
        g_assert_nonnull(metadata);
        g_assert_cmpstr(calendar_time_mode_get_name(mode), ==, metadata->name);
        infiltratr_temporal_clock_module_free(module);
        g_assert_cmpint(calendar_time_mode_supports_seconds(mode), ==,
                        info->supports_seconds);
        g_assert_cmpint(calendar_time_mode_requires_latitude(mode), ==,
                        info->requires_latitude);
        g_assert_cmpint(calendar_time_mode_requires_longitude(mode), ==,
                        info->requires_longitude);
    }

    g_assert_cmpint(calendar_time_mode_get_at(
                        calendar_time_mode_get_count()), ==,
                    CALENDAR_TIME_MODE_INVALID);
}

int
main(int argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/portable/clock-interfaces", test_clock_interfaces);
    g_test_add_func("/portable/clock-destroy-during-tick",
                    test_clock_destroy_during_tick);
    g_test_add_func("/portable/calendar-records", test_calendar_records);
    g_test_add_func("/portable/bounded-calendar-navigation-failure",
                    test_bounded_calendar_navigation_failure);
    g_test_add_func("/portable/calendar-reference-vectors",
                    test_calendar_reference_vectors);
    g_test_add_func("/portable/ui-language-calendar-presentation",
                    test_ui_language_calendar_presentation);
    g_test_add_func("/portable/native-cyclic-year-presentation",
                    test_native_cyclic_year_presentation);
    g_test_add_func("/portable/historical-calendar-edge-vectors",
                    test_historical_calendar_edge_vectors);
    g_test_add_func("/portable/gregorian-proleptic-cutover",
                    test_gregorian_proleptic_cutover);
    g_test_add_func("/portable/event-source", test_event_source_and_snapshot);
    g_test_add_func("/portable/event-scale", test_event_index_scale);
    g_test_add_func("/portable/time-catalogue", test_time_catalogue);
    g_test_add_func("/portable/nuremberg-clock-boundaries",
                    test_nuremberg_clock_boundaries);
    return g_test_run();
}
