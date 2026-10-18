// SPDX-License-Identifier: GPL-3.0-or-later
/* Independent Han-era 100-ke clock; owns notation and exact boundary timing. */
#include <infiltratr/temporal_clock_resource.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DAY_US INT64_C(86400000000)
#define SECOND_US INT64_C(1000000)
#define KE_US INT64_C(864000000)

static int64_t positive_mod(int64_t n, int64_t divisor)
{
    const int64_t r = n % divisor;
    return r < 0 ? r + divisor : r;
}

static uint64_t local_phase(int64_t instant, int32_t offset)
{
    const int64_t shifted = positive_mod(instant, DAY_US) +
                            (int64_t)(offset % INT32_C(86400)) * SECOND_US;
    return (uint64_t)positive_mod(shifted, DAY_US);
}

static const char *ke_label(const char *language)
{
    if (language == NULL) return "刻";
    if (strncmp(language, "zh", 2U) == 0) return "刻";
    if (strncmp(language, "ja", 2U) == 0) return "刻";
    if (strncmp(language, "ru", 2U) == 0) return "кэ";
    return "kè";
}

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *language, int64_t instant, int32_t offset,
                   bool seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t capacity,
                   size_t *length)
{
    char text[96];
    const uint64_t ke = local_phase(instant, offset) / (uint64_t)KE_US;
    const char *label = ke_label(language);
    int written;
    (void)provider; (void)seconds; (void)latitude; (void)longitude;
    written = snprintf(text, sizeof(text),
                       language == NULL
                         ? (vertical ? "%02llu\n%s" : "%02llu%s")
                         : (vertical ? "%02llu\n%s" : "%02llu %s"),
                       (unsigned long long)ke, label);
    if (buffer == NULL || written <= 0 || (size_t)written >= sizeof(text) ||
        (size_t)written >= capacity) return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool next_tick(const InfiltratrTemporalClockProvider *provider,
                      int64_t instant, int32_t offset, bool seconds,
                      double latitude, double longitude, uint32_t *millis)
{
    const uint64_t phase = local_phase(instant, offset);
    const uint64_t left = (uint64_t)KE_US - phase % (uint64_t)KE_US;
    (void)provider; (void)seconds; (void)latitude; (void)longitude;
    if (millis == NULL) return false;
    *millis = (uint32_t)((left + UINT64_C(999)) / UINT64_C(1000));
    return *millis > 0U;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    "chinese-ke", "Chinese hundred-kè time", false, false, false
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    "chinese-ke", render, next_tick
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
