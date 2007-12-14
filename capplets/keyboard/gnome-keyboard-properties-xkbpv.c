/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkbpv.c
 * Copyright (C) 2003-2007 Sergey V. Udaltsov
 *
 * Written by: Sergey V. Udaltsov <svu@gnome.org>
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

#include <gnome.h>
#include <glade/glade.h>

#include "capplet-util.h"

#include "gnome-keyboard-properties-xkb.h"
#include <libgnomekbd/gkbd-keyboard-drawing.h>

#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include "X11/XKBlib.h"
/**
 * BAD STYLE: Taken from xklavier_private_xkb.h
 * Any ideas on architectural improvements are WELCOME
 */
extern gboolean xkl_xkb_config_native_prepare (XklEngine * engine,
					       const XklConfigRec * data,
					       XkbComponentNamesPtr
					       component_names);

extern void xkl_xkb_config_native_cleanup (XklEngine * engine,
					   XkbComponentNamesPtr
					   component_names);

/* */
#endif

static GkbdKeyboardDrawingGroupLevel groupsLevels[] =
    { {0, 1}, {0, 3}, {0, 0}, {0, 2} };
static GkbdKeyboardDrawingGroupLevel *pGroupsLevels[] = {
	groupsLevels, groupsLevels + 1, groupsLevels + 2, groupsLevels + 3
};

GtkWidget *
xkb_layout_preview_create_widget (GladeXML * chooserDialog)
{
	GtkWidget *kbdraw = gkbd_keyboard_drawing_new ();

	gkbd_keyboard_drawing_set_groups_levels (GKBD_KEYBOARD_DRAWING
						 (kbdraw), pGroupsLevels);
	return kbdraw;
}

void
xkb_layout_preview_update (GladeXML * chooser_dialog)
{
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	GtkWidget *chooser = CWID ("xkb_layout_chooser");
	GtkWidget *kbdraw =
	    GTK_WIDGET (g_object_get_data (G_OBJECT (chooser), "kbdraw"));
	gchar *id = xkb_layout_chooser_get_selected_id (chooser_dialog);
	xkb_layout_preview_set_drawing_layout (kbdraw, id);
	g_free (id);
#endif
}

void
xkb_layout_preview_set_drawing_layout (GtkWidget * kbdraw, const gchar * id)
{
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	if (kbdraw != NULL && id != NULL) {
		XklConfigRec *data;
		char **p, *layout, *variant;
		XkbComponentNamesRec component_names;

		data = xkl_config_rec_new ();
		if (xkl_config_rec_get_from_server (data, engine)) {
			if ((p = data->layouts) != NULL)
				g_strfreev (data->layouts);

			if ((p = data->variants) != NULL)
				g_strfreev (data->variants);

			data->layouts = g_new0 (char *, 2);
			data->variants = g_new0 (char *, 2);
			if (gkbd_keyboard_config_split_items
			    (id, &layout, &variant)
			    && variant != NULL) {
				data->layouts[0] =
				    (layout ==
				     NULL) ? NULL : g_strdup (layout);
				data->variants[0] =
				    (variant ==
				     NULL) ? NULL : g_strdup (variant);
			} else {
				data->layouts[0] =
				    (id == NULL) ? NULL : g_strdup (id);
				data->variants[0] = NULL;
			}

			if (xkl_xkb_config_native_prepare
			    (engine, data, &component_names)) {
				gkbd_keyboard_drawing_set_keyboard
				    (GKBD_KEYBOARD_DRAWING (kbdraw),
				     &component_names);

				xkl_xkb_config_native_cleanup (engine,
							       &component_names);
			}
		}
		g_object_unref (G_OBJECT (data));
	}
#endif
}

typedef struct {
	GtkWidget *kbdraw;
	const gchar *id;
} XkbLayoutPreviewPrintData;


static void
xkb_layout_preview_begin_print (GtkPrintOperation         *operation,
				GtkPrintContext           *context,
				XkbLayoutPreviewPrintData *data)
{
	gtk_print_operation_set_n_pages (operation, 1);
}

static void
xkb_layout_preview_draw_page (GtkPrintOperation         *operation,
			      GtkPrintContext           *context,
			      gint                       page_nr,
			      XkbLayoutPreviewPrintData *data)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (context);
	PangoLayout *layout = gtk_print_context_create_pango_layout (context);
	PangoFontDescription *desc =
		pango_font_description_from_string ("sans 8");
	gdouble width = gtk_print_context_get_width (context);
	gdouble height = gtk_print_context_get_height (context);
	gdouble dpi_x = gtk_print_context_get_dpi_x (context);
	gdouble dpi_y = gtk_print_context_get_dpi_y (context);
	gchar *header, *description;

	gtk_print_operation_set_unit (operation, GTK_PIXELS);

	description = xkb_layout_description_utf8 (data->id);
	header = g_strdup_printf
		(_("Keyboard layout \"%s\"\n"
		   "Copyright &#169; X.Org Foundation and "
		   "XKeyboardConfig contributors\n"
		   "For licensing see package metadata"), description);
	g_free (description);
	pango_layout_set_markup (layout, header, -1);
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	pango_layout_set_width (layout, pango_units_from_double (width));
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_move_to (cr, 0, 0);
	pango_cairo_show_layout (cr, layout);

	gkbd_keyboard_drawing_render (GKBD_KEYBOARD_DRAWING (data->kbdraw),
				      cr, layout,
				      0.0, 0.0, width, height, dpi_x, dpi_y);

	g_object_unref (layout);
}

void
xkb_layout_preview_print (GtkWidget *kbdraw, GtkWindow *parent_window,
			  const gchar *id)
{
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	static GtkPrintSettings *settings = NULL;
	XkbLayoutPreviewPrintData data = { kbdraw, id };

	print = gtk_print_operation_new ();

	if (settings != NULL)
		gtk_print_operation_set_print_settings (print, settings);

	g_signal_connect (print, "begin_print",
			  G_CALLBACK (xkb_layout_preview_begin_print), &data);
	g_signal_connect (print, "draw_page",
			  G_CALLBACK (xkb_layout_preview_draw_page), &data);

	res = gtk_print_operation_run (print,
				       GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       parent_window,
				       NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (settings != NULL)
			g_object_unref (settings);
		settings = gtk_print_operation_get_print_settings (print);
		g_object_ref (settings);
	}

	g_object_unref (print);
}
