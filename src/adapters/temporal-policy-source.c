// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Optional System Settings / Cinnamon temporal-policy authority boundary.
 *
 * This module deliberately owns every platform concern involved in discovering
 * the effective policy: Cinnamon fallback settings, provider presence, policy
 * loading, filesystem monitoring and coalesced refresh. SystemClock therefore
 * remains a clock facade instead of also being a policy-discovery service.
 */

#include "temporal-policy-source.h"

#include <gio/gio.h>
#include <infiltratr/core.h>
#include <infiltratr/posix.h>
#include <infiltratr/posix_path.h>
#include <infiltratr/temporal_posix.h>
#include <stdbool.h>

struct _CalendarPlusTemporalPolicySource
{
    GSettings *cinnamon_interface_settings;

    gchar *policy_directory;
    gchar *provider_marker_basename;
    GFileMonitor *policy_directory_monitor;
    GFileMonitor *config_home_monitor;
    GFileMonitor *provider_directory_monitor;
    guint refresh_source_id;

    InfiltratrTemporalPolicyV3 effective_policy;
    gboolean provider_available;
    gboolean infiltrator_authority;

    CalendarPlusTemporalPolicyChangedFunc changed;
    gpointer changed_data;
};

static gboolean
policy_equal(const InfiltratrTemporalPolicyV3 *left,
             const InfiltratrTemporalPolicyV3 *right)
{
    return left != NULL && right != NULL &&
           left->version == right->version &&
           infiltratr_string_equal(left->clock_mode, right->clock_mode) &&
           infiltratr_string_equal(left->calendar, right->calendar) &&
           left->show_seconds == right->show_seconds &&
           left->location_configured == right->location_configured &&
           left->latitude == right->latitude &&
           left->longitude == right->longitude;
}

gboolean
calendar_plus_temporal_policy_resolve_effective(
    gboolean provider_available,
    gboolean policy_found,
    const InfiltratrTemporalPolicyV3 *persisted_policy,
    gboolean cinnamon_show_seconds,
    InfiltratrTemporalPolicyV3 *effective_policy,
    gboolean *infiltrator_authority)
{
    if (effective_policy == NULL || infiltrator_authority == NULL ||
        !infiltratr_temporal_policy_v3_default(effective_policy))
    {
        return FALSE;
    }

    *infiltrator_authority = FALSE;
    if (provider_available && policy_found && persisted_policy != NULL)
    {
        *effective_policy = *persisted_policy;
        *infiltrator_authority = TRUE;
        return TRUE;
    }

    effective_policy->show_seconds = cinnamon_show_seconds != FALSE;
    return TRUE;
}

static gboolean
cinnamon_show_seconds(const CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL || source->cinnamon_interface_settings == NULL)
        return FALSE;

    return g_settings_get_boolean(
        source->cinnamon_interface_settings, "clock-show-seconds");
}

static gboolean
refresh_effective_policy(CalendarPlusTemporalPolicySource *source,
                         gboolean notify)
{
    InfiltratrTemporalPolicyV3 persisted;
    InfiltratrTemporalPolicyV3 next;
    bool found = false;
    gboolean next_provider_available;
    gboolean next_infiltrator_authority = FALSE;
    gboolean changed;
    InfiltratrIoResult load_result = INFILTRATR_IO_NOT_FOUND;

    if (source == NULL)
        return FALSE;

    next_provider_available =
        infiltratr_temporal_posix_provider_available() ? TRUE : FALSE;

    if (next_provider_available)
    {
        load_result = infiltratr_temporal_posix_policy_load(
            &persisted, &found);
        if (load_result != INFILTRATR_IO_OK &&
            load_result != INFILTRATR_IO_NOT_FOUND)
        {
            g_warning("Calendar: unable to read temporal policy: %s",
                      infiltratr_io_result_name(load_result));
        }
    }

    if (!calendar_plus_temporal_policy_resolve_effective(
            next_provider_available,
            load_result == INFILTRATR_IO_OK && found,
            load_result == INFILTRATR_IO_OK && found ? &persisted : NULL,
            cinnamon_show_seconds(source),
            &next,
            &next_infiltrator_authority))
    {
        return FALSE;
    }

    changed =
        !policy_equal(&source->effective_policy, &next) ||
        source->provider_available != next_provider_available ||
        source->infiltrator_authority != next_infiltrator_authority;

    source->effective_policy = next;
    source->provider_available = next_provider_available;
    source->infiltrator_authority = next_infiltrator_authority;

    if (notify && changed && source->changed != NULL)
        source->changed(source->changed_data);
    return TRUE;
}

