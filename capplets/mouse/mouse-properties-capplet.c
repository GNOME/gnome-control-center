/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#include "capplet-util.h"
#include "bonobo-property-editor-range.h"

#include <glade/glade.h>

/* Needed only for the mouse capplet */
#include <X11/Xlib.h>
#include <gdk/gdkx.h>

/* Maximum number of mouse buttons we handle.  */
#define MAX_BUTTONS 10

/* Half the number of acceleration levels we support.  */
#define MAX_ACCEL 3

/* Maximum threshold we support.  */
#define MAX_THRESH 7

/* apply_settings
 *
 * Apply the settings of the property bag. This function is per-capplet, though
 * there are some cases where it does not do anything.
 */

static void
apply_settings (Bonobo_ConfigDatabase db) 
{
        unsigned char buttons[MAX_BUTTONS], i;
        int nbuttons, num, max, den;
	ulong accel, threshold;
        gboolean rtol;

        rtol = bonobo_config_get_boolean (db, "/main/right-to-left", NULL);

        nbuttons = XGetPointerMapping (GDK_DISPLAY (), buttons, MAX_BUTTONS);
        max = MIN (nbuttons, 3);
        for (i = 0; i < max; i++)
                buttons[i] = rtol ? (max - i) : (i + 1);

        XSetPointerMapping (GDK_DISPLAY (), buttons, nbuttons);

        accel = bonobo_config_get_ulong (db, "/main/acceleration", NULL);

        if (accel < MAX_ACCEL) {
                num = 1;
                den = MAX_ACCEL - accel;
        } else {
                num = accel - MAX_ACCEL + 1;
                den = 1;
        }

        threshold = bonobo_config_get_ulong (db, "/main/threshold", NULL);

        XChangePointerControl (GDK_DISPLAY (), True, True,
                               num, den, threshold);
}

/* set_pixmap_file
 *
 * Load the given pixmap and put it in the given widget. FIXME: Should this be in libcommon?
 */

static void
set_pixmap_file (GtkWidget *widget, const gchar *filename)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (filename != NULL);
	
	pixbuf = gdk_pixbuf_new_from_file (filename);

	if (pixbuf) {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 100);
		gtk_pixmap_set (GTK_PIXMAP (widget),
				pixmap, mask);
		gdk_pixbuf_unref (pixbuf);
	}
}

/* create_dialog
 *
 * Create the dialog box and return it as a GtkWidget
 */

static GtkWidget *
create_dialog (void) 
{
	GladeXML *dialog;
	GtkWidget *widget;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/mouse-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (dialog, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", dialog);

	set_pixmap_file (WID ("mouse_left_pixmap"), GNOMECC_PIXMAPS_DIR "/mouse-left.png");
	set_pixmap_file (WID ("mouse_right_pixmap"), GNOMECC_PIXMAPS_DIR "/mouse-right.png");

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
				   GTK_OBJECT (dialog));

	return widget;
}

/* setup_dialog
 *
 * Set up the property editors for our dialog
 */

static void
setup_dialog (GtkWidget *widget, Bonobo_PropertyBag bag) 
{
	GladeXML *dialog;
	BonoboPEditor *ed;
	GtkWidget *rbs[3];

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "glade-data");

	rbs[0] = WID ("right_handed_select");
	rbs[1] = WID ("left_handed_select");
	rbs[2] = NULL;
	ed = BONOBO_PEDITOR (bonobo_peditor_option_radio_construct (rbs));
	bonobo_peditor_set_property (ed, bag, "right-to-left", TC_ulong, NULL);

	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("acceleration_entry")));
        bonobo_peditor_set_property (ed, bag, "acceleration", TC_ulong, NULL);

	ed = BONOBO_PEDITOR (bonobo_peditor_range_construct (WID ("sensitivity_entry")));
        bonobo_peditor_set_property (ed, bag, "threshold", TC_ulong, NULL);
}

/* get_legacy_settings
 *
 * Retrieve older gnome_config -style settings and store them in the
 * configuration database.
 *
 * In most cases, it's best to use the COPY_FROM_LEGACY macro defined in
 * capplets/common/capplet-util.h.
 */

static void
get_legacy_settings (Bonobo_ConfigDatabase db) 
{
	gboolean val_boolean, def;
	gulong val_ulong;

	COPY_FROM_LEGACY (boolean, "/main/right-to-left", bool, "/Desktop/Mouse/right-to-left");
	COPY_FROM_LEGACY (ulong, "/main/acceleration", int, "/Desktop/Mouse/aceleration=4");
	COPY_FROM_LEGACY (ulong, "/main/threshold", int, "/Desktop/Mouse/threshold=4");
}

int
main (int argc, char **argv) 
{
	const gchar *legacy_files[] = { "Desktop", NULL };

	glade_gnome_init ();
	capplet_init (argc, argv, legacy_files, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	return 0;
}
