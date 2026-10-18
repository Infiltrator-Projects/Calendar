// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_CALENDAR_REGISTRY_H
#define CALENDAR_CALENDAR_REGISTRY_H

#include "calendar-internal.h"
#include "calendar-types.h"

#include <infiltratr/temporal.h>

G_BEGIN_DECLS

/*
 * Calendar's internal chronology-dispatch interface.
 *
 * This private adapter lets the grid use validated system resources through
 * one dispatch interface. Resource modules use only Common's product-neutral
 * executable ABI and never depend on this application dispatch contract.
 *
 * calendar_id is the stable semantic join key with Common resource metadata.
 * Never use catalogue position as provider identity: catalogue ordering is not
 * an ABI and may change independently of Calendar's implementation table.
 * common_index is retained only as the historical name of Calendar's local
 * built-in provider slot for existing internal tests; runtime lookup ignores
 * it for filesystem providers.
 */
typedef struct _CalendarCalendarProvider CalendarCalendarProvider;

typedef gboolean (*CalendarProviderFieldsFunc)(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    CalendarCalendarFields *fields);
typedef gchar *(*CalendarProviderFormatFunc)(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    CalendarDatePart part);
typedef gint64 (*CalendarProviderNavigateFunc)(
    const CalendarCalendarProvider *provider,
    gint64 jdn,
    gint amount);
typedef gint64 (*CalendarProviderPeriodStartFunc)(
    const CalendarCalendarProvider *provider,
    gint64 jdn);

struct _CalendarCalendarProvider
{
    guint abi_version;
    const gchar *calendar_id;
    gsize common_index;
    CalendarProviderFieldsFunc fields_from_jdn;
    CalendarProviderFormatFunc format;
    CalendarProviderPeriodStartFunc period_start;
    CalendarProviderNavigateFunc add_periods;
    CalendarProviderNavigateFunc add_years;
};

enum
{
    CALENDAR_CALENDAR_PROVIDER_ABI = 3
};

const CalendarCalendarProvider *calendar_calendar_provider_from_id(
    const gchar *calendar_id);
const InfiltratrTemporalCalendarInfo *calendar_calendar_provider_info(
    const CalendarCalendarProvider *provider);
gsize calendar_calendar_provider_get_count(void);
const CalendarCalendarProvider *calendar_calendar_provider_at(
    gsize index);

G_END_DECLS

#endif
