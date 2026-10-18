// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent Chinese twelve-double-hour clock resource.
 * Preserves native Earthly Branch names and translated text while keeping
 * the authentic Zi-hour 23:00 boundary and first/second-hour precision.
 * No Calendar application, Common clock renderer or private locale table
 * is linked into this module.
 */
#include <infiltratr/temporal_clock_resource.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DAY_US INT64_C(86400000000)
#define HOUR_US INT64_C(3600000000)
#define DOUBLE_HOUR_US INT64_C(7200000000)

typedef struct ClockWords {
    const char *coarse_prefix;
    const char *coarse_suffix;
    const char *fine_separator;
    const char *first;
    const char *second;
    const char *hour;
    bool suffix_style;
} ClockWords;

static const char *const pinyin_branches[] = {
    "Zǐ", "Chǒu", "Yín", "Mǎo", "Chén", "Sì",
    "Wǔ", "Wèi", "Shēn", "Yǒu", "Xū", "Hài"
};
static const char *const native_branches[] = {
    "子", "丑", "寅", "卯", "辰", "巳",
    "午", "未", "申", "酉", "戌", "亥"
};

static int64_t positive_mod(int64_t value, int64_t period)
{
    const int64_t rem = value % period;
    return rem < 0 ? rem + period : rem;
}

static int64_t shifted_phase(int64_t instant, int32_t offset)
{
    const int64_t utc = positive_mod(instant, DAY_US);
    const int64_t shifted =
        utc + (int64_t)(offset % INT32_C(86400)) * INT64_C(1000000) +
        HOUR_US;
    return positive_mod(shifted, DAY_US);
}

static bool language_prefix(const char *locale, const char *prefix)
{
    const size_t size = strlen(prefix);
    if (locale == NULL || strncmp(locale, prefix, size) != 0)
        return false;
    return locale[size] == '\0' || locale[size] == '_' ||
           locale[size] == '-' || locale[size] == '.' ||
           locale[size] == '@';
}

static bool traditional_chinese(const char *locale)
{
    return strstr(locale, "Hant") != NULL ||
           strstr(locale, "TW") != NULL ||
           strstr(locale, "HK") != NULL ||
           strstr(locale, "MO") != NULL;
}

static ClockWords words_for(const char *locale)
{
    if (language_prefix(locale, "zh")) {
        const bool traditional = traditional_chinese(locale);
        return (ClockWords){
            "", traditional ? "時" : "时", "",
            "初", "正", traditional ? "時" : "时", true
        };
    }
    if (language_prefix(locale, "ja"))
        return (ClockWords){ "", "時", "", "初", "正", "時", true };
    if (language_prefix(locale, "de"))
        return (ClockWords){ "", "-Stunde", ", ",
                             "erste Hälfte", "zweite Hälfte", "Stunde", true };
    if (language_prefix(locale, "es"))
        return (ClockWords){ "Hora ", "", ", ",
                             "primera mitad", "segunda mitad", "Hora", false };
    if (language_prefix(locale, "fr"))
        return (ClockWords){ "Heure ", "", ", ",
                             "première moitié", "seconde moitié", "Heure", false };
    if (language_prefix(locale, "it"))
        return (ClockWords){ "Ora ", "", ", ",
                             "prima metà", "seconda metà", "Ora", false };
    if (language_prefix(locale, "pt"))
        return (ClockWords){ "Hora ", "", ", ",
                             "primeira metade", "segunda metade", "Hora", false };
    if (language_prefix(locale, "ru"))
        return (ClockWords){ "Час ", "", ", ",
                             "первая половина", "вторая половина", "Час", false };
    return (ClockWords){ "", " hour", ", ",
                         "first half", "second half", "hour", true };
}

static bool render(const InfiltratrTemporalClockProvider *provider,
                   const char *locale, int64_t instant, int32_t offset,
                   bool show_seconds, bool vertical, double latitude,
                   double longitude, char *buffer, size_t capacity,
                   size_t *length)
{
    const int64_t phase = shifted_phase(instant, offset);
    const size_t branch_index = (size_t)(phase / DOUBLE_HOUR_US);
    const ClockWords words = words_for(locale);
    const char *branch =
        (language_prefix(locale, "zh") || language_prefix(locale, "ja"))
            ? native_branches[branch_index] : pinyin_branches[branch_index];
    const char *half = phase % DOUBLE_HOUR_US < HOUR_US
        ? words.first : words.second;
    char text[128];
    int written;
    (void)provider; (void)latitude; (void)longitude;

    if (show_seconds) {
        if (vertical) {
            written = snprintf(text, sizeof(text), "%s\n%s", branch, half);
        } else {
            written = snprintf(text, sizeof(text), "%s%s%s",
                               branch, words.fine_separator, half);
        }
    } else if (vertical) {
        written = snprintf(text, sizeof(text), "%s\n%s", branch, words.hour);
    } else if (words.suffix_style) {
        written = snprintf(text, sizeof(text), "%s%s", branch, words.coarse_suffix);
    } else {
        written = snprintf(text, sizeof(text), "%s%s", words.coarse_prefix, branch);
    }
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
    const int64_t period = show_seconds ? HOUR_US : DOUBLE_HOUR_US;
    const int64_t phase = shifted_phase(instant, offset);
    const uint64_t left = (uint64_t)(period - phase % period);
    const uint64_t ms = (left + UINT64_C(999)) / UINT64_C(1000);
    (void)provider; (void)latitude; (void)longitude;
    if (milliseconds == NULL || ms == 0U || ms > UINT32_MAX)
        return false;
    *milliseconds = (uint32_t)ms;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    "chinese-time", "Traditional Chinese double-hour time",
    true, false, false
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    "chinese-time", render, next_tick
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
