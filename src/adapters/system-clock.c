// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * GObject/GJS facade over the injected platform-neutral clock engine.
 *
 * SystemClock composes two independently owned services: the clock engine and
 * a private temporal-policy source. The policy source owns Cinnamon fallback
 * settings, optional System Settings discovery, filesystem monitors and
 * coalesced refresh. The facade itself owns no policy-discovery state; it only
 * publishes the source's coherent snapshot through the stable GObject ABI.
 */

#include "system-clock.h"

#include "clock-engine.h"
#include "clock-glib-adapter.h"
#include "system-clock-private.h"

#include <gio/gio.h>
#include <infiltratr/core.h>
#include <infiltratr/posix.h>
#include <infiltratr/posix_path.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>
#include <stdbool.h>

typedef struct _CalendarPlusTemporalPolicySource
    CalendarPlusTemporalPolicySource;
typedef void (*CalendarPlusTemporalPolicyChangedFunc)(gpointer user_data);

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

struct _CalendarPlusSystemClock
{
    GObject parent_instance;

    CalendarPlusClockEngine *engine;
    CalendarPlusTemporalPolicySource *policy_source;
};

enum
{
    SIGNAL_TICK,
    SIGNAL_POLICY_CHANGED,
    SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT];

/* GLib's type-registration macro contains an intentional pointer probe. */
// NOLINTNEXTLINE(performance-no-int-to-ptr)
G_DEFINE_TYPE(CalendarPlusSystemClock,
              calendar_plus_system_clock,
              G_TYPE_OBJECT)

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
calendar_plus_system_clock_resolve_effective_policy(
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
temporal_policy_source_cinnamon_show_seconds(
    const CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL || source->cinnamon_interface_settings == NULL)
        return FALSE;

    return g_settings_get_boolean(
        source->cinnamon_interface_settings, "clock-show-seconds");
}

static gboolean
temporal_policy_source_refresh(CalendarPlusTemporalPolicySource *source,
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

    if (!calendar_plus_system_clock_resolve_effective_policy(
            next_provider_available,
            load_result == INFILTRATR_IO_OK && found,
            load_result == INFILTRATR_IO_OK && found ? &persisted : NULL,
            temporal_policy_source_cinnamon_show_seconds(source),
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
temporal_policy_source_refresh_deferred(gpointer user_data)
{
    CalendarPlusTemporalPolicySource *source = user_data;

    source->refresh_source_id = 0;
    (void)temporal_policy_source_refresh(source, TRUE);
    return G_SOURCE_REMOVE;
}

static void
temporal_policy_source_schedule_refresh(
    CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL || source->refresh_source_id != 0)
        return;

    source->refresh_source_id =
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        temporal_policy_source_refresh_deferred,
                        source,
                        NULL);
}

static void
temporal_policy_source_cancel_monitor(GFileMonitor **monitor)
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
    temporal_policy_source_schedule_refresh(user_data);
}

static void
temporal_policy_source_setup_policy_directory_monitor(
    CalendarPlusTemporalPolicySource *source)
{
    g_autoptr(GFile) directory = NULL;
    g_autoptr(GError) error = NULL;

    if (source == NULL)
        return;

    temporal_policy_source_cancel_monitor(&source->policy_directory_monitor);
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

    temporal_policy_source_setup_policy_directory_monitor(source);
    temporal_policy_source_schedule_refresh(source);
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

    temporal_policy_source_schedule_refresh(source);
}

static void
on_cinnamon_seconds_changed(GSettings *settings G_GNUC_UNUSED,
                            gchar *key G_GNUC_UNUSED,
                            gpointer user_data)
{
    (void)temporal_policy_source_refresh(user_data, TRUE);
}

