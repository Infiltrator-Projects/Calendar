// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_TIME_FORMATS_H
#define CALENDAR_TIME_FORMATS_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * CalendarTimeMode:
 * @CALENDAR_TIME_MODE_INVALID: unknown or conventional mode
 * @CALENDAR_TIME_MODE_DECIMAL: local ten-hour decimal day
 * @CALENDAR_TIME_MODE_INTERNET: Swatch-style Internet Time
 * @CALENDAR_TIME_MODE_UNIX: seconds since the Unix epoch
 * @CALENDAR_TIME_MODE_HEXADECIMAL: local day divided into 65536 ticks
 * @CALENDAR_TIME_MODE_BINARY: conventional local fields in binary
 * @CALENDAR_TIME_MODE_SIDEREAL: local mean sidereal time
 * @CALENDAR_TIME_MODE_SOLAR: local apparent solar time
 * @CALENDAR_TIME_MODE_JULIAN: astronomical Julian Date
 * @CALENDAR_TIME_MODE_MEAN_SOLAR: local mean solar time
 * @CALENDAR_TIME_MODE_MODIFIED_JULIAN: Modified Julian Date
 * @CALENDAR_TIME_MODE_CHINESE: traditional Chinese double-hours
 * @CALENDAR_TIME_MODE_ROMAN_TEMPORAL: Roman seasonal daylight hours and night watches
 * @CALENDAR_TIME_MODE_JAPANESE_TEMPORAL: Edo Japanese seasonal time
 * @CALENDAR_TIME_MODE_ITALIAN_HOURS: equal hours elapsed since sunset
 * @CALENDAR_TIME_MODE_BABYLONIAN_HOURS: equal hours elapsed since sunrise
 * @CALENDAR_TIME_MODE_INDIAN_GHATI: Indian ghaṭī/vighaṭī count from sunrise
 * @CALENDAR_TIME_MODE_CHINESE_KE: Chinese hundred-kè civil-day division
 * @CALENDAR_TIME_MODE_NUREMBERG_HOURS: historical Nürnberg Great Clock with fixed Wendetage
 * @CALENDAR_TIME_MODE_BABYLONIAN_ANCIENT: ancient Babylonian bēru/UŠ from sunset
 * @CALENDAR_TIME_MODE_NUREMBERG_SOLAR: location-aware Nuremberg-style solar reconstruction
 * @CALENDAR_TIME_MODE_JAPANESE_TEMPORAL_EARLY: early-Edo sunrise/sunset unequal hours
 *
 * Native time systems supported by Calendar. Conventional 12-hour and
 * 24-hour modes remain with CinnamonDesktop.WallClock for locale handling.
 */
typedef enum
{
    CALENDAR_TIME_MODE_INVALID = 0,
    CALENDAR_TIME_MODE_DECIMAL,
    CALENDAR_TIME_MODE_INTERNET,
    CALENDAR_TIME_MODE_UNIX,
    CALENDAR_TIME_MODE_HEXADECIMAL,
    CALENDAR_TIME_MODE_BINARY,
    CALENDAR_TIME_MODE_SIDEREAL,
    CALENDAR_TIME_MODE_SOLAR,
    CALENDAR_TIME_MODE_JULIAN,
    CALENDAR_TIME_MODE_MEAN_SOLAR,
    CALENDAR_TIME_MODE_MODIFIED_JULIAN,
    CALENDAR_TIME_MODE_CHINESE,
    CALENDAR_TIME_MODE_ROMAN_TEMPORAL,
    CALENDAR_TIME_MODE_JAPANESE_TEMPORAL,
    CALENDAR_TIME_MODE_ITALIAN_HOURS,
    CALENDAR_TIME_MODE_BABYLONIAN_HOURS,
    CALENDAR_TIME_MODE_INDIAN_GHATI,
    CALENDAR_TIME_MODE_CHINESE_KE,
    CALENDAR_TIME_MODE_NUREMBERG_HOURS,
    CALENDAR_TIME_MODE_BABYLONIAN_ANCIENT,
    CALENDAR_TIME_MODE_NUREMBERG_SOLAR,
    CALENDAR_TIME_MODE_JAPANESE_TEMPORAL_EARLY
} CalendarTimeMode;

/**
 * calendar_time_mode_from_string:
 * @mode: a settings-schema clock-mode value
 *
 * Converts a stable settings value to its native enum.
 *
 * Returns: the matching mode, or %CALENDAR_TIME_MODE_INVALID
 */
CalendarTimeMode calendar_time_mode_from_string(const gchar *mode);

