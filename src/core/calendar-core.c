// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar conversion, navigation and six-row grid modelling.
 *
 * All externally visible results use ordinary C records.  Presentation
 * adapters are responsible for marshalling those records to GVariant, Win32,
 * another toolkit, or any later front end.  The engine resolves one provider
 * at construction and never branches on a provider's implementation family.
 */

#include "calendar-core.h"

#include "calendar-custom.h"
#include "calendar-internal.h"
#include "calendar-registry.h"
#include "locale-weekend.h"
#include "julian-day.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <string.h>

struct _CalendarCalendarEngine
{
    const CalendarCalendarProvider *provider;
};

bool
calendar_format_date_v1(const char *calendar_id,
                            int32_t gregorian_year,
                            int32_t gregorian_month,
                            int32_t gregorian_day,
                            const char *part,
                            char *buffer,
                            size_t capacity,
                            size_t *length)
{
    const CalendarDate date = {
        gregorian_year, gregorian_month, gregorian_day
    };
    CalendarDatePart date_part;
    CalendarCalendarEngine *engine;
    gchar *text;
    size_t size;

    if (buffer == NULL || capacity == 0 || part == NULL ||
        !calendar_date_is_valid(&date))
        return false;
    if (strcmp(part, "short") == 0)
        date_part = CALENDAR_DATE_PART_SHORT;
    else if (strcmp(part, "full") == 0)
        date_part = CALENDAR_DATE_PART_FULL;
    else
        return false;

    engine = calendar_calendar_engine_new(calendar_id);
    if (engine == NULL)
        return false;
    text = calendar_calendar_engine_format_date(engine, &date, date_part);
    calendar_calendar_engine_free(engine);
    if (text == NULL || text[0] == '\0')
    {
        g_free(text);
        return false;
    }
    size = strlen(text);
    if (size >= capacity)
    {
        g_free(text);
        return false;
    }
    infiltratr_copy_string(buffer, capacity, text);
    if (length != NULL)
        *length = size;
    g_free(text);
    return true;
}

static gboolean
date_to_jdn(const CalendarDate *date,
            gint64 *jdn)
{
    if (!calendar_date_is_valid(date) || jdn == NULL)
        return FALSE;

    *jdn = calendar_gregorian_to_jdn(date->year,
                                          date->month,
                                          date->day);
    return TRUE;
}

static gboolean
date_from_jdn(gint64 jdn,
              CalendarDate *date)
{
    const gint64 minimum =
        calendar_gregorian_to_jdn(G_MININT, 1, 1);
    const gint64 maximum =
        calendar_gregorian_to_jdn(G_MAXINT, 12, 31);

    if (date == NULL || jdn < minimum || jdn > maximum)
        return FALSE;

    calendar_jdn_to_gregorian(jdn,
                                   &date->year,
                                   &date->month,
                                   &date->day);
    return calendar_date_is_valid(date);
}

gboolean
calendar_date_is_valid(const CalendarDate *date)
{
    return date != NULL &&
           calendar_gregorian_date_is_valid(date->year,
                                                  date->month,
                                                  date->day);
}

gboolean
calendar_date_same(gint year_a,
                        gint month_a,
                        gint day_a,
                        gint year_b,
                        gint month_b,
                        gint day_b)
{
    const CalendarDate left = { year_a, month_a, day_a };
    const CalendarDate right = { year_b, month_b, day_b };

    return calendar_date_is_valid(&left) &&
           calendar_date_is_valid(&right) &&
           year_a == year_b && month_a == month_b && day_a == day_b;
}

gint
calendar_date_weekday(gint year,
                           gint month,
                           gint day)
{
    const CalendarDate date = { year, month, day };

    if (!calendar_date_is_valid(&date))
        return -1;

    return calendar_iso_weekday(
        calendar_gregorian_to_jdn(year, month, day)) % 7;
}

gboolean
calendar_date_add_days(const CalendarDate *date,
                            gint amount,
                            CalendarDate *result)
{
    gint64 jdn;
    gint64 shifted;

    return result != NULL &&
           date_to_jdn(date, &jdn) &&
           infiltratr_i64_add_checked(jdn, (gint64)amount, &shifted) &&
           date_from_jdn(shifted, result);
}

static gboolean
iso_weekday_is_work_day_for_locale(gint iso_weekday,
                                   const gchar *locale)
{
    gboolean locale_known = FALSE;
    const gboolean locale_workday =
        calendar_locale_is_work_day(locale,
                                         iso_weekday,
                                         &locale_known);

    /*
     * Calendar carries the small CLDR weekData projection it actually needs.
     * The fallback remains Monday-Friday if the input itself is invalid.
     */
    return locale_known ? locale_workday :
           iso_weekday >= 1 && iso_weekday <= 5;
}

