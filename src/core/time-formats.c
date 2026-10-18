// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native clock-provider registry, shared timing primitives and public dispatch.
 * Common owns stable identifiers and capability metadata. Eighteen executable
 * clock resources (sixteen alternative clocks, two explicit conventional
 * modes) own their notation and next-visible-tick scheduling. The 21-mode
 * alternative catalogue remains stable; OS-following standard mode retains
 * Cinnamon's locale fallback.
 */

#include "time-formats.h"
#include "time-formats-internal.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>
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
    CalendarTimeMode mode;
    /*
     * Stable binding key into Common's authoritative temporal catalogue.
     * Common renders the clock; Calendar owns only next-boundary scheduling.
     */
    const gchar *common_id;
    TimeDelayFunc next_tick;
} TimeProvider;

enum { CALENDAR_TIME_PROVIDER_ABI = 1 };

guint
calendar_time_fractional_day_tick(gint64 microseconds_of_day, guint ticks_per_day)
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
calendar_time_delay_continuous_microseconds_to_milliseconds(long double microseconds)
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
calendar_time_delay_for_integer_period(gint64 position_microseconds, gint64 period_microseconds)
{
    uint64_t remaining = 0;
    if (!infiltratr_i64_period_remaining(position_microseconds, period_microseconds, &remaining))
        return 1;
    return delay_exact_microseconds_to_milliseconds(remaining);
}

