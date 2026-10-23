// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Panel-clock presentation policy.
 *
 * This module has no actor ownership. It translates the system temporal
 * policy and Cinnamon desktop integration state into WallClock formatting,
 * native timer configuration and the final panel/date strings. Keeping those decisions
 * out of applet.js leaves the applet controller responsible for lifecycle and
 * wiring rather than clock-format policy.
 *
 * Explicit conventional 12/24-hour selections are rendered and scheduled by
 * their independent installed clock resources. OS-following "standard" still
 * uses CinnamonDesktop.WallClock for locale-specific time. WallClock remains
 * responsible for Gregorian date text surrounding the native clock token.
 */

const NativeCalendar = imports.gi.Calendar;
const CinnamonDesktop = imports.gi.CinnamonDesktop;

var CLOCK_MODE_STANDARD_24 = "standard-24";
var CLOCK_MODE_STANDARD_12 = "standard-12";

const DAY_FORMAT = CinnamonDesktop.WallClock.lctime_format("cinnamon", "%A");
const DEFAULT_CLOCK_FORMATS = Object.freeze({
    seconds24: CinnamonDesktop.WallClock.lctime_format("cinnamon", "%R:%S"),
    seconds12: CinnamonDesktop.WallClock.lctime_format("cinnamon", "%-l:%M:%S %p"),
    minutes24: CinnamonDesktop.WallClock.lctime_format("cinnamon", "%R"),
    minutes12: CinnamonDesktop.WallClock.lctime_format("cinnamon", "%-l:%M %p"),
});

function isNativeClockMode(mode) {
    /* Every explicit selection requires a resource, including an unknown or
     * uninstalled ID. Only OS-following standard may use WallClock. */
    return typeof mode === "string" && mode.length > 0 && mode !== "standard";
}

function nativeModeRequiresLocation(mode) {
    if (!isNativeClockMode(mode)) {
        return false;
    }
    const parsed = NativeCalendar.time_mode_from_string(mode);
    return NativeCalendar.time_mode_requires_longitude(parsed) ||
        NativeCalendar.time_mode_requires_latitude(parsed);
}

function locationIsConfigured(config) {
    if (!nativeModeRequiresLocation(config.mode)) {
        return true;
    }
    if (!Boolean(config.locationConfigured)) {
        return false;
    }

    const parsed = NativeCalendar.time_mode_from_string(config.mode);
    if (NativeCalendar.time_mode_requires_latitude(parsed) &&
        (!Number.isFinite(config.latitude) ||
         config.latitude < -90 || config.latitude > 90)) {
        return false;
    }
    if (NativeCalendar.time_mode_requires_longitude(parsed) &&
        (!Number.isFinite(config.longitude) ||
         config.longitude < -180 || config.longitude > 180)) {
        return false;
    }
    return true;
}

function clockForFormat(clock, format) {
    try {
        const value = clock.get_clock_for_format(format);
        return typeof value === "string" ? value : null;
    } catch (error) {
        return null;
    }
}

function uses24HourClock(mode, use24h) {
    if (mode === CLOCK_MODE_STANDARD_24) {
        return true;
    }
    if (mode === CLOCK_MODE_STANDARD_12) {
        return false;
    }
    return Boolean(use24h);
}

function defaultHorizontalFormat(config) {
    /* The calendar resource supplies the date independently of WallClock. */
    const use24 = uses24HourClock(config.mode, config.use24h);
    const seconds = config.showSeconds && !isNativeClockMode(config.mode);
    if (use24) {
        return seconds
            ? DEFAULT_CLOCK_FORMATS.seconds24
            : DEFAULT_CLOCK_FORMATS.minutes24;
    }
    return seconds
        ? DEFAULT_CLOCK_FORMATS.seconds12
        : DEFAULT_CLOCK_FORMATS.minutes12;
}

function configureWallClock(clock, config) {
    if (!clock) {
        return;
    }

    let format;
    if (config.vertical) {
        const hour = uses24HourClock(config.mode, config.use24h)
            ? "%H"
            : "%l";
        const seconds = config.showSeconds && !isNativeClockMode(config.mode);
        format = seconds ? `${hour}%n%M%n%S` : `${hour}%n%M`;
    } else {
        format = defaultHorizontalFormat(config);
    }

    if (!clock.set_format_string(format)) {
        global.logError("calendar@the-infiltratr: invalid panel time format.");
        clock.set_format_string("~CLOCK FORMAT ERROR~ %l:%M %p");
    }
}

function syncNativeClock(systemClock, config) {
    if (!systemClock) {
        return;
    }
    if (!isNativeClockMode(config.mode) || !locationIsConfigured(config)) {
        systemClock.stop();
        return;
    }

    systemClock.start_at_location(
        config.mode,
        config.showSeconds,
        config.vertical,
        config.latitude,
        config.longitude
    );
}
function nativePanelText(systemClock, config) {
    if (!locationIsConfigured(config)) {
        return config.vertical ? "N/A\nLOC" : "N/A LOC";
    }
    /* The resource owns the native display text. WallClock is already
     * configured with a time-only string; probing multiple patterns and
     * replacing a token duplicates native calls without changing output. */
    const nativeTime = systemClock.get_time();
    return typeof nativeTime === "string" && nativeTime.length > 0
        ? nativeTime : _("Clock unavailable");
}

function selectedCalendarPanelDate(primaryCalendarSystem, config) {
    if (config.vertical || !Boolean(config.showDate)) {
        return null;
    }

    if (!primaryCalendarSystem) return _("Calendar unavailable");
    const today = new Date();
    try {
        const text = primaryCalendarSystem.format_date_part(
            today.getFullYear(),
            today.getMonth() + 1,
            today.getDate(),
            NativeCalendar.DatePart.SHORT
        );
        return typeof text === "string" && text.length > 0 ? text : _("Calendar unavailable");
    } catch (error) {
        global.logError(error);
        return _("Calendar unavailable");
    }
}

function withSelectedCalendarDate(text, primaryCalendarSystem, config) {
    const date = selectedCalendarPanelDate(primaryCalendarSystem, config);
    if (!date || typeof text !== "string" || text.length === 0) {
        return text;
    }
    return `${date} ${text}`;
}

function panelText(clock, systemClock, config, primaryCalendarSystem = null) {
    let text;

    if (isNativeClockMode(config.mode)) {
        text = systemClock ? nativePanelText(systemClock, config) : _("Clock unavailable");
    } else {
        text = clock.get_clock();
    }

    return withSelectedCalendarDate(
        text,
        primaryCalendarSystem,
        config
    );
}

function todayDisplay(clock, primaryCalendarSystem, config) {
    const today = new Date();
    const args = [today.getFullYear(), today.getMonth() + 1, today.getDate()];

    let shortDate;
    let tooltip;
    shortDate = _("Calendar unavailable");
    tooltip = shortDate;
    if (primaryCalendarSystem) {
        try {
            shortDate = primaryCalendarSystem.format_date_part(
                ...args, NativeCalendar.DatePart.SHORT) || shortDate;
            tooltip = primaryCalendarSystem.format_date_part(
                ...args, NativeCalendar.DatePart.FULL) || shortDate;
        } catch (error) {
            global.logError(error);
        }
    }

    return { today, args, shortDate, tooltip };
}

function dayName(clock) {
    return clockForFormat(clock, DAY_FORMAT) || "";
}
