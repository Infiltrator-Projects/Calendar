// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent local mean solar clock (LMT). Longitude controls time, not
 * timezone or DST. Owns notation and the next visible display boundary.
 */
#include <infiltratr/temporal_clock_resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DAY_US INT64_C(86400000000)
#define SECOND_US INT64_C(1000000)

static int64_t positive_mod(int64_t n, int64_t divisor)
{
    const int64_t r = n % divisor;
    return r < 0 ? r + divisor : r;
}

static bool valid_longitude(double longitude)
{
    return isfinite(longitude) && longitude >= -180.0 &&
           longitude <= 180.0;
}

static long double solar_seconds(int64_t instant, double longitude)
{
    const int64_t utc_us = positive_mod(instant, DAY_US);
    long double value = (long double)utc_us / (long double)SECOND_US +
                        (long double)longitude * 240.0L;
    value = fmodl(value, 86400.0L);
    if (value < 0.0L) value += 86400.0L;
    return value;
}

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *language, int64_t instant, int32_t offset,
                   bool show_seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t capacity,
                   size_t *length)
{
    char text[96];
    const char *sep = vertical ? "\n" : ":";
    const char *suffix_sep = vertical ? "\n" : " ";
    int64_t whole;
    unsigned hour, minute, second;
    int written;
    (void)provider; (void)language; (void)offset; (void)latitude;
    if (!valid_longitude(longitude)) return false;
    whole = (int64_t)floorl(solar_seconds(instant, longitude));
    hour = (unsigned)(whole / INT64_C(3600));
    minute = (unsigned)((whole / INT64_C(60)) % INT64_C(60));
    second = (unsigned)(whole % INT64_C(60));
    if (show_seconds)
        written = snprintf(text, sizeof(text), "%02u%s%02u%s%02u%sLMT",
                           hour, sep, minute, sep, second, suffix_sep);
    else
        written = snprintf(text, sizeof(text), "%02u%s%02u%sLMT",
                           hour, sep, minute, suffix_sep);
    if (buffer == NULL || written <= 0 || (size_t)written >= sizeof(text) ||
        (size_t)written >= capacity) return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool next_tick(const InfiltratrTemporalClockProvider *provider,
                      int64_t instant, int32_t offset, bool show_seconds,
                      double latitude, double longitude, uint32_t *millis)
{
    const long double unit = show_seconds ? 1.0L : 60.0L;
    long double phase, remaining, milliseconds;
    (void)provider; (void)offset; (void)latitude;
    if (millis == NULL || !valid_longitude(longitude)) return false;
    phase = fmodl(solar_seconds(instant, longitude), unit);
    if (phase < 0.0L) phase += unit;
    remaining = unit - phase;
    if (remaining <= 0.0L || remaining > unit) remaining = unit;
    milliseconds = ceill(remaining * 1000.0L);
    if (!isfinite(milliseconds) || milliseconds < 1.0L ||
        milliseconds > (long double)UINT32_MAX) return false;
    *millis = (uint32_t)milliseconds;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    "mean-solar", "Local mean solar time", true, false, true
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    "mean-solar", render, next_tick
};
#ifdef _WIN32
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
