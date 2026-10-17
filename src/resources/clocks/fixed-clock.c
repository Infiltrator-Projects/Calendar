// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent fixed-unit clock resource. Compiled separately for five stable
 * time-system IDs; the implementation contains no Calendar or Common clock
 * formatting dependency. Common supplies the ABI and validates callbacks.
 */
#include <infiltratr/temporal_clock_resource.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef CLOCK_RESOURCE_KIND
#error CLOCK_RESOURCE_KIND is required (1 decimal, 2 internet, 3 unix, 4 hexadecimal, 5 binary)
#endif
#ifndef CLOCK_RESOURCE_ID
#error CLOCK_RESOURCE_ID is required
#endif
#ifndef CLOCK_RESOURCE_NAME
#error CLOCK_RESOURCE_NAME is required
#endif
#ifndef CLOCK_RESOURCE_SECONDS
#error CLOCK_RESOURCE_SECONDS is required
#endif

#define CLOCK_MICROSECONDS_PER_SECOND INT64_C(1000000)
#define CLOCK_MICROSECONDS_PER_DAY INT64_C(86400000000)

static int64_t positive_modulo(int64_t n, int64_t mod)
{
    const int64_t rem = n % mod;
    return rem < 0 ? rem + mod : rem;
}

 #if CLOCK_RESOURCE_KIND == 3
static int64_t floor_divide(int64_t n, int64_t divisor)
{
    const int64_t quotient = n / divisor;
    return n % divisor < 0 ? quotient - 1 : quotient;
}

#endif

static uint64_t ticks_at(uint64_t phase, uint64_t per_day)
{
    return phase * per_day / (uint64_t)CLOCK_MICROSECONDS_PER_DAY;
}

#if CLOCK_RESOURCE_KIND == 1 || CLOCK_RESOURCE_KIND == 4 || CLOCK_RESOURCE_KIND == 5
static uint64_t phase_local(int64_t instant, int32_t offset)
{
    const int64_t phase = positive_modulo(instant, CLOCK_MICROSECONDS_PER_DAY);
    const int64_t shifted = phase + (int64_t)offset * CLOCK_MICROSECONDS_PER_SECOND;
    return (uint64_t)positive_modulo(shifted, CLOCK_MICROSECONDS_PER_DAY);
}

#endif

#if CLOCK_RESOURCE_KIND == 5
static void to_binary(unsigned value, unsigned digits, char *dest)
{
    unsigned bit;
    for (bit = 0; bit < digits; bit++)
        dest[bit] = (value & (1U << (digits - bit - 1U))) ? '1' : '0';
    dest[digits] = '\0';
}

#endif

