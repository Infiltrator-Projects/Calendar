// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_SYSTEM_CLOCK_PRIVATE_H
#define CALENDAR_SYSTEM_CLOCK_PRIVATE_H

#include <glib.h>
#include <infiltratr/temporal.h>

/*
 * Pure authority-resolution seam used by the adapter and deterministic tests.
 * A provider marker alone is not authority: a valid persisted policy must have
 * been loaded. The adapter treats either an installed writer marker or that
 * valid persisted policy as an available temporal-v3 capability, allowing the
 * selected policy to survive removal of the System Settings writer/UI package.
 */
gboolean calendar_system_clock_resolve_effective_policy(
    gboolean provider_available,
    gboolean policy_found,
    const InfiltratrTemporalPolicyV3 *persisted_policy,
    gboolean cinnamon_show_seconds,
    InfiltratrTemporalPolicyV3 *effective_policy,
    gboolean *infiltrator_authority);

#endif
