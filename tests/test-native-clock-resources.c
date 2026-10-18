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
    g_test_add_func("/clock-resource/known-epoch-vectors",
                    test_resource_known_epoch_vectors);
    return g_test_run();
}
