/* keyboard-properties.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2001 Jonathan Blandford
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Rachel Hestilow <hestilow@ximian.com>
 *	       Jonathan Blandford <jrb@redhat.com>
 *             Rodrigo Moya <rodrigo@gnome.org>
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

#include <gio/gio.h>
#include <gconf/gconf-client.h>

#include "gnome-region-panel.h"
#include "gnome-region-panel-xkb.h"

enum {
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static GSettings *keyboard_settings = NULL;
static GSettings *interface_settings = NULL;

static void
create_dialog (GtkBuilder * dialog)
{
	GtkSizeGroup *size_group;
	GtkWidget *image;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_slow_label"));
	gtk_size_group_add_widget (size_group, WID ("delay_short_label"));
	gtk_size_group_add_widget (size_group, WID ("blink_slow_label"));
	g_object_unref (G_OBJECT (size_group));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_fast_label"));
	gtk_size_group_add_widget (size_group, WID ("delay_long_label"));
	gtk_size_group_add_widget (size_group, WID ("blink_fast_label"));
	g_object_unref (G_OBJECT (size_group));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_delay_scale"));
	gtk_size_group_add_widget (size_group, WID ("repeat_speed_scale"));
	gtk_size_group_add_widget (size_group,
				   WID ("cursor_blink_time_scale"));
	g_object_unref (G_OBJECT (size_group));

	image =
	    gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("xkb_layouts_add")), image);

	image =
	    gtk_image_new_from_stock (GTK_STOCK_REFRESH,
				      GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("xkb_reset_to_defaults")),
			      image);
}

static void
dialog_response (GtkWidget * widget,
		 gint response_id, GConfChangeSet * changeset)
{
/*
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (widget), "goscustperiph-2");
	else
		gtk_main_quit ();
*/
}

static void
setup_dialog (GtkBuilder * dialog)
{
	g_settings_bind (keyboard_settings, "repeat",
			 WID ("repeat_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (keyboard_settings, "delay",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_delay_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (keyboard_settings, "rate",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_speed_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (interface_settings, "cursor-blink",
			 WID ("cursor_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (interface_settings, "cursor-blink-time",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("cursor_blink_time_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (WID ("region_dialog"), "response",
			  (GCallback) dialog_response, NULL);

	setup_xkb_tabs (dialog);
}

GtkWidget *
gnome_region_properties_init (GtkBuilder * dialog)
{
	GtkWidget *dialog_win = NULL;

	if (keyboard_settings == NULL)
		keyboard_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.keyboard");

	if (interface_settings == NULL)
		interface_settings = g_settings_new ("org.gnome.desktop.interface");

	create_dialog (dialog);
	if (dialog) {
		setup_dialog (dialog);
		dialog_win = WID ("keyboard_dialog");
		/* g_signal_connect (dialog_win, "response",
		   G_CALLBACK (dialog_response_cb), NULL); */
	}

	return dialog_win;
}
