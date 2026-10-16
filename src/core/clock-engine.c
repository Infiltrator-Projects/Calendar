// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Boundary-aligned clock scheduling without a main-loop dependency.
 *
 * The engine accepts injected wall-time and one-shot scheduling interfaces.
 * GLib, Win32 or a test harness can therefore own the actual timer.  Every
 * callback is re-aligned from fresh wall time, avoiding cumulative drift after
 * delayed dispatch, suspend or an option change.
 */

#include "clock-engine.h"

#include <infiltratr/core.h>
#include <math.h>

struct _CalendarClockEngine
{
    CalendarClockTimeSource time_source;
    CalendarClockScheduler scheduler;
    CalendarClockTickFunc tick;
    gpointer tick_data;
    CalendarClockTimer timer;
    gboolean running;
    gboolean dispatching;
    gboolean destroy_pending;
    CalendarClockConfig config;
};

gboolean
calendar_clock_interfaces_are_valid(
    const CalendarClockTimeSource *time_source,
    const CalendarClockScheduler *scheduler)
{
    return time_source != NULL && scheduler != NULL &&
           time_source->abi_version ==
               CALENDAR_CLOCK_TIME_SOURCE_ABI &&
           scheduler->abi_version == CALENDAR_CLOCK_SCHEDULER_ABI &&
           time_source->now != NULL && time_source->utc_offset != NULL &&
           scheduler->schedule_once != NULL && scheduler->cancel != NULL;
}

CalendarClockEngine *
calendar_clock_engine_new(
    const CalendarClockTimeSource *time_source,
    const CalendarClockScheduler *scheduler,
    CalendarClockTickFunc tick,
    gpointer tick_data)
{
    CalendarClockEngine *engine;

    if (!calendar_clock_interfaces_are_valid(time_source, scheduler) ||
        tick == NULL)
    {
        return NULL;
    }

    engine = g_new0(CalendarClockEngine, 1);
    engine->time_source = *time_source;
    engine->scheduler = *scheduler;
    engine->tick = tick;
    engine->tick_data = tick_data;
    engine->config.mode = CALENDAR_TIME_MODE_INVALID;
    return engine;
}

static guint
current_delay(const CalendarClockEngine *engine)
{
    const gint64 now =
        engine->time_source.now(engine->time_source.context);
    const gint offset = engine->time_source.utc_offset(
        engine->time_source.context, now);

    return calendar_time_delay_to_next_tick_at_location(
        engine->config.mode,
        now,
        offset,
        engine->config.show_seconds,
        engine->config.latitude,
        engine->config.longitude);
}

static gboolean schedule_next_tick(CalendarClockEngine *engine);

static void
on_timer(gpointer user_data)
{
    CalendarClockEngine *engine = user_data;

    /* The one-shot handle is no longer cancellable while it is dispatching. */
    engine->timer = (CalendarClockTimer)0;
    engine->dispatching = TRUE;
    engine->tick(engine->tick_data);

    /* A callback may release its owning facade; defer the final free safely. */
    if (engine->destroy_pending)
    {
        g_free(engine);
        return;
    }

    engine->dispatching = FALSE;
    /* A callback may already have reconfigured and armed the next timer. */
    if (engine->running && engine->timer == (CalendarClockTimer)0 &&
        !schedule_next_tick(engine))
    {
        engine->running = FALSE;
    }
}

static gboolean
schedule_next_tick(CalendarClockEngine *engine)
{
    g_return_val_if_fail(engine != NULL, FALSE);
    g_return_val_if_fail(engine->running, FALSE);
    g_return_val_if_fail(engine->timer == (CalendarClockTimer)0, FALSE);

    engine->timer = engine->scheduler.schedule_once(
        engine->scheduler.context,
        current_delay(engine),
        on_timer,
        engine);
    return engine->timer != (CalendarClockTimer)0;
}

gboolean
calendar_clock_engine_start(CalendarClockEngine *engine,
                                 const CalendarClockConfig *config)
{
    CalendarClockConfig safe;
    gboolean changed;

    if (engine == NULL)
        return FALSE;
    if (config == NULL ||
        config->mode == CALENDAR_TIME_MODE_INVALID ||
        calendar_time_mode_get_id(config->mode) == NULL ||
        (calendar_time_mode_requires_latitude(config->mode) &&
         (!isfinite(config->latitude) ||
          config->latitude < -90.0 || config->latitude > 90.0)) ||
        (calendar_time_mode_requires_longitude(config->mode) &&
         (!isfinite(config->longitude) ||
          config->longitude < -180.0 || config->longitude > 180.0)))
    {
        calendar_clock_engine_stop(engine);
        return FALSE;
    }

    safe = *config;
    safe.latitude = isfinite(config->latitude) ?
        infiltratr_clamp_double(config->latitude, -90.0, 90.0) : 0.0;
    safe.longitude = isfinite(config->longitude) ?
        infiltratr_clamp_double(config->longitude, -180.0, 180.0) : 0.0;
    changed = engine->config.mode != safe.mode ||
              engine->config.show_seconds != safe.show_seconds ||
              engine->config.vertical != safe.vertical ||
              fabs(engine->config.latitude - safe.latitude) > 1.0e-9 ||
              fabs(engine->config.longitude - safe.longitude) > 1.0e-9;

    if (engine->running && !changed)
        return TRUE;

    calendar_clock_engine_stop(engine);
    engine->config = safe;
    engine->running = TRUE;
    if (!schedule_next_tick(engine))
    {
        engine->running = FALSE;
        return FALSE;
    }
    return TRUE;
}

void
calendar_clock_engine_stop(CalendarClockEngine *engine)
{
    if (engine == NULL)
        return;

    engine->running = FALSE;
    if (engine->timer != (CalendarClockTimer)0)
    {
        engine->scheduler.cancel(engine->scheduler.context, engine->timer);
        engine->timer = (CalendarClockTimer)0;
    }
}

gboolean
calendar_clock_engine_is_running(const CalendarClockEngine *engine)
{
    return engine != NULL && engine->running;
}

gchar *
calendar_clock_engine_format(const CalendarClockEngine *engine)
{
    gint64 now;
    gint offset;

    if (engine == NULL)
        return g_strdup("");

    now = engine->time_source.now(engine->time_source.context);
    offset = engine->time_source.utc_offset(engine->time_source.context, now);
    return calendar_format_time_at_location(
        engine->config.mode,
        now,
        offset,
        engine->config.show_seconds,
        engine->config.vertical,
        engine->config.latitude,
        engine->config.longitude);
}

void
calendar_clock_engine_free(CalendarClockEngine *engine)
{
    if (engine == NULL)
        return;

    calendar_clock_engine_stop(engine);
    if (engine->dispatching)
    {
        engine->destroy_pending = TRUE;
        return;
    }
    g_free(engine);
}
