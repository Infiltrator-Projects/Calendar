// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_SYSTEM_CLOCK_PRIVATE_H
#define CALENDAR_PLUS_SYSTEM_CLOCK_PRIVATE_H

#include <glib.h>
#include <infiltratr/temporal.h>

/*
 * Pure authority-resolution seam used by the adapter and deterministic tests.
 * A provider marker alone is not authority: the provider must be installed and
 * a valid policy must have been loaded. Otherwise Cinnamon remains the source
 * for conventional presentation.
 */
gboolean calendar_plus_system_clock_resolve_effective_policy(
    gboolean provider_available,
    gboolean policy_found,
    const InfiltratrTemporalPolicyV3 *persisted_policy,
    gboolean cinnamon_show_seconds,
    InfiltratrTemporalPolicyV3 *effective_policy,
    gboolean *infiltrator_authority);

#endif
