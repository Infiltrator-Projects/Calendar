// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/* GObject/GJS facade over the injected platform-neutral clock engine. */

#include "system-clock.h"

#include "clock-engine.h"
#include "clock-glib-adapter.h"
#include "system-clock-private.h"

#include <gio/gio.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>
#include <string.h>

struct _CalendarPlusSystemClock
{
    GObject parent_instance;

    CalendarPlusClockEngine *engine;
    GSettings *cinnamon_interface_settings;

    gchar *policy_directory;
    GFileMonitor *policy_directory_monitor;
    GFileMonitor *config_home_monitor;
    GFileMonitor *provider_directory_monitor;

    InfiltratrTemporalPolicyV3 effective_policy;
    gboolean provider_available;
    gboolean infiltrator_authority;
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
           strcmp(left->clock_mode, right->clock_mode) == 0 &&
           strcmp(left->calendar, right->calendar) == 0 &&
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
cinnamon_show_seconds(CalendarPlusSystemClock *self)
{
    if (self->cinnamon_interface_settings == NULL)
        return FALSE;

    return g_settings_get_boolean(
        self->cinnamon_interface_settings, "clock-show-seconds");
}

static gboolean
refresh_effective_policy(CalendarPlusSystemClock *self,
                         gboolean emit_signal)
{
    InfiltratrTemporalPolicyV3 persisted;
    InfiltratrTemporalPolicyV3 next;
    bool found = false;
    gboolean next_provider_available;
    gboolean next_infiltrator_authority = FALSE;
    gboolean changed;
    InfiltratrIoResult load_result = INFILTRATR_IO_NOT_FOUND;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);

    next_provider_available =
        infiltratr_temporal_posix_provider_available() ? TRUE : FALSE;

    if (next_provider_available)
    {
        load_result = infiltratr_temporal_posix_policy_load(
            &persisted, &found);
    }

    if (!calendar_plus_system_clock_resolve_effective_policy(
            next_provider_available,
            load_result == INFILTRATR_IO_OK && found,
            load_result == INFILTRATR_IO_OK && found ? &persisted : NULL,
            cinnamon_show_seconds(self),
            &next,
            &next_infiltrator_authority))
    {
        return FALSE;
    }

    changed =
        !policy_equal(&self->effective_policy, &next) ||
        self->provider_available != next_provider_available ||
        self->infiltrator_authority != next_infiltrator_authority;

    self->effective_policy = next;
    self->provider_available = next_provider_available;
    self->infiltrator_authority = next_infiltrator_authority;

    if (emit_signal && changed)
        g_signal_emit(self, signals[SIGNAL_POLICY_CHANGED], 0);
    return TRUE;
}

static void
on_engine_tick(gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_TICK], 0);
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
    return g_strcmp0(actual, basename) == 0;
}

static void
on_policy_directory_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                            GFile *file G_GNUC_UNUSED,
                            GFile *other_file G_GNUC_UNUSED,
                            GFileMonitorEvent event_type G_GNUC_UNUSED,
                            gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    (void)refresh_effective_policy(self, TRUE);
}

static void
setup_policy_directory_monitor(CalendarPlusSystemClock *self)
{
    g_autoptr(GFile) directory = NULL;
    g_autoptr(GError) error = NULL;

    cancel_monitor(&self->policy_directory_monitor);
    if (self->policy_directory == NULL)
        return;

    directory = g_file_new_for_path(self->policy_directory);
    self->policy_directory_monitor =
        g_file_monitor_directory(directory,
                                 G_FILE_MONITOR_NONE,
                                 NULL,
                                 &error);
    if (self->policy_directory_monitor != NULL)
    {
        g_signal_connect(self->policy_directory_monitor,
                         "changed",
                         G_CALLBACK(on_policy_directory_changed),
                         self);
    }
}

static void
on_config_home_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                       GFile *file,
                       GFile *other_file,
                       GFileMonitorEvent event_type G_GNUC_UNUSED,
                       gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    if (!file_has_basename(file, "infiltrator") &&
        !file_has_basename(other_file, "infiltrator"))
    {
        return;
    }

    setup_policy_directory_monitor(self);
    (void)refresh_effective_policy(self, TRUE);
}

