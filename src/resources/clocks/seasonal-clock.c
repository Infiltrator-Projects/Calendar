// SPDX-License-Identifier: GPL-3.0-or-later
/* Seasonal clock resources. Sole owner of clock-face arithmetic and scheduling. */

#if defined(CLOCK_ROMAN)
#define CLOCK_ID "roman-temporal"
#define CLOCK_NAME "Roman seasonal time (12 unequal daylight hours; night in 4 military watches)"
#define CLOCK_SECONDS true
#define CLOCK_LOCATION true
#elif defined(CLOCK_JAPANESE_EARLY)
#define CLOCK_ID "japanese-temporal-early"
#define CLOCK_NAME "Early Edo Japanese unequal hours (sunrise/sunset; 6 day / 6 night)"
#define CLOCK_SECONDS false
#define CLOCK_LOCATION true
#elif defined(CLOCK_JAPANESE_LATE)
#define CLOCK_ID "japanese-temporal"
#define CLOCK_NAME "Late Edo Japanese unequal hours (1797 twilight; 6 day / 6 night)"
#define CLOCK_SECONDS false
#define CLOCK_LOCATION true
#elif defined(CLOCK_NUREMBERG_LOCAL)
#define CLOCK_ID "nuremberg-solar"
#define CLOCK_NAME "Nuremberg-style seasonal reconstruction (location-based, not the fixed civic schedule)"
#define CLOCK_SECONDS true
#define CLOCK_LOCATION true
#else
#define CLOCK_ID "nuremberg-hours"
#define CLOCK_NAME "Historical Nuremberg Great Clock (equal hours; day/night counts change on fixed dates)"
#define CLOCK_SECONDS true
#define CLOCK_LOCATION false
#endif
#include <infiltratr/temporal_clock_resource.h>

#include "infiltratr/arithmetic.h"
#include "infiltratr/core.h"
#include "infiltratr/i18n.h"
#include "infiltratr/timing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MICROSECONDS_PER_SECOND INT64_C(1000000)
#define SECONDS_PER_MINUTE INT64_C(60)
#define MINUTES_PER_HOUR INT64_C(60)
#define SECONDS_PER_HOUR INT64_C(3600)
#define SECONDS_PER_DAY INT64_C(86400)
#define MICROSECONDS_PER_DAY (SECONDS_PER_DAY * MICROSECONDS_PER_SECOND)
#define INTERNET_TICKS_PER_DAY UINT64_C(1000)
#define HEX_TICKS_PER_DAY UINT64_C(65536)
#define DECIMAL_SECONDS_PER_DAY UINT64_C(100000)
#define CHINESE_DOUBLE_HOURS_PER_DAY UINT64_C(12)
#define CHINESE_KE_PER_DAY UINT64_C(100)
#define VIGHATI_PER_DAY UINT64_C(3600)
#define SIDEREAL_RATE_PER_SI_SECOND (24.06570982441908L / 24.0L)
#define INFILTRATR_PI 3.141592653589793238462643383279502884L

static bool write_text(char *buffer, size_t capacity, size_t *length,
                       const char *text)
{
    size_t needed;

    if (buffer == NULL || capacity == 0U || text == NULL) {
        return false;
    }
    needed = strlen(text);
    if (needed >= capacity) {
        return false;
    }
    memcpy(buffer, text, needed + 1U);
    if (length != NULL) {
        *length = needed;
    }
    return true;
}

static bool finish_output(char *buffer,size_t capacity,size_t *length,int written)
{
    if (written < 0 || (size_t)written >= capacity) {
        if (buffer != NULL && capacity != 0U) buffer[0]='\0';
        return false;
    }
    if (length != NULL) *length=(size_t)written;
    return true;
}

static int64_t floor_divide(int64_t value, int64_t divisor)
{
    int64_t quotient = 0;

    if (!infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL)) {
        return 0;
    }
    return quotient;
}

static int64_t positive_modulo(int64_t value, int64_t modulus)
{
    int64_t remainder = 0;

    if (!infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder)) {
        return 0;
    }
    return remainder;
}

/*
 * Convert a Unix day number to Gregorian day-of-year with exact 400-year
 * cycle arithmetic. Keeping this independent of time_t makes astronomical
 * formatting deterministic on 32-bit and 64-bit hosts alike.
 */
