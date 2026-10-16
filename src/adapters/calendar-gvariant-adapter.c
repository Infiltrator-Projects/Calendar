/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * GVariant marshalling for CalendarSystem's GJS ABI.
 *
 * The tuple layouts are Cinnamon adapter contracts, not calendar-engine data
 * structures. Keeping them here lets other native front ends use
 * calendar-core.h without understanding GVariant or GObject Introspection.
 */

#include "calendar-system-private.h"

static GVariant *
date_to_variant(const CalendarDate *date)
{
    return g_variant_ref_sink(
        g_variant_new("(iii)", date->year, date->month, date->day));
}

typedef gboolean (*DateOperation)(const CalendarCalendarEngine *engine,
                                  const CalendarDate *date,
                                  gint amount,
                                  CalendarDate *result);

static gboolean
add_days_operation(const CalendarCalendarEngine *engine,
                   const CalendarDate *date,
                   gint amount,
                   CalendarDate *result)
{
    (void)engine;
    return calendar_date_add_days(date, amount, result);
}

static gboolean
period_start_operation(const CalendarCalendarEngine *engine,
                       const CalendarDate *date,
                       gint amount,
                       CalendarDate *result)
{
    (void)amount;
    return calendar_calendar_engine_period_start(engine, date, result);
}

static GVariant *
navigate_to_variant(CalendarCalendarSystem *self,
                    gint year,
                    gint month,
                    gint day,
                    gint amount,
                    DateOperation operation)
{
    const CalendarDate input = { year, month, day };
    CalendarDate result;
    CalendarCalendarEngine *engine =
        calendar_calendar_system_get_engine(self);

    if (engine == NULL || !operation(engine, &input, amount, &result))
        return NULL;
    return date_to_variant(&result);
}

GVariant *
calendar_calendar_system_month_start_parts(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               0,
                               period_start_operation);
}

GVariant *
calendar_calendar_system_add_days_parts(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               amount,
                               add_days_operation);
}

GVariant *
calendar_calendar_system_add_months_parts(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               amount,
                               calendar_calendar_engine_add_periods);
}

GVariant *
calendar_calendar_system_add_years_parts(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               amount,
                               calendar_calendar_engine_add_years);
}

GVariant *
calendar_calendar_system_build_grid(
    CalendarCalendarSystem *self,
    gint selected_year,
    gint selected_month,
    gint selected_day,
    gint today_year,
    gint today_month,
    gint today_day,
    gint week_start)
{
    const CalendarDate selected = {
        selected_year, selected_month, selected_day
    };
    const CalendarDate today = { today_year, today_month, today_day };
    CalendarCalendarGrid grid;
    GVariantBuilder cells;
    guint index;

    if (!calendar_calendar_engine_build_grid(
            calendar_calendar_system_get_engine(self),
            &selected,
            &today,
            week_start,
            &grid))
    {
        return NULL;
    }

    g_variant_builder_init(&cells,
                           G_VARIANT_TYPE("a(siiiiiibbbbbb)"));
    for (index = 0; index < CALENDAR_CALENDAR_GRID_CELLS; index++)
    {
        const CalendarCalendarCell *cell = &grid.cells[index];

        g_variant_builder_add(&cells,
                              "(siiiiiibbbbbb)",
                              cell->day_label,
                              cell->date.year,
                              cell->date.month,
                              cell->date.day,
                              cell->row,
                              cell->column,
                              cell->week_number,
                              cell->is_work_day,
                              cell->is_today,
                              cell->is_selected,
                              cell->is_current_period,
                              cell->is_top_row,
                              cell->is_left_edge);
    }

    calendar_calendar_grid_clear(&grid);
    return g_variant_ref_sink(g_variant_builder_end(&cells));
}