/**
 * calendar_time_mode_get_id:
 * @mode: a native clock mode
 *
 * Returns: (transfer none) (nullable): the stable settings identifier, or
 *   %NULL for an invalid mode
 */
const gchar *calendar_time_mode_get_id(CalendarTimeMode mode);

/**
 * calendar_time_mode_get_count:
 *
 * Returns: number of registered native time providers
 */
gsize calendar_time_mode_get_count(void);

/**
 * calendar_time_mode_get_at:
 * @index: zero-based provider catalogue index
 * Returns: the registered mode, or %CALENDAR_TIME_MODE_INVALID
 */
CalendarTimeMode calendar_time_mode_get_at(gsize index);

/**
 * calendar_time_mode_get_name:
 * @mode: a native clock mode
 * Returns: (transfer none) (nullable): stable English presentation label
 */
const gchar *calendar_time_mode_get_name(CalendarTimeMode mode);

/**
 * calendar_time_mode_supports_seconds:
 * @mode: a native clock mode
 * Returns: whether the per-applet seconds switch changes this mode's presentation precision
 */
gboolean calendar_time_mode_supports_seconds(CalendarTimeMode mode);

/**
 * calendar_time_mode_requires_longitude:
 * @mode: a native clock mode
 * Returns: whether the result depends on configured astronomical longitude
 */
gboolean calendar_time_mode_requires_longitude(CalendarTimeMode mode);

/**
 * calendar_time_mode_requires_latitude:
 * @mode: a native clock mode
 * Returns: whether the result depends on configured astronomical latitude
 */
gboolean calendar_time_mode_requires_latitude(CalendarTimeMode mode);

/**
 * calendar_format_time:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since 1970-01-01 00:00:00 UTC
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode should show its finer practical unit
 * @vertical: whether clock fields should be stacked for a vertical panel
 * @longitude: degrees east of Greenwich in the range -180 through 180
 *
 * Deterministically formats a native time system. Legacy longitude-only entry
 * point retained for ABI compatibility; location-dependent modes should use
 * calendar_format_time_at_location().
 * Returns: (transfer full): newly allocated panel-clock string
 */
gchar *calendar_format_time(CalendarTimeMode mode,
                                 gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble longitude);

/**
 * calendar_format_time_at_location:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since 1970-01-01 00:00:00 UTC
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode should show its finer practical unit
 * @vertical: whether clock fields should be stacked for a vertical panel
 * @latitude: degrees north in the range -90 through 90
 * @longitude: degrees east of Greenwich in the range -180 through 180
 *
 * Authoritative location-explicit formatter for historical solar-origin and
 * seasonal clocks. A non-finite or out-of-range coordinate required by @mode
 * is invalid and returns an empty string rather than silently substituting or
 * normalising a different geographic location.
 * Returns: (transfer full): newly allocated panel-clock string
 */
gchar *calendar_format_time_at_location(
    CalendarTimeMode mode,
    gint64 unix_microseconds,
    gint utc_offset_seconds,
    gboolean show_seconds,
    gboolean vertical,
    gdouble latitude,
    gdouble longitude);

/**
 * calendar_time_delay_to_next_tick:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since 1970-01-01 00:00:00 UTC
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode shows its finer practical unit
 * @longitude: degrees east of Greenwich in the range -180 through 180
 * Returns: milliseconds until the next display boundary, at least one
 */
guint calendar_time_delay_to_next_tick(CalendarTimeMode mode,
                                             gint64 unix_microseconds,
                                             gint utc_offset_seconds,
                                             gboolean show_seconds,
                                             gdouble longitude);

/**
 * calendar_time_delay_to_next_tick_at_location:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since the Unix epoch
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode shows its finer practical unit
 * @latitude: degrees north in the range -90 through 90
 * @longitude: degrees east of Greenwich in the range -180 through 180
 *
 * A non-finite or out-of-range coordinate required by @mode is unavailable
 * and returns a one-hour retry interval rather than scheduling from a
 * fabricated or normalised location.
 * Returns: milliseconds until the next visible boundary, at least one
 */
guint calendar_time_delay_to_next_tick_at_location(
    CalendarTimeMode mode,
    gint64 unix_microseconds,
    gint utc_offset_seconds,
    gboolean show_seconds,
    gdouble latitude,
    gdouble longitude);

/**
 * calendar_replace_time:
 * @label: a complete Cinnamon panel-clock label
 * @conventional_time: the conventional time substring to replace
 * @replacement_time: a native time-system replacement
 * Returns: (transfer full) (nullable): newly allocated replacement label, or %NULL
 */
gchar *calendar_replace_time(const gchar *label,
                                  const gchar *conventional_time,
                                  const gchar *replacement_time);

G_END_DECLS

#endif
