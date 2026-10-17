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

typedef enum
{
    /*
     * Local compiled implementation slots only. Persisted identity and Common
     * metadata are joined by stable calendar ID in calendar-registry.c; these
     * numeric values must never be treated as Common catalogue positions.
     * Native filesystem resources have no slot in this enum.
     */
    CALENDAR_CALENDAR_MODE_GREGORIAN,
    CALENDAR_CALENDAR_MODE_ISO_WEEK,
    CALENDAR_CALENDAR_MODE_JULIAN,
    CALENDAR_CALENDAR_MODE_REVISED_JULIAN,
    CALENDAR_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA,
    CALENDAR_CALENDAR_MODE_ISLAMIC_CIVIL,
    CALENDAR_CALENDAR_MODE_ISLAMIC_TBLA,
    CALENDAR_CALENDAR_MODE_ISLAMIC,
    CALENDAR_CALENDAR_MODE_PERSIAN,
    CALENDAR_CALENDAR_MODE_BAHAI,
    CALENDAR_CALENDAR_MODE_BUDDHIST,
    CALENDAR_CALENDAR_MODE_COPTIC,
    CALENDAR_CALENDAR_MODE_ETHIOPIAN,
    CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM,
    CALENDAR_CALENDAR_MODE_CHINESE,
    CALENDAR_CALENDAR_MODE_DANGI,
    CALENDAR_CALENDAR_MODE_INDIAN,
    CALENDAR_CALENDAR_MODE_JAPANESE,
    CALENDAR_CALENDAR_MODE_MINGUO,
    CALENDAR_CALENDAR_MODE_ROMAN,
    CALENDAR_CALENDAR_MODE_BYZANTINE,
    CALENDAR_CALENDAR_MODE_EGYPTIAN_NABONASSAR,
    CALENDAR_CALENDAR_MODE_ARMENIAN_TRADITIONAL,
    CALENDAR_CALENDAR_MODE_MAYAN,
    CALENDAR_CALENDAR_MODE_FRENCH_REPUBLICAN,
    CALENDAR_CALENDAR_MODE_SWEDISH_HISTORICAL,
    CALENDAR_CALENDAR_MODE_INTERNATIONAL_FIXED,
    CALENDAR_CALENDAR_MODE_WORLD,
    CALENDAR_CALENDAR_MODE_POSITIVIST,
    CALENDAR_CALENDAR_MODE_COUNT
} CalendarCalendarMode;

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
