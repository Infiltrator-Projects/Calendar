// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Native Calendar About helper.
 *
 * The Cinnamon applet owns the panel popup, but About follows the suite-wide
 * native GTK contract used by System Monitor.  Running it out of process keeps
 * the shell free of an additional modal/input grab.  --print-metadata remains
 * intentionally headless for release and package qualification.
 */

#include <gtk/gtk.h>
#include <infiltratr/core.h>
#include <stdio.h>

#include "project-info.h"

int
main(int argc,
     char **argv)
{
    const InfiltratrProjectInfo *info = calendar_plus_project_info();
    const char *profile;
    const char *authors[] = {
        "Shannon Smith — Author and project maintainer",
        NULL
    };
    char comments[512];
    GtkWidget *widget;
    GtkAboutDialog *dialog;

    if (argc == 2 && infiltratr_string_equal(argv[1], "--print-metadata"))
        return infiltratr_project_info_print(stdout, info);

    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--print-metadata]\n", argv[0]);
        return 2;
    }

    gtk_init(&argc, &argv);

    profile = info->build_profile;
    if (infiltratr_string_equal(profile, "aggressive") ||
        infiltratr_string_equal(profile, "portable"))
        profile = "native";

    (void)snprintf(comments, sizeof(comments), "%s\n\nBuild: %s",
                   info->comments,
                   infiltratr_build_profile_label(profile));

    widget = gtk_about_dialog_new();
    dialog = GTK_ABOUT_DIALOG(widget);
    gtk_window_set_title(GTK_WINDOW(widget), "About Calendar");
    gtk_about_dialog_set_program_name(dialog, info->program_name);
    gtk_about_dialog_set_version(dialog, info->version);
    gtk_about_dialog_set_comments(dialog, comments);
    gtk_about_dialog_set_authors(dialog, authors);
    gtk_about_dialog_set_website(dialog, info->website);
    gtk_about_dialog_set_website_label(dialog, "Website");
    gtk_about_dialog_set_copyright(dialog, info->copyright_text);
    gtk_about_dialog_set_license_type(dialog, GTK_LICENSE_CUSTOM);
    gtk_about_dialog_set_license(
        dialog,
        "Calendar is free software licensed under the GNU General Public "
        "License version 3 or, at your option, any later version "
        "(GPL-3.0-or-later).\n\n"
        "See LICENSE in the source package for the complete licence text.");
    gtk_about_dialog_set_wrap_license(dialog, TRUE);
    gtk_about_dialog_set_logo_icon_name(dialog, info->icon_name);

    (void)gtk_dialog_run(GTK_DIALOG(widget));
    gtk_widget_destroy(widget);
    return 0;
}
