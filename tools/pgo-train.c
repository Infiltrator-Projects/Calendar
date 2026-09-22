// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Representative workload for hardware-local profile-guided optimisation.
 * This is not a correctness test.  It exercises the production shared library
 * across the paths a normal Calendar session repeatedly uses: calendar grids
 * and navigation, locale/workday queries, clock formatting/tick calculation
 * (including astronomical modes), and event timing classification.
 */

#include "calendar-core.h"
#include "calendar-system.h"
#include "event-store.h"
#include "time-formats.h"

#include <glib.h>


int
main(void)
{
    const gint64 base_us = G_GINT64_CONSTANT(1788254700) * G_USEC_PER_SEC;
    guint pass;

    for (pass = 0; pass < 24; pass++)
    {
        gsize index;

        for (index = 0;
             index < calendar_plus_calendar_catalogue_get_count();
             index++)
        {
            CalendarPlusCalendarDescriptor descriptor = { 0 };
            g_autoptr(CalendarPlusCalendarSystem) calendar = NULL;
            gint month;

            if (!calendar_plus_calendar_catalogue_get(index, &descriptor))
                return 1;
            calendar = calendar_plus_calendar_system_new(descriptor.id);
            if (calendar == NULL)
                return 1;

            for (month = 1; month <= 12; month += 2)
            {
                const gint day = month == 2 ? 28 : 15;
                g_autofree gchar *short_date =
                    calendar_plus_calendar_system_format_date(
                        calendar, 2026, month, day, "short");
                g_autofree gchar *full_date =
                    calendar_plus_calendar_system_format_date(
                        calendar, 2026, month, day, "full");
                g_autofree gchar *next_period =
                    calendar_plus_calendar_system_add_months(
                        calendar, 2026, month, day, 1);
                g_autofree gchar *next_year =
                    calendar_plus_calendar_system_add_years(
                        calendar, 2026, month, day, 1);
                g_autoptr(GVariant) grid =
                    calendar_plus_calendar_system_build_grid(
                        calendar, 2026, month, day,
                        2026, month, day, 1);

                if (short_date == NULL || full_date == NULL ||
                    next_period == NULL || next_year == NULL || grid == NULL)
                    return 1;

                (void)calendar_plus_date_is_work_day(2026, month, day);
            }
        }

        for (index = 0; index < calendar_plus_time_mode_get_count(); index++)
        {
            const CalendarPlusTimeMode mode =
                calendar_plus_time_mode_get_at(index);
            guint sample;

            for (sample = 0; sample < 48; sample++)
            {
                const gint64 instant =
                    base_us + (gint64)(sample * 1800U) * G_USEC_PER_SEC;
                g_autofree gchar *text =
                    calendar_plus_format_time_at_location(
                        mode, instant, 36000, TRUE, FALSE,
                        -36.3805, 145.3997);

                if (text == NULL)
                    return 1;

                (void)calendar_plus_time_delay_to_next_tick_at_location(
                    mode, instant, 36000, TRUE, -36.3805, 145.3997);
            }
        }

        for (index = 0; index < 4096; index++)
        {
            const gint64 start = G_GINT64_CONSTANT(1788254700) + (gint64)index;
            const gint64 end = start + 3600;
            const gint64 now = start + (gint64)(index % 5400U);
            g_autoptr(GVariant) timing =
                calendar_plus_event_timing(start, end, now);

            (void)calendar_plus_event_state(start, end, now);
            (void)calendar_plus_event_day_relation(
                start - (start % 86400),
                end - (end % 86400),
                now);
            if (timing == NULL)
                return 1;
        }
    }

    g_print("Calendar representative PGO training workload completed.\n");
    return 0;
}