static bool clock_render(const InfiltratrTemporalClockProvider *provider,
                         const char *locale, int64_t instant, int32_t offset,
                         bool seconds, bool vertical, double latitude, double longitude,
                         char *buffer, size_t capacity, size_t *length)
{
    char text[128] = {0};
    int written = -1;
    uint64_t tick;
    const char *sep = vertical ? "\n" : ":";

    (void)provider; (void)locale; (void)latitude; (void)longitude;
    (void)instant; (void)offset; (void)seconds; (void)vertical;
    (void)tick; (void)sep;
#if CLOCK_RESOURCE_KIND == 1
    tick = ticks_at(phase_local(instant, offset),
                    seconds ? UINT64_C(100000) : UINT64_C(1000));
    if (seconds)
        written = snprintf(text, sizeof(text), "%llu%s%02llu%s%02llu",
            (unsigned long long)(tick / 10000U), sep,
            (unsigned long long)((tick / 100U) % 100U), sep,
            (unsigned long long)(tick % 100U));
    else
        written = snprintf(text, sizeof(text), "%llu%s%02llu",
            (unsigned long long)(tick / 100U), sep,
            (unsigned long long)(tick % 100U));
#elif CLOCK_RESOURCE_KIND == 2
    tick = ticks_at((uint64_t)positive_modulo(
            positive_modulo(instant, CLOCK_MICROSECONDS_PER_DAY) +
                INT64_C(3600000000), CLOCK_MICROSECONDS_PER_DAY),
            seconds ? UINT64_C(100000) : UINT64_C(1000));
    written = seconds
        ? snprintf(text, sizeof(text), "@%03llu.%02llu",
            (unsigned long long)(tick / 100U),
            (unsigned long long)(tick % 100U))
        : snprintf(text, sizeof(text), "@%03llu",
            (unsigned long long)tick);
#elif CLOCK_RESOURCE_KIND == 3
    written = snprintf(text, sizeof(text), "%lld",
        (long long)floor_divide(instant, CLOCK_MICROSECONDS_PER_SECOND));
#elif CLOCK_RESOURCE_KIND == 4
    tick = ticks_at(phase_local(instant, offset), UINT64_C(65536));
    written = snprintf(text, sizeof(text), "%04llX", (unsigned long long)tick);
#elif CLOCK_RESOURCE_KIND == 5
    {
        const uint64_t whole = phase_local(instant, offset) /
            (uint64_t)CLOCK_MICROSECONDS_PER_SECOND;
        const unsigned hour = (unsigned)(whole / 3600U);
        const unsigned minute = (unsigned)((whole / 60U) % 60U);
        const unsigned second = (unsigned)(whole % 60U);
        char h[6], m[7], s[7];
        to_binary(hour, 5U, h);
        to_binary(minute, 6U, m);
        to_binary(second, 6U, s);
        written = seconds
            ? snprintf(text, sizeof(text), "%s%s%s%s%s", h, sep, m, sep, s)
            : snprintf(text, sizeof(text), "%s%s%s", h, sep, m);
    }
#else
#error unsupported CLOCK_RESOURCE_KIND
#endif
    if (buffer == NULL || written <= 0 || (size_t)written >= sizeof(text) ||
        (size_t)written >= capacity) return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool clock_next_tick(const InfiltratrTemporalClockProvider *provider,
                            int64_t instant, int32_t offset, bool seconds,
                            double latitude, double longitude, uint32_t *milliseconds)
{
    uint64_t phase, units, tick, boundary, remaining, result;
    (void)provider; (void)latitude; (void)longitude; (void)offset; (void)seconds;
    if (milliseconds == NULL) return false;
#if CLOCK_RESOURCE_KIND == 1
    units = seconds ? UINT64_C(100000) : UINT64_C(1000);
    phase = phase_local(instant, offset);
#elif CLOCK_RESOURCE_KIND == 2
    units = seconds ? UINT64_C(100000) : UINT64_C(1000);
    phase = (uint64_t)positive_modulo(
        positive_modulo(instant, CLOCK_MICROSECONDS_PER_DAY) +
            INT64_C(3600000000), CLOCK_MICROSECONDS_PER_DAY);
#elif CLOCK_RESOURCE_KIND == 3
    units = UINT64_C(86400);
    phase = (uint64_t)positive_modulo(instant, CLOCK_MICROSECONDS_PER_DAY);
#elif CLOCK_RESOURCE_KIND == 4
    units = UINT64_C(65536);
    phase = phase_local(instant, offset);
#elif CLOCK_RESOURCE_KIND == 5
    units = seconds ? UINT64_C(86400) : UINT64_C(1440);
    phase = phase_local(instant, offset);
#else
#error unsupported CLOCK_RESOURCE_KIND
#endif
    tick = ticks_at(phase, units);
    boundary = ((tick + 1U) * (uint64_t)CLOCK_MICROSECONDS_PER_DAY +
                 units - 1U) / units;
    remaining = boundary > phase ? boundary - phase : 1U;
    result = (remaining + UINT64_C(999)) / UINT64_C(1000);
    if (result == 0U || result > UINT32_MAX) return false;
    *milliseconds = (uint32_t)result;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    CLOCK_RESOURCE_ID, CLOCK_RESOURCE_NAME,
    CLOCK_RESOURCE_SECONDS, false, false
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    CLOCK_RESOURCE_ID, clock_render, clock_next_tick
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
