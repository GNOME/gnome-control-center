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
        int nbuttons, num, den, idx_1 = 0, idx_3 = 1;
	ulong accel, threshold;
        gboolean rtol;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

        rtol = bonobo_config_get_ulong (db, "/main/right-to-left", &ev);

        nbuttons = XGetPointerMapping (GDK_DISPLAY (), buttons, MAX_BUTTONS);

        for (i = 0; i < nbuttons; i++) {
		if (buttons[i] == 1)
			idx_1 = i;
		else if (buttons[i] == ((nbuttons < 3) ? 2 : 3))
			idx_3 = i;
	}

	if ((rtol && idx_1 < idx_3) || (!rtol && idx_1 > idx_3)) {
		buttons[idx_1] = ((nbuttons < 3) ? 2 : 3);
		buttons[idx_3] = 1;
	}

        XSetPointerMapping (GDK_DISPLAY (), buttons, nbuttons);

	CORBA_exception_init (&ev);

        accel = bonobo_config_get_ulong (db, "/main/acceleration", &ev);

        if (accel < MAX_ACCEL) {
                num = 1;
                den = MAX_ACCEL - accel;
        } else {
                num = accel - MAX_ACCEL + 1;
                den = 1;
        }

	CORBA_exception_init (&ev);

        threshold = MAX_THRESH - bonobo_config_get_ulong (db, "/main/threshold", &ev);

        XChangePointerControl (GDK_DISPLAY (), True, True,
                               num, den, threshold);

	CORBA_exception_free (&ev);
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


static GtkWidget *
mouse_capplet_create_image_widget_canvas (gchar *filename)
{
	GtkWidget *canvas;
	GdkPixbuf *pixbuf;
	double width, height;
	gchar *filename_dup;

	filename_dup = g_strdup (filename);
	pixbuf = gdk_pixbuf_new_from_file (filename_dup);

	if (!pixbuf) {
		g_warning ("Pixmap %s not found.", filename_dup);
		g_free (filename_dup);
		return NULL;
	}
		
	width  = gdk_pixbuf_get_width  (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	canvas = gnome_canvas_new_aa();
	GTK_OBJECT_UNSET_FLAGS (GTK_WIDGET (canvas), GTK_CAN_FOCUS);
	gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(canvas)),
			       gnome_canvas_pixbuf_get_type (),
			       "pixbuf", pixbuf,
			       NULL);
	gtk_widget_set_usize (canvas, width, height);

	gdk_pixbuf_unref (pixbuf);
	gtk_widget_show (canvas);
	g_free (filename_dup);

	return canvas;
}

GtkWidget *
mouse_capplet_create_image_widget (gchar *name,
							gchar *string1, gchar *string2,
							gint int1, gint int2)
{
	GtkWidget *canvas, *alignment;
	gchar *full_path;

	if (!string1)
		return NULL;

	full_path = g_strdup_printf ("%s/%s", GNOMECC_PIXMAPS_DIR, string1);
	canvas = mouse_capplet_create_image_widget_canvas (full_path);
	g_free (full_path);
	
	g_return_val_if_fail (canvas != NULL, NULL);
	
	alignment = gtk_widget_new (gtk_alignment_get_type(),
						   "child", canvas,
						   "xalign", (double) 0,
						   "yalign", (double) 0,
						   "xscale", (double) 0,
						   "yscale", (double) 0,
						   NULL);
	
	gtk_widget_show (alignment);

	return alignment;
}


/**
 * xst_fool_the_linker:
 * @void: 
 * 
 * We need to keep the symbol for the create image widget function
 * so that libglade can find it to create the icons.
 **/
void capplet_fool_the_linker (void);
void
capplet_fool_the_linker (void)
{
	mouse_capplet_create_image_widget (NULL, NULL, NULL, 0, 0);
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
