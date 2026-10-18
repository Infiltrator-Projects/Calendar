// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Validate missing calendar modules through the real resource registry, with
 * the scan directory deliberately empty. Migrated calendar IDs are not
 * compiled alternative implementations in the Calendar application.
 */
#include "calendar-registry.h"

#include <infiltratr/temporal.h>

#include <glib.h>

static void test_missing_migrated_calendar_modules(void)
{
    static const gchar *const migrated[] = {
        "gregorian", "hebrew", "persian", "coptic"
    };

    for (gsize i = 0; i < G_N_ELEMENTS(migrated); ++i)
    {
        g_assert_null(calendar_calendar_provider_from_id(migrated[i]));
        g_assert_null(infiltratr_temporal_calendar_find(migrated[i]));
    }

    /* The absence of independent resources must not disable unrelated
     * legacy providers or manufacture Gregorian/Persian/Coptic fallbacks. */
    g_assert_nonnull(calendar_calendar_provider_from_id("islamic-civil"));
    g_assert_cmpuint(calendar_calendar_provider_get_count(), ==, 26U);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/calendar-resource/missing-migrated-modules",
                    test_missing_migrated_calendar_modules);
    return g_test_run();
}