static bool gregorian_day_of_year(int64_t unix_microseconds,
                                  int *day_of_year,
                                  bool *is_leap_year)
{
    static const int cumulative_days[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int64_t unix_seconds;
    int64_t days;
    int64_t z;
    int64_t era;
    int64_t doe;
    int64_t yoe;
    int64_t doy_march;
    int64_t mp;
    int64_t year;
    int month;
    int day;
    bool leap;

    if (day_of_year == NULL) {
        return false;
    }
    unix_seconds = floor_divide(unix_microseconds, MICROSECONDS_PER_SECOND);
    days = floor_divide(unix_seconds, SECONDS_PER_DAY);
    if (!infiltratr_i64_add_checked(days, INT64_C(719468), &z)) {
        return false;
    }

    era = floor_divide(z, INT64_C(146097));
    doe = z - era * INT64_C(146097);
    yoe = (doe - doe / INT64_C(1460) + doe / INT64_C(36524) -
           doe / INT64_C(146096)) / INT64_C(365);
    year = yoe + era * INT64_C(400);
    doy_march = doe - (INT64_C(365) * yoe + yoe / INT64_C(4) -
                       yoe / INT64_C(100));
    mp = (INT64_C(5) * doy_march + INT64_C(2)) / INT64_C(153);
    day = (int)(doy_march - (INT64_C(153) * mp + INT64_C(2)) /
                INT64_C(5) + INT64_C(1));
    month = (int)(mp + (mp < 10 ? 3 : -9));
    if (month <= 2) {
        year++;
    }

    leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    *day_of_year = cumulative_days[month - 1] + day +
                   (leap && month > 2 ? 1 : 0);
    if (is_leap_year != NULL) {
        *is_leap_year = leap;
    }
    return true;
}

/*
 * Compact equation-of-time and solar-declination approximation shared by the
 * apparent-solar and seasonal clock families. Both values come from the same
 * fractional-year phase so all location-aware modes remain internally
 * consistent.
 */
static bool solar_terms(int64_t unix_microseconds,
                        double *equation_minutes,
                        double *declination_radians)
{
    const int64_t utc_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    const long double fractional_hour =
        (long double)utc_phase /
        ((long double)SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND);
    int day_of_year;
    long double gamma;

    if (!gregorian_day_of_year(unix_microseconds, &day_of_year, NULL)) {
        return false;
    }
    gamma = 2.0L * INFILTRATR_PI / 365.0L *
            ((long double)day_of_year - 1.0L +
             (fractional_hour - 12.0L) / 24.0L);

    if (equation_minutes != NULL) {
        *equation_minutes = 229.18 *
            (0.000075 + 0.001868 * cos((double)gamma) -
             0.032077 * sin((double)gamma) -
             0.014615 * cos((double)(2.0L * gamma)) -
             0.040849 * sin((double)(2.0L * gamma)));
    }
    if (declination_radians != NULL) {
        *declination_radians =
            0.006918 - 0.399912 * cos((double)gamma) +
            0.070257 * sin((double)gamma) -
            0.006758 * cos((double)(2.0L * gamma)) +
            0.000907 * sin((double)(2.0L * gamma)) -
            0.002697 * cos((double)(3.0L * gamma)) +
            0.001480 * sin((double)(3.0L * gamma));
    }
    return true;
}

static double clamp_longitude(double longitude)
{
    return isfinite(longitude)
        ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
}

static double clamp_latitude(double latitude)
{
    return isfinite(latitude)
        ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
}

static long double mean_solar_seconds(int64_t unix_microseconds,
                                      double longitude)
{
    const int64_t utc_microseconds =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    long double seconds =
        (long double)utc_microseconds / MICROSECONDS_PER_SECOND +
        (long double)clamp_longitude(longitude) * 240.0L;

    seconds = fmodl(seconds, (long double)SECONDS_PER_DAY);
    if (seconds < 0.0L) {
        seconds += (long double)SECONDS_PER_DAY;
    }
    return seconds;
}

static long double apparent_solar_seconds(int64_t unix_microseconds,
                                          double longitude)
{
    double equation_minutes = 0.0;
    long double seconds;

    if (!solar_terms(unix_microseconds, &equation_minutes, NULL)) {
        return mean_solar_seconds(unix_microseconds, longitude);
    }
    seconds = mean_solar_seconds(unix_microseconds, longitude) +
              (long double)equation_minutes * 60.0L;
    seconds = fmodl(seconds, (long double)SECONDS_PER_DAY);
    if (seconds < 0.0L) {
        seconds += (long double)SECONDS_PER_DAY;
    }
    return seconds;
}

static bool solar_day_boundaries(int64_t unix_microseconds,
                                 double latitude,
                                 double solar_depression_degrees,
                                 long double *dawn_seconds,
                                 long double *dusk_seconds)
{
    const double safe_latitude = clamp_latitude(latitude);
    double declination = 0.0;
    double latitude_radians;
    double zenith_radians;
    double cosine_hour_angle;
    long double hour_angle;
    long double half_day_seconds;

    if (dawn_seconds == NULL || dusk_seconds == NULL ||
        !isfinite(solar_depression_degrees) ||
        solar_depression_degrees < 0.0 ||
        solar_depression_degrees >= 90.0 ||
        !solar_terms(unix_microseconds, NULL, &declination)) {
        return false;
    }

    latitude_radians = safe_latitude * (double)INFILTRATR_PI / 180.0;
    zenith_radians =
        (90.0 + solar_depression_degrees) * (double)INFILTRATR_PI / 180.0;
    if (fabs(cos(latitude_radians) * cos(declination)) < 1.0e-12) {
        return false;
    }

    cosine_hour_angle =
        cos(zenith_radians) /
            (cos(latitude_radians) * cos(declination)) -
        tan(latitude_radians) * tan(declination);
    if (cosine_hour_angle < -1.0 || cosine_hour_angle > 1.0) {
        return false;
    }

    hour_angle = acosl((long double)infiltratr_clamp_double(
        cosine_hour_angle, -1.0, 1.0));
    half_day_seconds =
        hour_angle * (long double)SECONDS_PER_DAY / (2.0L * INFILTRATR_PI);
    *dawn_seconds = (long double)SECONDS_PER_DAY / 2.0L - half_day_seconds;
    *dusk_seconds = (long double)SECONDS_PER_DAY / 2.0L + half_day_seconds;
    return true;
}

typedef struct SeasonalPeriod {
    bool daylight;
    unsigned index;
    long double fraction;
} SeasonalPeriod;

static bool seasonal_period_at(int64_t unix_microseconds,
                               double latitude,
                               double longitude,
                               double solar_depression_degrees,
                               unsigned daylight_parts,
                               unsigned night_parts,
                               SeasonalPeriod *period)
{
    long double dawn;
    long double dusk;
    const long double solar =
        apparent_solar_seconds(unix_microseconds, longitude);
    long double span;
    long double position;
    long double unit;
    unsigned parts;

    if (period == NULL || daylight_parts == 0U || night_parts == 0U ||
        !solar_day_boundaries(unix_microseconds, latitude,
                              solar_depression_degrees, &dawn, &dusk)) {
        return false;
    }

    if (solar >= dawn && solar < dusk) {
        period->daylight = true;
        span = dusk - dawn;
        position = solar - dawn;
        parts = daylight_parts;
    } else {
        period->daylight = false;
        span = (long double)SECONDS_PER_DAY - (dusk - dawn);
        position = solar >= dusk
            ? solar - dusk
            : (long double)SECONDS_PER_DAY - dusk + solar;
        parts = night_parts;
    }

    unit = span / parts;
    {
        long double scaled = position / unit;

        if (scaled < 0.0L) {
            scaled = 0.0L;
        }
        if (scaled >= (long double)parts) {
            scaled = nextafterl((long double)parts, 0.0L);
        }
        period->index = (unsigned)floorl(scaled);
        period->fraction = scaled - (long double)period->index;
    }
    return true;
}

static bool nuremberg_local_hour_allocation(int64_t unix_microseconds,
                                            double latitude,
                                            unsigned *day_hours,
                                            unsigned *night_hours,
                                            long double *day_start,
                                            long double *day_end)
{
    long double dawn;
    long double dusk;
    long double rounded;
    unsigned day_count;

    if (day_hours == NULL || night_hours == NULL ||
        day_start == NULL || day_end == NULL ||
        !solar_day_boundaries(
            unix_microseconds, latitude, 0.833, &dawn, &dusk)) {
        return false;
    }

    rounded = floorl(
        (dusk - dawn) / (long double)SECONDS_PER_HOUR + 0.5L);
    if (rounded < 8.0L) rounded = 8.0L;
    if (rounded > 16.0L) rounded = 16.0L;
    day_count = (unsigned)rounded;

    *day_hours = day_count;
    *night_hours = 24U - day_count;
    *day_start =
        ((long double)(24U - day_count) / 2.0L) * SECONDS_PER_HOUR;
    *day_end = *day_start +
        (long double)day_count * SECONDS_PER_HOUR;
    return true;
}

static bool nuremberg_historical_hour_allocation(
    int64_t unix_microseconds,
    unsigned *day_hours,
    unsigned *night_hours,
    long double *day_start,
    long double *day_end)
{
    /*
     * Nürnberg's 1488 Wendetage were recorded in the Julian calendar.
     * Converting those dates to the proleptic Gregorian calendar of that
     * epoch adds nine days. The allocation therefore changes on the modern
     * seasonal equivalents below, not at every actual sunrise/sunset.
     */
    static const int thresholds[] = {
        16, 37, 54, 71, 87, 104, 122, 144,
        201, 223, 241, 257, 274, 290, 309, 329
    };
    static const unsigned allocations[] = {
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U,
        15U, 14U, 13U, 12U, 11U, 10U, 9U, 8U
    };
    int day_of_year;
    bool leap = false;
    unsigned count = 8U;
    size_t index;

    if (day_hours == NULL || night_hours == NULL ||
        day_start == NULL || day_end == NULL ||
        !gregorian_day_of_year(unix_microseconds, &day_of_year, &leap)) {
        return false;
    }

    for (index = 0U; index < INFILTRATR_ARRAY_LENGTH(thresholds); ++index) {
        int threshold = thresholds[index];
        if (leap && threshold >= 71) {
            threshold += 1;
        }
        if (day_of_year < threshold) {
            break;
        }
        count = allocations[index];
    }

    *day_hours = count;
    *night_hours = 24U - count;
    *day_start =
        ((long double)(24U - count) / 2.0L) * SECONDS_PER_HOUR;
    *day_end = *day_start + (long double)count * SECONDS_PER_HOUR;
    return true;
}

static bool format_nuremberg_hours(char *buffer,
                                   size_t capacity,
                                   size_t *length,
                                   int64_t unix_microseconds,
                                   bool show_seconds,
                                   bool vertical,
                                   bool historical,
                                   double latitude,
                                   double longitude)
{
    static const double nuremberg_longitude = 11.0767;
    unsigned day_hours;
    unsigned night_hours;
    long double day_start;
    long double day_end;
    const double clock_longitude = historical ? nuremberg_longitude : longitude;
    const long double solar =
        apparent_solar_seconds(unix_microseconds, clock_longitude);
    long double elapsed;
    int hour;
    int minute;
    int second;
    const char *suffix;
    const char *separator = vertical ? "\n" : ":";
    bool ok;

    ok = historical
        ? nuremberg_historical_hour_allocation(
            unix_microseconds, &day_hours, &night_hours,
            &day_start, &day_end)
        : nuremberg_local_hour_allocation(
            unix_microseconds, latitude,
            &day_hours, &night_hours, &day_start, &day_end);
    if (!ok) {
        return write_text(buffer, capacity, length,
                          vertical ? "N/A\nNUR" : "N/A NUR");
    }

    if (solar >= day_start && solar < day_end) {
        elapsed = solar - day_start;
        suffix = historical ? "NUR-D" : "NUR-LD";
    } else {
        elapsed = solar >= day_end
            ? solar - day_end
            : (long double)SECONDS_PER_DAY - day_end + solar;
        suffix = historical ? "NUR-N" : "NUR-LN";
    }
    (void)day_hours;
    (void)night_hours;

    hour = (int)floorl(elapsed / SECONDS_PER_HOUR);
    minute = (int)floorl(
        fmodl(elapsed, (long double)SECONDS_PER_HOUR) /
        SECONDS_PER_MINUTE);
    second = (int)floorl(
        fmodl(elapsed, (long double)SECONDS_PER_MINUTE));

    if (show_seconds) {
        return finish_output(buffer, capacity, length,
            snprintf(buffer, capacity, "%02d%s%02d%s%02d%s%s", hour, separator, minute, separator, second, vertical ? "\n" : " ", suffix));
    }
    return finish_output(buffer, capacity, length,
            snprintf(buffer, capacity, "%02d%s%02d%s%s", hour, separator, minute, vertical ? "\n" : " ", suffix));
}

static bool mode_location_valid(const InfiltratrTemporalClockModeInfo *info,
                                bool location_configured,
                                double latitude,
                                double longitude)
{
    if (info == NULL) {
        return false;
    }
    if ((info->requires_latitude || info->requires_longitude) &&
        !location_configured) {
        return false;
    }
    return (!info->requires_latitude ||
            (isfinite(latitude) && latitude >= -90.0 && latitude <= 90.0)) &&
           (!info->requires_longitude ||
            (isfinite(longitude) && longitude >= -180.0 &&
             longitude <= 180.0));
}


/* Names are transliterated proper names, not animal/help labels. Wording
 * belongs to the selected language; the shared phase calculation stays below. */
#define CLOCK_REMAINING_WORDS(hour, watch, twelfth, twelfths) \
    { "hour", hour }, { "watch", watch }, \
    { "twelfth", twelfth }, { "twelfths", twelfths }
#define JAPANESE_BRANCHES \
    { "jp-ne", "Ne" }, { "jp-ushi", "Ushi" }, { "jp-tora", "Tora" }, \
    { "jp-u", "U" }, { "jp-tatsu", "Tatsu" }, { "jp-mi", "Mi" }, \
    { "jp-uma", "Uma" }, { "jp-hitsuji", "Hitsuji" }, { "jp-saru", "Saru" }, \
    { "jp-tori", "Tori" }, { "jp-inu", "Inu" }, { "jp-i", "I" }
#define JAPANESE_NATIVE_BRANCHES \
    { "jp-ne", "子" }, { "jp-ushi", "丑" }, { "jp-tora", "寅" }, \
    { "jp-u", "卯" }, { "jp-tatsu", "辰" }, { "jp-mi", "巳" }, \
    { "jp-uma", "午" }, { "jp-hitsuji", "未" }, { "jp-saru", "申" }, \
    { "jp-tori", "酉" }, { "jp-inu", "戌" }, { "jp-i", "亥" }
#define EDO_WORDS(horizontal, vertical) \
    { "edo", horizontal }, { "edo-vertical", vertical }
static const InfiltratrI18nEntry clock_en[] = {
    CLOCK_REMAINING_WORDS("Hour", "Watch", "twelfth", "twelfths"),
    JAPANESE_BRANCHES,
    EDO_WORDS("{branch} · {bell} bells", "{branch}\n{bell} bells"),
};
static const InfiltratrI18nEntry clock_de[] = {
    CLOCK_REMAINING_WORDS("Stunde", "Wache", "Zwölftel", "Zwölftel"),
    EDO_WORDS("{branch} · {bell} Schläge", "{branch}\n{bell} Schläge"),
};
static const InfiltratrI18nEntry clock_es[] = {
    CLOCK_REMAINING_WORDS("Hora", "Vigilia", "doceavo", "doceavos"),
    EDO_WORDS("{branch} · {bell} campanadas", "{branch}\n{bell} campanadas"),
};
static const InfiltratrI18nEntry clock_fr[] = {
    CLOCK_REMAINING_WORDS("Heure", "Veille", "douzième", "douzièmes"),
    EDO_WORDS("{branch} · {bell} coups", "{branch}\n{bell} coups"),
};
static const InfiltratrI18nEntry clock_it[] = {
    CLOCK_REMAINING_WORDS("Ora", "Vigilia", "dodicesimo", "dodicesimi"),
    EDO_WORDS("{branch} · {bell} rintocchi", "{branch}\n{bell} rintocchi"),
};
static const InfiltratrI18nEntry clock_pt[] = {
    CLOCK_REMAINING_WORDS("Hora", "Vigília", "duodécimo", "duodécimos"),
    EDO_WORDS("{branch} · {bell} badaladas", "{branch}\n{bell} badaladas"),
};
static const InfiltratrI18nEntry clock_ru[] = {
    CLOCK_REMAINING_WORDS("Час", "Стража", "двенадцатая", "двенадцатых"),
    EDO_WORDS("{branch} · {bell} ударов", "{branch}\n{bell} ударов"),
};
static const InfiltratrI18nEntry clock_ja[] = {
    CLOCK_REMAINING_WORDS("時", "夜番", "十二分の一", "十二分の一"),
    JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}の刻 · {bell}つ", "{branch}の刻\n{bell}つ"),
};
static const InfiltratrI18nEntry clock_zh[] = {
    CLOCK_REMAINING_WORDS("时", "更", "十二分之一时", "十二分之一时"),
    JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}刻 · {bell}声", "{branch}刻\n{bell}声"),
};
static const InfiltratrI18nEntry clock_zh_hant[] = {
    CLOCK_REMAINING_WORDS("時", "更", "十二分之一時", "十二分之一時"),
    JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}刻 · {bell}聲", "{branch}刻\n{bell}聲"),
};
static const InfiltratrI18nEntry clock_ko[] = {
    /* Other specialized-clock wording keeps the documented English fallback. */
    EDO_WORDS("{branch} · {bell}타", "{branch}\n{bell}타"),
};
#undef CLOCK_REMAINING_WORDS
#undef JAPANESE_BRANCHES
#undef JAPANESE_NATIVE_BRANCHES
#undef EDO_WORDS

