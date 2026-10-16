/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Platform-neutral calendar engine contract.
 *
 * This header deliberately depends on GLib base types only.  It contains no
 * GObject, GVariant, Cinnamon or operating-system UI types, so another front
 * end can consume the same calendar model without adopting the GJS adapter.
 */

#ifndef CALENDAR_CALENDAR_CORE_H
#define CALENDAR_CALENDAR_CORE_H

#include "calendar-types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

G_BEGIN_DECLS

enum
{
    CALENDAR_CALENDAR_GRID_CELLS = 42
};

typedef struct _CalendarCalendarEngine CalendarCalendarEngine;

/* Proleptic Gregorian civil date used at every platform boundary. */
typedef struct
{
    gint year;
    gint month;
    gint day;
} CalendarDate;

typedef struct
{
    /* Owned UTF-8 label. calendar_calendar_grid_clear() releases it. */
    gchar *day_label;
    /* Absolute proleptic-Gregorian coordinate for event and selection use. */
    CalendarDate date;
    /* Table row 2..7 and logical weekday column 0..6. */
    gint row;
    gint column;
    /* ISO week number on the row's Thursday cell; zero on other cells. */
    gint week_number;
    gboolean is_work_day;
    gboolean is_today;
    gboolean is_selected;
    gboolean is_current_period;
    gboolean is_top_row;
    gboolean is_left_edge;
} CalendarCalendarCell;

typedef struct
{
    CalendarCalendarCell cells[CALENDAR_CALENDAR_GRID_CELLS];
} CalendarCalendarGrid;

typedef struct
{
    const gchar *id;
    const gchar *name;
} CalendarCalendarDescriptor;

/**
 * calendar_calendar_engine_new:
 * @calendar_id: stable provider id such as "gregorian"
 *
 * Resolves one immutable provider for formatting and navigation.
 *
 * Returns: (transfer full) (nullable): new engine, or %NULL for unknown id
 */
CalendarCalendarEngine *calendar_calendar_engine_new(
    const gchar *calendar_id);

/**
 * calendar_calendar_engine_free:
 * @engine: (transfer full) (nullable): engine to release
 *
 * Accepts %NULL.
 */
void calendar_calendar_engine_free(CalendarCalendarEngine *engine);

/** Returns: (transfer none): process-lifetime stable provider id. */
const gchar *calendar_calendar_engine_get_id(
    const CalendarCalendarEngine *engine);
/** Returns: (transfer none): process-lifetime stable provider name. */
const gchar *calendar_calendar_engine_get_name(
    const CalendarCalendarEngine *engine);

/**
 * calendar_calendar_engine_format_date:
 * @engine: calendar engine
 * @date: proleptic Gregorian coordinate
 * @part: requested provider date component
 *
 * Returns: (transfer full): allocated UTF-8 text; invalid input yields ""
 */
gchar *calendar_calendar_engine_format_date(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    CalendarDatePart part);

/*
 * Versioned, toolkit-free runtime ABI consumed by Common's optional POSIX date
 * bridge. Parts are "short" and "full". Caller owns buffer; buffer and length
 * remain unchanged on invalid/unavailable output or insufficient capacity.
 * This uses the same chronology as the engine/GObject/Cinnamon surfaces.
 */
bool calendar_format_date_v1(
    const char *calendar_id,
    int32_t gregorian_year,
    int32_t gregorian_month,
    int32_t gregorian_day,
    const char *part,
    char *buffer,
    size_t capacity,
    size_t *length);
/**
 * calendar_calendar_engine_period_start:
 * @engine: calendar engine
 * @date: source Gregorian coordinate
 * @result: (out): first Gregorian day of the provider period
 *
 * Writes @result only on success.
 */
gboolean calendar_calendar_engine_period_start(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    CalendarDate *result);
/**
 * calendar_calendar_engine_add_periods:
 * @engine: calendar engine
 * @date: source Gregorian coordinate
 * @amount: signed provider-period delta
 * @result: (out): destination Gregorian coordinate
 *
 * Writes @result only on success.
 */
gboolean calendar_calendar_engine_add_periods(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    gint amount,
    CalendarDate *result);
/**
 * calendar_calendar_engine_add_years:
 * @engine: calendar engine
 * @date: source Gregorian coordinate
 * @amount: signed provider-year delta
 * @result: (out): destination Gregorian coordinate
 *
 * Writes @result only on success.
 */
gboolean calendar_calendar_engine_add_years(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date,
    gint amount,
    CalendarDate *result);
/**
 * calendar_calendar_engine_period_key:
 * @engine: calendar engine
 * @date: source Gregorian coordinate
 *
 * Returns: (transfer full): allocated stable comparison key, or ""
 */
gchar *calendar_calendar_engine_period_key(
    const CalendarCalendarEngine *engine,
    const CalendarDate *date);

/**
 * calendar_calendar_engine_build_grid:
 * @engine: calendar engine
 * @selected: selected Gregorian date
 * @today: current Gregorian date
 * @week_start: 0=Sunday through 6=Saturday
 * @grid: (out): complete 42-cell model
 *
 * Fills all 42 cells on success. Each day_label is owned by @grid and released
 * by calendar_calendar_grid_clear(). Invalid week starts fail.
 */
gboolean calendar_calendar_engine_build_grid(
    const CalendarCalendarEngine *engine,
    const CalendarDate *selected,
    const CalendarDate *today,
    gint week_start,
    CalendarCalendarGrid *grid);
/**
 * calendar_calendar_grid_clear:
 * @grid: (nullable): grid to clear
 *
 * Idempotently releases owned labels and zeroes the model.
 */
void calendar_calendar_grid_clear(CalendarCalendarGrid *grid);

/** Returns: number of immutable registered calendar providers. */
gsize calendar_calendar_catalogue_get_count(void);
/**
 * calendar_calendar_catalogue_get:
 * @index: zero-based provider index
 * @descriptor: (out): borrowed stable id/name descriptor
 *
 * Returns: %TRUE when @index names a registered provider.
 */
gboolean calendar_calendar_catalogue_get(
    gsize index,
    CalendarCalendarDescriptor *descriptor);

/** Returns: whether @date is a valid proleptic Gregorian civil date. */
gboolean calendar_date_is_valid(const CalendarDate *date);
/** Returns: whether two valid Gregorian field triples identify one day. */
gboolean calendar_date_same(gint year_a,
                                 gint month_a,
                                 gint day_a,
                                 gint year_b,
                                 gint month_b,
                                 gint day_b);
/** Returns: Sunday=0 through Saturday=6, or -1 for invalid input. */
gint calendar_date_weekday(gint year, gint month, gint day);
/** Adds signed Gregorian civil days without passing through local time. */
gboolean calendar_date_add_days(const CalendarDate *date,
                                     gint amount,
                                     CalendarDate *result);
/**
 * calendar_date_is_work_day:
 * @year: Gregorian year
 * @month: Gregorian month 1..12
 * @day: Gregorian day
 *
 * Uses Calendar's compact CLDR weekData weekend projection, with
 * Monday-Friday fallback only when the input weekday itself is invalid.
 */
gboolean calendar_date_is_work_day(gint year,
                                        gint month,
                                        gint day);

G_END_DECLS

#endif
