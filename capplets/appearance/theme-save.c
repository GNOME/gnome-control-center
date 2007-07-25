/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "appearance.h"

#include <libgnomevfs/gnome-vfs-ops.h>

#include "theme-save.h"
#include "theme-util.h"
#include "capplet-util.h"

static GQuark error_quark;
enum {
  INVALID_THEME_NAME
};

/* taken from gnome-desktop-item.c */
static gchar *
escape_string_and_dup (const gchar *s)
{
  gchar *return_value, *p;
  const gchar *q;
  int len = 0;

  if (s == NULL)
    return g_strdup("");

  q = s;
  while (*q)
    {
      len++;
      if (strchr ("\n\r\t\\", *q) != NULL)
	len++;
      q++;
    }
  return_value = p = (gchar *) g_malloc (len + 1);
  do
    {
      switch (*s)
	{
	case '\t':
	  *p++ = '\\';
	  *p++ = 't';
	  break;
	case '\n':
	  *p++ = '\\';
	  *p++ = 'n';
	  break;
	case '\r':
	  *p++ = '\\';
	  *p++ = 'r';
	  break;
	case '\\':
	  *p++ = '\\';
	  *p++ = '\\';
	  break;
	default:
	  *p++ = *s;
	}
    }
  while (*s++);
  return return_value;
}

static gboolean
check_theme_name (const gchar  *theme_name,
		  GError      **error)
{
  if (theme_name == NULL) {
    g_set_error (error,
		 error_quark,
		 INVALID_THEME_NAME,
		 _("Theme name must be present"));
    return FALSE;
  }
  return TRUE;
}

static gchar *
str_remove_slash (const gchar *src)
{
  const gchar *i;
  gchar *rtn;
  gint len = 0;
  i = src;

  while (*i) {
    if (*i != '/')
      len++;
    i++;
  }

  rtn = (gchar *) g_malloc (len + 1);
  while (*src) {
    if (*src != '/') {
      *rtn = *src;
      rtn++;
    }
    src++;
  }
  *rtn = '\0';
  return rtn - len;
}

static gboolean
setup_directory_structure (const gchar  *theme_name,
			   GError      **error)
{
  gchar *dir, *theme_name_dir;
  GnomeVFSURI *uri;
  gboolean retval = TRUE;

  theme_name_dir = str_remove_slash (theme_name);

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);
  g_free (dir);

  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, NULL);
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);
  g_free (dir);

  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme", NULL);
  g_free (theme_name_dir);
  uri = gnome_vfs_uri_new (dir);
  g_free (dir);

  if (gnome_vfs_uri_exists (uri)) {
    GtkDialog *dialog;
    GtkWidget *button;
    gint response;

    dialog = (GtkDialog *) gtk_message_dialog_new (NULL,
						   GTK_DIALOG_MODAL,
						   GTK_MESSAGE_QUESTION,
	 					   GTK_BUTTONS_CANCEL,
						   _("The theme already exists. Would you like to replace it?"));
    button = gtk_dialog_add_button (dialog, _("_Overwrite"), GTK_RESPONSE_ACCEPT);
    gtk_button_set_image (GTK_BUTTON (button),
			  gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_BUTTON));
    response = gtk_dialog_run (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
    retval = (response != GTK_RESPONSE_CANCEL);
  }

  gnome_vfs_uri_unref (uri);

  return retval;
}

static gboolean
write_theme_to_disk (GnomeThemeMetaInfo  *theme_info,
		     const gchar         *theme_name,
		     const gchar         *theme_description,
		     gboolean		  save_background,
		     GError             **error)
{
  gchar *dir, *theme_name_dir;
  GnomeVFSURI *uri;
  GnomeVFSURI *target_uri;
  GnomeVFSHandle *handle = NULL;
  GnomeVFSFileSize bytes_written;
  gchar *str, *current_background;
  GConfClient *client;
  const gchar *theme_header =
      "[Desktop Entry]\n"
      "Name=%s\n"
      "Type=X-GNOME-Metatheme\n"
      "Comment=%s\n"
      "Encoding=UTF-8\n"
      "\n"
      "[X-GNOME-Metatheme]\n"
      "GtkTheme=%s\n"
      "MetacityTheme=%s\n"
      "IconTheme=%s\n";

  theme_name_dir = str_remove_slash (theme_name);
  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme~", NULL);
  g_free (theme_name_dir);

  uri = gnome_vfs_uri_new (dir);
  dir [strlen (dir) - 1] = '\000';
  target_uri = gnome_vfs_uri_new (dir);
  g_free (dir);
  gnome_vfs_create_uri (&handle, uri, GNOME_VFS_OPEN_READ | GNOME_VFS_OPEN_WRITE, FALSE, 0644);

  gnome_vfs_truncate_handle (handle, 0);

  /* start making the theme file */
  str = g_strdup_printf (theme_header, theme_name, theme_description,
			 theme_info->gtk_theme_name,
			 theme_info->metacity_theme_name,
			 theme_info->icon_theme_name);
  gnome_vfs_write (handle, str, strlen (str), &bytes_written);
  g_free (str);

  if (theme_info->gtk_color_scheme) {
    gchar *a, *tmp;
    tmp = g_strdup (theme_info->gtk_color_scheme);
    for (a = tmp; *a != '\0'; a++)
      if (*a == '\n')
        *a = ',';
    str = g_strdup_printf ("GtkColorScheme=%s\n", tmp);
    gnome_vfs_write (handle, str, strlen (str), &bytes_written);
    g_free (str);
    g_free (tmp);
  }

  if (theme_info->cursor_theme_name) {
    str = g_strdup_printf ("CursorTheme=%s\n"
                           "CursorSize=%i\n",
                           theme_info->cursor_theme_name,
                           theme_info->cursor_size);
    gnome_vfs_write (handle, str, strlen (str), &bytes_written);
    g_free (str);
  }

  if (save_background) {
    client = gconf_client_get_default ();
    current_background = gconf_client_get_string (client, BACKGROUND_KEY, NULL);

    if (current_background != NULL) {
      str = g_strdup_printf ("BackgroundImage=%s\n", current_background);

      gnome_vfs_write (handle, str, strlen (str), &bytes_written);

      g_free (current_background);
      g_free (str);
    }
    g_object_unref (client);
  }

  gnome_vfs_close (handle);

  gnome_vfs_move_uri (uri, target_uri, TRUE);
  gnome_vfs_uri_unref (uri);
  gnome_vfs_uri_unref (target_uri);

  return TRUE;
}

