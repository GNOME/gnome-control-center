/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _GNOME_DA_CAPPLET_H_
#define _GNOME_DA_CAPPLET_H_

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

typedef struct _GnomeDACapplet GnomeDACapplet;

struct _GnomeDACapplet {
    GtkBuilder *builder;

    GtkIconTheme *icon_theme;

    GtkWidget *window;

    GtkWidget *web_combo_box;
    GtkWidget *mail_combo_box;
    GtkWidget *term_combo_box;
    GtkWidget *visual_combo_box;
    GtkWidget *mobility_combo_box;

    GtkWidget *terminal_command_entry;
    GtkWidget *terminal_command_label;
    GtkWidget *terminal_exec_flag_entry;
    GtkWidget *terminal_exec_flag_label;

    GtkWidget *visual_command_entry;
    GtkWidget *visual_command_label;
    GtkWidget *visual_startup_checkbutton;

    GtkWidget *mobility_command_entry;
    GtkWidget *mobility_command_label;
    GtkWidget *mobility_startup_checkbutton;

    GSettings *terminal_settings;
    GSettings *at_mobility_settings;
    GSettings *at_visual_settings;

    GList *web_browsers;
    GList *mail_readers;
    GList *terminals;
    GList *visual_ats;
    GList *mobility_ats;

    guint theme_changed_id;
};

void gnome_default_applications_panel_init (GnomeDACapplet *capplet);

#endif