#define CLOCK_CATALOG(locale, entries) \
    { locale, entries, sizeof(entries) / sizeof((entries)[0]) }
static const InfiltratrI18nCatalog clock_catalogs[] = {
    CLOCK_CATALOG("en", clock_en), CLOCK_CATALOG("de", clock_de),
    CLOCK_CATALOG("es", clock_es), CLOCK_CATALOG("fr", clock_fr),
    CLOCK_CATALOG("it", clock_it), CLOCK_CATALOG("pt", clock_pt),
    CLOCK_CATALOG("ru", clock_ru), CLOCK_CATALOG("ja", clock_ja),
    CLOCK_CATALOG("ko", clock_ko),
    CLOCK_CATALOG("zh", clock_zh), CLOCK_CATALOG("zh-TW", clock_zh_hant),
    CLOCK_CATALOG("zh-HK", clock_zh_hant), CLOCK_CATALOG("zh-MO", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant-TW", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant-HK", clock_zh_hant)
};
#undef CLOCK_CATALOG

static bool write_localized(char *buffer, size_t capacity, size_t *length,
                            const char *pattern,
                            const InfiltratrI18nArgument *arguments,
                            size_t argument_count)
{
    const size_t needed = infiltratr_i18n_format(
        buffer, capacity, pattern, arguments, argument_count);
    if (needed >= capacity) {
        buffer[0] = '\0';
        return false;
    }
    if (length != NULL) *length = needed;
    return true;
}