guint
calendar_time_delay_for_day_ticks(gint64 microseconds_of_day, guint ticks_per_day)
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
calendar_time_delay_for_clock_seconds(long double clock_seconds,
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
    [CALENDAR_TIME_MODE_##mode_] = { CALENDAR_TIME_PROVIDER_ABI, \
        CALENDAR_TIME_MODE_##mode_, id_, delay_##token_##_provider }
#define NATIVE_TIME_PROVIDER(mode_, id_) \
    [CALENDAR_TIME_MODE_##mode_] = { CALENDAR_TIME_PROVIDER_ABI, \
        CALENDAR_TIME_MODE_##mode_, id_, NULL }

static const TimeProvider time_providers[] = {
    NATIVE_TIME_PROVIDER(DECIMAL, "decimal"),
    NATIVE_TIME_PROVIDER(INTERNET, "internet"),
    NATIVE_TIME_PROVIDER(UNIX, "unix"),
    NATIVE_TIME_PROVIDER(HEXADECIMAL, "hexadecimal"),
    NATIVE_TIME_PROVIDER(BINARY, "binary"),
    NATIVE_TIME_PROVIDER(SIDEREAL, "sidereal"),
    NATIVE_TIME_PROVIDER(SOLAR, "solar"),
    NATIVE_TIME_PROVIDER(JULIAN, "julian"),
    NATIVE_TIME_PROVIDER(MEAN_SOLAR, "mean-solar"),
    NATIVE_TIME_PROVIDER(MODIFIED_JULIAN, "modified-julian"),
    NATIVE_TIME_PROVIDER(CHINESE, "chinese-time"),
    TIME_PROVIDER(ROMAN_TEMPORAL, roman_temporal, "roman-temporal"),
    TIME_PROVIDER(JAPANESE_TEMPORAL, japanese_temporal, "japanese-temporal"),
    NATIVE_TIME_PROVIDER(ITALIAN_HOURS, "italian-hours"),
    NATIVE_TIME_PROVIDER(BABYLONIAN_HOURS, "babylonian-hours"),
    NATIVE_TIME_PROVIDER(INDIAN_GHATI, "indian-ghati"),
    NATIVE_TIME_PROVIDER(CHINESE_KE, "chinese-ke"),
    TIME_PROVIDER(NUREMBERG_HOURS, nuremberg_hours, "nuremberg-hours"),
    NATIVE_TIME_PROVIDER(BABYLONIAN_ANCIENT, "babylonian-ancient"),
    TIME_PROVIDER(NUREMBERG_SOLAR, nuremberg_solar, "nuremberg-solar"),
    TIME_PROVIDER(JAPANESE_TEMPORAL_EARLY, japanese_temporal_early, "japanese-temporal-early"),
    NATIVE_TIME_PROVIDER(STANDARD_12, "standard-12"),
    NATIVE_TIME_PROVIDER(STANDARD_24, "standard-24")
};

G_STATIC_ASSERT(G_N_ELEMENTS(time_providers) == CALENDAR_TIME_MODE_STANDARD_24 + 1);

/*
 * These eighteen resource-backed clocks have no Calendar-owned implementation
 * or fallback at runtime. OS-following standard uses Cinnamon unchanged.
 * Common validates and calls the independent system resources.
 * A test build may override only the scan directory, never the clock ABI.
 */
#ifdef CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY
#define CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY_OVERRIDE 1
#else
#define CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY \
    INFILTRATR_TEMPORAL_NATIVE_CLOCK_DIRECTORY
#endif

static InfiltratrTemporalClockModule *
    clock_modules[CALENDAR_TIME_MODE_STANDARD_24 + 1];
static GOnce clock_modules_once = G_ONCE_INIT;

static gboolean
is_native_clock(CalendarTimeMode mode)
{
    return mode == CALENDAR_TIME_MODE_BABYLONIAN_ANCIENT ||
           mode == CALENDAR_TIME_MODE_INDIAN_GHATI ||
           mode == CALENDAR_TIME_MODE_ITALIAN_HOURS ||
           mode == CALENDAR_TIME_MODE_BABYLONIAN_HOURS ||
           mode == CALENDAR_TIME_MODE_SOLAR ||
           mode == CALENDAR_TIME_MODE_CHINESE ||
           mode == CALENDAR_TIME_MODE_SIDEREAL ||
           mode == CALENDAR_TIME_MODE_DECIMAL ||
           mode == CALENDAR_TIME_MODE_INTERNET ||
           mode == CALENDAR_TIME_MODE_UNIX ||
           mode == CALENDAR_TIME_MODE_HEXADECIMAL ||
           mode == CALENDAR_TIME_MODE_BINARY ||
           mode == CALENDAR_TIME_MODE_JULIAN ||
           mode == CALENDAR_TIME_MODE_MODIFIED_JULIAN ||
           mode == CALENDAR_TIME_MODE_CHINESE_KE ||
           mode == CALENDAR_TIME_MODE_MEAN_SOLAR ||
           mode == CALENDAR_TIME_MODE_STANDARD_12 ||
           mode == CALENDAR_TIME_MODE_STANDARD_24;
}

static gpointer
initialise_clock_resources(gpointer unused)
{
    static const CalendarTimeMode selected[] = {
        CALENDAR_TIME_MODE_BABYLONIAN_ANCIENT,
        CALENDAR_TIME_MODE_INDIAN_GHATI,
        CALENDAR_TIME_MODE_ITALIAN_HOURS,
        CALENDAR_TIME_MODE_BABYLONIAN_HOURS,
        CALENDAR_TIME_MODE_SOLAR,
        CALENDAR_TIME_MODE_CHINESE,
        CALENDAR_TIME_MODE_SIDEREAL,
        CALENDAR_TIME_MODE_DECIMAL,
        CALENDAR_TIME_MODE_INTERNET,
        CALENDAR_TIME_MODE_UNIX,
        CALENDAR_TIME_MODE_HEXADECIMAL,
        CALENDAR_TIME_MODE_BINARY,
        CALENDAR_TIME_MODE_JULIAN,
        CALENDAR_TIME_MODE_MODIFIED_JULIAN,
        CALENDAR_TIME_MODE_CHINESE_KE,
        CALENDAR_TIME_MODE_MEAN_SOLAR,
        CALENDAR_TIME_MODE_STANDARD_12,
        CALENDAR_TIME_MODE_STANDARD_24
    };
    gsize index;

    (void)unused;
    for (index = 0; index < G_N_ELEMENTS(selected); index++)
    {
        const CalendarTimeMode mode = selected[index];
        const gchar *id = time_providers[mode].common_id;
#ifdef CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY_OVERRIDE
        g_autofree gchar *filename = g_strconcat(id, ".so", NULL);
        g_autofree gchar *path = g_build_filename(
            CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY, filename, NULL);
        clock_modules[mode] = infiltratr_temporal_clock_module_open(path);
#else
        clock_modules[mode] = infiltratr_temporal_clock_module_open_id(id);
#endif
        if (clock_modules[mode] != NULL)
        {
            const InfiltratrTemporalResourceMetadata *info =
                infiltratr_temporal_clock_module_metadata(clock_modules[mode]);
            if (info == NULL || !infiltratr_string_equal(info->id, id))
            {
                infiltratr_temporal_clock_module_free(clock_modules[mode]);
                clock_modules[mode] = NULL;
            }
        }
    }
    return &clock_modules_once;
}

static const InfiltratrTemporalClockModule *
clock_resource_for_mode(CalendarTimeMode mode)
{
    if (!is_native_clock(mode))
        return NULL;
    g_once(&clock_modules_once, initialise_clock_resources, NULL);
    return clock_modules[mode];
}


static const InfiltratrTemporalClockModeInfo *
common_mode_info_for_provider(const TimeProvider *provider)
{
    return provider != NULL && provider->common_id != NULL
        ? infiltratr_temporal_clock_mode_find(provider->common_id)
        : NULL;
}

static const TimeProvider *
time_provider_for_mode(CalendarTimeMode mode)
{
    const TimeProvider *provider;

    if (mode < CALENDAR_TIME_MODE_DECIMAL ||
        mode > CALENDAR_TIME_MODE_STANDARD_24)
        return NULL;

    provider = &time_providers[mode];
    return provider->abi_version == CALENDAR_TIME_PROVIDER_ABI &&
           common_mode_info_for_provider(provider) != NULL &&
           (!is_native_clock(mode) || clock_resource_for_mode(mode) != NULL)
        ? provider : NULL;
}

CalendarTimeMode
calendar_time_mode_from_string(const gchar *mode)
{
    const InfiltratrTemporalClockModeInfo *info;
    gsize index;

    if (mode == NULL)
        return CALENDAR_TIME_MODE_INVALID;

    info = infiltratr_temporal_clock_mode_find(mode);
    if (info == NULL)
        return CALENDAR_TIME_MODE_INVALID;

    for (index = 1; index < G_N_ELEMENTS(time_providers); index++)
        if (time_providers[index].common_id != NULL &&
            infiltratr_string_equal(info->id, time_providers[index].common_id))
            return time_providers[index].mode;

    return CALENDAR_TIME_MODE_INVALID;
}

const gchar *calendar_time_mode_get_id(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL ? info->id : NULL;
}

gsize calendar_time_mode_get_count(void)
{
    /* Explicit conventional modes are native, but not alternative systems. */
    return CALENDAR_TIME_MODE_JAPANESE_TEMPORAL_EARLY;
}

CalendarTimeMode calendar_time_mode_get_at(gsize index)
{
    return index < calendar_time_mode_get_count()
        ? time_providers[index + 1].mode
        : CALENDAR_TIME_MODE_INVALID;
}

const gchar *calendar_time_mode_get_name(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL ? info->name : NULL;
}

gboolean calendar_time_mode_supports_seconds(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL && info->supports_seconds;
}

gboolean calendar_time_mode_requires_longitude(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        common_mode_info_for_provider(time_provider_for_mode(mode));
    return info != NULL && info->requires_longitude;
}

gboolean calendar_time_mode_requires_latitude(CalendarTimeMode mode)
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
           (!info->requires_latitude ||
            (isfinite(latitude) && latitude >= -90.0 && latitude <= 90.0)) &&
           (!info->requires_longitude ||
            (isfinite(longitude) &&
             longitude >= -180.0 && longitude <= 180.0));
}

gchar *
calendar_format_time_at_location(CalendarTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gboolean vertical, gdouble latitude, gdouble longitude)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    gchar text[256];

    if (!location_is_valid_for_provider(provider, latitude, longitude))
        return g_strdup("");
    if (is_native_clock(mode))
    {
        const InfiltratrTemporalClockModule *resource =
            clock_resource_for_mode(mode);
        if (resource == NULL || !infiltratr_temporal_clock_format(
                resource, g_get_language_names()[0], unix_microseconds,
                utc_offset_seconds, show_seconds, vertical,
                safe_latitude, safe_longitude, text, sizeof(text), NULL))
            return g_strdup("");
        return g_strdup(text);
    }
    if (!infiltratr_temporal_format_clock_mode_localized(
            g_get_language_names()[0], provider->common_id,
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
calendar_format_time(CalendarTimeMode mode, gint64 unix_microseconds,
                          gint utc_offset_seconds, gboolean show_seconds,
                          gboolean vertical, gdouble longitude)
{
    return calendar_format_time_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, vertical, 0.0, longitude);
}

guint
calendar_time_delay_to_next_tick_at_location(CalendarTimeMode mode,
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
    if (is_native_clock(mode))
    {
        const InfiltratrTemporalClockModule *resource =
            clock_resource_for_mode(mode);
        uint32_t delay = 0U;
        return resource != NULL && infiltratr_temporal_clock_next_tick(
            resource, unix_microseconds, utc_offset_seconds, show_seconds,
            safe_latitude, safe_longitude, &delay) ? delay : 1000U;
    }
    return provider->next_tick(unix_microseconds, utc_offset_seconds, show_seconds,
                               safe_latitude, safe_longitude);
}

guint
calendar_time_delay_to_next_tick(CalendarTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gdouble longitude)
{
    return calendar_time_delay_to_next_tick_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, 0.0, longitude);
}

gchar *
calendar_replace_time(const gchar *label, const gchar *conventional_time,
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
