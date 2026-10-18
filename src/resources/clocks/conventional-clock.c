// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent civil wall-clock resource, compiled for standard-12 and
 * standard-24. Common supplies discovery/ABI, not the time arithmetic.
 * Only explicit selections use this clock. OS locale-following "standard"
 * stays in Cinnamon's own WallClock and is deliberately not overridden.
 */
#define _GNU_SOURCE 1
#include <infiltratr/temporal_clock_resource.h>
#include <langinfo.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef CONVENTIONAL_CLOCK_KIND
#error CONVENTIONAL_CLOCK_KIND must be 12 or 24
#endif
#if CONVENTIONAL_CLOCK_KIND != 12 && CONVENTIONAL_CLOCK_KIND != 24
#error Unsupported conventional clock kind
#endif

#define DAY_US INT64_C(86400000000)
#define SECOND_US INT64_C(1000000)
#if CONVENTIONAL_CLOCK_KIND == 12
#define CLOCK_ID "standard-12"
#define CLOCK_NAME "Standard time (12-hour)"
#else
#define CLOCK_ID "standard-24"
#define CLOCK_NAME "Standard time (24-hour)"
#endif

static int64_t positive_mod(int64_t n, int64_t mod)
{
    const int64_t r = n % mod;
    return r < 0 ? r + mod : r;
}

static uint64_t local_phase(int64_t epoch, int32_t offset)
{
    /* Reduce offset before multiplication to avoid overflow at extremes. */
    const int64_t phase = positive_mod(epoch, DAY_US);
    const int64_t shifted =
        phase + (int64_t)(offset % 86400) * SECOND_US;
    return (uint64_t)positive_mod(shifted, DAY_US);
}

#if CONVENTIONAL_CLOCK_KIND == 12
static bool day_period(const char *language, bool am, char *out, size_t cap)
{
    const char *fallback = am ? "AM" : "PM";
    locale_t loc = (locale_t)0;
    /* These UI-language forms and their leading placement must remain valid
     * even when the host has not generated the corresponding libc locale. */
    if (language != NULL && strlen(language) >= 2U &&
        (language[2] == '\0' || strchr("_-.@", language[2]) != NULL)) {
        if (strncmp(language,"ja",2U) == 0) {
            snprintf(out,cap,"%s",am ? "午前" : "午後");
            return true;
        }
        if (strncmp(language,"zh",2U) == 0) {
            snprintf(out,cap,"%s",am ? "上午" : "下午");
            return true;
        }
    }
    if (language != NULL && *language != '\0') {
        char name[96];
        const size_t length = strcspn(language, ".@");
        if (length > 0U && length < sizeof(name) - 7U) {
            memcpy(name, language, length);
            name[length] = '\0';
            loc = newlocale(LC_TIME_MASK, name, (locale_t)0);
            if (!loc) {
                snprintf(name + length, sizeof(name) - length, ".UTF-8");
                loc = newlocale(LC_TIME_MASK, name, (locale_t)0);
            }
        }
    }
    const char *period = loc ? nl_langinfo_l(am ? AM_STR : PM_STR, loc) : NULL;
    if (period == NULL || period[0] == '\0')
        period = fallback;
    snprintf(out, cap, "%s", period);
    if (loc) freelocale(loc);
    return false;
}
#endif

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *locale, int64_t instant, int32_t offset,
                   bool seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t cap, size_t *length)
{
    char text[128] = {0};
    const uint64_t whole = local_phase(instant, offset) / SECOND_US;
    const unsigned hour24 = (unsigned)(whole / 3600U);
    const unsigned minute = (unsigned)((whole / 60U) % 60U);
    const unsigned second = (unsigned)(whole % 60U);
    const char *sep = vertical ? "\n" : ":";
    int written;
    (void)provider; (void)latitude; (void)longitude;
#if CONVENTIONAL_CLOCK_KIND == 12
    char period[48] = {0};
    const unsigned hour12 = hour24 % 12U == 0U ? 12U : hour24 % 12U;
    const bool leading = day_period(locale, hour24 < 12U, period, sizeof(period));
    if (leading) {
        written = seconds
            ? snprintf(text,sizeof(text),"%s%s%u%s%02u%s%02u",
                period,vertical ? "\n" : "",hour12,sep,minute,sep,second)
            : snprintf(text,sizeof(text),"%s%s%u%s%02u",
                period,vertical ? "\n" : "",hour12,sep,minute);
    } else if (seconds)
        written = snprintf(text, sizeof(text), "%u%s%02u%s%02u%s%s",
            hour12, sep, minute, sep, second, vertical ? "\n" : " ", period);
    else
        written = snprintf(text, sizeof(text), "%u%s%02u%s%s",
            hour12, sep, minute, vertical ? "\n" : " ", period);
#else
    (void)locale;
    if (seconds)
        written = snprintf(text, sizeof(text), "%02u%s%02u%s%02u",
                           hour24, sep, minute, sep, second);
    else
        written = snprintf(text, sizeof(text), "%02u%s%02u",
                           hour24, sep, minute);
#endif
    if (buffer == NULL || cap == 0U || written <= 0 ||
        (size_t)written >= cap || (size_t)written >= sizeof(text))
        return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool next_tick(const InfiltratrTemporalClockProvider *provider,
                      int64_t instant, int32_t offset, bool seconds,
                      double latitude, double longitude, uint32_t *millis)
{
    const uint64_t period = seconds ? UINT64_C(1000000) : UINT64_C(60000000);
    const uint64_t phase = local_phase(instant, offset) % period;
    const uint64_t remain = period - phase;
    (void)provider; (void)latitude; (void)longitude;
    if (millis == NULL) return false;
    *millis = (uint32_t)((remain + UINT64_C(999)) / UINT64_C(1000));
    return *millis > 0U;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    CLOCK_ID, CLOCK_NAME, true, false, false
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    CLOCK_ID, render, next_tick
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
