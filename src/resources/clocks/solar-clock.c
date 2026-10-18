// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent local apparent solar clock (SOL).
 * NOAA/GML compact fractional-year equation-of-time approximation, matching
 * the Infiltrator documented presentation model (not an ephemeris).
 * Time is a canonical Unix-microsecond instant; longitude is east positive.
 * Locale, conventional time zone and daylight-saving rules do not change
 * solar geometry. Exact visible boundaries are resolved from the resource's
 * own output rather than a duplicate application polling policy.
 */
#include <infiltratr/temporal_clock_resource.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SECOND_US INT64_C(1000000)
#define DAY_US INT64_C(86400000000)
#define PI 3.141592653589793238462643383279502884L

static int64_t floor_div(int64_t n, int64_t divisor)
{
    const int64_t q = n / divisor;
    return n % divisor < 0 ? q - 1 : q;
}

static int64_t positive_mod(int64_t n, int64_t divisor)
{
    const int64_t r = n % divisor;
    return r < 0 ? r + divisor : r;
}

/* Exact Gregorian 400-year arithmetic; no time_t range or locale dependence. */
static bool day_of_year(int64_t instant, int *ordinal)
{
    static const int cumulative[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    const int64_t days = floor_div(floor_div(instant, SECOND_US),
                                   INT64_C(86400));
    const int64_t z = days + INT64_C(719468);
    const int64_t era = floor_div(z, INT64_C(146097));
    const int64_t doe = z - era * INT64_C(146097);
    const int64_t yoe = (doe - doe / INT64_C(1460) +
                        doe / INT64_C(36524) -
                        doe / INT64_C(146096)) / INT64_C(365);
    const int64_t doy_march = doe -
        (INT64_C(365) * yoe + yoe / INT64_C(4) - yoe / INT64_C(100));
    const int64_t mp =
        (INT64_C(5) * doy_march + INT64_C(2)) / INT64_C(153);
    const int day = (int)(doy_march -
        (INT64_C(153) * mp + INT64_C(2)) / INT64_C(5) + 1);
    const int month = (int)(mp + (mp < 10 ? 3 : -9));
    const int64_t year = yoe + era * INT64_C(400) + (month <= 2 ? 1 : 0);
    const bool leap =
        (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (ordinal == NULL || month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    *ordinal = cumulative[month - 1] + day +
               (leap && month > 2 ? 1 : 0);
    return true;
}

static bool apparent_seconds(int64_t instant, double longitude,
                             long double *result)
{
    const long double utc_phase = (long double)positive_mod(instant, DAY_US);
    const long double fractional_hour =
        utc_phase / (3600.0L * (long double)SECOND_US);
    long double phase, seconds;
    double equation_minutes;
    int ordinal;
    if (result == NULL || !isfinite(longitude) ||
        longitude < -180.0 || longitude > 180.0 ||
        !day_of_year(instant, &ordinal))
        return false;

    phase = 2.0L * PI / 365.0L *
            ((long double)ordinal - 1.0L +
             (fractional_hour - 12.0L) / 24.0L);
    equation_minutes = 229.18 *
        (0.000075 + 0.001868 * cos((double)phase) -
         0.032077 * sin((double)phase) -
         0.014615 * cos((double)(2.0L * phase)) -
         0.040849 * sin((double)(2.0L * phase)));

    seconds = utc_phase / (long double)SECOND_US +
              (long double)longitude * 240.0L +
              (long double)equation_minutes * 60.0L;
    seconds = fmodl(seconds, 86400.0L);
    if (seconds < 0.0L) seconds += 86400.0L;
    *result = seconds;
    return true;
}

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *locale, int64_t instant, int32_t offset,
                   bool show_seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t capacity,
                   size_t *length)
{
    const char *sep = vertical ? "\n" : ":";
    const char *suffix_sep = vertical ? "\n" : " ";
    long double solar;
    int64_t whole;
    unsigned int hour, minute, second;
    char text[96];
    int written;
    (void)provider; (void)locale; (void)offset; (void)latitude;
    if (!apparent_seconds(instant, longitude, &solar))
        return false;
    whole = (int64_t)floorl(solar);
    hour = (unsigned int)(whole / INT64_C(3600));
    minute = (unsigned int)((whole / INT64_C(60)) % INT64_C(60));
    second = (unsigned int)(whole % INT64_C(60));
    if (show_seconds)
        written = snprintf(text, sizeof(text), "%02u%s%02u%s%02u%sSOL",
                           hour, sep, minute, sep, second, suffix_sep);
    else
        written = snprintf(text, sizeof(text), "%02u%s%02u%sSOL",
                           hour, sep, minute, suffix_sep);
    if (buffer == NULL || written <= 0 ||
        (size_t)written >= sizeof(text) || (size_t)written >= capacity)
        return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool text_changed(int64_t instant, uint32_t delta_ms,
                         bool show_seconds, double longitude,
                         const char *original)
{
    char next[96] = {0};
    const int64_t delta = (int64_t)delta_ms * INT64_C(1000);
    if (instant > INT64_MAX - delta)
        return false;
    if (!render(NULL, NULL, instant + delta, 0, show_seconds,
                false, 0.0, longitude, next, sizeof(next), NULL))
        return false;
    return strcmp(original, next) != 0;
}

/* Binary-search the first displayed change at millisecond resolution. */
static bool next_tick(const InfiltratrTemporalClockProvider *provider,
                      int64_t instant, int32_t offset, bool show_seconds,
                      double latitude, double longitude,
                      uint32_t *milliseconds)
{
    char original[96] = {0};
    uint32_t low = 0U;
    uint32_t high = show_seconds ? 2000U : 61000U;
    (void)provider; (void)offset; (void)latitude;
    if (milliseconds == NULL ||
        !render(NULL, NULL, instant, 0, show_seconds, false,
                0.0, longitude, original, sizeof(original), NULL) ||
        !text_changed(instant, high, show_seconds, longitude, original))
        return false;
    while (high - low > 1U) {
        const uint32_t middle = low + (high - low) / 2U;
        if (text_changed(instant, middle, show_seconds,
                         longitude, original))
            high = middle;
        else
            low = middle;
    }
    *milliseconds = high;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    "solar", "Apparent solar time (compact Sun-position approximation)",
    true, false, true
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    "solar", render, next_tick
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