static gboolean
refresh_effective_policy_deferred(gpointer user_data)
{
    CalendarPlusTemporalPolicySource *source = user_data;

    source->refresh_source_id = 0;
    (void)refresh_effective_policy(source, TRUE);
    return G_SOURCE_REMOVE;
}

static void
schedule_effective_policy_refresh(CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL || source->refresh_source_id != 0)
        return;

    source->refresh_source_id =
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        refresh_effective_policy_deferred,
                        source,
                        NULL);
}

static void
cancel_monitor(GFileMonitor **monitor)
{
    if (monitor == NULL || *monitor == NULL)
        return;

    g_file_monitor_cancel(*monitor);
    g_object_unref(*monitor);
    *monitor = NULL;
}

static gboolean
file_has_basename(GFile *file, const gchar *basename)
{
    g_autofree gchar *actual = NULL;

    if (file == NULL || basename == NULL)
        return FALSE;
    actual = g_file_get_basename(file);
    return infiltratr_string_equal(actual, basename);
}

static void
on_policy_directory_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                            GFile *file G_GNUC_UNUSED,
                            GFile *other_file G_GNUC_UNUSED,
                            GFileMonitorEvent event_type G_GNUC_UNUSED,
                            gpointer user_data)
{
    schedule_effective_policy_refresh(user_data);
}

static void
setup_policy_directory_monitor(CalendarPlusTemporalPolicySource *source)
{
    g_autoptr(GFile) directory = NULL;
    g_autoptr(GError) error = NULL;

    if (source == NULL)
        return;

    cancel_monitor(&source->policy_directory_monitor);
    if (source->policy_directory == NULL)
        return;

    directory = g_file_new_for_path(source->policy_directory);
    source->policy_directory_monitor =
        g_file_monitor_directory(directory,
                                 G_FILE_MONITOR_NONE,
                                 NULL,
                                 &error);
    if (source->policy_directory_monitor != NULL)
    {
        g_signal_connect(source->policy_directory_monitor,
                         "changed",
                         G_CALLBACK(on_policy_directory_changed),
                         source);
    }
    else
    {
        g_warning("Calendar: unable to monitor temporal policy directory '%s': %s",
                  source->policy_directory,
                  error != NULL ? error->message : "unknown monitor error");
    }
}

static void
on_config_home_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                       GFile *file,
                       GFile *other_file,
                       GFileMonitorEvent event_type G_GNUC_UNUSED,
                       gpointer user_data)
{
    CalendarPlusTemporalPolicySource *source = user_data;
    const gchar *policy_basename =
        source != NULL && source->policy_directory != NULL
            ? infiltratr_path_basename(source->policy_directory)
            : NULL;

    if (policy_basename == NULL ||
        (!file_has_basename(file, policy_basename) &&
         !file_has_basename(other_file, policy_basename)))
    {
        return;
    }

    setup_policy_directory_monitor(source);
    schedule_effective_policy_refresh(source);
}

static void
on_provider_directory_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                              GFile *file,
                              GFile *other_file,
                              GFileMonitorEvent event_type G_GNUC_UNUSED,
                              gpointer user_data)
{
    CalendarPlusTemporalPolicySource *source = user_data;

    if (source == NULL || source->provider_marker_basename == NULL ||
        (!file_has_basename(file, source->provider_marker_basename) &&
         !file_has_basename(other_file, source->provider_marker_basename)))
    {
        return;
    }

    schedule_effective_policy_refresh(source);
}

static void
on_cinnamon_seconds_changed(GSettings *settings G_GNUC_UNUSED,
                            gchar *key G_GNUC_UNUSED,
                            gpointer user_data)
{
    (void)refresh_effective_policy(user_data, TRUE);
}

static GFileMonitor *
monitor_directory_path(const gchar *path,
                       GCallback callback,
                       gpointer user_data)
{
    g_autoptr(GFile) directory = NULL;
    g_autoptr(GError) error = NULL;
    GFileMonitor *monitor;

    if (path == NULL || callback == NULL)
        return NULL;

    directory = g_file_new_for_path(path);
    monitor = g_file_monitor_directory(directory,
                                       G_FILE_MONITOR_NONE,
                                       NULL,
                                       &error);
    if (monitor != NULL)
    {
        g_signal_connect(monitor, "changed", callback, user_data);
    }
    else
    {
        g_warning("Calendar: unable to monitor directory '%s': %s",
                  path,
                  error != NULL ? error->message : "unknown monitor error");
    }
    return monitor;
}

