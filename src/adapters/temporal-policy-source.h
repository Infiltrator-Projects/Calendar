// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_TEMPORAL_POLICY_SOURCE_H
#define CALENDAR_PLUS_TEMPORAL_POLICY_SOURCE_H

#include <glib.h>
#include <infiltratr/temporal.h>

G_BEGIN_DECLS

typedef struct _CalendarPlusTemporalPolicySource CalendarPlusTemporalPolicySource;
typedef void (*CalendarPlusTemporalPolicyChangedFunc)(gpointer user_data);

CalendarPlusTemporalPolicySource *calendar_plus_temporal_policy_source_new(
    CalendarPlusTemporalPolicyChangedFunc changed,
    gpointer user_data);
void calendar_plus_temporal_policy_source_free(
    CalendarPlusTemporalPolicySource *source);

const InfiltratrTemporalPolicyV3 *calendar_plus_temporal_policy_source_get_policy(
    const CalendarPlusTemporalPolicySource *source);
gboolean calendar_plus_temporal_policy_source_get_provider_available(
    const CalendarPlusTemporalPolicySource *source);
gboolean calendar_plus_temporal_policy_source_get_infiltrator_authority(
    const CalendarPlusTemporalPolicySource *source);

G_END_DECLS

#endif
