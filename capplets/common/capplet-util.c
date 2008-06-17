/* -*- mode: c; style: linux -*- */

/* capplet-util.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>

/* For stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "capplet-util.h"

static void
capplet_error_dialog (GtkWindow *parent, char const *msg, GError *err)
{
	if (err != NULL) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			msg, err->message);

		g_signal_connect (G_OBJECT (dialog),
			"response",
			G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (err);
	}
}

/**
 * capplet_help :
 * @parent :
 * @helpfile :
 * @section  :
 *
 * A quick utility routine to display help for capplets, and handle errors in a
 * Havoc happy way.
 **/
void
capplet_help (GtkWindow *parent, char const *helpfile, char const *section)
{
	GError *error = NULL;

	g_return_if_fail (helpfile != NULL);
	g_return_if_fail (section != NULL);

	gnome_help_display_desktop (NULL,
		"user-guide",
		helpfile, section, &error);
	if (error != NULL)
		capplet_error_dialog (parent, 
			_("There was an error displaying help: %s"),
			error);
}

/**
 * capplet_set_icon :
 * @window :
 * @file_name  :
 *
 * A quick utility routine to avoid the cut-n-paste of bogus code
 * that caused several bugs.
 **/
void
capplet_set_icon (GtkWidget *window, char const *icon_file_name)
{
	/* Make sure that every window gets an icon */
	gtk_window_set_default_icon_name (icon_file_name);
	gtk_window_set_icon_name (GTK_WINDOW (window), icon_file_name);
}