static void
on_provider_directory_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                              GFile *file,
                              GFile *other_file,
                              GFileMonitorEvent event_type G_GNUC_UNUSED,
                              gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    if (!file_has_basename(file, "temporal-v3") &&
        !file_has_basename(other_file, "temporal-v3"))
    {
        return;
    }

    (void)refresh_effective_policy(self, TRUE);
}

static void
on_cinnamon_seconds_changed(GSettings *settings G_GNUC_UNUSED,
                            gchar *key G_GNUC_UNUSED,
                            gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    (void)refresh_effective_policy(self, TRUE);
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
        g_signal_connect(monitor, "changed", callback, user_data);
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

static void
calendar_plus_system_clock_dispose(GObject *object)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(object);

    cancel_monitor(&self->policy_directory_monitor);
    cancel_monitor(&self->config_home_monitor);
    cancel_monitor(&self->provider_directory_monitor);

    g_clear_pointer(&self->policy_directory, g_free);
    if (self->cinnamon_interface_settings != NULL)
    {
        g_object_unref(self->cinnamon_interface_settings);
        self->cinnamon_interface_settings = NULL;
    }

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
    char policy_directory[4096];
    char provider_marker[4096];

    calendar_plus_clock_glib_interfaces(&time_source, &scheduler);
    self->engine = calendar_plus_clock_engine_new(&time_source,
                                                  &scheduler,
                                                  on_engine_tick,
                                                  self);
    self->cinnamon_interface_settings =
        create_cinnamon_interface_settings();
    if (self->cinnamon_interface_settings != NULL)
    {
        g_signal_connect(self->cinnamon_interface_settings,
                         "changed::clock-show-seconds",
                         G_CALLBACK(on_cinnamon_seconds_changed),
                         self);
    }

    if (infiltratr_temporal_posix_policy_directory(
            policy_directory, sizeof(policy_directory)))
    {
        self->policy_directory = g_strdup(policy_directory);
        setup_policy_directory_monitor(self);
    }

    self->config_home_monitor =
        monitor_directory_path(g_get_user_config_dir(),
                               G_CALLBACK(on_config_home_changed),
                               self);

    if (infiltratr_temporal_posix_provider_marker_path(
            provider_marker, sizeof(provider_marker)))
    {
        g_autofree gchar *provider_directory =
            g_path_get_dirname(provider_marker);
        self->provider_directory_monitor =
            monitor_directory_path(provider_directory,
                                   G_CALLBACK(on_provider_directory_changed),
                                   self);
    }

    (void)infiltratr_temporal_policy_v3_default(&self->effective_policy);
    (void)refresh_effective_policy(self, FALSE);
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
    if (config.mode == CALENDAR_PLUS_TIME_MODE_INVALID ||
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
    const gchar *authority;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), NULL);

    authority = self->infiltrator_authority
        ? "infiltrator-system-settings"
        : "mint-cinnamon";

    return g_variant_ref_sink(g_variant_new(
        "(ssbbddsb)",
        self->effective_policy.clock_mode,
        self->effective_policy.calendar,
        self->effective_policy.show_seconds,
        self->effective_policy.location_configured,
        self->effective_policy.latitude,
        self->effective_policy.longitude,
        authority,
        self->provider_available));
}

gchar *
calendar_plus_system_clock_get_system_mode(CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("standard"));
    return g_strdup(self->effective_policy.clock_mode);
}

gchar *
calendar_plus_system_clock_get_system_calendar(CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("gregorian"));
    return g_strdup(self->effective_policy.calendar);
}

gboolean
calendar_plus_system_clock_get_system_show_seconds(
    CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    return self->effective_policy.show_seconds;
}

gboolean
calendar_plus_system_clock_get_system_location_configured(
    CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    return self->effective_policy.location_configured;
}

gdouble
calendar_plus_system_clock_get_system_latitude(
    CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    return self->effective_policy.latitude;
}

gdouble
calendar_plus_system_clock_get_system_longitude(
    CalendarPlusSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    return self->effective_policy.longitude;
}
