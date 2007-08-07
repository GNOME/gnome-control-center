/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Jonathan Blandford <jrb@gnome.org>
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

#include "gconf-property-editor.h"

static GConfEnumStringPair toolbar_style_enums[] = {
  { 0, "both" },
  { 1, "both-horiz" },
  { 2, "icons" },
  { 3, "text" },
  { -1, NULL }
};


static void
show_handlebar (AppearanceData *data, gboolean show)
{
  GtkWidget *handlebox = glade_xml_get_widget (data->xml, "toolbar_handlebox");
  GtkWidget *toolbar = glade_xml_get_widget (data->xml, "toolbar_toolbar");
  GtkWidget *align = glade_xml_get_widget (data->xml, "toolbar_align");

  g_object_ref (handlebox);
  g_object_ref (toolbar);

  if (GTK_BIN (align)->child)
    gtk_container_remove (GTK_CONTAINER (align), GTK_BIN (align)->child);

  if (GTK_BIN (handlebox)->child)
    gtk_container_remove (GTK_CONTAINER (handlebox), GTK_BIN (handlebox)->child);

  if (show) {
    gtk_container_add (GTK_CONTAINER (align), handlebox);
    gtk_container_add (GTK_CONTAINER (handlebox), toolbar);
    g_object_unref (handlebox);
  } else {
    gtk_container_add (GTK_CONTAINER (align), toolbar);
  }

  g_object_unref (toolbar);
}

static void
set_toolbar_style (AppearanceData *data, const char *value)
{
  static const GtkToolbarStyle gtk_toolbar_styles[] =
    { GTK_TOOLBAR_BOTH, GTK_TOOLBAR_BOTH_HORIZ, GTK_TOOLBAR_ICONS, GTK_TOOLBAR_TEXT };

  int enum_val;

  if (!gconf_string_to_enum (toolbar_style_enums, value, &enum_val))
	  enum_val = 0;

  gtk_toolbar_set_style (GTK_TOOLBAR (glade_xml_get_widget (data->xml, "toolbar_toolbar")),
			 gtk_toolbar_styles[enum_val]);
}

static void
set_have_icons (AppearanceData *data, gboolean value)
{
  static const char *menu_item_names[] = {
    "menu_item_1",
    "menu_item_2",
    "menu_item_3",
    "menu_item_4",
    "menu_item_5",
    "cut",
    "copy",
    "paste",
    NULL
  };

  const char **name;

  for (name = menu_item_names; *name != NULL; name++) {
    GtkImageMenuItem *item = GTK_IMAGE_MENU_ITEM (glade_xml_get_widget (data->xml, *name));
    GtkWidget *image;

    if (value) {
      image = g_object_get_data (G_OBJECT (item), "image");
      if (image) {
	gtk_image_menu_item_set_image (item, image);
	g_object_unref (image);
      }
    } else {
      image = gtk_image_menu_item_get_image (item);
      g_object_set_data (G_OBJECT (item), "image", image);
      g_object_ref (image);
      gtk_image_menu_item_set_image (item, NULL);
    }
  }
}

/** GConf Callbacks and Conversions **/

static GConfValue *
toolbar_from_widget (GConfPropertyEditor *peditor, GConfValue *value)
{
  GConfValue *new_value;

  new_value = gconf_value_new (GCONF_VALUE_STRING);
  gconf_value_set_string (new_value,
      gconf_enum_to_string (toolbar_style_enums,
			    gconf_value_get_int (value)));

  return new_value;
}

static GConfValue *
toolbar_to_widget (GConfPropertyEditor *peditor, GConfValue *value)
{
  GConfValue *new_value;
  const gchar *str;
  gint val;

  str = (value && (value->type == GCONF_VALUE_STRING)) ?
	gconf_value_get_string (value) : NULL;

  if (!gconf_string_to_enum (toolbar_style_enums, str, &val))
    val = 0;

  new_value = gconf_value_new (GCONF_VALUE_INT);
  gconf_value_set_int (new_value, val);
  return new_value;
}

static void
toolbar_style_cb (GConfPropertyEditor *peditor,
		  gchar               *key,
		  GConfValue          *value,
		  AppearanceData      *data)
{
  set_toolbar_style (data, gconf_value_get_string (value));
}

static void
menus_have_icons_cb (GConfPropertyEditor *peditor,
		     gchar               *key,
		     GConfValue          *value,
		     AppearanceData      *data)
{
  set_have_icons (data, gconf_value_get_bool (value));
}

static void
toolbar_detachable_cb (GConfClient    *client,
		       guint           id,
		       GConfEntry     *entry,
		       AppearanceData *data)
{
  show_handlebar (data, gconf_value_get_bool (entry->value));
}

/** GUI Callbacks **/

static gint
button_press_block_cb (GtkWidget *toolbar,
		       GdkEvent  *event,
		       gpointer   data)
{
  return TRUE;
}

/** Public Functions **/

void
ui_init (AppearanceData *data)
{
  GObject *peditor;
  char *toolbar_style;

  gconf_client_add_dir (data->client, "/desktop/gnome/interface",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  peditor = gconf_peditor_new_boolean
    (NULL, "/desktop/gnome/interface/can_change_accels",
     glade_xml_get_widget (data->xml, "menu_accel_toggle"), NULL);

  peditor = gconf_peditor_new_boolean
    (NULL, "/desktop/gnome/interface/menus_have_icons",
     glade_xml_get_widget (data->xml, "menu_icons_toggle"), NULL);
  g_signal_connect (peditor, "value_changed",
		    (GCallback) menus_have_icons_cb, data);

  set_have_icons (data,
    gconf_client_get_bool (data->client,
			   "/desktop/gnome/interface/menus_have_icons",
			   NULL));

  peditor = gconf_peditor_new_combo_box
    (NULL, "/desktop/gnome/interface/toolbar_style",
     glade_xml_get_widget (data->xml, "toolbar_style_select"),
     "conv-to-widget-cb", toolbar_to_widget,
     "conv-from-widget-cb", toolbar_from_widget,
     NULL);
  g_signal_connect (peditor, "value_changed",
		    (GCallback) toolbar_style_cb, data);

  g_signal_connect (glade_xml_get_widget (data->xml, "toolbar_handlebox"),
		    "button_press_event",
		    (GCallback) button_press_block_cb, NULL);

  show_handlebar (data,
    gconf_client_get_bool (data->client,
			   "/desktop/gnome/interface/toolbar_detachable",
			   NULL));

  toolbar_style = gconf_client_get_string
    (data->client,
     "/desktop/gnome/interface/toolbar_style",
     NULL);
  set_toolbar_style (data, toolbar_style);
  g_free (toolbar_style);

  /* no ui for detachable toolbars */
  gconf_client_notify_add (data->client,
			   "/desktop/gnome/interface/toolbar_detachable",
                           (GConfClientNotifyFunc) toolbar_detachable_cb,
                           data, NULL, NULL);
}
