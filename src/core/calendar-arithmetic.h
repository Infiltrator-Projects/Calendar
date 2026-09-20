// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_CALENDAR_ARITHMETIC_H
#define CALENDAR_PLUS_CALENDAR_ARITHMETIC_H

#include "calendar-internal.h"

G_BEGIN_DECLS

/*
 * Calendar-owned arithmetic replacements for fixed-rule ICU providers.
 *
 * Formatting remains delegated to ICU while locale data is being migrated,
 * but conversion and navigation for these providers no longer depend on ICU.
 */
gboolean calendar_plus_arithmetic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_arithmetic_month_start(
    CalendarPlusCalendarMode mode,
    gint64 jdn);
gint64 calendar_plus_arithmetic_add_months(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount);
gint64 calendar_plus_arithmetic_add_years(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount);

G_END_DECLS

#endif
