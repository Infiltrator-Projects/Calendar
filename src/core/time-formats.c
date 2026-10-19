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

/* Only persisted mode-to-resource bindings live here. Common's metadata
 * is authoritative for names/capabilities and the module owns execution. */
#define CLOCK_RESOURCE(mode_, id_) [CALENDAR_TIME_MODE_##mode_] = id_

static const gchar *const clock_resource_ids[] = {
    CLOCK_RESOURCE(DECIMAL, "decimal"),
    CLOCK_RESOURCE(INTERNET, "internet"),
    CLOCK_RESOURCE(UNIX, "unix"),
    CLOCK_RESOURCE(HEXADECIMAL, "hexadecimal"),
    CLOCK_RESOURCE(BINARY, "binary"),
    CLOCK_RESOURCE(SIDEREAL, "sidereal"),
    CLOCK_RESOURCE(SOLAR, "solar"),
    CLOCK_RESOURCE(JULIAN, "julian"),
    CLOCK_RESOURCE(MEAN_SOLAR, "mean-solar"),
    CLOCK_RESOURCE(MODIFIED_JULIAN, "modified-julian"),
    CLOCK_RESOURCE(CHINESE, "chinese-time"),
    CLOCK_RESOURCE(ROMAN_TEMPORAL, "roman-temporal"),
    CLOCK_RESOURCE(JAPANESE_TEMPORAL, "japanese-temporal"),
    CLOCK_RESOURCE(ITALIAN_HOURS, "italian-hours"),
    CLOCK_RESOURCE(BABYLONIAN_HOURS, "babylonian-hours"),
    CLOCK_RESOURCE(INDIAN_GHATI, "indian-ghati"),
    CLOCK_RESOURCE(CHINESE_KE, "chinese-ke"),
    CLOCK_RESOURCE(NUREMBERG_HOURS, "nuremberg-hours"),
    CLOCK_RESOURCE(BABYLONIAN_ANCIENT, "babylonian-ancient"),
    CLOCK_RESOURCE(NUREMBERG_SOLAR, "nuremberg-solar"),
    CLOCK_RESOURCE(JAPANESE_TEMPORAL_EARLY, "japanese-temporal-early"),
    CLOCK_RESOURCE(STANDARD_12, "standard-12"),
    CLOCK_RESOURCE(STANDARD_24, "standard-24")
};

G_STATIC_ASSERT(G_N_ELEMENTS(clock_resource_ids) == CALENDAR_TIME_MODE_STANDARD_24 + 1);

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
            !infiltratr_string_equal(clock_resource_ids[mode],
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
        const gchar *id = clock_resource_ids[mode];
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


/* An enum is an application ABI identity, not evidence that its executable
 * resource is installed. Never advertise an unloaded or invalid provider. */
static const InfiltratrTemporalClockModeInfo *
available_mode_info(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info;

    if (!is_native_clock(mode))
        return NULL;
    info = infiltratr_temporal_clock_mode_find(clock_resource_ids[mode]);
    return info != NULL && clock_resource_for_mode(mode) != NULL ? info : NULL;
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

    for (index = 1; index < G_N_ELEMENTS(clock_resource_ids); index++)
        if (clock_resource_ids[index] != NULL &&
            infiltratr_string_equal(info->id, clock_resource_ids[index]))
            return clock_resource_for_mode((CalendarTimeMode)index) != NULL
                ? (CalendarTimeMode)index : CALENDAR_TIME_MODE_INVALID;

    return CALENDAR_TIME_MODE_INVALID;
}

const gchar *calendar_time_mode_get_id(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        available_mode_info(mode);
    return info != NULL ? info->id : NULL;
}

gsize calendar_time_mode_get_count(void)
{
    /* Explicit conventional modes are native, but not alternative systems. */
    return CALENDAR_TIME_MODE_STANDARD_12 - CALENDAR_TIME_MODE_DECIMAL;
}

CalendarTimeMode calendar_time_mode_get_at(gsize index)
{
    return index < calendar_time_mode_get_count()
        ? (CalendarTimeMode)(index + CALENDAR_TIME_MODE_DECIMAL)
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
        available_mode_info(mode);
    return info != NULL && info->supports_seconds;
}

gboolean calendar_time_mode_requires_longitude(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        available_mode_info(mode);
    return info != NULL && info->requires_longitude;
}

gboolean calendar_time_mode_requires_latitude(CalendarTimeMode mode)
{
    const InfiltratrTemporalClockModeInfo *info =
        available_mode_info(mode);
    return info != NULL && info->requires_latitude;
}

static gboolean
location_is_valid_for_mode(CalendarTimeMode mode,
                               gdouble latitude,
                               gdouble longitude)
{
    const InfiltratrTemporalClockModeInfo *info =
        available_mode_info(mode);

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
                                 gboolean vertical, gdouble latitude,
                                 gdouble longitude)
{
    const InfiltratrTemporalClockModule *resource =
        clock_resource_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ?
        infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ?
        infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    gchar text[256];

    if (resource == NULL || !location_is_valid_for_mode(mode, latitude, longitude) ||
        !infiltratr_temporal_clock_format(
            resource, g_get_language_names()[0], unix_microseconds,
            utc_offset_seconds, show_seconds, vertical,
            safe_latitude, safe_longitude, text, sizeof(text), NULL))
        return g_strdup("");

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
    const InfiltratrTemporalClockModule *resource =
        clock_resource_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ?
        infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ?
        infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    uint32_t delay = 0U;

    if (resource == NULL || available_mode_info(mode) == NULL)
        return 1000U;
    if (!location_is_valid_for_mode(mode, latitude, longitude))
        return 3600000U;

    return infiltratr_temporal_clock_next_tick(
        resource, unix_microseconds, utc_offset_seconds, show_seconds,
        safe_latitude, safe_longitude, &delay) ? delay : 1000U;
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
