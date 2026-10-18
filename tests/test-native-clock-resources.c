// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Exact external clock-resource contract. Compare installed native providers
 * against the stable Common presentation reference at positive/negative Unix
 * epochs, offsets, layouts and precision levels. These tests invoke the
 * independent .so directly; a metadata-only resource cannot pass.
 */
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>

#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef TEST_CLOCK_RESOURCE_DIRECTORY
#error TEST_CLOCK_RESOURCE_DIRECTORY is required
#endif

static void test_five_clock_resources(void)
{
    static const char *ids[] = {
        "decimal", "internet", "unix", "hexadecimal", "binary"
    };
    static const int64_t instants[] = {
        INT64_C(-1000001), INT64_C(-1), INT64_C(0),
        INT64_C(123456789), INT64_C(1792256400000000)
    };
    static const int32_t offsets[] = { 0, 39600, -19800 };
    size_t i, a, b;
    g_assert_null(infiltratr_temporal_clock_module_open(
        "/definitely/nonexistent/clock.so"));

    for (i = 0; i < G_N_ELEMENTS(ids); i++)
    {
        g_autofree gchar *filename = g_strconcat(ids[i], ".so", NULL);
        g_autofree gchar *path =
            g_build_filename(TEST_CLOCK_RESOURCE_DIRECTORY, filename, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        const InfiltratrTemporalResourceMetadata *metadata;

        g_assert_nonnull(module);
        metadata = infiltratr_temporal_clock_module_metadata(module);
        g_assert_nonnull(metadata);
        g_assert_cmpstr(metadata->id, ==, ids[i]);

        for (a = 0; a < G_N_ELEMENTS(instants); a++)
        {
            for (b = 0; b < G_N_ELEMENTS(offsets); b++)
            {
                for (gint seconds = 0; seconds <= 1; seconds++)
                {
                    for (gint vertical = 0; vertical <= 1; vertical++)
                    {
                        char actual[256] = {0};
                        char following[256] = {0};
                        uint32_t delay = 0U;
                        const int64_t instant = instants[a];
                        const int32_t offset = offsets[b];

                        g_assert_true(infiltratr_temporal_clock_format(
                            module, "en_AU", instant, offset,
                            seconds != 0, vertical != 0,
                            0.0, 0.0, actual, sizeof(actual), NULL));
                        g_assert_true(g_utf8_validate(actual, -1, NULL));

                        g_assert_true(infiltratr_temporal_clock_next_tick(
                            module, instant, offset, seconds != 0,
                            0.0, 0.0, &delay));
                        g_assert_cmpuint(delay, >, 0U);
                        g_assert_true(infiltratr_temporal_clock_format(
                            module, "en_AU", instant + (int64_t)delay * INT64_C(1000),
                            offset, seconds != 0, vertical != 0,
                            0.0, 0.0, following, sizeof(following), NULL));
                        g_assert_cmpstr(actual, !=, following);
                    }
                }
            }
        }
        infiltratr_temporal_clock_module_free(module);
    }
}

/*
 * JD's 12:00 UTC turnover and MJD's 00:00 UTC turnover are different
 * presentation boundaries. Assert exact notation and the next tick with no
 * dependency on either the Calendar application or Common's old renderer.
 */
static void test_julian_clock_resources(void)
{
    static const struct {
        const char *id;
        const char *epoch;
        const char *before_epoch;
        const char *at_rollover;
        int64_t rollover;
    } modes[] = {
        { "julian", "JD 2440587.50000", "JD 2440587.49999",
          "JD 2440588.00000", INT64_C(43200000000) },
        { "modified-julian", "MJD 40587.00000", "MJD 40586.99999",
          "MJD 40588.00000", INT64_C(86400000000) }
    };
    for (gsize i = 0; i < G_N_ELEMENTS(modes); ++i)
    {
        g_autofree gchar *filename = g_strconcat(modes[i].id, ".so", NULL);
        g_autofree gchar *path =
            g_build_filename(TEST_CLOCK_RESOURCE_DIRECTORY, filename, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        char buffer[256] = {0};
        uint32_t delay = 0U;
        g_assert_nonnull(module);
        g_assert_true(infiltratr_temporal_clock_format(
            module, "en_AU", 0, 36000, true, false, 0.0, 0.0,
            buffer, sizeof(buffer), NULL));
        g_assert_cmpstr(buffer, ==, modes[i].epoch);
        g_assert_true(infiltratr_temporal_clock_format(
            module, "en_AU", -1, 0, true, false, 0.0, 0.0,
            buffer, sizeof(buffer), NULL));
        g_assert_cmpstr(buffer, ==, modes[i].before_epoch);
        g_assert_true(infiltratr_temporal_clock_format(
            module, "en_AU", modes[i].rollover, -39600, true, false,
            0.0, 0.0, buffer, sizeof(buffer), NULL));
        g_assert_cmpstr(buffer, ==, modes[i].at_rollover);
        g_assert_true(infiltratr_temporal_clock_format(
            module, "en_AU", 0, 0, false, true, 0.0, 0.0,
            buffer, sizeof(buffer), NULL));
        g_assert_nonnull(strchr(buffer, '\n'));
        g_assert_true(infiltratr_temporal_clock_next_tick(
            module, 0, 0, true, 0.0, 0.0, &delay));
        g_assert_cmpuint(delay, ==, 864U);
        infiltratr_temporal_clock_module_free(module);
    }
}

/* The explicit civil clocks use local wall-time and tick at an actual
 * second or minute boundary (including a non-hour UTC offset). */
static void test_conventional_clock_resources(void)
{
    static const char *ids[] = { "standard-12", "standard-24" };
    static const gint64 instants[] = {
        -INT64_C(1), INT64_C(0), INT64_C(43200000000),
        INT64_C(86399999999), INT64_C(86400000000)
    };
    static const gint32 offsets[] = { 0, 39600, -19800, 30 };
    for (gsize i = 0; i < G_N_ELEMENTS(ids); ++i)
    {
        g_autofree gchar *filename = g_strconcat(ids[i], ".so", NULL);
        g_autofree gchar *path =
            g_build_filename(TEST_CLOCK_RESOURCE_DIRECTORY, filename, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        const InfiltratrTemporalResourceMetadata *metadata;
        g_assert_nonnull(module);
        metadata = infiltratr_temporal_clock_module_metadata(module);
        g_assert_nonnull(metadata);
        g_assert_cmpstr(metadata->id, ==, ids[i]);
        for (gsize a = 0; a < G_N_ELEMENTS(instants); ++a)
            for (gsize b = 0; b < G_N_ELEMENTS(offsets); ++b)
                for (gint seconds = 0; seconds <= 1; ++seconds)
                    for (gint vertical = 0; vertical <= 1; ++vertical)
                    {
                        const gint64 instant = instants[a];
                        const gint32 offset = offsets[b];
                        char actual[256] = {0};
                        char following[256] = {0};
                        guint32 ms = 0;
                        g_assert_true(infiltratr_temporal_clock_format(
                            module, "en_AU", instant, offset, seconds != 0,
                            vertical != 0, 0.0, 0.0,
                            actual, sizeof(actual), NULL));
                        g_assert_true(g_utf8_validate(actual, -1, NULL));
                        g_assert_true(infiltratr_temporal_clock_next_tick(
                            module, instant, offset, seconds != 0,
                            0.0, 0.0, &ms));
                        g_assert_cmpuint(ms, >, 0);
                        g_assert_cmpuint(ms, <=, seconds ? 1000 : 60000);
                        g_assert_true(infiltratr_temporal_clock_format(
                            module, "en_AU", instant + (gint64)ms * 1000,
                            offset, seconds != 0, vertical != 0, 0.0, 0.0,
                            following, sizeof(following), NULL));
                        g_assert_cmpstr(following, !=, actual);
                    }
        infiltratr_temporal_clock_module_free(module);
    }
}

static void test_conventional_ui_language(void)
{
    g_autofree gchar *path=g_build_filename(TEST_CLOCK_RESOURCE_DIRECTORY,"standard-12.so",NULL);
    InfiltratrTemporalClockModule *module=infiltratr_temporal_clock_module_open(path);
    g_assert_nonnull(module);
    const char *locales[] = {"ja_JP","zh_TW"};
    const char *expected[] = {"午後12:00","下午12:00"};
    for (gsize i=0;i<G_N_ELEMENTS(locales);i++) {
        char output[128];
        g_assert_true(infiltratr_temporal_clock_format(module,locales[i],
            INT64_C(43200000000),0,false,false,0,0,output,sizeof(output),NULL));
        g_assert_cmpstr(output,==,expected[i]);
    }
    infiltratr_temporal_clock_module_free(module);
}

/* Cross-language output and exact next-tick verification for the two newly
 * independent alternative clocks, with negative epochs, odd UTC offsets,
 * longitude changes, horizontal/vertical and precision settings. */
static void test_hundred_ke_and_mean_solar(void)
{
    static const char *ids[] = { "chinese-ke", "mean-solar" };
    static const char *langs[] = {
        "en_AU", "zh_CN", "zh_Hant_TW", "ja_JP", "ru_RU"
    };
    static const gint64 instants[] = {
        -INT64_C(86400000001), -INT64_C(1), INT64_C(0),
        INT64_C(43200000000), INT64_C(86399999999),
        INT64_C(1792256400123456)
    };
    static const gint32 offsets[] = { 0, 39600, -19800, 30 };
    static const double longitudes[] = { -145.1234, 0.0, 15.0, 145.3499 };
    for (gsize i = 0; i < G_N_ELEMENTS(ids); ++i)
    {
        g_autofree gchar *name = g_strconcat(ids[i], ".so", NULL);
        g_autofree gchar *path =
            g_build_filename(TEST_CLOCK_RESOURCE_DIRECTORY, name, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        const InfiltratrTemporalResourceMetadata *metadata;
        g_assert_nonnull(module);
        metadata = infiltratr_temporal_clock_module_metadata(module);
        g_assert_nonnull(metadata);
        g_assert_cmpstr(metadata->id, ==, ids[i]);
        g_assert_cmpint(metadata->requires_longitude, ==, i == 1);
        for (gsize j = 0; j < G_N_ELEMENTS(instants); ++j)
            for (gsize k = 0; k < G_N_ELEMENTS(offsets); ++k)
                for (gsize n = 0; n < G_N_ELEMENTS(langs); ++n)
                    for (gsize g = 0; g < G_N_ELEMENTS(longitudes); ++g)
                        for (gint vertical = 0; vertical <= 1; ++vertical)
                            for (gint seconds = 0; seconds <= 1; ++seconds)
                            {
                                char actual[256] = {0};
                                char following[256] = {0};
                                guint32 ms = 0;
                                const gint64 instant = instants[j];
                                const gint32 offset = offsets[k];
                                const double longitude = longitudes[g];
                                const char *language = langs[n];
                                g_assert_true(infiltratr_temporal_clock_format(
                                    module, language, instant, offset,
                                    seconds != 0, vertical != 0, 0.0,
                                    longitude, actual, sizeof(actual), NULL));
                                g_assert_true(g_utf8_validate(actual, -1, NULL));
                                g_assert_true(infiltratr_temporal_clock_next_tick(
                                    module, instant, offset, seconds != 0,
                                    0.0, longitude, &ms));
                                g_assert_cmpuint(ms, >, 0U);
                                g_assert_cmpuint(ms, <=, i == 0
                                    ? 864000U
                                    : (seconds ? 1000U : 60000U));
                                g_assert_true(infiltratr_temporal_clock_format(
                                    module, language, instant + (gint64)ms * 1000,
                                    offset, seconds != 0, vertical != 0, 0.0,
                                    longitude, following, sizeof(following),
                                    NULL));
                                g_assert_cmpstr(actual, !=, following);
                            }
        infiltratr_temporal_clock_module_free(module);
    }
}

/* Known independent reference vectors assert clock meaning, not parity with
 * the retired Common formatter. The eleven installed providers have only one
 * implementation at runtime: their resource .so files. */
static void test_resource_known_epoch_vectors(void)
{
    static const struct {
        const char *id;
        const char *expected;
    } vectors[] = {
        { "decimal", "0:00:00" },
        { "internet", "@041.66" },
        { "unix", "0" },
        { "hexadecimal", "0000" },
        { "binary", "00000:000000:000000" },
        { "julian", "JD 2440587.50000" },
        { "modified-julian", "MJD 40587.00000" },
        { "standard-12", "12:00:00 AM" },
        { "standard-24", "00:00:00" },
        { "chinese-ke", "00 kè" },
        { "mean-solar", "00:00:00 LMT" }
    };
    for (gsize i = 0; i < G_N_ELEMENTS(vectors); ++i) {
        g_autofree gchar *filename = g_strconcat(vectors[i].id, ".so", NULL);
        g_autofree gchar *path = g_build_filename(
            TEST_CLOCK_RESOURCE_DIRECTORY, filename, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        char result[256] = {0};
        g_assert_nonnull(module);
        g_assert_true(infiltratr_temporal_clock_format(
            module, "en_AU", INT64_C(0), 0, true, false,
            0.0, 0.0, result, sizeof(result), NULL));
        g_assert_cmpstr(result, ==, vectors[i].expected);
        infiltratr_temporal_clock_module_free(module);
    }
}

static void test_sidereal_resource(void)
{
    g_autofree gchar *path = g_build_filename(
        TEST_CLOCK_RESOURCE_DIRECTORY, "sidereal.so", NULL);
    InfiltratrTemporalClockModule *module =
        infiltratr_temporal_clock_module_open(path);
    const InfiltratrTemporalResourceMetadata *info;
    char text[256] = {0};
    char following[256] = {0};
    uint32_t delay = 0U;

    g_assert_nonnull(module);
    info = infiltratr_temporal_clock_module_metadata(module);
    g_assert_nonnull(info);
    g_assert_cmpstr(info->id, ==, "sidereal");
    g_assert_true(info->requires_longitude);

    /* J2000 (2000-01-01 12:00:00 UTC): 18.697374558 h GMST. */
    g_assert_true(infiltratr_temporal_clock_format(
        module, "en_AU", INT64_C(946728000000000), 0,
        true, false, 0.0, 0.0, text, sizeof(text), NULL));
    g_assert_cmpstr(text, ==, "18:41:50 LST");
    g_assert_true(infiltratr_temporal_clock_next_tick(
        module, INT64_C(946728000000000), 0, true,
        0.0, 0.0, &delay));
    g_assert_cmpuint(delay, >, 0U);
    g_assert_cmpuint(delay, <=, 1000U);
    g_assert_true(infiltratr_temporal_clock_format(
        module, "en_AU", INT64_C(946728000000000) +
        (int64_t)delay * INT64_C(1000), 0, true, false,
        0.0, 0.0, following, sizeof(following), NULL));
    g_assert_cmpstr(text, !=, following);

    g_assert_true(infiltratr_temporal_clock_format(
        module, "en_AU", INT64_C(946728000000000), 39600,
        false, false, 0.0, 15.0, text, sizeof(text), NULL));
    g_assert_cmpstr(text, ==, "19:41 LST");

    /* Invalid coordinates fail instead of using Greenwich implicitly. */
    g_assert_false(infiltratr_temporal_clock_format(
        module, "en_AU", 0, 0, true, false, 0.0,
        INFINITY, text, sizeof(text), NULL));
    infiltratr_temporal_clock_module_free(module);
}

static void test_chinese_double_hour_resource(void)
{
    g_autofree gchar *path = g_build_filename(
        TEST_CLOCK_RESOURCE_DIRECTORY, "chinese-time.so", NULL);
    InfiltratrTemporalClockModule *module =
        infiltratr_temporal_clock_module_open(path);
    static const struct {
        const char *locale;
        int64_t instant;
        bool seconds;
        bool vertical;
        const char *expected;
    } vectors[] = {
        { "en_AU.UTF-8", 0, false, false, "Zǐ hour" },
        { "en", 0, false, true, "Zǐ\nHour" },
        { "en", 0, true, false, "Zǐ, second half" },
        { "en", -1, true, false, "Zǐ, first half" },
        { "en", INT64_C(39600000000), false, false, "Wǔ hour" },
        { "zh_CN.UTF-8", 0, false, false, "子时" },
        { "zh-Hant-TW", 0, false, false, "子時" },
        { "zh-Hant-TW", 0, true, false, "子正" },
        { "fr_FR.UTF-8", 0, true, false, "Zǐ, seconde moitié" },
        { "de_DE", 0, true, false, "Zǐ, zweite Hälfte" },
        { "C", 0, true, false, "Zǐ, second half" }
    };
    uint32_t millis = 0U;
    g_assert_nonnull(module);
    for (gsize i = 0; i < G_N_ELEMENTS(vectors); ++i) {
        char text[256] = {0};
        g_assert_true(infiltratr_temporal_clock_format(
            module, vectors[i].locale, vectors[i].instant, 0,
            vectors[i].seconds, vectors[i].vertical,
            0.0, 0.0, text, sizeof(text), NULL));
        g_assert_cmpstr(text, ==, vectors[i].expected);
    }

    /* Midnight is the halfway point of Zi, not a new branch. */
    g_assert_true(infiltratr_temporal_clock_next_tick(
        module, INT64_C(0), 0, false, 0.0, 0.0, &millis));
    g_assert_cmpuint(millis, ==, 3600000U);
    g_assert_true(infiltratr_temporal_clock_next_tick(
        module, INT64_C(23) * INT64_C(3600000000), 0,
        false, 0.0, 0.0, &millis));
    g_assert_cmpuint(millis, ==, 7200000U);
    g_assert_true(infiltratr_temporal_clock_next_tick(
        module, INT64_C(23) * INT64_C(3600000000), 0,
        true, 0.0, 0.0, &millis));
    g_assert_cmpuint(millis, ==, 3600000U);
    infiltratr_temporal_clock_module_free(module);
}

static void test_apparent_solar_resource(void)
{
    g_autofree gchar *path = g_build_filename(
        TEST_CLOCK_RESOURCE_DIRECTORY, "solar.so", NULL);
    InfiltratrTemporalClockModule *module =
        infiltratr_temporal_clock_module_open(path);
    const int64_t j2000 = INT64_C(946728000000000);
    char initial[256] = {0};
    char later[256] = {0};
    uint32_t delay = 0U;

    g_assert_nonnull(module);
    /* Independent NOAA compact-model vector at J2000, Greenwich. */
    g_assert_true(infiltratr_temporal_clock_format(
        module, "en", j2000, 0, true, false,
        0.0, 0.0, initial, sizeof(initial), NULL));
    g_assert_cmpstr(initial, ==, "11:57:05 SOL");
    g_assert_true(infiltratr_temporal_clock_format(
        module, "en", j2000, 39600, true, false,
        0.0, 0.0, later, sizeof(later), NULL));
    g_assert_cmpstr(initial, ==, later);

    /* The module, rather than Calendar polling, owns the next exact tick. */
    g_assert_true(infiltratr_temporal_clock_next_tick(
        module, j2000, 0, true, 0.0, 0.0, &delay));
    g_assert_cmpuint(delay, >, 0U);
    g_assert_cmpuint(delay, <=, 2000U);
    g_assert_true(infiltratr_temporal_clock_format(
        module, "en", j2000 + (int64_t)delay * INT64_C(1000), 0,
        true, false, 0.0, 0.0, later, sizeof(later), NULL));
    g_assert_cmpstr(initial, !=, later);
    g_assert_false(infiltratr_temporal_clock_format(
        module, "en", j2000, 0, true, false,
        0.0, -180.0001, later, sizeof(later), NULL));
    infiltratr_temporal_clock_module_free(module);
}


/* Both equal-hour solar-origin resources must own their rendered boundaries. */
static void test_solar_origin_hour_resources(void)
{
    static const char *const ids[] = {
        "italian-hours", "babylonian-hours",
        "babylonian-ancient", "indian-ghati"
    };
    static const int64_t instants[] = {
        INT64_C(-1000000000), INT64_C(0),
        INT64_C(946728000000000), INT64_C(1792256400000000)
    };
    for (gsize m = 0; m < G_N_ELEMENTS(ids); ++m)
    {
        g_autofree gchar *name = g_strconcat(ids[m], ".so", NULL);
        g_autofree gchar *path = g_build_filename(
            TEST_CLOCK_RESOURCE_DIRECTORY, name, NULL);
        InfiltratrTemporalClockModule *module =
            infiltratr_temporal_clock_module_open(path);
        g_assert_nonnull(module);
        const InfiltratrTemporalResourceMetadata *info =
            infiltratr_temporal_clock_module_metadata(module);
        g_assert_nonnull(info);
        g_assert_cmpstr(info->id, ==, ids[m]);
        g_assert_true(info->requires_latitude);
        g_assert_true(info->requires_longitude);
        g_assert_cmpint(info->supports_seconds, ==, m < 2U);
        for (gsize k = 0; k < G_N_ELEMENTS(instants); ++k)
        {
            char before[256] = {0}, offset[256] = {0}, after[256] = {0};
            uint32_t delay = 0U;
            g_assert_true(infiltratr_temporal_clock_format(
                module, "en", instants[k], 0, true, false,
                -36.39, 145.36, before, sizeof(before), NULL));
            g_assert_true(infiltratr_temporal_clock_format(
                module, "en", instants[k], 39600, true, false,
                -36.39, 145.36, offset, sizeof(offset), NULL));
            g_assert_cmpstr(before, ==, offset);
            g_assert_true(infiltratr_temporal_clock_next_tick(
                module, instants[k], 0, true, -36.39, 145.36, &delay));
            g_assert_cmpuint(delay, >, 0U);
            g_assert_cmpuint(delay, <=, m == 2U ? 241000U :
                                    m == 3U ? 25000U : 2000U);
            g_assert_true(infiltratr_temporal_clock_format(
                module, "en", instants[k] + (int64_t)delay * INT64_C(1000),
                0, true, false, -36.39, 145.36,
                after, sizeof(after), NULL));
            g_assert_cmpstr(before, !=, after);
            g_assert_false(infiltratr_temporal_clock_format(
                module, "en", instants[k], 0, true, false,
                100.0, 145.36, after, sizeof(after), NULL));
        }
        infiltratr_temporal_clock_module_free(module);
    }
}

#define CHECK(expression) g_assert_true(expression)
static bool resource_test_format_localized(const char *locale,const char *id,
    int64_t instant,int32_t offset,bool seconds,bool vertical,bool configured,
    double latitude,double longitude,char *buffer,size_t capacity,size_t *length)
{
    gchar *path=g_strdup_printf("%s/%s.so",TEST_CLOCK_RESOURCE_DIRECTORY,id);
    InfiltratrTemporalClockModule *module=infiltratr_temporal_clock_module_open(path);
    g_free(path);
    const InfiltratrTemporalResourceMetadata *metadata=infiltratr_temporal_clock_module_metadata(module);
    const bool result=metadata != NULL &&
        (!(metadata->requires_latitude || metadata->requires_longitude) || configured) &&
        infiltratr_temporal_clock_format(module,locale,instant,offset,seconds,vertical,
            latitude,longitude,buffer,capacity,length);
    infiltratr_temporal_clock_module_free(module);
    return result;
}
static bool resource_test_format(const char *id,int64_t instant,int32_t offset,
    bool seconds,bool vertical,bool configured,double latitude,double longitude,
    char *buffer,size_t capacity,size_t *length)
{ return resource_test_format_localized(NULL,id,instant,offset,seconds,vertical,
    configured,latitude,longitude,buffer,capacity,length); }

static void test_seasonal_resources(void)
{
    static const char *const ids[] = {"roman-temporal","japanese-temporal",
        "japanese-temporal-early","nuremberg-hours","nuremberg-solar"};
    const int64_t instants[] = {INT64_C(-1),INT64_C(946728000000000),
        INT64_C(1792256400123456),INT64_C(1789990000000000)};
    for (gsize i=0;i<G_N_ELEMENTS(ids);i++) {
        gchar *path=g_strdup_printf("%s/%s.so",TEST_CLOCK_RESOURCE_DIRECTORY,ids[i]);
        InfiltratrTemporalClockModule *module=infiltratr_temporal_clock_module_open(path);
        g_free(path);g_assert_nonnull(module);
        for (gsize j=0;j<G_N_ELEMENTS(instants);j++) {
            for (unsigned detailed=0;detailed<2;detailed++) {
                char before[256],after[256],offset[256];uint32_t delay=0;
                g_assert_true(infiltratr_temporal_clock_format(module,"en",instants[j],0,
                    detailed,false,-36.39,145.36,before,sizeof(before),NULL));
                g_assert_true(infiltratr_temporal_clock_format(module,"en",instants[j],39600,
                    detailed,false,-36.39,145.36,offset,sizeof(offset),NULL));
                g_assert_cmpstr(before,==,offset);
                g_assert_true(infiltratr_temporal_clock_next_tick(module,instants[j],0,
                    detailed,-36.39,145.36,&delay));
                g_assert_cmpuint(delay,>,0);
                g_assert_true(infiltratr_temporal_clock_format(module,"en",
                    instants[j]+(int64_t)delay*1000,0,detailed,false,-36.39,145.36,
                    after,sizeof(after),NULL));
                g_assert_cmpstr(before,!=,after);
                if (delay>1) {
                    g_assert_true(infiltratr_temporal_clock_format(module,"en",
                        instants[j]+(int64_t)(delay-1)*1000,0,detailed,false,-36.39,145.36,
                        after,sizeof(after),NULL));
                    g_assert_cmpstr(before,==,after);
                }
            }
        }
        infiltratr_temporal_clock_module_free(module);
    }
    char text[256];
    g_assert_true(resource_test_format_localized("en","roman-temporal",
        INT64_C(45000000000),0,true,false,true,0,0,text,sizeof(text),NULL));
    g_assert_true(g_str_has_prefix(text,"Hour "));
    g_assert_nonnull(strstr(text,"twelfth"));
    g_assert_true(resource_test_format_localized("fr","roman-temporal",
        INT64_C(45000000000),0,true,false,true,0,0,text,sizeof(text),NULL));
    g_assert_true(g_str_has_prefix(text,"Heure "));
}

static void test_localized_edo_clock(void)
{
    static const struct {
        const char *locale;
        bool vertical;
        const char *expected;
    } vectors[] = {
        { "en_AU", false, "Mi · 四 bells" },
        { "ja_JP", false, "巳の刻 · 四つ" },
        { "ja", true, "巳の刻\n四つ" },
        { "zh_CN", false, "巳刻 · 四声" },
        { "zh_TW.UTF-8", true, "巳刻\n四聲" },
        { "fr_FR", false, "Mi · 四 coups" },
        { "de_DE", false, "Mi · 四 Schläge" },
        { "ko_KR", false, "Mi · 四타" },
        { "xx_XX", false, "Mi · 四 bells" }
    };
    static const char *const modes[] = {
        "japanese-temporal", "japanese-temporal-early"
    };
    char text[192];
    for (size_t mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        for (size_t i = 0U; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
            size_t length = 0U;
            CHECK(resource_test_format_localized(
                vectors[i].locale, modes[mode], INT64_C(946728000000000),
                0, false, vectors[i].vertical, true, 0.0, 0.0,
                text, sizeof(text), &length));
            CHECK(strcmp(text, vectors[i].expected) == 0);
            CHECK(length == strlen(text));
        }
        /* Branches distinguish repeated daytime/nighttime bell counts. */
        char day[192];
        CHECK(resource_test_format_localized("en", modes[mode],
            INT64_C(946731600000000), 0, false, false, true, 0.0, 0.0,
            day, sizeof(day), NULL));
        CHECK(resource_test_format_localized("en", modes[mode],
            INT64_C(946688400000000), 0, false, false, true, 0.0, 0.0,
            text, sizeof(text), NULL));
        CHECK(strstr(day, "九 bells") != NULL);
        CHECK(strstr(text, "九 bells") != NULL);
        CHECK(strcmp(day, text) != 0);
        CHECK(!resource_test_format_localized("ja", modes[mode],
            INT64_C(946728000000000), 0, false, false, true, 0.0, 0.0,
            text, 8U, NULL));
        /* Common preserves the caller buffer on a short-buffer failure. */
        CHECK(resource_test_format(modes[mode],
            INT64_C(946728000000000), 0, false, false, true, 0.0, 0.0,
            text, sizeof(text), NULL));
        CHECK(strcmp(text, "巳の刻 · 四つ") == 0);
    }
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/clock-resource/five-executable-modes",
                    test_five_clock_resources);
    g_test_add_func("/clock-resource/julian-executable-modes",
                    test_julian_clock_resources);
    g_test_add_func("/clock-resource/conventional-executable-modes",
                    test_conventional_clock_resources);
    g_test_add_func("/clock-resource/chinese-ke-and-mean-solar",
                    test_hundred_ke_and_mean_solar);
    g_test_add_func("/clock-resource/sidereal", test_sidereal_resource);
    g_test_add_func("/clock-resource/chinese-double-hour",
                    test_chinese_double_hour_resource);
    g_test_add_func("/clock-resource/apparent-solar",
                    test_apparent_solar_resource);
    g_test_add_func("/clock-resource/sunrise-sunset-equal-hours",
                    test_solar_origin_hour_resources);
    g_test_add_func("/clock-resource/known-epoch-vectors",
                    test_resource_known_epoch_vectors);
    g_test_add_func("/clock-resource/seasonal-boundaries",test_seasonal_resources);
    g_test_add_func("/clock-resource/edo-locales",test_localized_edo_clock);
    g_test_add_func("/clock-resource/conventional-ui-language",test_conventional_ui_language);
    return g_test_run();
}