static GSettings *
create_cinnamon_interface_settings(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    GSettingsSchema *schema;
    GSettings *settings;

    if (source == NULL)
        return NULL;

    schema = g_settings_schema_source_lookup(
        source, "org.cinnamon.desktop.interface", TRUE);
    if (schema == NULL)
        return NULL;

    if (!g_settings_schema_has_key(schema, "clock-show-seconds"))
    {
        g_settings_schema_unref(schema);
        return NULL;
    }

    settings = g_settings_new_full(schema, NULL, NULL);
    g_settings_schema_unref(schema);
    return settings;
}

CalendarPlusTemporalPolicySource *
calendar_plus_temporal_policy_source_new(
    CalendarPlusTemporalPolicyChangedFunc changed,
    gpointer user_data)
{
    CalendarPlusTemporalPolicySource *source =
        g_new0(CalendarPlusTemporalPolicySource, 1);
    char policy_directory[4096];
    char policy_parent[4096];
    char provider_marker[4096];
    char provider_directory[4096];

    source->changed = changed;
    source->changed_data = user_data;
    if (!infiltratr_temporal_policy_v3_default(&source->effective_policy))
    {
        g_free(source);
        return NULL;
    }

    source->cinnamon_interface_settings = create_cinnamon_interface_settings();
    if (source->cinnamon_interface_settings != NULL)
    {
        g_signal_connect(source->cinnamon_interface_settings,
                         "changed::clock-show-seconds",
                         G_CALLBACK(on_cinnamon_seconds_changed),
                         source);
    }

    if (infiltratr_temporal_posix_policy_directory(
            policy_directory, sizeof(policy_directory)))
    {
        source->policy_directory = g_strdup(policy_directory);
        setup_policy_directory_monitor(source);
        if (infiltratr_path_dirname(
                policy_directory, policy_parent, sizeof(policy_parent)))
        {
            source->config_home_monitor =
                monitor_directory_path(policy_parent,
                                       G_CALLBACK(on_config_home_changed),
                                       source);
        }
    }

    if (infiltratr_temporal_posix_provider_marker_path(
            provider_marker, sizeof(provider_marker)))
    {
        const gchar *provider_basename =
            infiltratr_path_basename(provider_marker);
        if (provider_basename != NULL)
            source->provider_marker_basename = g_strdup(provider_basename);

        if (infiltratr_path_dirname(
                provider_marker,
                provider_directory,
                sizeof(provider_directory)))
        {
            source->provider_directory_monitor =
                monitor_directory_path(
                    provider_directory,
                    G_CALLBACK(on_provider_directory_changed),
                    source);
        }
    }

    (void)refresh_effective_policy(source, FALSE);
    return source;
}

void
calendar_plus_temporal_policy_source_free(
    CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL)
        return;

    if (source->refresh_source_id != 0)
    {
        g_source_remove(source->refresh_source_id);
        source->refresh_source_id = 0;
    }

    cancel_monitor(&source->policy_directory_monitor);
    cancel_monitor(&source->config_home_monitor);
    cancel_monitor(&source->provider_directory_monitor);
    g_clear_pointer(&source->policy_directory, g_free);
    g_clear_pointer(&source->provider_marker_basename, g_free);
    g_clear_object(&source->cinnamon_interface_settings);
    source->changed = NULL;
    source->changed_data = NULL;
    g_free(source);
}

const InfiltratrTemporalPolicyV3 *
calendar_plus_temporal_policy_source_get_policy(
    const CalendarPlusTemporalPolicySource *source)
{
    return source != NULL ? &source->effective_policy : NULL;
}

gboolean
calendar_plus_temporal_policy_source_provider_available(
    const CalendarPlusTemporalPolicySource *source)
{
    return source != NULL && source->provider_available;
}

gboolean
calendar_plus_temporal_policy_source_has_infiltrator_authority(
    const CalendarPlusTemporalPolicySource *source)
{
    return source != NULL && source->infiltrator_authority;
}
