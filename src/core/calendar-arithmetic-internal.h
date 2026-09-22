/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Private family contracts for the arithmetic calendar engine.
 *
 * These declarations are translation-unit boundaries only.  The public
 * arithmetic API remains calendar-arithmetic.h; chronology-specific state
 * and algorithms stay owned by their family modules.
 */

#ifndef CALENDAR_PLUS_CALENDAR_ARITHMETIC_INTERNAL_H
#define CALENDAR_PLUS_CALENDAR_ARITHMETIC_INTERNAL_H

#include "calendar-internal.h"

G_BEGIN_DECLS

gint64 calendar_plus_arithmetic_japanese_signed_year_from_fields(
    const CalendarPlusCalendarFields *fields);
void calendar_plus_arithmetic_japanese_set_year_for_gregorian(
    gint64 gregorian_year,
    CalendarPlusCalendarFields *fields);
void calendar_plus_arithmetic_japanese_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);

gint64 calendar_plus_arithmetic_coptic_signed_year_from_fields(
    CalendarPlusCalendarMode mode,
    const CalendarPlusCalendarFields *fields);
void calendar_plus_arithmetic_coptic_set_signed_year(
    CalendarPlusCalendarMode mode,
    gint64 signed_year,
    CalendarPlusCalendarFields *fields);
gint calendar_plus_arithmetic_coptic_month_length(gint64 year, gint month);
gint64 calendar_plus_arithmetic_coptic_fields_to_jdn(
    CalendarPlusCalendarMode mode, gint64 year, gint month, gint day);
gboolean calendar_plus_arithmetic_coptic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);

gint calendar_plus_arithmetic_indian_month_length(gint64 year, gint month);
gint64 calendar_plus_arithmetic_indian_fields_to_jdn(
    gint64 year, gint month, gint day);
void calendar_plus_arithmetic_indian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);

gboolean calendar_plus_arithmetic_islamic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_arithmetic_islamic_fields_to_jdn(
    CalendarPlusCalendarMode mode,
    gint64 year,
    gint month,
    gint day);
gint calendar_plus_arithmetic_islamic_month_length(
    CalendarPlusCalendarMode mode,
    gint64 year,
    gint month);
gboolean calendar_plus_arithmetic_islamic_year_supported(
    CalendarPlusCalendarMode mode,
    gint64 year);

gboolean calendar_plus_arithmetic_hebrew_is_leap(gint64 year);
gint calendar_plus_arithmetic_hebrew_month_length(gint64 year,
                                                   gint month);
gint64 calendar_plus_arithmetic_hebrew_fields_to_jdn(gint64 year,
                                                       gint month,
                                                       gint day);
void calendar_plus_arithmetic_hebrew_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
void calendar_plus_arithmetic_hebrew_add_months_to_fields(
    CalendarPlusCalendarFields *fields,
    gint amount);

gint calendar_plus_arithmetic_persian_month_length(gint64 year,
                                                    gint month);
gint64 calendar_plus_arithmetic_persian_fields_to_jdn(gint64 year,
                                                       gint month,
                                                       gint day);
void calendar_plus_arithmetic_persian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);

G_END_DECLS

#endif
