// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Exact external clock-resource contract. Compare all five native providers
 * against the stable Common presentation reference at positive/negative Unix
 * epochs, offsets, layouts and precision levels. These tests invoke the
 * independent .so directly; a metadata-only resource cannot pass.
 */
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>

#include <glib.h>
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
                        char expected[256] = {0};
                        char following[256] = {0};
                        uint32_t delay = 0U;
                        const int64_t instant = instants[a];
                        const int32_t offset = offsets[b];

                        g_assert_true(infiltratr_temporal_clock_format(
                            module, "en_AU", instant, offset,
                            seconds != 0, vertical != 0,
                            0.0, 0.0, actual, sizeof(actual), NULL));
                        g_assert_true(infiltratr_temporal_format_clock_mode_localized(
                            "en_AU", ids[i], instant, offset,
                            seconds != 0, vertical != 0, true,
                            0.0, 0.0, expected, sizeof(expected), NULL));
                        g_assert_cmpstr(actual, ==, expected);

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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/clock-resource/five-executable-modes",
                    test_five_clock_resources);
    return g_test_run();
}
