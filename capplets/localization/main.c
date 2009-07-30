/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtk.h>

static GtkWidget *
create_dialog (void)
{
	GtkBuilder *ui;
	GtkWidget *dialog;

	ui = gtk_builder_new ();
	gtk_builder_add_from_file (ui, GNOMECC_UI_DIR "/localization.ui", NULL);
	dialog = GTK_WIDGET (gtk_builder_get_object (ui, "i18n_dialog"));
	g_object_unref (ui);

	return dialog;
}

int
main (int argc, char *argv[])
{
	GtkWidget *dialog;

	capplet_init (NULL, &argc, &argv);

	/* open main dialog */
	dialog = create_dialog ();
	g_signal_connect (dialog, "response", gtk_main_quit, NULL);
	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}