static bool format_clock_mode(const InfiltratrI18n *language,
                                           const char *mode,
                                           int64_t unix_microseconds,
                                           int32_t utc_offset_seconds,
                                           bool show_seconds,
                                           bool vertical,
                                           bool location_configured,
                                           double latitude,
                                           double longitude,
                                           char *buffer,
                                           size_t capacity,
                                           size_t *length)
{
    const InfiltratrTemporalClockModeInfo *info;

    // The extension modes use absolute instants and, when applicable, the
    // selected solar location. Civil UTC offsets remain part of the stable
    // caller contract, but do not affect this custom-mode renderer.
    (void)utc_offset_seconds;
    if (buffer == NULL || capacity == 0U || mode == NULL) {
        return false;
    }
    buffer[0] = '\0';

    const InfiltratrTemporalClockModeInfo selected = {
        CLOCK_ID, CLOCK_NAME, CLOCK_SECONDS, CLOCK_LOCATION, CLOCK_LOCATION
    };
    info = strcmp(mode, CLOCK_ID) == 0 ? &selected : NULL;
    if (info == NULL ||
        !mode_location_valid(
            info, location_configured, latitude, longitude)) {
        return false;
    }

    if (strcmp(mode, "standard") == 0) {
        return false;
    }

    if (strcmp(mode, "roman-temporal") == 0) {
        static const char *const roman[] = {
            "I", "II", "III", "IV", "V", "VI",
            "VII", "VIII", "IX", "X", "XI", "XII"
        };
        SeasonalPeriod period;

        if (!seasonal_period_at(
                unix_microseconds, latitude, longitude,
                0.833, 12U, 4U, &period)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\nROM" : "N/A ROM");
        }

        /*
         * Roman daylight horae were seasonal, so finer presentation must
         * subdivide the current unequal hora rather than append modern
         * minutes. An uncia is one twelfth of an hour. Night remains in the
         * selected four-watch civil convention because a fractional military
         * watch would manufacture a precision convention this profile does
         * not claim historically.
         */
        if (show_seconds && period.daylight) {
            unsigned uncia =
                (unsigned)floorl(period.fraction * 12.0L);

            if (uncia > 11U) {
                uncia = 11U;
            }
            if (uncia != 0U) {
                return finish_output(buffer, capacity, length,
            snprintf(buffer, capacity, vertical ? "%s\n%s\n%s %s"
                             : "%s %s · %s %s", language != NULL ? infiltratr_i18n_get(language, "hour") : "Hora", roman[period.index], roman[uncia - 1U], language != NULL ? infiltratr_i18n_get(language,
                        uncia == 1U ? "twelfth" : "twelfths")
                        : (uncia == 1U ? "uncia" : "unciae")));
            }
        }

        return finish_output(buffer, capacity, length,
            snprintf(buffer, capacity, vertical ? "%s\n%s" : "%s %s", language != NULL ? infiltratr_i18n_get(language,
                period.daylight ? "hour" : "watch")
                : (period.daylight ? "Hora" : "Vigilia"), roman[period.index]));
    }

    if (strcmp(mode, "japanese-temporal") == 0 ||
        strcmp(mode, "japanese-temporal-early") == 0) {
        typedef struct JapaneseToki {
            const char *character;
            const char *bell;
            const char *key;
        } JapaneseToki;
        static const JapaneseToki day_toki[] = {
            { "卯", "六", "jp-u" }, { "辰", "五", "jp-tatsu" },
            { "巳", "四", "jp-mi" }, { "午", "九", "jp-uma" },
            { "未", "八", "jp-hitsuji" }, { "申", "七", "jp-saru" }
        };
        static const JapaneseToki night_toki[] = {
            { "酉", "六", "jp-tori" }, { "戌", "五", "jp-inu" },
            { "亥", "四", "jp-i" }, { "子", "九", "jp-ne" },
            { "丑", "八", "jp-ushi" }, { "寅", "七", "jp-tora" }
        };
        const double depression =
            strcmp(mode, "japanese-temporal") == 0
                ? 7.0 + 21.0 / 60.0 + 40.0 / 3600.0
                : 0.833;
        SeasonalPeriod period;
        const JapaneseToki *toki;

        if (!seasonal_period_at(
                unix_microseconds, latitude, longitude,
                depression, 6U, 6U, &period)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\n和時" : "N/A 和時");
        }
        toki = period.daylight
            ? &day_toki[period.index]
            : &night_toki[period.index];
        if (language != NULL) {
            const InfiltratrI18nArgument arguments[] = {
                { "branch", infiltratr_i18n_get(language, toki->key) },
                { "bell", toki->bell }
            };
            return write_localized(buffer, capacity, length,
                infiltratr_i18n_get(language, vertical ? "edo-vertical" : "edo"),
                arguments, sizeof(arguments) / sizeof(arguments[0]));
        }
        return finish_output(buffer, capacity, length,
            snprintf(buffer, capacity, vertical ? "%sの刻\n%sつ" : "%sの刻 · %sつ", toki->character, toki->bell));
    }

    if (strcmp(mode, "nuremberg-hours") == 0) {
        return format_nuremberg_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, true, 0.0, 0.0);
    }

    if (strcmp(mode, "nuremberg-solar") == 0) {
        return format_nuremberg_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, false, latitude, longitude);
    }

    return false;
}