static gboolean
save_theme_to_disk (GnomeThemeMetaInfo  *theme_info,
		    const gchar         *theme_name,
		    const gchar         *theme_description,
		    gboolean		 save_background,
		    GError             **error)
{
  if (!check_theme_name (theme_name, error))
    return FALSE;

  if (!setup_directory_structure (theme_name, error))
    return FALSE;

  if (!write_theme_to_disk (theme_info, theme_name, theme_description, save_background, error))
    return FALSE;

  return TRUE;
}

static void
save_dialog_response (GtkWidget      *save_dialog,
		      gint            response_id,
		      AppearanceData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    GtkWidget *entry;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    gchar *buffer_text;
    GnomeThemeMetaInfo *theme_info;
    gchar *theme_description = NULL;
    gchar *theme_name = NULL;
    gboolean save_background;
    GError *error = NULL;

    entry = glade_xml_get_widget (data->xml, "save_dialog_entry");
    theme_name = escape_string_and_dup (gtk_entry_get_text (GTK_ENTRY (entry)));

    text_view = glade_xml_get_widget (data->xml, "save_dialog_textview");
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_get_start_iter (buffer, &start_iter);
    gtk_text_buffer_get_end_iter (buffer, &end_iter);
    buffer_text = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
    theme_description = escape_string_and_dup (buffer_text);
    g_free (buffer_text);
    theme_info = (GnomeThemeMetaInfo *) g_object_get_data (G_OBJECT (save_dialog), "meta-theme-info");
    save_background = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
		      glade_xml_get_widget (data->xml, "save_background_checkbutton")));

    save_theme_to_disk (theme_info, theme_name, theme_description, save_background, &error);

    g_free (theme_name);
    g_free (theme_description);
    g_clear_error (&error);
  }

  gtk_widget_hide (save_dialog);
}

static void
entry_text_changed (GtkEditable *editable,
		    GladeXML    *dialog)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (editable));
  gtk_widget_set_sensitive (glade_xml_get_widget (dialog, "save_dialog_save_button"),
			    text != NULL && text[0] != '\000');
}

void
theme_save_dialog_run (GnomeThemeMetaInfo *theme_info,
		       AppearanceData     *data)
{
  GtkWidget *entry;
  GtkWidget *text_view;
  GtkTextBuffer *text_buffer;

  entry = glade_xml_get_widget (data->xml, "save_dialog_entry");
  text_view = glade_xml_get_widget (data->xml, "save_dialog_textview");

  if (data->theme_save_dialog == NULL) {
    data->theme_save_dialog = glade_xml_get_widget (data->xml, "theme_save_dialog");

    g_signal_connect (data->theme_save_dialog, "response", (GCallback) save_dialog_response, data);
    g_signal_connect (data->theme_save_dialog, "delete-event", (GCallback) gtk_true, NULL);
    g_signal_connect (entry, "changed", (GCallback) entry_text_changed, data->xml);

    error_quark = g_quark_from_string ("gnome-theme-save");
    gtk_widget_set_size_request (text_view, 300, 100);
  }

  gtk_entry_set_text (GTK_ENTRY (entry), "");
  entry_text_changed (GTK_EDITABLE (entry), data->xml);
  gtk_widget_grab_focus (entry);

  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_buffer_set_text (text_buffer, "", 0);
  g_object_set_data (G_OBJECT (data->theme_save_dialog), "meta-theme-info", theme_info);
  gtk_window_set_transient_for (GTK_WINDOW (data->theme_save_dialog),
				GTK_WINDOW (glade_xml_get_widget (data->xml, "appearance_window")));
  gtk_widget_show (data->theme_save_dialog);
}
