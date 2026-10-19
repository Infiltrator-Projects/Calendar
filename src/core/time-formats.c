// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native clock-provider registry and Common-backed public dispatch.
 * Common owns stable IDs, validation and loading. Twenty-three independent
 * resource clocks (21 alternatives and two explicit conventional modes) own
 * rendering and next-visible-tick scheduling. OS-following standard time
 * remains Cinnamon's platform policy choice.
 */

#include "time-formats.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>
#include <infiltratr/temporal_runtime.h>
#include <math.h>
#include <string.h>

/* Persisted mode -> Common resource ID. The executable resource exclusively
 * owns clock calculation, presentation and next-visible-tick scheduling. */
typedef struct
{
    CalendarTimeMode mode;
    const gchar *common_id;
} TimeProvider;

#define NATIVE_TIME_PROVIDER(mode_, id_) \
    [CALENDAR_TIME_MODE_##mode_] = { CALENDAR_TIME_MODE_##mode_, id_ }

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
    NATIVE_TIME_PROVIDER(ROMAN_TEMPORAL, "roman-temporal"),
    NATIVE_TIME_PROVIDER(JAPANESE_TEMPORAL, "japanese-temporal"),
    NATIVE_TIME_PROVIDER(ITALIAN_HOURS, "italian-hours"),
    NATIVE_TIME_PROVIDER(BABYLONIAN_HOURS, "babylonian-hours"),
    NATIVE_TIME_PROVIDER(INDIAN_GHATI, "indian-ghati"),
    NATIVE_TIME_PROVIDER(CHINESE_KE, "chinese-ke"),
    NATIVE_TIME_PROVIDER(NUREMBERG_HOURS, "nuremberg-hours"),
    NATIVE_TIME_PROVIDER(BABYLONIAN_ANCIENT, "babylonian-ancient"),
    NATIVE_TIME_PROVIDER(NUREMBERG_SOLAR, "nuremberg-solar"),
    NATIVE_TIME_PROVIDER(JAPANESE_TEMPORAL_EARLY, "japanese-temporal-early"),
    NATIVE_TIME_PROVIDER(STANDARD_12, "standard-12"),
    NATIVE_TIME_PROVIDER(STANDARD_24, "standard-24")
};

G_STATIC_ASSERT(G_N_ELEMENTS(time_providers) == CALENDAR_TIME_MODE_STANDARD_24 + 1);

/*
 * These twenty-three resource-backed clocks have no Calendar-owned implementation
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
    return mode >= CALENDAR_TIME_MODE_DECIMAL &&
           mode <= CALENDAR_TIME_MODE_STANDARD_24;
}

#ifdef CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY_OVERRIDE
static bool
load_clock_test_resource(const InfiltratrTemporalResourceMetadata *metadata,
                         const char *module_path,
                         void *user_data)
{
    (void)user_data;
    if (metadata == NULL || module_path == NULL)
        return true;

    /* File names are not provider IDs. The overridden test directory must
     * exercise the same executable metadata/ABI validation as production,
     * including modules compiled under canonical human-readable filenames. */
    for (CalendarTimeMode mode = CALENDAR_TIME_MODE_DECIMAL;
         mode <= CALENDAR_TIME_MODE_STANDARD_24; mode++)
    {
        if (clock_modules[mode] != NULL ||
            !infiltratr_string_equal(time_providers[mode].common_id,
                                     metadata->id))
            continue;
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(module_path);
        if (module != NULL)
        {
            const InfiltratrTemporalResourceMetadata *actual =
                infiltratr_temporal_clock_module_metadata(module);
            if (actual != NULL &&
                infiltratr_string_equal(actual->id, metadata->id))
                clock_modules[mode] = module;
            else
                infiltratr_temporal_clock_module_free(module);
        }
        break;
    }
    return true;
}
#endif

static gpointer
initialise_clock_resources(gpointer unused)
{
    (void)unused;
#ifdef CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY_OVERRIDE
    size_t visited = 0U;
    (void)infiltratr_temporal_runtime_scan_directory(
        CALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY,
        INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
        load_clock_test_resource, NULL, &visited);
#else
    for (CalendarTimeMode mode = CALENDAR_TIME_MODE_DECIMAL;
         mode <= CALENDAR_TIME_MODE_STANDARD_24; mode++)
    {
        const gchar *id = time_providers[mode].common_id;
        clock_modules[mode] = infiltratr_temporal_clock_module_open_id(id);
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
#endif
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
    return common_mode_info_for_provider(provider) != NULL &&
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
            return clock_resource_for_mode(time_providers[index].mode) != NULL
                ? time_providers[index].mode : CALENDAR_TIME_MODE_INVALID;

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
    const InfiltratrTemporalClockModule *module = clock_resource_for_mode(mode);
    const InfiltratrTemporalResourceMetadata *metadata =
        module != NULL ? infiltratr_temporal_clock_module_metadata(module) : NULL;
    /* The executable module owns this borrowed process-lifetime name, not
     * the compiled catalogue of persisted mode identifiers. */
    return metadata != NULL ? metadata->name : NULL;
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
    return g_strdup("");
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
    return 1000U;
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