static GFileMonitor *
temporal_policy_source_monitor_directory(
    const gchar *path,
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
temporal_policy_source_create_cinnamon_settings(void)
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

static CalendarPlusTemporalPolicySource *
temporal_policy_source_new(CalendarPlusTemporalPolicyChangedFunc changed,
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

    source->cinnamon_interface_settings =
        temporal_policy_source_create_cinnamon_settings();
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
        temporal_policy_source_setup_policy_directory_monitor(source);
        if (infiltratr_path_dirname(
                policy_directory, policy_parent, sizeof(policy_parent)))
        {
            source->config_home_monitor =
                temporal_policy_source_monitor_directory(
                    policy_parent,
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
                temporal_policy_source_monitor_directory(
                    provider_directory,
                    G_CALLBACK(on_provider_directory_changed),
                    source);
        }
    }

    (void)temporal_policy_source_refresh(source, FALSE);
    return source;
}

static void
temporal_policy_source_free(CalendarPlusTemporalPolicySource *source)
{
    if (source == NULL)
        return;

    if (source->refresh_source_id != 0)
    {
        g_source_remove(source->refresh_source_id);
        source->refresh_source_id = 0;
    }

    temporal_policy_source_cancel_monitor(&source->policy_directory_monitor);
    temporal_policy_source_cancel_monitor(&source->config_home_monitor);
    temporal_policy_source_cancel_monitor(&source->provider_directory_monitor);
    g_clear_pointer(&source->policy_directory, g_free);
    g_clear_pointer(&source->provider_marker_basename, g_free);
    if (source->cinnamon_interface_settings != NULL)
    {
        g_object_unref(source->cinnamon_interface_settings);
        source->cinnamon_interface_settings = NULL;
    }
    source->changed = NULL;
    source->changed_data = NULL;
    g_free(source);
}

static const InfiltratrTemporalPolicyV3 *
temporal_policy_source_get_policy(
    const CalendarPlusTemporalPolicySource *source)
{
    return source != NULL ? &source->effective_policy : NULL;
}

static void
on_engine_tick(gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_TICK], 0);
}

static void
on_policy_changed(gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_POLICY_CHANGED], 0);
}

static gboolean
policy_snapshot(CalendarPlusSystemClock *self,
                InfiltratrTemporalPolicyV3 *policy,
                gboolean *provider_available,
                gboolean *infiltrator_authority)
{
    const InfiltratrTemporalPolicyV3 *current;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    if (policy == NULL || provider_available == NULL ||
        infiltrator_authority == NULL)
    {
        return FALSE;
    }

    current = temporal_policy_source_get_policy(self->policy_source);
    if (current == NULL)
    {
        if (!infiltratr_temporal_policy_v3_default(policy))
            return FALSE;
        *provider_available = FALSE;
        *infiltrator_authority = FALSE;
        return TRUE;
    }

    *policy = *current;
    *provider_available = self->policy_source->provider_available;
    *infiltrator_authority = self->policy_source->infiltrator_authority;
    return TRUE;
}

static void
calendar_plus_system_clock_dispose(GObject *object)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(object);

    temporal_policy_source_free(self->policy_source);
    self->policy_source = NULL;
    calendar_plus_clock_engine_free(self->engine);
    self->engine = NULL;
    G_OBJECT_CLASS(calendar_plus_system_clock_parent_class)->dispose(object); // NOLINT(bugprone-casting-through-void)
}

static void
calendar_plus_system_clock_class_init(CalendarPlusSystemClockClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass); // NOLINT(bugprone-casting-through-void)

    object_class->dispose = calendar_plus_system_clock_dispose;
    signals[SIGNAL_TICK] =
        g_signal_new("tick",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     0);
    signals[SIGNAL_POLICY_CHANGED] =
        g_signal_new("policy-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     0);
}

static void
calendar_plus_system_clock_init(CalendarPlusSystemClock *self)
{
    CalendarPlusClockTimeSource time_source;
    CalendarPlusClockScheduler scheduler;

    calendar_plus_clock_glib_interfaces(&time_source, &scheduler);
    self->engine = calendar_plus_clock_engine_new(&time_source,
                                                  &scheduler,
                                                  on_engine_tick,
                                                  self);
    self->policy_source = temporal_policy_source_new(on_policy_changed, self);
}

