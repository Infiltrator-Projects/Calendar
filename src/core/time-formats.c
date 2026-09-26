// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native clock-provider registry, shared timing primitives and public dispatch.
 * Common owns each mode's stable identifier, capability metadata and rendered
 * clock text. Calendar retains only boundary scheduling needed to wake the
 * Cinnamon panel at the exact instant that Common's visible value can change.
 */

#include "time-formats.h"
#include "time-formats-internal.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/timing.h>
#include <math.h>
#include <string.h>

typedef guint (*TimeDelayFunc)(gint64, gint, gboolean, gdouble, gdouble);

guint delay_babylonian_ancient_provider(gint64, gint, gboolean, gdouble, gdouble);
guint delay_nuremberg_solar_provider(gint64, gint, gboolean, gdouble, gdouble);
guint delay_japanese_temporal_early_provider(gint64, gint, gboolean, gdouble, gdouble);

typedef struct
{
    guint abi_version;
    CalendarPlusTimeMode mode;
    /*
     * Stable binding key into Common's authoritative temporal catalogue.
     * Common renders the clock; Calendar owns only next-boundary scheduling.
     */
    const gchar *common_id;
    TimeDelayFunc next_tick;
} TimeProvider;

enum { CALENDAR_PLUS_TIME_PROVIDER_ABI = 1 };

guint
calendar_plus_time_fractional_day_tick(gint64 microseconds_of_day, guint ticks_per_day)
{
    uint64_t tick = 0;
    g_return_val_if_fail(microseconds_of_day >= 0, 0);
    g_return_val_if_fail(ticks_per_day > 0, 0);
    g_return_val_if_fail(infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                                        (uint64_t)MICROSECONDS_PER_DAY,
                                                        (uint64_t)ticks_per_day,
                                                        &tick, NULL), 0);
    return (guint)tick;
}


