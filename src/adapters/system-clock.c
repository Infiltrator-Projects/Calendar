// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/* GObject/GJS facade over the injected platform-neutral clock engine. */

#include "system-clock.h"

#include "clock-engine.h"
#include "clock-glib-adapter.h"

#include <gio/gio.h>
#include <infiltratr/temporal.h>

struct _CalendarPlusSystemClock
{
    GObject parent_instance;

    CalendarPlusClockEngine *engine;
    gchar *policy_path;
    GFileMonitor *policy_monitor;
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

static void
on_engine_tick(gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_TICK], 0);
}

static void
on_policy_file_changed(GFileMonitor *monitor G_GNUC_UNUSED,
                       GFile *file G_GNUC_UNUSED,
                       GFile *other_file G_GNUC_UNUSED,
                       GFileMonitorEvent event_type G_GNUC_UNUSED,
                       gpointer user_data)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_POLICY_CHANGED], 0);
}

static void
calendar_plus_system_clock_dispose(GObject *object)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(object);

    if (self->policy_monitor != NULL)
    {
        g_file_monitor_cancel(self->policy_monitor);
        g_object_unref(self->policy_monitor);
        self->policy_monitor = NULL;
    }
    g_free(self->policy_path);
    self->policy_path = NULL;
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

    self->policy_path = g_build_filename(g_get_user_config_dir(),
                                         "infiltrator",
                                         "presentation.conf",
                                         NULL);
    if (self->policy_path != NULL)
    {
        g_autoptr(GFile) policy_file = g_file_new_for_path(self->policy_path);
        g_autoptr(GError) error = NULL;

        self->policy_monitor = g_file_monitor_file(policy_file,
                                                   G_FILE_MONITOR_NONE,
                                                   NULL,
                                                   &error);
        if (self->policy_monitor != NULL)
        {
            g_signal_connect(self->policy_monitor,
                             "changed",
                             G_CALLBACK(on_policy_file_changed),
                             self);
        }
    }
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



static gboolean
load_system_temporal_policy(CalendarPlusSystemClock *self,
                            InfiltratrTemporalPolicyV3 *policy)
{
    g_autofree gchar *contents = NULL;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    g_return_val_if_fail(policy != NULL, FALSE);

    if (!infiltratr_temporal_policy_v3_default(policy))
        return FALSE;

    if (self->policy_path == NULL ||
        !g_file_get_contents(self->policy_path, &contents, NULL, NULL))
        return TRUE;

    return infiltratr_temporal_policy_v3_parse(contents, policy);
}

gchar *
calendar_plus_system_clock_get_system_mode(CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    if (!load_system_temporal_policy(self, &policy))
        return g_strdup("standard");
    return g_strdup(policy.clock_mode);
}

gchar *
calendar_plus_system_clock_get_system_calendar(CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    if (!load_system_temporal_policy(self, &policy))
        return g_strdup("gregorian");
    return g_strdup(policy.calendar);
}

gboolean
calendar_plus_system_clock_get_system_show_seconds(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    return load_system_temporal_policy(self, &policy) &&
           policy.show_seconds;
}

gboolean
calendar_plus_system_clock_get_system_location_configured(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    return load_system_temporal_policy(self, &policy) &&
           policy.location_configured;
}

gdouble
calendar_plus_system_clock_get_system_latitude(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    if (!load_system_temporal_policy(self, &policy))
        return 0.0;
    return policy.latitude;
}

gdouble
calendar_plus_system_clock_get_system_longitude(
    CalendarPlusSystemClock *self)
{
    InfiltratrTemporalPolicyV3 policy;

    if (!load_system_temporal_policy(self, &policy))
        return 0.0;
    return policy.longitude;
}