static bool seasonal_format_clock_mode(const char *mode,
    int64_t unix_microseconds, int32_t utc_offset_seconds,
    bool show_seconds, bool vertical, bool location_configured,
    double latitude, double longitude,
    char *buffer, size_t capacity, size_t *length)
{
    return format_clock_mode(NULL, mode, unix_microseconds, utc_offset_seconds,
        show_seconds, vertical, location_configured, latitude, longitude,
        buffer, capacity, length);
}

static bool seasonal_format_clock_mode_localized(
    const char *locale, const char *mode,
    int64_t unix_microseconds, int32_t utc_offset_seconds,
    bool show_seconds, bool vertical, bool location_configured,
    double latitude, double longitude,
    char *buffer, size_t capacity, size_t *length)
{
    InfiltratrI18n language;
    if (!infiltratr_i18n_init(&language, clock_catalogs,
            sizeof(clock_catalogs) / sizeof(clock_catalogs[0]), "en")) {
        return false;
    }
    if (locale != NULL) (void)infiltratr_i18n_set_locale(&language, locale);
    return format_clock_mode(&language, mode, unix_microseconds, utc_offset_seconds,
        show_seconds, vertical, location_configured, latitude, longitude,
        buffer, capacity, length);
}

static bool render(const InfiltratrTemporalClockProvider *self,
    const char *locale, int64_t instant, int32_t offset, bool seconds,
    bool vertical, double latitude, double longitude,
    char *buffer, size_t capacity, size_t *length)
{
    (void)self;
    if (locale == NULL)
        return seasonal_format_clock_mode(CLOCK_ID, instant, offset, seconds,
            vertical, true, latitude, longitude, buffer, capacity, length);
    return seasonal_format_clock_mode_localized(locale, CLOCK_ID, instant,
        offset, seconds, vertical, true, latitude, longitude,
        buffer, capacity, length);
}

