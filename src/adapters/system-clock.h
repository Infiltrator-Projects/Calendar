// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_SYSTEM_CLOCK_H
#define CALENDAR_SYSTEM_CLOCK_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CALENDAR_TYPE_SYSTEM_CLOCK \
    (calendar_system_clock_get_type())

G_DECLARE_FINAL_TYPE(CalendarSystemClock,
                     calendar_system_clock,
                     CALENDAR,
                     SYSTEM_CLOCK,
                     GObject)

/**
 * calendar_system_clock_new:
 *
 * Creates the native timing engine used by the Cinnamon presentation layer.
 *
 * Returns: (transfer full): a new multi-system clock
 */
CalendarSystemClock *calendar_system_clock_new(void);

/**
 * calendar_system_clock_start:
 * @self: a multi-system clock
 * @mode: a native clock-mode settings value
 * @show_seconds: whether to show the mode's finer practical unit
 * @vertical: whether fields should be stacked for a vertical panel
 * @longitude: degrees east of Greenwich
 *
 * Starts the native clock or atomically applies changed display options. The
 * clock emits #CalendarSystemClock::tick only when its visible value can
 * change.
 */
void calendar_system_clock_start(CalendarSystemClock *self,
                                      const gchar *mode,
                                      gboolean show_seconds,
                                      gboolean vertical,
                                      gdouble longitude);

/**
 * calendar_system_clock_start_at_location:
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
void calendar_system_clock_start_at_location(
    CalendarSystemClock *self,
    const gchar *mode,
    gboolean show_seconds,
    gboolean vertical,
    gdouble latitude,
    gdouble longitude);

/**
 * calendar_system_clock_stop:
 * @self: a multi-system clock
 *
 * Stops the native timer. Calling this more than once is harmless.
 */
void calendar_system_clock_stop(CalendarSystemClock *self);

/**
 * calendar_system_clock_get_time:
 * @self: a multi-system clock
 *
 * Formats the current instant using this clock's active time system.
 *
 * Returns: (transfer full): the newly allocated clock string
 */
gchar *calendar_system_clock_get_time(CalendarSystemClock *self);

/**
 * calendar_system_clock_is_running:
 * @self: a multi-system clock
 *
 * Returns: %TRUE while the native timer is active
 */
gboolean calendar_system_clock_is_running(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_policy:
 * @self: a multi-system clock
 *
 * Returns one coherent snapshot of the effective temporal presentation state.
 * The tuple fields are clock mode, calendar, show-seconds, location-configured,
 * latitude, longitude, authority identifier and provider-available flag.
 *
 * A valid persisted temporal-v3 policy remains authoritative after the System
 * Settings writer/UI package is removed. The provider-available flag therefore
 * describes availability of the richer temporal-v3 capability, whether from an
 * installed writer marker or from a valid persisted policy document.
 *
 * Returns: (transfer full): a #GVariant tuple (ssbbddsb)
 */
GVariant *calendar_system_clock_get_system_policy(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_mode:
 * @self: a multi-system clock
 *
 * Resolves the persisted system temporal policy to a Calendar clock mode. The
 * selected policy survives removal of the System Settings writer/UI package.
 * If no valid temporal-v3 policy exists, Calendar resolves to "standard" and
 * leaves Cinnamon's native locale and 12/24-hour preference authoritative.
 *
 * Returns: (transfer full): a newly allocated Calendar clock-mode identifier
 */
gchar *calendar_system_clock_get_system_mode(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_calendar:
 * @self: a multi-system clock
 *
 * Resolves the persisted system-wide calendar even when System Settings itself
 * is no longer installed. If no valid temporal-v3 policy exists, this resolves
 * to Gregorian, matching the stock Cinnamon month view.
 *
 * Returns: (transfer full): the effective system-wide calendar identifier
 */
gchar *calendar_system_clock_get_system_calendar(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_show_seconds:
 * @self: a multi-system clock
 *
 * A valid persisted temporal-v3 policy is authoritative. Otherwise this mirrors
 * Cinnamon's clock-show-seconds preference so Calendar remains a drop-in Mint
 * replacement when no Infiltrator temporal policy has been configured.
 *
 * Returns: whether the effective system policy requests seconds or a finer unit
 */
gboolean calendar_system_clock_get_system_show_seconds(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_location_configured:
 * @self: a multi-system clock
 *
 * Returns: whether a system-wide geographic location is configured
 */
gboolean calendar_system_clock_get_system_location_configured(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_latitude:
 * @self: a multi-system clock
 *
 * Returns: system-wide latitude in degrees north
 */
gdouble calendar_system_clock_get_system_latitude(
    CalendarSystemClock *self);

/**
 * calendar_system_clock_get_system_longitude:
 * @self: a multi-system clock
 *
 * Returns: system-wide longitude in degrees east
 */
gdouble calendar_system_clock_get_system_longitude(
    CalendarSystemClock *self);

G_END_DECLS

#endif
