// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent local mean sidereal time resource.
 * Uses the same explicitly documented compact J2000 approximation as
 * Calendar's original scientific clock, without depending on Calendar or
 * Common's legacy rendering implementation. Longitude is east-positive.
 */
#include <infiltratr/temporal_clock_resource.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DAY_US INT64_C(86400000000)
#define J2000_JULIAN_DATE 2451545.0L
#define UNIX_JULIAN_DATE 2440587.5L
#define SIDEREAL_RATE (24.06570982441908L / 24.0L)

static bool valid_longitude(double longitude)
{
    return isfinite(longitude) && longitude >= -180.0 &&
           longitude <= 180.0;
}

/* Long double preserves sub-second phase at dates far from J2000. */
static long double sidereal_seconds(int64_t instant, double longitude)
{
    const long double jd = UNIX_JULIAN_DATE +
        (long double)instant / (long double)DAY_US;
    const long double days = jd - J2000_JULIAN_DATE;
    const long double gmst_hours =
        18.697374558L + 24.06570982441908L * days;
    long double hours = fmodl(gmst_hours + longitude / 15.0L, 24.0L);
    if (hours < 0.0L) hours += 24.0L;
    return hours * 3600.0L;
}

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *language, int64_t instant, int32_t offset,
                   bool show_seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t capacity,
                   size_t *length)
{
    char text[96];
    const char *sep = vertical ? "\n" : ":";
    const char *suffix = vertical ? "\n" : " ";
    int64_t whole;
    unsigned int hour, minute, second;
    int written;
    (void)provider; (void)language; (void)offset; (void)latitude;

    if (!valid_longitude(longitude)) return false;
    whole = (int64_t)floorl(sidereal_seconds(instant, longitude));
    hour = (unsigned int)(whole / INT64_C(3600));
    minute = (unsigned int)((whole / INT64_C(60)) % INT64_C(60));
    second = (unsigned int)(whole % INT64_C(60));
    if (show_seconds)
        written = snprintf(text, sizeof(text), "%02u%s%02u%s%02u%sLST",
                           hour, sep, minute, sep, second, suffix);
    else
        written = snprintf(text, sizeof(text), "%02u%s%02u%sLST",
                           hour, sep, minute, suffix);
    if (buffer == NULL || written <= 0 ||
        (size_t)written >= sizeof(text) || (size_t)written >= capacity)
        return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool next_tick(const InfiltratrTemporalClockProvider *provider,
                      int64_t instant, int32_t offset, bool show_seconds,
                      double latitude, double longitude, uint32_t *milliseconds)
{
    const long double unit = show_seconds ? 1.0L : 60.0L;
    long double phase, remaining, elapsed_ms;
    (void)provider; (void)offset; (void)latitude;
    if (milliseconds == NULL || !valid_longitude(longitude)) return false;
    phase = fmodl(sidereal_seconds(instant, longitude), unit);
    if (phase < 0.0L) phase += unit;
    remaining = unit - phase;
    if (remaining <= 0.0L) remaining = unit;
    elapsed_ms = ceill(remaining * 1000.0L / SIDEREAL_RATE);
    if (!isfinite(elapsed_ms) || elapsed_ms < 1.0L ||
        elapsed_ms > (long double)UINT32_MAX)
        return false;
    *milliseconds = (uint32_t)elapsed_ms;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    "sidereal", "Local sidereal time (LST; compact J2000 approximation)",
    true, false, true
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    "sidereal", render, next_tick
};

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void);
EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void) { return &metadata; }
EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void) { return &provider; }
