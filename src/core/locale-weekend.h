// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_LOCALE_WEEKEND_H
#define CALENDAR_PLUS_LOCALE_WEEKEND_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * Return whether an ISO weekday (Monday=1 ... Sunday=7) is a work day for
 * the territory encoded in a POSIX/BCP-47 style locale. The rules are a
 * compact projection of CLDR supplemental weekData. Unknown or regionless
 * locales inherit CLDR territory 001 (Saturday/Sunday weekend).
 */
gboolean calendar_plus_locale_is_work_day(
    const gchar *locale,
    gint iso_weekday,
    gboolean *known);

G_END_DECLS

#endif
