// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * GObject/GJS facade over the injected platform-neutral clock engine.
 *
 * Clock scheduling and formatting stay in CalendarPlusClockEngine. Effective
 * temporal-policy discovery, persistence fallback and filesystem/settings
 * monitoring live in the private CalendarPlusTemporalPolicySource adapter.
 * This facade composes the two responsibilities and publishes their signals to
 * GJS without owning either subsystem's implementation details.
 */

#include "system-clock.h"

#include "clock-engine.h"
#include "clock-glib-adapter.h"
#include "temporal-policy-source.h"

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

static const InfiltratrTemporalPolicyV3 *
effective_policy(CalendarPlusSystemClock *self)
{
    return self != NULL
        ? calendar_plus_temporal_policy_source_get_policy(self->policy_source)
        : NULL;
}

static void
calendar_plus_system_clock_dispose(GObject *object)
{
    CalendarPlusSystemClock *self = CALENDAR_PLUS_SYSTEM_CLOCK(object);

    calendar_plus_temporal_policy_source_free(self->policy_source);
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
    self->policy_source = calendar_plus_temporal_policy_source_new(
        on_policy_changed, self);
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
    const InfiltratrTemporalPolicyV3 *policy;
    const gchar *authority;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), NULL);
    policy = effective_policy(self);
    if (policy == NULL)
        return NULL;

    authority = calendar_plus_temporal_policy_source_get_infiltrator_authority(
        self->policy_source)
        ? "infiltrator-system-settings"
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
        calendar_plus_temporal_policy_source_get_provider_available(
            self->policy_source)));
}

gchar *
calendar_plus_system_clock_get_system_mode(CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("standard"));
    policy = effective_policy(self);
    return g_strdup(policy != NULL ? policy->clock_mode : "standard");
}

gchar *
calendar_plus_system_clock_get_system_calendar(CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self),
                         g_strdup("gregorian"));
    policy = effective_policy(self);
    return g_strdup(policy != NULL ? policy->calendar : "gregorian");
}

gboolean
calendar_plus_system_clock_get_system_show_seconds(
    CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    policy = effective_policy(self);
    return policy != NULL && policy->show_seconds;
}

gboolean
calendar_plus_system_clock_get_system_location_configured(
    CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), FALSE);
    policy = effective_policy(self);
    return policy != NULL && policy->location_configured;
}

gdouble
calendar_plus_system_clock_get_system_latitude(
    CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    policy = effective_policy(self);
    return policy != NULL ? policy->latitude : 0.0;
}

gdouble
calendar_plus_system_clock_get_system_longitude(
    CalendarPlusSystemClock *self)
{
    const InfiltratrTemporalPolicyV3 *policy;

    g_return_val_if_fail(CALENDAR_PLUS_IS_SYSTEM_CLOCK(self), 0.0);
    policy = effective_policy(self);
    return policy != NULL ? policy->longitude : 0.0;
}
