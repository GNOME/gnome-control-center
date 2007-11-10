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
#include <gnome.h>
#include <glade/glade-xml.h>

static GtkWidget *
create_dialog (void)
{
	GladeXML *xml;
	GtkWidget *dialog;

	xml = glade_xml_new (GNOMECC_GLADEDIR "/localization.glade", "i18n_dialog", NULL);
	dialog = glade_xml_get_widget (xml, "i18n_dialog");
	g_object_unref (xml);

	return dialog;
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;
	GtkWidget *dialog;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-localization-properties", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
				      NULL);

	/* open main dialog */
	dialog = create_dialog ();
	g_signal_connect (dialog, "response", gtk_main_quit, NULL);
	gtk_widget_show (dialog);

	gtk_main ();
	g_object_unref (program);

	return 0;
}