static gboolean
iso_weekday_is_work_day(gint iso_weekday)
{
    return iso_weekday_is_work_day_for_locale(iso_weekday, NULL);
}

gboolean
calendar_date_is_work_day(gint year,
                               gint month,
                               gint day)
{
    const CalendarDate date = { year, month, day };

    if (!calendar_date_is_valid(&date))
        return FALSE;

    return iso_weekday_is_work_day(calendar_iso_weekday(
        calendar_gregorian_to_jdn(year, month, day)));
}

CalendarCalendarEngine *
calendar_calendar_engine_new(const gchar *calendar_id)
{
    const CalendarCalendarProvider *provider =
        calendar_calendar_provider_from_id(calendar_id);
    CalendarCalendarEngine *engine;

    if (provider == NULL ||
        provider->abi_version != CALENDAR_CALENDAR_PROVIDER_ABI)
    {
        return NULL;
    }

    engine = g_new0(CalendarCalendarEngine, 1);
    engine->provider = provider;
    return engine;
}

void
calendar_calendar_engine_free(CalendarCalendarEngine *engine)
{
    g_free(engine);
}

const gchar *
calendar_calendar_engine_get_id(
    const CalendarCalendarEngine *engine)
{
    return engine != NULL && engine->provider != NULL &&
           engine->provider->calendar_id != NULL ?
        engine->provider->calendar_id : "";
}

const gchar *
calendar_calendar_engine_get_name(
    const CalendarCalendarEngine *engine)
{
    const InfiltratrTemporalCalendarInfo *info =
        engine != NULL ?
        calendar_calendar_provider_info(engine->provider) : NULL;

    if (info != NULL && info->name != NULL && info->name[0] != '\0')
        return info->name;
    return calendar_calendar_engine_get_id(engine);
}

gchar *
calendar_calendar_engine_format_date(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    CalendarDatePart part)
{
    CalendarCalendarFields fields;
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL ||
        !date_to_jdn(date, &jdn) ||
        part <= CALENDAR_DATE_PART_INVALID ||
        part > CALENDAR_DATE_PART_FULL ||
        !engine->provider->fields_from_jdn(engine->provider, jdn, &fields))
    {
        return g_strdup("");
    }

    /*
     * Formatting must obey the same supported-domain contract as arithmetic.
     * This is especially important for bounded providers such as Umm al-Qura:
     * host ICU may be willing to format dates that Calendar intentionally does
     * not claim to convert or navigate.
     */
    return engine->provider->format(engine->provider, jdn, part);
}

typedef gint64 (*NavigateFunc)(const CalendarCalendarProvider *provider,
                               gint64 jdn,
                               gint amount);

static gboolean
navigate(const CalendarCalendarEngine *engine,
         const CalendarDate *date,
         gint amount,
         NavigateFunc operation,
         CalendarDate *result)
{
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL || operation == NULL ||
        !date_to_jdn(date, &jdn))
    {
        return FALSE;
    }

    /*
     * Providers signal an unrepresentable destination with an out-of-domain
     * JDN. date_from_jdn() converts that sentinel into an ordinary operation
     * failure without confusing it with a legitimate unchanged destination.
     */
    return date_from_jdn(operation(engine->provider, jdn, amount), result);
}

static gint64
period_start_adapter(const CalendarCalendarProvider *provider,
                     gint64 jdn,
                     gint amount)
{
    (void)amount;
    return provider->period_start(provider, jdn);
}

gboolean
calendar_calendar_engine_period_start(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    CalendarDate *result)
{
    return navigate(engine, date, 0, period_start_adapter, result);
}

gboolean
calendar_calendar_engine_add_periods(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    gint amount,
    CalendarDate *result)
{
    return navigate(engine, date, amount,
                    engine != NULL && engine->provider != NULL ?
                    engine->provider->add_periods : NULL,
                    result);
}

gboolean
calendar_calendar_engine_add_years(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    gint amount,
    CalendarDate *result)
{
    return navigate(engine, date, amount,
                    engine != NULL && engine->provider != NULL ?
                    engine->provider->add_years : NULL,
                    result);
}

gchar *
calendar_calendar_engine_period_key(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date)
{
    CalendarCalendarFields fields;
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL ||
        !date_to_jdn(date, &jdn) ||
        !engine->provider->fields_from_jdn(engine->provider, jdn, &fields))
    {
        return g_strdup("");
    }

    return g_strdup_printf("%d:%" G_GINT64_FORMAT ":%d:%d",
                           fields.auxiliary,
                           fields.year,
                           fields.month,
                           fields.special ? 1 : 0);
}

