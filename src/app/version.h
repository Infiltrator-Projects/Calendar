// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_VERSION_H
#define CALENDAR_PLUS_VERSION_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * calendar_plus_get_version:
 *
 * Returns the Calendar source version used to build the loaded native
 * library.  This is intentionally part of the introspected ABI so the GJS
 * front end can reject a stale or mismatched native library instead of
 * failing later with misleading secondary errors.
 *
 * Returns: (transfer none): the semantic source version
 */
const gchar *calendar_plus_get_version(void);

/**
 * calendar_plus_get_source_id:
 *
 * Returns a deterministic source identity compiled into the native library.
 * Release builds use `calendar-plus-<version>`; packagers may override the
 * build macro when they have a reproducible downstream source identifier.
 *
 * Returns: (transfer none): the native source identity
 */
const gchar *calendar_plus_get_source_id(void);

/**
 * calendar_plus_get_build_profile:
 *
 * Returns the canonical machine-readable build profile compiled into the
 * loaded native library. Repository packages use generic; local hardware-
 * native installer builds use native.
 *
 * Returns: (transfer none): the canonical build profile
 */
const gchar *calendar_plus_get_build_profile(void);

/**
 * calendar_plus_get_build_profile_label:
 *
 * Returns Infiltratr Common's canonical human-readable label for the compiled
 * build profile so every project presents build identity consistently.
 *
 * Returns: (transfer none): the canonical shared build-profile label
 */
const gchar *calendar_plus_get_build_profile_label(void);

G_END_DECLS

#endif
