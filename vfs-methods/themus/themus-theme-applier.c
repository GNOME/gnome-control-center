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

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libbonobo.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gnome-theme-info.h>
#include <gnome-theme-apply.h>
#include <gconf/gconf-client.h>

#include <stdlib.h>

#define FONT_KEY           "/desktop/gnome/interface/font_name"

int main (int argc, char* argv[])
{
	GValue value = { 0, };
	GnomeVFSURI *uri;
	GnomeThemeMetaInfo *theme;
	GConfClient *client;
	GnomeProgram *program;
	poptContext ctx;
	gchar **args;

	program = gnome_program_init ("ThemeApplier", "0.3.0", LIBGNOMEUI_MODULE, argc, 
		argv, GNOME_PARAM_NONE);
	
	g_value_init (&value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
	ctx = g_value_get_pointer (&value);
	g_value_unset (&value);
	args = (char**) poptGetArgs(ctx);

	if (args)
	{
		gnome_vfs_init ();
		gnome_theme_init (FALSE);
		
		uri = gnome_vfs_uri_new (args[0]);
		g_assert (uri != NULL);
		
		theme = gnome_theme_read_meta_theme (uri);
		gnome_vfs_uri_unref (uri);
		
		g_assert (theme != NULL);
		
		gnome_meta_theme_set (theme);
		if (theme->application_font)
		{
			client = gconf_client_get_default ();
			gconf_client_set_string (client, FONT_KEY, theme->application_font, NULL);
		}
		return 0;
	}
	else return 1;
}
