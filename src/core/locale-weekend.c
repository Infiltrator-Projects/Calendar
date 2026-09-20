// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Compact CLDR weekend policy.
 *
 * Authority:
 * Unicode CLDR common/supplemental/supplementalData.xml, weekData.
 *
 * CLDR defines territory 001 as Saturday through Sunday. Only territories
 * whose weekend start/end differ from that default need to be recorded here.
 * Keeping this tiny policy table locally removes a runtime ICU dependency from
 * Calendar's work-day styling without turning locale policy into guesswork.
 */

#include "locale-weekend.h"

#include <ctype.h>
#include <string.h>

typedef struct
{
    const gchar *territories;
    gint start;
    gint end;
} WeekendRule;

/* ISO weekdays: Monday=1 ... Sunday=7. */
static const WeekendRule weekend_rules[] = {
    { "AF", 4, 5 },
    { "IR", 5, 5 },
    { "BH DZ EG IL IQ JO KW LY OM QA SA SD SY YE", 5, 6 },
    { "IN UG", 7, 7 }
};

static gboolean
territory_list_contains(const gchar *list,
                        const gchar territory[3])
{
    const gchar *cursor = list;

    while (*cursor != '\0')
    {
        while (*cursor == ' ')
            cursor++;
        if (cursor[0] == territory[0] &&
            cursor[1] == territory[1] &&
            (cursor[2] == '\0' || cursor[2] == ' '))
        {
            return TRUE;
        }
        while (*cursor != '\0' && *cursor != ' ')
            cursor++;
    }
    return FALSE;
}

static void
locale_territory(const gchar *locale,
                 gchar territory[3])
{
    const gchar *cursor;

    territory[0] = '0';
    territory[1] = '0';
    territory[2] = '1';

    if (locale == NULL)
        return;

    for (cursor = locale; *cursor != '\0'; cursor++)
    {
        if ((*cursor != '_' && *cursor != '-') ||
            !g_ascii_isalpha(cursor[1]) ||
            !g_ascii_isalpha(cursor[2]))
        {
            continue;
        }

        if (cursor[3] != '\0' &&
            cursor[3] != '.' &&
            cursor[3] != '@' &&
            cursor[3] != '-' &&
            cursor[3] != '_')
        {
            continue;
        }

        territory[0] = (gchar)g_ascii_toupper(cursor[1]);
        territory[1] = (gchar)g_ascii_toupper(cursor[2]);
        territory[2] = '\0';
        return;
    }
}

gboolean
calendar_plus_locale_is_work_day(const gchar *locale,
                                 gint iso_weekday,
                                 gboolean *known)
{
    gchar territory[3];
    gint weekend_start = 6;
    gint weekend_end = 7;
    gsize index;

    if (known != NULL)
        *known = FALSE;
    if (iso_weekday < 1 || iso_weekday > 7)
        return FALSE;

    locale_territory(locale, territory);
    for (index = 0; index < G_N_ELEMENTS(weekend_rules); index++)
    {
        if (!territory_list_contains(
                weekend_rules[index].territories, territory))
        {
            continue;
        }
        weekend_start = weekend_rules[index].start;
        weekend_end = weekend_rules[index].end;
        break;
    }

    if (known != NULL)
        *known = TRUE;

    if (weekend_start <= weekend_end)
        return iso_weekday < weekend_start || iso_weekday > weekend_end;

    return iso_weekday > weekend_end && iso_weekday < weekend_start;
}
