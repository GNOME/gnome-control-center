/*
 * capplet-stock-icons.c
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Rajkumar Sivasamy <rajkumar.siva@wipro.com> 
 *	Taken bits of code from panel-stock-icons.c, Thanks Mark <mark@skynet.ie>
 */

#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <gnome.h>

#include "capplet-stock-icons.h"

static GtkIconSize keyboard_capplet_icon_size = 0;
static GtkIconSize mouse_capplet_icon_size = 0;
static GtkIconSize mouse_capplet_dblclck_icon_size = 0;

GtkIconSize
keyboard_capplet_icon_get_size (void)
{
	return keyboard_capplet_icon_size;
}

GtkIconSize
mouse_capplet_icon_get_size (void)
{
	return mouse_capplet_icon_size;
}

GtkIconSize
mouse_capplet_dblclck_icon_get_size (void)
{
	return mouse_capplet_dblclck_icon_size;
}

typedef struct 
{
	char *stock_id;
	char *name;
} CappletStockIcon;


static CappletStockIcon items [] = {
	{ KEYBOARD_REPEAT, "keyboard-repeat.png" },
	{ KEYBOARD_CURSOR, "keyboard-cursor.png" },
	{ KEYBOARD_VOLUME, "keyboard-volume.png" },
	{ KEYBOARD_BELL, "keyboard-bell.png" },
	{ ACCESSX_KEYBOARD_BOUNCE, "accessibility-keyboard-bouncekey.png"},
	{ ACCESSX_KEYBOARD_SLOW, "accessibility-keyboard-slowkey.png"},
	{ ACCESSX_KEYBOARD_MOUSE, "accessibility-keyboard-mousekey.png"},
	{ ACCESSX_KEYBOARD_STICK, "accessibility-keyboard-stickykey.png"},
	{ ACCESSX_KEYBOARD_TOGGLE, "accessibility-keyboard-togglekey.png"},
	{ MOUSE_DBLCLCK_MAYBE, "double-click-maybe.png"},
	{ MOUSE_DBLCLCK_ON, "double-click-on.png"},
	{ MOUSE_DBLCLCK_OFF, "double-click-off.png"},
	{ MOUSE_RIGHT_HANDED, "mouse-right.png"},
	{ MOUSE_LEFT_HANDED, "mouse-left.png"}
};

static void
capplet_register_stock_icons (GtkIconFactory *factory)
{
	gint i;
	GtkIconSource *source;
	GnomeProgram *program;

	source =  gtk_icon_source_new ();
	program = gnome_program_get ();

	for (i = 0; i <  G_N_ELEMENTS (items); ++i) {
		GtkIconSet *icon_set; 
		char *filename;
		filename = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, items[i].name, TRUE, NULL);

		if (!filename) {
			g_warning (_("Unable to load capplet stock icon '%s'\n"), items[i].name);
			icon_set = gtk_icon_factory_lookup_default (GTK_STOCK_MISSING_IMAGE);
			gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
			continue;
		}

		gtk_icon_source_set_filename (source, filename);
		g_free (filename);

		icon_set = gtk_icon_set_new ();
		gtk_icon_set_add_source (icon_set, source);
		gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
		gtk_icon_set_unref (icon_set);
	}
	gtk_icon_source_free (source);
}

void
capplet_init_stock_icons (void)
{
	GtkIconFactory *factory;
	static gboolean initialized = FALSE;

	if (initialized) 
		return;
	initialized = TRUE;

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);
	capplet_register_stock_icons (factory);

	keyboard_capplet_icon_size = gtk_icon_size_register ("keyboard-capplet",
							      KEYBOARD_CAPPLET_DEFAULT_ICON_SIZE,
							      KEYBOARD_CAPPLET_DEFAULT_ICON_SIZE);

	mouse_capplet_icon_size = gtk_icon_size_register ("mouse-capplet",
							   MOUSE_CAPPLET_DEFAULT_WIDTH,
							   MOUSE_CAPPLET_DEFAULT_HEIGHT);

	mouse_capplet_dblclck_icon_size = gtk_icon_size_register ("mouse-capplet-dblclck-icon",
								   MOUSE_CAPPLET_DBLCLCK_ICON_SIZE,
								   MOUSE_CAPPLET_DBLCLCK_ICON_SIZE);
	g_object_unref (factory);
}
