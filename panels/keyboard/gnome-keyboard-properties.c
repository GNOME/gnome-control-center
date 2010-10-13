/* -*- mode: c; style: linux -*- */

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

#include "gconf-property-editor.h"

#include "gnome-keyboard-properties.h"
#include "gnome-keyboard-properties-a11y.h"
#include "gnome-keyboard-properties-xkb.h"

enum {
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static void keyboard_settings_changed (GSettings *settings, const gchar *key, GtkBuilder *dialog);

static GSettings *keyboard_settings = NULL;

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

static GConfValue *
blink_from_widget (GConfPropertyEditor * peditor, const GConfValue * value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value,
			     2600 - gconf_value_get_int (value));

	return new_value;
}

static GConfValue *
blink_to_widget (GConfPropertyEditor * peditor, const GConfValue * value)
{
	GConfValue *new_value;
	gint current_rate;

	current_rate = gconf_value_get_int (value);
	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value,
			     CLAMP (2600 - current_rate, 100, 2500));

	return new_value;
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
repeat_delay_scale_changed (GtkRange *range, GtkBuilder *builder)
{
	g_signal_handlers_block_by_func (keyboard_settings, keyboard_settings_changed, builder);
	g_settings_set_int (keyboard_settings, "delay", (gint) gtk_range_get_value (range));
	g_signal_handlers_unblock_by_func (keyboard_settings, keyboard_settings_changed, builder);
}

static void
repeat_speed_scale_changed (GtkRange *range, GtkBuilder *builder)
{
	g_signal_handlers_block_by_func (keyboard_settings, keyboard_settings_changed, builder);
	g_settings_set_int (keyboard_settings, "rate", (gint) gtk_range_get_value (range));
	g_signal_handlers_unblock_by_func (keyboard_settings, keyboard_settings_changed, builder);
}

static void
setup_dialog (GtkBuilder * dialog)
{
	GObject *peditor;

	g_settings_bind (keyboard_settings, "repeat",
			 WID ("repeat_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);

	/* For scale widgets, the mapping does not work, so connect to signals */
	g_signal_connect (WID ("repeat_delay_scale"), "value_changed",
			  G_CALLBACK (repeat_delay_scale_changed), NULL);
	g_signal_connect (WID ("repeat_speed_scale"), "value_changed",
			  G_CALLBACK (repeat_speed_scale_changed), NULL);

	/* FIXME: GConf stuff that needs to be solved */
	peditor = gconf_peditor_new_boolean
	    (NULL, "/desktop/gnome/interface/cursor_blink",
	     WID ("cursor_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
					WID ("cursor_hbox"));
	gconf_peditor_new_numeric_range (NULL,
					 "/desktop/gnome/interface/cursor_blink_time",
					 WID ("cursor_blink_time_scale"),
					 "conv-to-widget-cb",
					 blink_to_widget,
					 "conv-from-widget-cb",
					 blink_from_widget, NULL);

	g_signal_connect (WID ("keyboard_dialog"), "response",
			  (GCallback) dialog_response, NULL);

	setup_xkb_tabs (dialog, NULL);
	setup_a11y_tabs (dialog, NULL);
}

static void
keyboard_settings_changed (GSettings *settings, const gchar *key, GtkBuilder *dialog)
{
	if (g_str_equal (key, "delay")) {
		g_signal_handlers_block_by_func (WID ("repeat_delay_scale"), repeat_delay_scale_changed, dialog);
		gtk_range_set_value (GTK_RANGE (WID ("repeat_delay_scale")), (gdouble) g_settings_get_int (settings, "delay"));
		g_signal_handlers_unblock_by_func (WID ("repeat_delay_scale"), repeat_delay_scale_changed, dialog);
	} else if (g_str_equal (key, "rate")) {
		g_signal_handlers_block_by_func (WID ("repeat_speed_scale"), repeat_speed_scale_changed, dialog);
		gtk_range_set_value (GTK_RANGE (WID ("repeat_speed_scale")), (gdouble) g_settings_get_int (settings, "rate"));
		g_signal_handlers_unblock_by_func (WID ("repeat_speed_scale"), repeat_speed_scale_changed, dialog);
	}
}

GtkWidget *
gnome_keyboard_properties_init (GtkBuilder * dialog)
{
	GtkWidget *dialog_win = NULL;

	if (keyboard_settings == NULL) {
		keyboard_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.keyboard");
		g_signal_connect (keyboard_settings, "changed",
				  G_CALLBACK (keyboard_settings_changed), dialog);
	}

	create_dialog (dialog);
	if (dialog) {
		setup_dialog (dialog);
		dialog_win = WID ("keyboard_dialog");
		/* g_signal_connect (dialog_win, "response",
		   G_CALLBACK (dialog_response_cb), NULL); */
	}

	return dialog_win;
}
