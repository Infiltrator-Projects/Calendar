// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * GObject/GJS facade over the injected platform-neutral clock engine.
 *
 * Clock scheduling and formatting stay in CalendarClockEngine. Effective
 * temporal-policy discovery, persistence fallback and filesystem/settings
 * monitoring live in CalendarTemporalPolicySource, compiled as its own
 * adapter translation unit. This facade composes the two responsibilities and
 * publishes their signals to GJS without owning either implementation.
 */

#include "system-clock.h"

#include "clock-engine.h"
#include "clock-glib-adapter.h"
#include "temporal-policy-source.h"

#include <infiltratr/temporal.h>

struct _CalendarSystemClock
{
    GObject parent_instance;
    CalendarClockEngine *engine;
    CalendarTemporalPolicySource *policy_source;
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
G_DEFINE_TYPE(CalendarSystemClock,
              calendar_system_clock,
              G_TYPE_OBJECT)

static void
on_engine_tick(gpointer user_data)
{
    CalendarSystemClock *self = CALENDAR_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_TICK], 0);
}

static void
on_policy_changed(gpointer user_data)
{
    CalendarSystemClock *self = CALENDAR_SYSTEM_CLOCK(user_data);

    g_signal_emit(self, signals[SIGNAL_POLICY_CHANGED], 0);
}

static const InfiltratrTemporalPolicyV3 *
effective_policy(CalendarSystemClock *self)
{
    return self != NULL
        ? calendar_temporal_policy_source_get_policy(self->policy_source)
        : NULL;
}

static void
calendar_system_clock_dispose(GObject *object)
{
    CalendarSystemClock *self = CALENDAR_SYSTEM_CLOCK(object);

    calendar_temporal_policy_source_free(self->policy_source);
    self->policy_source = NULL;
    calendar_clock_engine_free(self->engine);
    self->engine = NULL;
    G_OBJECT_CLASS(calendar_system_clock_parent_class)->dispose(object); // NOLINT(bugprone-casting-through-void)
}

static void
calendar_system_clock_class_init(CalendarSystemClockClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass); // NOLINT(bugprone-casting-through-void)

    object_class->dispose = calendar_system_clock_dispose;
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
calendar_system_clock_init(CalendarSystemClock *self)
{
    CalendarClockTimeSource time_source;
    CalendarClockScheduler scheduler;

    calendar_clock_glib_interfaces(&time_source, &scheduler);
    self->engine = calendar_clock_engine_new(&time_source,
                                                  &scheduler,
                                                  on_engine_tick,
                                                  self);
    self->policy_source = calendar_temporal_policy_source_new(
        on_policy_changed, self);
}

CalendarSystemClock *
calendar_system_clock_new(void)
{
    return g_object_new(CALENDAR_TYPE_SYSTEM_CLOCK, NULL);
}

void
calendar_system_clock_start_at_location(CalendarSystemClock *self,
                                             const gchar *mode,
                                             gboolean show_seconds,
                                             gboolean vertical,
                                             gdouble latitude,
                                             gdouble longitude)
{
    const CalendarClockConfig config = {
        .mode = calendar_time_mode_from_string(mode),
        .show_seconds = show_seconds,
        .vertical = vertical,
        .latitude = latitude,
        .longitude = longitude
    };

    g_return_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self));
    if (config.mode == CALENDAR_TIME_MODE_INVALID ||
        !calendar_clock_engine_start(self->engine, &config))
    {
        calendar_clock_engine_stop(self->engine);
    }
}

void
calendar_system_clock_start(CalendarSystemClock *self,
                                 const gchar *mode,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble longitude)
{
    calendar_system_clock_start_at_location(
        self, mode, show_seconds, vertical, 0.0, longitude);
}

void
calendar_system_clock_stop(CalendarSystemClock *self)
{
    g_return_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self));
    calendar_clock_engine_stop(self->engine);
}

gchar *
calendar_system_clock_get_time(CalendarSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self),
                         g_strdup(""));
    return calendar_clock_engine_format(self->engine);
}

gboolean
calendar_system_clock_is_running(CalendarSystemClock *self)
{
    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), FALSE);
    return calendar_clock_engine_is_running(self->engine);
}

GVariant *
calendar_system_clock_get_system_policy(CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;
    const gchar *authority;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), NULL);
    policy = effective_policy(self);
    if (policy == NULL)
        return NULL;

    authority = calendar_temporal_policy_source_get_infiltrator_authority(
        self->policy_source)
        ? "system-settings"
        : "mint-cinnamon";

    return g_variant_ref_sink(g_variant_new(
        "(ssbbddsb)",
        policy->clock_mode,
        policy->calendar,
        policy->show_seconds,
        policy->location_configured,
        policy->latitude,
        policy->longitude,
        authority,
        calendar_temporal_policy_source_get_provider_available(
            self->policy_source)));
}

gchar *
calendar_system_clock_get_system_mode(CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self),
                         g_strdup("standard"));
    policy = effective_policy(self);
    return g_strdup(policy != NULL ? policy->clock_mode : "standard");
}

gchar *
calendar_system_clock_get_system_calendar(CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self),
                         g_strdup("gregorian"));
    policy = effective_policy(self);
    return g_strdup(policy != NULL ? policy->calendar : "gregorian");
}

gboolean
calendar_system_clock_get_system_show_seconds(
    CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), FALSE);
    policy = effective_policy(self);
    return policy != NULL && policy->show_seconds;
}

gboolean
calendar_system_clock_get_system_location_configured(
    CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), FALSE);
    policy = effective_policy(self);
    return policy != NULL && policy->location_configured;
}

gdouble
calendar_system_clock_get_system_latitude(
    CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), 0.0);
    policy = effective_policy(self);
    return policy != NULL ? policy->latitude : 0.0;
}

gdouble
calendar_system_clock_get_system_longitude(
    CalendarSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_IS_SYSTEM_CLOCK(self), 0.0);
    policy = effective_policy(self);
    return policy != NULL ? policy->longitude : 0.0;
}
