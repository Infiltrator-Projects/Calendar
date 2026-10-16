// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Effective temporal-policy boundary for the Cinnamon composition root.
 *
 * The native SystemClock publishes one coherent policy snapshot.  This module
 * alone decodes and validates that snapshot, caches one immutable generation
 * and reduces it to the scalar clock configuration needed by presentation.
 */

const NativeCalendar = imports.gi.Calendar;

function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }
    return require("./runtimeSupport");
}

const RuntimeSupport = _loadRuntimeSupport();
const NativeBoundary = RuntimeSupport.loadLocalModule(
    "nativeBoundary"
).NativeBoundary;

const FALLBACK_POLICY = Object.freeze({
    mode: "standard",
    showSeconds: false,
    locationConfigured: false,
    latitude: 0.0,
    longitude: 0.0,
    calendar: "gregorian",
    authority: "mint-cinnamon",
    providerAvailable: false,
});

function _freezePolicy(decoded, isNativeClockMode) {
    const numericLatitude = Number(decoded.latitude);
    const numericLongitude = Number(decoded.longitude);
    const modeIsValid =
        decoded.mode === "standard" ||
        decoded.mode === "standard-24" ||
        decoded.mode === "standard-12" ||
        isNativeClockMode(decoded.mode);
    const calendarIsValid =
        typeof decoded.calendar === "string" &&
        decoded.calendar.length > 0 &&
        NativeCalendar.CalendarSystem.new(decoded.calendar) !== null;
    const authorityIsValid =
        decoded.authority === "mint-cinnamon" ||
        decoded.authority === "system-settings";

    if (typeof decoded.mode !== "string" || !modeIsValid ||
        !calendarIsValid ||
        !Number.isFinite(numericLatitude) ||
        numericLatitude < -90 || numericLatitude > 90 ||
        !Number.isFinite(numericLongitude) ||
        numericLongitude < -180 || numericLongitude > 180 ||
        !authorityIsValid) {
        throw new Error("invalid effective temporal policy");
    }

    return Object.freeze({
        mode: decoded.mode,
        showSeconds: Boolean(decoded.showSeconds),
        locationConfigured: Boolean(decoded.locationConfigured),
        latitude: numericLatitude,
        longitude: numericLongitude,
        calendar: decoded.calendar,
        authority: decoded.authority,
        providerAvailable: Boolean(decoded.providerAvailable),
    });
}

var TemporalPolicyResolver = class TemporalPolicyResolver {
    constructor(systemClock, isNativeClockMode) {
        this._systemClock = systemClock;
        this._isNativeClockMode = typeof isNativeClockMode === "function"
            ? isNativeClockMode
            : () => false;
        this._cache = null;
    }

    invalidate() {
        this._cache = null;
    }

    get() {
        if (this._cache !== null) {
            return this._cache;
        }
        if (!this._systemClock) {
            this._cache = FALLBACK_POLICY;
            return this._cache;
        }
        try {
            const variant = this._systemClock.get_system_policy();
            if (!variant) {
                throw new Error("missing effective temporal policy");
            }
            this._cache = _freezePolicy(
                NativeBoundary.decodeTemporalPolicy(variant),
                this._isNativeClockMode
            );
        } catch (error) {
            global.logError(error);
            this._cache = FALLBACK_POLICY;
        }
        return this._cache;
    }

    clockConfig({ vertical, use24h, showDate }) {
        const policy = this.get();
        return Object.freeze({
            mode: policy.mode,
            showSeconds: policy.showSeconds,
            locationConfigured: policy.locationConfigured,
            latitude: policy.latitude,
            longitude: policy.longitude,
            vertical: Boolean(vertical),
            use24h: Boolean(use24h),
            showDate: Boolean(showDate),
            primaryCalendar: policy.calendar,
        });
    }
};