static guint
delay_seconds_to_milliseconds(long double seconds)
{
    uint64_t milliseconds = 0;
    if (!infiltratr_seconds_to_milliseconds_ceil(seconds, &milliseconds))
        return 1;
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

guint
calendar_plus_time_delay_continuous_microseconds_to_milliseconds(long double microseconds)
{
    return delay_seconds_to_milliseconds(microseconds / (long double)G_USEC_PER_SEC);
}

static guint
delay_exact_microseconds_to_milliseconds(uint64_t microseconds)
{
    uint64_t milliseconds = 0;
    if (!infiltratr_microseconds_to_milliseconds_ceil(microseconds, &milliseconds))
        return 1;
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

guint
calendar_plus_time_delay_for_integer_period(gint64 position_microseconds, gint64 period_microseconds)
{
    uint64_t remaining = 0;
    if (!infiltratr_i64_period_remaining(position_microseconds, period_microseconds, &remaining))
        return 1;
    return delay_exact_microseconds_to_milliseconds(remaining);
}

guint
calendar_plus_time_delay_for_day_ticks(gint64 microseconds_of_day, guint ticks_per_day)
{
    uint64_t remaining = 0;
    if (microseconds_of_day < 0 || ticks_per_day == 0 ||
        !infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                        (uint64_t)MICROSECONDS_PER_DAY,
                                        (uint64_t)ticks_per_day, NULL, &remaining))
        return 1;
    return delay_exact_microseconds_to_milliseconds(remaining);
}

guint
calendar_plus_time_delay_for_clock_seconds(long double clock_seconds,
                                           long double clock_rate,
                                           gboolean show_seconds)
{
    const long double unit = show_seconds ? 1.0L : 60.0L;
    long double remaining = 0.0L;
    if (!isfinite(clock_rate) || clock_rate <= 0.0L ||
        !infiltratr_period_remaining(clock_seconds, unit, &remaining))
        return 1;
    return delay_seconds_to_milliseconds(remaining / clock_rate);
}

#define TIME_PROVIDER(mode_, token_, id_) \
    [CALENDAR_PLUS_TIME_MODE_##mode_] = { CALENDAR_PLUS_TIME_PROVIDER_ABI, \
        CALENDAR_PLUS_TIME_MODE_##mode_, id_, delay_##token_##_provider }

static const TimeProvider time_providers[] = {
    TIME_PROVIDER(DECIMAL, decimal, "decimal"),
    TIME_PROVIDER(INTERNET, internet, "internet"),
    TIME_PROVIDER(UNIX, unix, "unix"),
    TIME_PROVIDER(HEXADECIMAL, hexadecimal, "hexadecimal"),
    TIME_PROVIDER(BINARY, binary, "binary"),
    TIME_PROVIDER(SIDEREAL, sidereal, "sidereal"),
    TIME_PROVIDER(SOLAR, solar, "solar"),
    TIME_PROVIDER(JULIAN, julian, "julian"),
    TIME_PROVIDER(MEAN_SOLAR, mean_solar, "mean-solar"),
    TIME_PROVIDER(MODIFIED_JULIAN, modified_julian, "modified-julian"),
    TIME_PROVIDER(CHINESE, chinese, "chinese-time"),
    TIME_PROVIDER(ROMAN_TEMPORAL, roman_temporal, "roman-temporal"),
    TIME_PROVIDER(JAPANESE_TEMPORAL, japanese_temporal, "japanese-temporal"),
    TIME_PROVIDER(ITALIAN_HOURS, italian_hours, "italian-hours"),
    TIME_PROVIDER(BABYLONIAN_HOURS, babylonian_hours, "babylonian-hours"),
    TIME_PROVIDER(INDIAN_GHATI, indian_ghati, "indian-ghati"),
    TIME_PROVIDER(CHINESE_KE, chinese_ke, "chinese-ke"),
    TIME_PROVIDER(NUREMBERG_HOURS, nuremberg_hours, "nuremberg-hours"),
    TIME_PROVIDER(BABYLONIAN_ANCIENT, babylonian_ancient, "babylonian-ancient"),
    TIME_PROVIDER(NUREMBERG_SOLAR, nuremberg_solar, "nuremberg-solar"),
    TIME_PROVIDER(JAPANESE_TEMPORAL_EARLY, japanese_temporal_early, "japanese-temporal-early")
};

G_STATIC_ASSERT(G_N_ELEMENTS(time_providers) == CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL_EARLY + 1);

static const InfiltratrTemporalClockModeInfo *
common_mode_info_for_provider(const TimeProvider *provider)
{
    return provider != NULL && provider->common_id != NULL
        ? infiltratr_temporal_clock_mode_find(provider->common_id)
        : NULL;
}

static const TimeProvider *
time_provider_for_mode(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider;

    if (mode < CALENDAR_PLUS_TIME_MODE_DECIMAL ||
        mode > CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL_EARLY)
        return NULL;

    provider = &time_providers[mode];
    return provider->abi_version == CALENDAR_PLUS_TIME_PROVIDER_ABI &&
           common_mode_info_for_provider(provider) != NULL
        ? provider : NULL;
}

CalendarPlusTimeMode
calendar_plus_time_mode_from_string(const gchar *mode)
{
    const InfiltratrTemporalClockModeInfo *info;
    gsize index;

    if (mode == NULL)
        return CALENDAR_PLUS_TIME_MODE_INVALID;

    info = infiltratr_temporal_clock_mode_find(mode);
    if (info == NULL)
        return CALENDAR_PLUS_TIME_MODE_INVALID;

    for (index = 1; index < G_N_ELEMENTS(time_providers); index++)
        if (time_providers[index].common_id != NULL &&
            infiltratr_string_equal(info->id, time_providers[index].common_id))
            return time_providers[index].mode;

    return CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *calendar_plus_time_mode_get_id(CalendarPlusTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL ? info->id : NULL;
}

gsize calendar_plus_time_mode_get_count(void)
{
    return G_N_ELEMENTS(time_providers) - 1;
}

CalendarPlusTimeMode calendar_plus_time_mode_get_at(gsize index)
{
    return index < calendar_plus_time_mode_get_count()
        ? time_providers[index + 1].mode
        : CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *calendar_plus_time_mode_get_name(CalendarPlusTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL ? info->name : NULL;
}

gboolean calendar_plus_time_mode_supports_seconds(CalendarPlusTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL && info->supports_seconds;
}

gboolean calendar_plus_time_mode_requires_longitude(CalendarPlusTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL && info->requires_longitude;
}

gboolean calendar_plus_time_mode_requires_latitude(CalendarPlusTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL && info->requires_latitude;
}

static gboolean
location_is_valid_for_provider(const TimeProvider *provider,
                               gdouble latitude,
                               gdouble longitude)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(provider);

    return info != NULL &&
           (!info->requires_latitude || isfinite(latitude)) &&
           (!info->requires_longitude || isfinite(longitude));
}

gchar *
calendar_plus_format_time_at_location(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gboolean vertical, gdouble latitude, gdouble longitude)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    gchar text[256];

    if (!location_is_valid_for_provider(provider, latitude, longitude))
        return g_strdup("");
    if (!infiltratr_temporal_format_clock_mode(
            provider->common_id,
            unix_microseconds,
            utc_offset_seconds,
            show_seconds,
            vertical,
            TRUE,
            safe_latitude,
            safe_longitude,
            text,
            sizeof(text),
            NULL))
    {
        return g_strdup("");
    }
    return g_strdup(text);
}

gchar *
calendar_plus_format_time(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                          gint utc_offset_seconds, gboolean show_seconds,
                          gboolean vertical, gdouble longitude)
{
    return calendar_plus_format_time_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, vertical, 0.0, longitude);
}

guint
calendar_plus_time_delay_to_next_tick_at_location(CalendarPlusTimeMode mode,
    gint64 unix_microseconds, gint utc_offset_seconds, gboolean show_seconds,
    gdouble latitude, gdouble longitude)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    if (provider == NULL)
        return 1000;
    if (!location_is_valid_for_provider(provider, latitude, longitude))
        return 3600000U;
    return provider->next_tick(unix_microseconds, utc_offset_seconds, show_seconds,
                               safe_latitude, safe_longitude);
}

guint
calendar_plus_time_delay_to_next_tick(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gdouble longitude)
{
    return calendar_plus_time_delay_to_next_tick_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, 0.0, longitude);
}

gchar *
calendar_plus_replace_time(const gchar *label, const gchar *conventional_time,
                           const gchar *replacement_time)
{
    const gchar *match;
    gsize prefix_length;
    g_return_val_if_fail(label != NULL, NULL);
    g_return_val_if_fail(replacement_time != NULL, NULL);
    if (conventional_time == NULL || conventional_time[0] == '\0')
        return NULL;
    match = g_strstr_len(label, -1, conventional_time);
    if (match == NULL)
        return NULL;
    prefix_length = (gsize)(match - label);
    return g_strdup_printf("%.*s%s%s", (gint)prefix_length, label,
                           replacement_time, match + strlen(conventional_time));
}
