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

#define WID(s) GTK_WIDGET (gtk_builder_get_object (dialog, s))

static void
create_dialog (GtkBuilder * dialog)
{
	GtkWidget *image;

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
setup_dialog (GtkBuilder * dialog)
{
	setup_xkb_tabs (dialog);
}

GtkWidget *
gnome_region_properties_init (GtkBuilder * dialog)
{
	GtkWidget *dialog_win = NULL;

	create_dialog (dialog);
	if (dialog) {
		setup_dialog (dialog);
		dialog_win = WID ("region_dialog");
		/* g_signal_connect (dialog_win, "response",
		   G_CALLBACK (dialog_response_cb), NULL); */
	}

	return dialog_win;
}
