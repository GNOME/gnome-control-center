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

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>

#include <gnome-theme-info.h>
#include <gnome-theme-apply.h>

#define FONT_KEY           "/desktop/gnome/interface/font_name"

static gboolean
run_apply_font_dialog (GnomeThemeMetaInfo *theme)
{
	gboolean apply_font = FALSE;

	if (theme->application_font) {
		GladeXML *font_xml;

		font_xml = glade_xml_new (GNOMECC_GLADE_DIR "/apply-font.glade",
					  NULL, NULL);
		if (font_xml) {
			GtkWidget *font_dialog;
			GtkWidget *font_sample;

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

			g_object_unref (font_xml);
		} else {
			/* if installation is borked, recover and apply the font */
			apply_font = TRUE;
		}
	}
	return apply_font;
}

int
main (int argc, char **argv)
{
	GnomeVFSURI *uri;
	GnomeThemeMetaInfo *theme;
	GnomeProgram *program;
	gboolean apply_font;
	GOptionContext *context;
	gchar **arguments = NULL;
	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &arguments, NULL, N_("[FILE]") },
		{NULL}
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("- GNOME Theme Applier");
#if GLIB_CHECK_VERSION (2, 12, 0)
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
#endif
	g_option_context_add_main_entries (context, options, NULL);

	program = gnome_program_init ("themus-theme-applier", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_NONE);

	if (!arguments || g_strv_length (arguments) != 1) {
		/* FIXME: print help once bug 336089 is fixed */
		g_strfreev (arguments);
		goto error;
	}

	uri = gnome_vfs_uri_new (arguments[0]);
	g_strfreev (arguments);

	if (!uri)
		goto error;

	gnome_theme_init (NULL);

	theme = gnome_theme_read_meta_theme (uri);
	gnome_vfs_uri_unref (uri);

	if (!theme)
		goto error;

	apply_font = run_apply_font_dialog (theme);

	gnome_meta_theme_set (theme);

	if (apply_font) {
		GConfClient *client = gconf_client_get_default ();
		gconf_client_set_string (client, FONT_KEY, theme->application_font, NULL);
		g_object_unref (client);
	}

	g_object_unref (program);
	return 0;

error:
	g_object_unref (program);
	return 1;
}