static gboolean
fields_share_period(const CalendarCalendarFields *left,
                    const CalendarCalendarFields *right)
{
    return left->auxiliary == right->auxiliary &&
           left->year == right->year &&
           left->month == right->month &&
           left->special == right->special;
}

void
calendar_calendar_grid_clear(CalendarCalendarGrid *grid)
{
    guint index;

    if (grid == NULL)
        return;

    for (index = 0; index < CALENDAR_CALENDAR_GRID_CELLS; index++)
        g_clear_pointer(&grid->cells[index].day_label, g_free);
    *grid = (CalendarCalendarGrid){ 0 };
}

gboolean
calendar_calendar_engine_build_grid_for_locale(
    const CalendarCalendarEngine *engine,
    const CalendarDate *selected,
    const CalendarDate *today,
    gint week_start,
    const gchar *locale,
    CalendarCalendarGrid *grid)
{
    CalendarCalendarFields selected_fields;
    gint64 selected_jdn;
    gint64 today_jdn;
    gint64 period_start;
    gint64 first_cell;
    gint start_weekday;
    guint index;

    if (grid == NULL)
        return FALSE;
    *grid = (CalendarCalendarGrid){ 0 };

    if (engine == NULL || engine->provider == NULL ||
        week_start < 0 || week_start >= CALENDAR_DAYS_PER_WEEK ||
        !date_to_jdn(selected, &selected_jdn) ||
        !date_to_jdn(today, &today_jdn) ||
        !engine->provider->fields_from_jdn(engine->provider,
                                           selected_jdn,
                                           &selected_fields))
    {
        return FALSE;
    }

    period_start = engine->provider->period_start(engine->provider,
                                                   selected_jdn);
    start_weekday = calendar_iso_weekday(period_start) %
                    CALENDAR_DAYS_PER_WEEK;
    first_cell = period_start - calendar_positive_modulo(
        start_weekday - week_start, CALENDAR_DAYS_PER_WEEK);

    for (index = 0; index < CALENDAR_CALENDAR_GRID_CELLS; index++)
    {
        CalendarCalendarCell *cell = &grid->cells[index];
        const gint64 cell_jdn = first_cell + (gint64)index;
        const gint iso_weekday = calendar_iso_weekday(cell_jdn);
        const gint weekday = iso_weekday % CALENDAR_DAYS_PER_WEEK;
        CalendarCalendarFields fields;

        if (!date_from_jdn(cell_jdn, &cell->date) ||
            !engine->provider->fields_from_jdn(engine->provider,
                                               cell_jdn,
                                               &fields))
        {
            calendar_calendar_grid_clear(grid);
            return FALSE;
        }

        cell->day_label = engine->provider->format(
            engine->provider, cell_jdn, CALENDAR_DATE_PART_DAY);
        if (cell->day_label == NULL)
        {
            calendar_calendar_grid_clear(grid);
            return FALSE;
        }

        cell->row = 2 + (gint)(index / CALENDAR_DAYS_PER_WEEK);
        cell->column = (gint)(index % CALENDAR_DAYS_PER_WEEK);
        cell->week_number = weekday == 4 ?
            calendar_custom_iso_week_number(cell_jdn) : 0;
        cell->is_work_day =
            iso_weekday_is_work_day_for_locale(iso_weekday, locale);
        cell->is_today = cell_jdn == today_jdn;
        cell->is_selected = cell_jdn == selected_jdn;
        cell->is_current_period = fields_share_period(&selected_fields,
                                                      &fields);
        cell->is_top_row = index < CALENDAR_DAYS_PER_WEEK;
        cell->is_left_edge = weekday == week_start;
    }

    return TRUE;
}

gboolean
calendar_calendar_engine_build_grid(
    const CalendarCalendarEngine *engine,
    const CalendarDate *selected,
    const CalendarDate *today,
    gint week_start,
    CalendarCalendarGrid *grid)
{
    return calendar_calendar_engine_build_grid_for_locale(
        engine, selected, today, week_start, NULL, grid);
}

gsize
calendar_calendar_catalogue_get_count(void)
{
    return calendar_calendar_provider_get_count();
}

gboolean
calendar_calendar_catalogue_get(
    gsize index,
    CalendarCalendarDescriptor *descriptor)
{
    const CalendarCalendarProvider *provider;
    const InfiltratrTemporalCalendarInfo *info;

    if (descriptor == NULL)
        return FALSE;

    provider = calendar_calendar_provider_at(index);
    if (provider == NULL || provider->calendar_id == NULL)
        return FALSE;

    info = calendar_calendar_provider_info(provider);
    descriptor->id = provider->calendar_id;
    descriptor->name = info != NULL && info->name != NULL && info->name[0] != '\0'
        ? info->name
        : provider->calendar_id;
    return TRUE;
}
