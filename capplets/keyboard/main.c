/* -*- mode: c; style: linux -*- */

/* main.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Richard Hestilow <hestilow@ximian.com>
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

#include "capplet-util.h"
#include "bonobo-property-editor-range.h"

#include <glade/glade.h>
#include <libgnomeui/gnome-window-icon.h>

#include <gdk/gdkx.h>
#include <X11/X.h>

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#include <X11/extensions/xf86misc.h>
#endif

static void
apply_settings (Bonobo_ConfigDatabase db)
{
	gboolean repeat, click;
	int rate, delay, volume;
	int bell_volume, bell_pitch, bell_duration;

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
	XF86MiscKbdSettings kbdsettings;
#endif
	XKeyboardControl kbdcontrol;
        int event_base_return, error_base_return;

	repeat = bonobo_config_get_boolean (db, "/main/repeat", NULL);
	click = bonobo_config_get_boolean (db, "/main/click", NULL);
	rate = bonobo_config_get_ulong (db, "/main/rate", NULL);
	delay = bonobo_config_get_ulong (db, "/main/delay", NULL);
	volume = bonobo_config_get_ulong (db, "/main/volume", NULL);
	bell_volume = bonobo_config_get_ulong (db, "/main/bell_volume", NULL);
	bell_pitch = bonobo_config_get_ulong (db, "/main/bell_pitch", NULL);
	bell_duration = bonobo_config_get_ulong (db, "/main/bell_duration", NULL);

        if (repeat) {
		XAutoRepeatOn (GDK_DISPLAY ());
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
		if (XF86MiscQueryExtension (GDK_DISPLAY (),
					    &event_base_return,
					    &error_base_return) == True)
		{
                        kbdsettings.rate = rate;
                        kbdsettings.delay = delay;
                        XF86MiscSetKbdSettings (GDK_DISPLAY (), &kbdsettings);
                } else {
                        XAutoRepeatOff (GDK_DISPLAY ());
                }
#endif
	} else {
		XAutoRepeatOff (GDK_DISPLAY ());
	}

	kbdcontrol.key_click_percent = 
		click ? volume : 0;
	kbdcontrol.bell_percent = bell_volume;
	kbdcontrol.bell_pitch = bell_pitch;
	kbdcontrol.bell_duration = bell_duration;
	XChangeKeyboardControl (GDK_DISPLAY (), KBKeyClickPercent, 
				&kbdcontrol);
}

static gulong
get_value_ulong (Bonobo_PropertyBag bag, const gchar *prop)
{
	BonoboArg *arg;
	gulong val;
	
	arg = bonobo_property_bag_client_get_value_any (bag, prop, NULL);
	val = BONOBO_ARG_GET_GENERAL (arg, TC_ulong, CORBA_unsigned_long, NULL);
	bonobo_arg_release (arg);
	return val;
}

static void
bell_cb (GtkWidget *widget, Bonobo_PropertyBag bag)
{
	XKeyboardState backup;
	XKeyboardControl kbdcontrol;

	XGetKeyboardControl (GDK_DISPLAY (), &backup);

	kbdcontrol.bell_percent = get_value_ulong (bag, "bell_volume");
	kbdcontrol.bell_pitch = get_value_ulong (bag, "bell_pitch");
	kbdcontrol.bell_duration = get_value_ulong (bag, "bell_duration");
	XChangeKeyboardControl (GDK_DISPLAY (),
				KBBellPercent | KBBellPitch | KBBellDuration, 
				&kbdcontrol);
	XBell (GDK_DISPLAY (), 0);

	kbdcontrol.bell_percent = backup.bell_percent;
	kbdcontrol.bell_pitch = backup.bell_pitch;
	kbdcontrol.bell_duration = backup.bell_duration;

	XChangeKeyboardControl (GDK_DISPLAY (),
				KBBellPercent | KBBellPitch | KBBellDuration, 
				&kbdcontrol);
}

static GtkWidget*
create_dialog (Bonobo_PropertyBag bag)
{
	GladeXML *dialog;
	GtkWidget *widget, *pixmap;

	dialog = glade_xml_new (GLADE_DATADIR "/keyboard-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (dialog, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", dialog);

	/* Minor GUI addition */
	pixmap = gnome_stock_pixmap_widget (WID ("bell_test_button"),
					    GNOME_STOCK_PIXMAP_VOLUME);
	gtk_box_pack_start (GTK_BOX (WID ("bell_test_holder")), pixmap,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (WID ("bell_test_button"));

	gtk_signal_connect (GTK_OBJECT (WID ("bell_test_button")),
			    "clicked", bell_cb, bag);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
				   GTK_OBJECT (dialog));

	return widget;
}

static void
setup_dialog (GtkWidget *widget, Bonobo_PropertyBag bag)
{
	GladeXML *dialog;
	BonoboPEditor *ed;
	
	dialog = gtk_object_get_data (GTK_OBJECT (widget), "glade-data");
	
	CREATE_PEDITOR (boolean, "repeat", "repeat_toggle");
	
	ed = BONOBO_PEDITOR (bonobo_peditor_option_construct (0, WID ("delay_menu")));
	bonobo_peditor_set_property (ed, bag, "delay", TC_ulong, NULL);
	
	ed = BONOBO_PEDITOR (bonobo_peditor_option_construct (0, WID ("repeat_menu")));
	bonobo_peditor_set_property (ed, bag, "rate", TC_ulong, NULL);
	bonobo_peditor_set_guard (WID ("repeat_table"), bag, "repeat");

	CREATE_PEDITOR (boolean, "click", "click_toggle");
	
	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("click_volume_entry")));
	bonobo_peditor_set_property (ed, bag, "volume", TC_ulong, NULL);
	bonobo_peditor_set_guard (WID ("click_hbox"), bag, "click");

	/* Bell properties */
	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("bell_volume_range")));
	bonobo_peditor_set_property (ed, bag, "bell_volume", TC_ulong, NULL);

	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("bell_pitch_range")));
	bonobo_peditor_set_property (ed, bag, "bell_pitch", TC_ulong, NULL);

	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("bell_duration_range")));
	bonobo_peditor_set_property (ed, bag, "bell_duration", TC_ulong, NULL);
}

static void
get_legacy_settings (Bonobo_ConfigDatabase db)
{
	gboolean val_boolean, def;
	gulong val_ulong;

	COPY_FROM_LEGACY (boolean, "/main/repeat", bool, "/Desktop/Keyboard/repeat=true");
	COPY_FROM_LEGACY (boolean, "/main/click", bool, "/Desktop/Keyboard/click=true");
	COPY_FROM_LEGACY (ulong, "/main/rate", int, "/Desktop/Keyboard/rate=30");
	COPY_FROM_LEGACY (ulong, "/main/delay", int, "/Desktop/Keyboard/delay=500");
	COPY_FROM_LEGACY (ulong, "/main/volume", int, "/Desktop/Keyboard/clickvolume=0");
	COPY_FROM_LEGACY (ulong, "/main/bell_volume", int, "/Desktop/Bell/percent=50");
	COPY_FROM_LEGACY (ulong, "/main/bell_pitch", int, "/Desktop/Bell/pitch=50");
	COPY_FROM_LEGACY (ulong, "/main/bell_duration", int, "/Desktop/Bell/duration=100");
}

int
main (int argc, char **argv) 
{
	glade_gnome_init ();
	capplet_init (argc, argv, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	gnome_window_icon_set_default_from_file
		(GNOMECC_ICONS_DIR"keyboard-capplet.png.png");
	
	return 0;
}
