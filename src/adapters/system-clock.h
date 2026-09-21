// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_PLUS_SYSTEM_CLOCK_H
#define CALENDAR_PLUS_SYSTEM_CLOCK_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CALENDAR_PLUS_TYPE_SYSTEM_CLOCK \
    (calendar_plus_system_clock_get_type())

G_DECLARE_FINAL_TYPE(CalendarPlusSystemClock,
                     calendar_plus_system_clock,
                     CALENDAR_PLUS,
                     SYSTEM_CLOCK,
                     GObject)

/**
 * calendar_plus_system_clock_new:
 *
 * Creates the native timing engine used by the Cinnamon presentation layer.
 *
 * Returns: (transfer full): a new multi-system clock
 */
CalendarPlusSystemClock *calendar_plus_system_clock_new(void);

/**
 * calendar_plus_system_clock_start:
 * @self: a multi-system clock
 * @mode: a native clock-mode settings value
 * @show_seconds: whether to show the mode's finer practical unit
 * @vertical: whether fields should be stacked for a vertical panel
 * @longitude: degrees east of Greenwich
 *
 * Starts the native clock or atomically applies changed display options. The
 * clock emits #CalendarPlusSystemClock::tick only when its visible value can
 * change.
 */
void calendar_plus_system_clock_start(CalendarPlusSystemClock *self,
                                      const gchar *mode,
                                      gboolean show_seconds,
                                      gboolean vertical,
                                      gdouble longitude);

/**
 * calendar_plus_system_clock_start_at_location:
 * @self: a multi-system clock
 * @mode: a native clock-mode settings value
 * @show_seconds: whether to show the mode's finer practical unit
 * @vertical: whether fields should be stacked for a vertical panel
 * @latitude: degrees north
 * @longitude: degrees east of Greenwich
 *
 * Location-explicit start/reconfigure entry point. Use this for historical
 * solar-origin and seasonal clocks. The legacy start method remains an
 * ABI-compatible equatorial wrapper.
 */
void calendar_plus_system_clock_start_at_location(
    CalendarPlusSystemClock *self,
    const gchar *mode,
    gboolean show_seconds,
    gboolean vertical,
    gdouble latitude,
    gdouble longitude);

/**
 * calendar_plus_system_clock_stop:
 * @self: a multi-system clock
 *
 * Stops the native timer. Calling this more than once is harmless.
 */
void calendar_plus_system_clock_stop(CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_time:
 * @self: a multi-system clock
 *
 * Formats the current instant using this clock's active time system.
 *
 * Returns: (transfer full): the newly allocated clock string
 */
gchar *calendar_plus_system_clock_get_time(CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_is_running:
 * @self: a multi-system clock
 *
 * Returns: %TRUE while the native timer is active
 */
gboolean calendar_plus_system_clock_is_running(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_policy:
 * @self: a multi-system clock
 *
 * Returns one coherent snapshot of the effective temporal presentation state.
 * The tuple fields are clock mode, calendar, show-seconds, location-configured,
 * latitude, longitude, authority identifier and provider-available flag.
 *
 * The authority identifier is "mint-cinnamon" unless an installed temporal-v3
 * provider and a valid persisted policy are both present; only then is it
 * "infiltrator-system-settings". The provider-available flag therefore
 * distinguishes an installed-but-not-yet-configured System Settings package
 * from an active richer policy.
 *
 * Returns: (transfer full): a #GVariant tuple (ssbbddsb)
 */
GVariant *calendar_plus_system_clock_get_system_policy(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_mode:
 * @self: a multi-system clock
 *
 * Resolves System Settings' installed temporal policy to a Calendar clock mode.
 * If System Settings itself is absent, or its policy is missing/invalid,
 * Calendar resolves to "standard" and leaves Cinnamon's native locale and
 * 12/24-hour preference authoritative. A stale policy file from an uninstalled
 * System Settings package is deliberately ignored.
 *
 * Returns: (transfer full): a newly allocated Calendar clock-mode identifier
 */
gchar *calendar_plus_system_clock_get_system_mode(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_calendar:
 * @self: a multi-system clock
 *
 * If System Settings is absent, or its Infiltrator policy is missing/invalid,
 * this resolves to Gregorian, matching the stock Cinnamon month view.
 *
 * Returns: (transfer full): the effective system-wide calendar identifier
 */
gchar *calendar_plus_system_clock_get_system_calendar(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_show_seconds:
 * @self: a multi-system clock
 *
 * A valid policy from installed System Settings is authoritative. Otherwise
 * this mirrors Cinnamon's clock-show-seconds preference so Calendar remains a
 * drop-in Mint replacement when System Settings is not installed.
 *
 * Returns: whether the effective system policy requests seconds or a finer unit
 */
gboolean calendar_plus_system_clock_get_system_show_seconds(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_location_configured:
 * @self: a multi-system clock
 *
 * Returns: whether a system-wide geographic location is configured
 */
gboolean calendar_plus_system_clock_get_system_location_configured(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_latitude:
 * @self: a multi-system clock
 *
 * Returns: system-wide latitude in degrees north
 */
gdouble calendar_plus_system_clock_get_system_latitude(
    CalendarPlusSystemClock *self);

/**
 * calendar_plus_system_clock_get_system_longitude:
 * @self: a multi-system clock
 *
 * Returns: system-wide longitude in degrees east
 */
gdouble calendar_plus_system_clock_get_system_longitude(
    CalendarPlusSystemClock *self);

G_END_DECLS

#endif
