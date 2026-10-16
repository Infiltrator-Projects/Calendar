// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_TEMPORAL_POLICY_SOURCE_H
#define CALENDAR_TEMPORAL_POLICY_SOURCE_H

#include <glib.h>
#include <infiltratr/temporal.h>

typedef struct _CalendarTemporalPolicySource CalendarTemporalPolicySource;
typedef void (*CalendarTemporalPolicyChangedFunc)(gpointer user_data);

CalendarTemporalPolicySource *calendar_temporal_policy_source_new(
    CalendarTemporalPolicyChangedFunc changed,
    gpointer user_data);
void calendar_temporal_policy_source_free(
    CalendarTemporalPolicySource *source);
const InfiltratrTemporalPolicyV3 *calendar_temporal_policy_source_get_policy(
    const CalendarTemporalPolicySource *source);
gboolean calendar_temporal_policy_source_get_provider_available(
    const CalendarTemporalPolicySource *source);
gboolean calendar_temporal_policy_source_get_infiltrator_authority(
    const CalendarTemporalPolicySource *source);

#endif
