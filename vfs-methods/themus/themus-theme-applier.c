/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more av.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/libgnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>

#include <gnome-theme-info.h>
#include <gnome-theme-apply.h>

#define FONT_KEY           "/desktop/gnome/interface/font_name"

int main (int argc, char **argv)
{
	GnomeVFSURI *uri;
	GnomeThemeMetaInfo *theme;
	GnomeProgram *program;
	GladeXML *font_xml;
	GtkWidget *font_dialog;
	GtkWidget *font_sample;
	gboolean apply_font = FALSE;
	poptContext ctx;
	gchar **args;

	gtk_init (&argc, &argv);
	program = gnome_program_init ("ThemeApplier", "0.3.0", LIBGNOME_MODULE,
				      argc, argv, GNOME_PARAM_NONE);
	
	g_object_get (program, GNOME_PARAM_POPT_CONTEXT, &ctx, NULL);
	args = (char**) poptGetArgs(ctx);

	if (!args) return 1;

	gnome_vfs_init ();
	gnome_theme_init (NULL);
		
	uri = gnome_vfs_uri_new (args[0]);
	g_assert (uri != NULL);
		
	theme = gnome_theme_read_meta_theme (uri);
	gnome_vfs_uri_unref (uri);
		
	g_assert (theme != NULL);

	if (theme->application_font) {
		glade_init ();
		font_xml = glade_xml_new (GNOMECC_GLADE_DIR "/apply-font.glade",
					  NULL, NULL);
		if (font_xml) {
			font_dialog = glade_xml_get_widget (font_xml, "ApplyFontAlert");
			font_sample = glade_xml_get_widget (font_xml, "font_sample");
			gtk_label_set_markup (GTK_LABEL (font_sample),
				g_strconcat ("<span font_desc=\"",
					     theme->application_font,
					     "\">",
	/* translators: you may want to include non-western chars here */
					     _("ABCDEFG"),
					     "</span>",
					     NULL));
						
			if (gtk_dialog_run (GTK_DIALOG(font_dialog)) == GTK_RESPONSE_OK)
				apply_font = TRUE;
		} else {
			/* if installation is borked, recover and apply the font */
			apply_font = TRUE;
		}
	}

	gnome_meta_theme_set (theme);
			
	if (apply_font) {
		GConfClient *client = gconf_client_get_default ();
		gconf_client_set_string (client, FONT_KEY, theme->application_font, NULL);
		g_object_unref (client);
	}
		
	return 0;
}