CalendarPlusSystemClock *
calendar_plus_system_clock_new(void)
{
    return g_object_new(CALENDAR_PLUS_TYPE_SYSTEM_CLOCK, NULL);
}

void
calendar_plus_system_clock_start_at_location(CalendarPlusSystemClock *self,
                                             const gchar *mode,
                                             gboolean show_seconds,
                                             gboolean vertical,
                                             gdouble latitude,
                                             gdouble longitude)
{
    const CalendarPlusClockConfig config = {
        .mode = calendar_plus_time_mode_from_string(mode),
        .show_seconds = show_seconds,
        .vertical = vertical,
        .latitude = latitude,
        .longitude = longitude
    };

    g_return_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self));
    if (config.mode == CALENDAR_PLUS_TIME_MODE_INVALID || self->engine == NULL ||
        !calendar_plus_clock_engine_start(self->engine, &config))
    {
        calendar_plus_clock_engine_stop(self->engine);
    }
}

void
calendar_plus_system_clock_start(CalendarPlusSystemClock *self,
                                 const gchar *mode,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble longitude)
{
    calendar_plus_system_clock_start_at_location(
        self, mode, show_seconds, vertical, 0.0, longitude);
}

void
calendar_plus_system_clock_stop(CalendarPlusSystemClock *self)
{
    g_return_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self));
    calendar_plus_clock_engine_stop(self->engine);
}

gchar *
calendar_plus_system_clock_get_time(CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup(""));
    return calendar_plus_clock_engine_format(self->engine);
}

gboolean
calendar_plus_system_clock_is_running(CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    return calendar_plus_clock_engine_is_running(self->engine);
}

GVariant *
calendar_plus_system_clock_get_system_policy(CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;
    gboolean provider_available = FALSE;
    gboolean infiltrator_authority = FALSE;
    const gchar *authority;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), NULL);
    if (!policy_snapshot(self,
                         &policy,
                         &provider_available,
                         &infiltrator_authority))
    {
        return NULL;
    }

    authority = infiltrator_authority
        ? "infiltrator-system-settings"
        : "mint-cinnamon";

    return g_variant_ref_sink(g_variant_new(
        "(ssbbddsb)",
        policy.clock_mode,
        policy.calendar,
        policy.show_seconds,
        policy.location_configured,
        policy.latitude,
        policy.longitude,
        authority,
        provider_available));
}

static gboolean
legacy_policy(CalendarPlusSystemClock *self,
              InfiltratrTemporalPolicyV3 *policy)
{
    gboolean provider_available = FALSE;
    gboolean infiltrator_authority = FALSE;

    return policy_snapshot(self,
                           policy,
                           &provider_available,
                           &infiltrator_authority);
}

gchar *
calendar_plus_system_clock_get_system_mode(CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("standard"));
    return legacy_policy(self, &policy)
        ? g_strdup(policy.clock_mode)
        : g_strdup("standard");
}

gchar *
calendar_plus_system_clock_get_system_calendar(CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("gregorian"));
    return legacy_policy(self, &policy)
        ? g_strdup(policy.calendar)
        : g_strdup("gregorian");
}

gboolean
calendar_plus_system_clock_get_system_show_seconds(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    return legacy_policy(self, &policy) ? policy.show_seconds : FALSE;
}

gboolean
calendar_plus_system_clock_get_system_location_configured(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    return legacy_policy(self, &policy) ? policy.location_configured : FALSE;
}

gdouble
calendar_plus_system_clock_get_system_latitude(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    return legacy_policy(self, &policy) ? policy.latitude : 0.0;
}

gdouble
calendar_plus_system_clock_get_system_longitude(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    return legacy_policy(self, &policy) ? policy.longitude : 0.0;
}
