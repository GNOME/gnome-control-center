/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Rodrigo Moya <rodrigo@gnome-db.org>
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
#include "wm-common.h"

#include <string.h>
#include <glib/gi18n.h>

static void
display_error (const gchar *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
set_busy (GtkWidget *widget, gboolean busy)
{
  GdkCursor *cursor;
   
  if (busy)
    cursor = gdk_cursor_new (GDK_WATCH);
  else
    cursor = NULL;
 
  gdk_window_set_cursor (widget->window, cursor);
   
  if (cursor)
    gdk_cursor_unref (cursor);
 
  gdk_flush ();
}

static void
enable_desktop_effects_cb (GtkToggleButton *toggle_button, AppearanceData *data)
{
  GError *error = NULL;
  gboolean toggled = gtk_toggle_button_get_active (toggle_button);
  const gchar *cmd_line =  toggled ? "compiz --replace ccp" : "metacity --replace";

  if (!g_spawn_command_line_async (cmd_line, &error)) {
    display_error (error->message);
    g_error_free (error);
    gtk_toggle_button_set_active (toggle_button, !toggled);
  } else {
    gchar *wm_name = wm_common_get_current_window_manager ();

    /* disable customize button for metacity */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->enable_effects_button))
        && !strcmp (wm_name, "compiz"))
      gtk_widget_show (data->customize_effects_button);
    else
      gtk_widget_hide (data->customize_effects_button);

    g_free (wm_name);
  }
}

static void
customize_desktop_effects_cb (GtkButton *button, AppearanceData *data)
{
  GError *error = NULL;
  gchar *wm_name;

  wm_name = wm_common_get_current_window_manager ();

  if (!strcmp (wm_name, "compiz")) {
    if (!g_spawn_command_line_async ("ccsm", &error)) {
      display_error (error->message);
      g_error_free (error);
    }
  }

  g_free (wm_name);
}

static void
window_manager_changed_cb (gpointer wm_name, AppearanceData *data)
{
}

void
desktop_effects_init (AppearanceData *data)
{
  gchar *wm_name;

  wm_common_register_window_manager_change ((GFunc) window_manager_changed_cb, data);
  wm_name = wm_common_get_current_window_manager ();

  /* initialize widgets */
  data->enable_effects_button = glade_xml_get_widget (data->xml, "enable_desktop_effects");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->enable_effects_button),
				gtk_widget_is_composited (data->enable_effects_button));
  g_signal_connect (G_OBJECT (data->enable_effects_button), "toggled",
		    (GCallback) enable_desktop_effects_cb, data);

  data->customize_effects_button = glade_xml_get_widget (data->xml, "customize_desktop_effects");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->enable_effects_button))
      && !strcmp (wm_name, "compiz"))
    gtk_widget_show (data->customize_effects_button);
  else
    gtk_widget_hide (data->customize_effects_button);
  g_signal_connect (G_OBJECT (data->customize_effects_button), "clicked",
		    (GCallback) customize_desktop_effects_cb, data);

  g_free (wm_name);
}

void
desktop_effects_shutdown (AppearanceData *data)
{
}
