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
    g_autofree char *author = NULL;
    g_autofree char *comments = NULL;
    const char *authors[2];
    GtkWidget *widget;
    GtkAboutDialog *dialog;

    if (argc == 2 && infiltratr_string_equal(argv[1], "--print-metadata"))
        return infiltratr_project_info_print(stdout, info);

    if (argc != 1) {
        g_printerr("Usage: %s [--print-metadata]\n", argv[0]);
        return 2;
    }

    gtk_init(&argc, &argv);

    author = g_strdup_printf("%s — Author and project maintainer", info->author);
    comments = g_strdup_printf("%s\n\nBuild: %s",
                               info->comments,
                               infiltratr_build_profile_label(info->build_profile));
    authors[0] = author;
    authors[1] = NULL;

    widget = gtk_about_dialog_new();
    /* GTK 3's official cast macros intentionally pass through void * inside
     * G_TYPE_CHECK_INSTANCE_CAST; the local call remains runtime type-checked. */
    dialog = GTK_ABOUT_DIALOG(widget); /* NOLINT(bugprone-casting-through-void) */
    gtk_window_set_title(GTK_WINDOW(widget), /* NOLINT(bugprone-casting-through-void) */
                         "About Calendar");
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

    (void)gtk_dialog_run(GTK_DIALOG(widget)); /* NOLINT(bugprone-casting-through-void) */
    gtk_widget_destroy(widget);
    return 0;
}
