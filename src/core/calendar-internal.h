/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Private calendar-engine types.
 *
 * This header is deliberately excluded from GObject Introspection. The
 * presentation layer sees the stable CalendarSystem API; native modules use
 * these types to exchange values without stringly typed or layout-dependent
 * contracts.
 */

#ifndef CALENDAR_CALENDAR_INTERNAL_H
#define CALENDAR_CALENDAR_INTERNAL_H

#include "calendar-core.h"

#include <glib.h>

G_BEGIN_DECLS



/*
 * A neutral calendar tuple. "auxiliary" distinguishes eras and leap-month
 * cycles when year/month/day alone are ambiguous. "special" identifies an
 * intercalary day or leap month. The invariant used by period comparison is:
 *
 *   same period iff auxiliary, year, month and special all compare equal.
 */
typedef struct
{
    gint64 year;
    gint month;
    gint day;
    gint auxiliary;
    gboolean special;
} CalendarCalendarFields;

enum
{
    CALENDAR_DAYS_PER_WEEK = 7
};

/*
 * Testable internal variant of the public grid builder. Production callers
 * pass NULL and locale-weekend.c resolves the active process LC_TIME locale;
 * deterministic regression tests may inject a CLDR locale explicitly.
 */
gboolean calendar_calendar_engine_build_grid_for_locale(
    const CalendarCalendarEngine *engine,
    const CalendarDate *selected,
    const CalendarDate *today,
    gint week_start,
    const gchar *locale,
    CalendarCalendarGrid *grid);

G_END_DECLS

#endif
