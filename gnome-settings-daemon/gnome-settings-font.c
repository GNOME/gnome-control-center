/*
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Authors:  Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gnome-settings-module.h"
#include "utils.h"

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleFont;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleFontClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_font_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_font_initialize (GnomeSettingsModule *module, GConfClient *config_client);

static void
gnome_settings_module_font_class_init (GnomeSettingsModuleFontClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_font_get_runlevel;
	module_class->initialize = gnome_settings_module_font_initialize;
}

static void
gnome_settings_module_font_init (GnomeSettingsModuleFont *module)
{
}

GType
gnome_settings_module_font_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleFontClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_font_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleFont),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_font_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleFont",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_font_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS;
}

static void
load_xcursor_theme (GConfClient *client)
{
	gchar *cursor_theme;
	gint size;
	char *add[] = { "xrdb", "-nocpp", "-merge", NULL };
	GString *add_string = g_string_new (NULL);

	cursor_theme = gconf_client_get_string (client,
						"/desktop/gnome/peripherals/mouse/cursor_theme",
						NULL);
	size = gconf_client_get_int (client,
				     "/desktop/gnome/peripherals/mouse/cursor_size",
				     NULL);
	if (cursor_theme == NULL || size <= 0)
		return;

	g_string_append_printf (add_string,
				"Xcursor.theme: %s\n", cursor_theme);
	g_string_append (add_string, "Xcursor.theme_core: true\n");
	g_string_append_printf (add_string,
				"Xcursor.size: %d\n", size);

	gnome_settings_spawn_with_input (add, add_string->str);

	g_free (cursor_theme);
	g_string_free (add_string, TRUE);
}

static void
load_cursor (GConfClient *client)
{
	DIR *dir;
	gchar *font_dir_name;
	gchar *dir_name;
	struct dirent *file_dirent;
	gchar *cursor_font;
	gchar **font_path;
	gchar **new_font_path;
	gint n_fonts;
	gint new_n_fonts;
	gint i;
	gchar *mkfontdir_cmd;

	/* setting up the dir */
	font_dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome2/share/fonts", NULL);
	if (! g_file_test (font_dir_name, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (font_dir_name, 0755) != 0) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 0,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 _("Cannot create the directory \"%s\".\n"\
							   "This is needed to allow changing the mouse pointer theme."),
							 font_dir_name);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gnome_settings_delayed_show_dialog (dialog);
			g_free (font_dir_name);

			return;
		}
	}

	dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome2/share/cursor-fonts", NULL);
	if (! g_file_test (dir_name, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (dir_name, 0755) != 0) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 0,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 (_("Cannot create the directory \"%s\".\n"\
							    "This is needed to allow changing cursors.")),
							 dir_name);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gnome_settings_delayed_show_dialog (dialog);
			g_free (dir_name);

			return;
		}
	}

	dir = opendir (dir_name);
  
	while ((file_dirent = readdir (dir)) != NULL) {
		struct stat st;
		gchar *link_name;

		link_name = g_build_filename (dir_name, file_dirent->d_name, NULL);
		if (lstat (link_name, &st)) {
			g_free (link_name);
			continue;
		} 
		g_free (link_name);
      	  
		if (S_ISLNK (st.st_mode))
			unlink (link_name);
	}

	closedir (dir);

	cursor_font = gconf_client_get_string (client,
					       "/desktop/gnome/peripherals/mouse/cursor_font",
					       NULL);

	if ((cursor_font != NULL) &&
	    (g_file_test (cursor_font, G_FILE_TEST_IS_REGULAR)) &&
	    (g_path_is_absolute (cursor_font))) {
		gchar *newpath;
		gchar *font_name;

		font_name = strrchr (cursor_font, G_DIR_SEPARATOR);
		newpath = g_build_filename (dir_name, font_name, NULL);
		symlink (cursor_font, newpath);
		g_free (newpath);
	}
	g_free (cursor_font);

	/* run mkfontdir */
	mkfontdir_cmd = g_strdup_printf ("mkfontdir %s %s", dir_name, font_dir_name);
	/* maybe check for error...
	 * also, it's not going to like that if there are spaces in dir_name/font_dir_name.
	 */
	g_spawn_command_line_sync (mkfontdir_cmd, NULL, NULL, NULL, NULL);
	g_free (mkfontdir_cmd);

	/* Set the font path */
	font_path = XGetFontPath (gdk_x11_get_default_xdisplay (), &n_fonts);
	new_n_fonts = n_fonts;
	if (n_fonts == 0 || strcmp (font_path[0], dir_name))
		new_n_fonts++;
	if (n_fonts == 0 || strcmp (font_path[n_fonts-1], font_dir_name))
		new_n_fonts++;

	new_font_path = g_new0 (gchar*, new_n_fonts);
	if (n_fonts == 0 || strcmp (font_path[0], dir_name)) {
		new_font_path[0] = dir_name;
		for (i = 0; i < n_fonts; i++)
			new_font_path [i+1] = font_path [i];
	} else {
		for (i = 0; i < n_fonts; i++)
			new_font_path [i] = font_path [i];
	}

	if (n_fonts == 0 || strcmp (font_path[n_fonts-1], font_dir_name)) {
		new_font_path[new_n_fonts-1] = font_dir_name;
	}

	gdk_error_trap_push ();
	XSetFontPath (gdk_display, new_font_path, new_n_fonts);
	gdk_flush ();

	/* if there was an error setting the new path, revert */
	if (gdk_error_trap_pop ())
		XSetFontPath (gdk_display, font_path, n_fonts); 

	XFreeFontPath (font_path);

	g_free (new_font_path);
	g_free (font_dir_name);
	g_free (dir_name);
}

static gboolean
gnome_settings_module_font_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	load_xcursor_theme (config_client);
	load_cursor (config_client);

	return TRUE;
}
