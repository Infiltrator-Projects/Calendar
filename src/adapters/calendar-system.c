// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * GObject facade for the platform-neutral calendar engine.
 *
 * This file owns object lifetime, translated provider names and the legacy
 * string API.  GVariant marshalling lives in calendar-gvariant-adapter.c;
 * conversion, navigation and grid semantics live in calendar-core.c.
 */

#include "calendar-system-private.h"

#include "julian-day.h"

#include <glib/gi18n-lib.h>
#include <infiltratr/core.h>

struct _CalendarCalendarSystem
{
    GObject parent_instance;

    CalendarCalendarEngine *engine;
};

/* GLib's type-registration macro contains an intentional pointer probe. */
// NOLINTNEXTLINE(performance-no-int-to-ptr)
G_DEFINE_TYPE(CalendarCalendarSystem,
              calendar_calendar_system,
              G_TYPE_OBJECT)

static void
calendar_calendar_system_dispose(GObject *object)
{
    CalendarCalendarSystem *self =
        CALENDAR_CALENDAR_SYSTEM(object);

    calendar_calendar_engine_free(self->engine);
    self->engine = NULL;
    G_OBJECT_CLASS(calendar_calendar_system_parent_class)->dispose(object); // NOLINT(bugprone-casting-through-void)
}

static void
calendar_calendar_system_class_init(
    CalendarCalendarSystemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass); // NOLINT(bugprone-casting-through-void)

    object_class->dispose = calendar_calendar_system_dispose;
}

static void
calendar_calendar_system_init(CalendarCalendarSystem *self)
{
    self->engine = calendar_calendar_engine_new("gregorian");
}

CalendarCalendarSystem *
calendar_calendar_system_new(const gchar *calendar_id)
{
    CalendarCalendarEngine *engine =
        calendar_calendar_engine_new(calendar_id);
    CalendarCalendarSystem *self;

    if (engine == NULL)
        return NULL;

    self = g_object_new(CALENDAR_TYPE_CALENDAR_SYSTEM, NULL);
    calendar_calendar_engine_free(self->engine);
    self->engine = engine;
    return self;
}

CalendarCalendarEngine *
calendar_calendar_system_get_engine(CalendarCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self), NULL);
    return self->engine;
}

const gchar *
calendar_calendar_system_get_id(CalendarCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self), "");
    return calendar_calendar_engine_get_id(self->engine);
}

const gchar *
calendar_calendar_system_get_name(CalendarCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self), "");
    return _(calendar_calendar_engine_get_name(self->engine));
}

static CalendarDatePart
date_part_from_string(const gchar *part)
{
    if (infiltratr_string_equal(part, "day"))
        return CALENDAR_DATE_PART_DAY;
    if (infiltratr_string_equal(part, "month"))
        return CALENDAR_DATE_PART_MONTH;
    if (infiltratr_string_equal(part, "year"))
        return CALENDAR_DATE_PART_YEAR;
    if (infiltratr_string_equal(part, "short"))
        return CALENDAR_DATE_PART_SHORT;
    if (infiltratr_string_equal(part, "full"))
        return CALENDAR_DATE_PART_FULL;
    return CALENDAR_DATE_PART_INVALID;
}

gchar *
calendar_calendar_system_format_date(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    const gchar *part)
{
    return calendar_calendar_system_format_date_part(
        self,
        gregorian_year,
        gregorian_month,
        gregorian_day,
        date_part_from_string(part));
}

gchar *
calendar_calendar_system_format_date_part(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    CalendarDatePart part)
{
    const CalendarDate date = {
        gregorian_year, gregorian_month, gregorian_day
    };

    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    return calendar_calendar_engine_format_date(self->engine,
                                                     &date,
                                                     part);
}

typedef gboolean (*DateOperation)(const CalendarCalendarEngine *engine,
                                  const CalendarDate *date,
                                  gint amount,
                                  CalendarDate *result);

static gboolean
period_start_operation(const CalendarCalendarEngine *engine,
                       const CalendarDate *date,
                       gint amount,
                       CalendarDate *result)
{
    (void)amount;
    return calendar_calendar_engine_period_start(engine, date, result);
}

static gchar *
navigate_as_iso(CalendarCalendarSystem *self,
                gint year,
                gint month,
                gint day,
                gint amount,
                DateOperation operation)
{
    const CalendarDate input = { year, month, day };
    CalendarDate result;

    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    if (!operation(self->engine, &input, amount, &result))
        return g_strdup("");

    return calendar_format_iso_date(
        calendar_gregorian_to_jdn(result.year,
                                       result.month,
                                       result.day));
}

gchar *
calendar_calendar_system_month_start(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           0,
                           period_start_operation);
}

gchar *
calendar_calendar_system_add_months(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           amount,
                           calendar_calendar_engine_add_periods);
}

gchar *
calendar_calendar_system_add_years(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           amount,
                           calendar_calendar_engine_add_years);
}

gchar *
calendar_calendar_system_month_key(
    CalendarCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    const CalendarDate date = {
        gregorian_year, gregorian_month, gregorian_day
    };

    g_return_val_if_fail(CALENDAR_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    return calendar_calendar_engine_period_key(self->engine, &date);
}
