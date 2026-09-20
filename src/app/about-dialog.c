// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Compatibility metadata helper.
 *
 * The user-facing About surface now lives inside Cinnamon/St, so Calendar no
 * longer needs to load GTK solely to show one dialog. Keep this tiny helper
 * for installed-package qualification and existing --print-metadata callers.
 */

#include <infiltratr/core.h>
#include <stdio.h>

#include "project-info.h"

int
main(int argc,
     char **argv)
{
    if (argc == 2 && infiltratr_string_equal(argv[1], "--print-metadata"))
        return infiltratr_project_info_print(stdout, calendar_plus_project_info());

    fprintf(stderr, "Usage: %s --print-metadata\n", argv[0]);
    return 2;
}