/* Bracket and refine the first visible boundary against this very renderer.
 * Rendering and scheduling cannot disagree over moving seasonal boundaries,
 * Wendetage or floating-point quantisation. This is a bounded one-shot search.
 */
static bool next_tick(const InfiltratrTemporalClockProvider *self,
    int64_t instant, int32_t offset, bool seconds,
    double latitude, double longitude, uint32_t *milliseconds)
{
    char initial[256], future[256];
    uint32_t low = 0U, high = 1U, step = 1U;
    const uint32_t maximum = 129600000U;
    (void)self;
    if (milliseconds == NULL || !render(NULL, NULL, instant, offset, seconds,
            false, latitude, longitude, initial, sizeof(initial), NULL))
        return false;
    for (;;) {
        const int64_t delta = (int64_t)high * 1000;
        if (instant > INT64_MAX - delta ||
            !render(NULL, NULL, instant + delta, offset, seconds, false,
                latitude, longitude, future, sizeof(future), NULL))
            return false;
        if (strcmp(initial, future) != 0) break;
        if (high == maximum) {
            /* Polar N/A intervals are explicitly unavailable, retried later. */
            *milliseconds = 60000U;
            return true;
        }
        low = high;
        if (step < 60000U) step = step > 30000U ? 60000U : step * 2U;
        high = maximum - high < step ? maximum : high + step;
    }
    while (high - low > 1U) {
        const uint32_t middle = low + (high - low) / 2U;
        if (!render(NULL, NULL, instant + (int64_t)middle * 1000, offset,
                seconds, false, latitude, longitude, future, sizeof(future), NULL))
            return false;
        if (strcmp(initial, future) == 0) low = middle;
        else high = middle;
    }
    *milliseconds = high;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK, CLOCK_ID, CLOCK_NAME,
    CLOCK_SECONDS, CLOCK_LOCATION, CLOCK_LOCATION
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI, CLOCK_ID, render, next_tick
};
#if defined(_WIN32)
#define RESOURCE_EXPORT __declspec(dllexport)
#else
#define RESOURCE_EXPORT __attribute__((visibility("default")))
#endif
RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
RESOURCE_EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void);
RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void) { return &metadata; }
RESOURCE_EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void) { return &provider; }
