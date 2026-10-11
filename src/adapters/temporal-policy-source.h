// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_TEMPORAL_POLICY_SOURCE_H
#define CALENDAR_PLUS_TEMPORAL_POLICY_SOURCE_H

#include <glib.h>
#include <infiltratr/temporal.h>

G_BEGIN_DECLS

typedef struct _CalendarPlusTemporalPolicySource CalendarPlusTemporalPolicySource;
typedef void (*CalendarPlusTemporalPolicyChangedFunc)(gpointer user_data);

/*
 * Resolve one effective temporal policy without touching process-global state.
 * A provider marker alone is never authority: the provider must be installed
 * and a valid temporal-v3 policy must have been loaded. Otherwise Cinnamon's
 * conventional seconds preference remains authoritative over the default
 * standard/Gregorian policy.
 */
gboolean calendar_plus_temporal_policy_resolve_effective(
    gboolean provider_available,
    gboolean policy_found,
    const InfiltratrTemporalPolicyV3 *persisted_policy,
    gboolean cinnamon_show_seconds,
    InfiltratrTemporalPolicyV3 *effective_policy,
    gboolean *infiltrator_authority);

/*
 * Platform policy source used by SystemClock. It owns Cinnamon fallback
 * settings, System Settings provider/policy discovery and all filesystem
 * monitors. The clock facade consumes only the resolved immutable snapshot.
 */
CalendarPlusTemporalPolicySource *calendar_plus_temporal_policy_source_new(
    CalendarPlusTemporalPolicyChangedFunc changed,
    gpointer user_data);
void calendar_plus_temporal_policy_source_free(
    CalendarPlusTemporalPolicySource *source);
const InfiltratrTemporalPolicyV3 *calendar_plus_temporal_policy_source_get_policy(
    const CalendarPlusTemporalPolicySource *source);
gboolean calendar_plus_temporal_policy_source_provider_available(
    const CalendarPlusTemporalPolicySource *source);
gboolean calendar_plus_temporal_policy_source_has_infiltrator_authority(
    const CalendarPlusTemporalPolicySource *source);

G_END_DECLS

#endif
