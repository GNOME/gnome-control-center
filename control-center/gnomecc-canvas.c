/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2005 Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Jody Goldberg          <jody@gnome.org>
 *          Carlos Garnacho Parro  <carlosg@gnome.org>
 */

#include "gnomecc-canvas.h"
#include "gnomecc-event-box.h"
#include "gnomecc-rounded-rect.h"
#include <libgnomecanvas/libgnomecanvas.h>
#include <libgnome/gnome-desktop-item.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <atk/atk.h>

#define GNOMECC_CANVAS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOMECC_TYPE_CANVAS, GnomeccCanvasPrivate))

#define PAD 5 /*when scrolling keep a few pixels above or below if possible */
#define ABOVE_LINE_SPACING 4
#define UNDER_LINE_SPACING 0
#define UNDER_TITLE_SPACING 0 /* manually insert 1 blank line of text */
#define LINE_HEIGHT 1
#define BORDERS 7
#define MAX_ITEM_WIDTH	120
#define ITEMS_SEPARATION 12

typedef struct _GnomeccCanvasPrivate GnomeccCanvasPrivate;

struct _GnomeccCanvasPrivate {
	GnomeCanvasItem *under_cover;
	double height;
	double width;

	double min_height;
	double max_width;
	ControlCenterInformation *info;
	ControlCenterEntry *selected;

	gboolean rtl;
	gint items_per_row;

	/* calculated sizes
	   for the elements */
	gdouble max_item_width;
	gdouble max_item_height;
	gdouble max_icon_height;

	/* accessibility stuff */
	GHashTable *accessible_children;
};

typedef struct {
	GnomeccCanvas *canvas;

	GnomeCanvasGroup *group;
	GnomeCanvasItem *text;
	GnomeCanvasItem *pixbuf;
	GnomeCanvasItem *highlight_pixbuf;
	GnomeCanvasItem *cover;
	GnomeCanvasItem *selection;
	double icon_height;
	double icon_width;
	double text_height;
	guint launching : 1;
	guint selected : 1;
	guint highlighted : 1;

	gint n_category;
	gint n_entry;
	gint index;
} EntryInfo;

typedef struct {
	GnomeCanvasGroup *group;
	GnomeCanvasItem *title;
	GnomeCanvasItem *line;
} CategoryInfo;

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_INFO
};

static guint gnomecc_canvas_signals [LAST_SIGNAL] = { 0 };


static void gnomecc_canvas_class_init (GnomeccCanvasClass *class);
static void gnomecc_canvas_init       (GnomeccCanvas      *canvas);
static void gnomecc_canvas_finalize   (GObject            *object);

static void gnomecc_canvas_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec);
static void gnomecc_canvas_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec);

static void gnomecc_canvas_draw_background (GnomeCanvas *canvas, GdkDrawable *drawable,
					    int x, int y, int width, int height);

static void gnomecc_canvas_size_allocate   (GtkWidget     *canvas,
					    GtkAllocation *allocation);
static void gnomecc_canvas_style_set       (GtkWidget     *canvas,
					    GtkStyle      *previous_style);
static void gnomecc_canvas_realize         (GtkWidget     *canvas);

static AtkObject* gnomecc_canvas_get_accessible (GtkWidget *widget);



G_DEFINE_TYPE (GnomeccCanvas, gnomecc_canvas, GNOME_TYPE_CANVAS);

static void
gnomecc_canvas_class_init (GnomeccCanvasClass *class)
{
	GObjectClass     *object_class = G_OBJECT_CLASS (class);
	GnomeCanvasClass *canvas_class = GNOME_CANVAS_CLASS (class);
	GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (class);

	object_class->set_property = gnomecc_canvas_set_property;
	object_class->get_property = gnomecc_canvas_get_property;
	object_class->finalize = gnomecc_canvas_finalize;
	canvas_class->draw_background = gnomecc_canvas_draw_background;

	widget_class->style_set = gnomecc_canvas_style_set;
	widget_class->size_allocate = gnomecc_canvas_size_allocate;
	widget_class->realize = gnomecc_canvas_realize;
	widget_class->get_accessible = gnomecc_canvas_get_accessible;

	class->changed = NULL;

	g_object_class_install_property (object_class,
					 PROP_INFO,
					 g_param_spec_pointer ("info",
							       "information for the canvas",
							       "information for the canvas",
							       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	gnomecc_canvas_signals [SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GnomeccCanvasClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class,
				  sizeof (GnomeccCanvasPrivate));
}

static void
gnomecc_canvas_init (GnomeccCanvas *canvas)
{
	GnomeccCanvasPrivate *priv;
	
	g_return_if_fail (GNOMECC_IS_CANVAS (canvas));

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	priv->max_width = 300;
	priv->min_height = 0;
	priv->info = NULL;
	priv->selected = NULL;

	priv->max_item_width = 0;
	priv->max_item_height = 0;
	priv->items_per_row = 0;
	priv->rtl = (gtk_widget_get_direction (GTK_WIDGET (canvas)) == GTK_TEXT_DIR_RTL);

	priv->accessible_children = g_hash_table_new (g_int_hash, g_int_equal);

	gtk_widget_show_all (GTK_WIDGET (canvas));
}

static gboolean
single_click_activates (void)
{
	static gboolean needs_init = TRUE;
	static gboolean use_single_click = FALSE;
	if (needs_init)  {
		GConfClient *client = gconf_client_get_default ();
		char *policy = gconf_client_get_string (client, "/apps/nautilus/preferences/click_policy", NULL);
		g_object_unref (G_OBJECT (client));

		if (policy != NULL) {
			use_single_click = (0 == g_ascii_strcasecmp (policy, "single"));
			g_free (policy);
		}
		needs_init = FALSE;
	}

	return	use_single_click;
}

static void
gnome_canvas_item_show_hide (GnomeCanvasItem *item, gboolean show)
{
	if (show)
		gnome_canvas_item_show (item);
	else
		gnome_canvas_item_hide (item);
}

static void
setup_entry (GnomeccCanvas *canvas, ControlCenterEntry *entry)
{
	GnomeccCanvasPrivate *priv;
	EntryInfo *ei;
	GtkWidget *widget;
	GtkStateType state;

	if (!entry)
		return;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	widget = GTK_WIDGET (canvas);
	ei = entry->user_data;

	if (ei->pixbuf) {
		gnome_canvas_item_show_hide (ei->highlight_pixbuf, ei->highlighted);
		gnome_canvas_item_show_hide (ei->pixbuf, !ei->highlighted);
	}
	if (!ei->selected)
		state = GTK_STATE_NORMAL;
	else if (gtk_window_has_toplevel_focus (GTK_WINDOW (gtk_widget_get_toplevel (widget))))
		state = GTK_STATE_SELECTED;
	else
		state = GTK_STATE_ACTIVE;
	gnome_canvas_item_show_hide (ei->selection, ei->selected);
	g_object_set (ei->selection,
		      "fill_color_gdk", &widget->style->base [state],
		      NULL);
	g_object_set (ei->text,
		      "fill_color_gdk", &widget->style->text [state],
		      NULL);
}

static gboolean
cb_entry_info_reset (gpointer data)
{
	EntryInfo *ei = data;
	ei->launching = FALSE;
	return FALSE;
}

static void
activate_entry (ControlCenterEntry *entry)
{
	EntryInfo *ei = entry->user_data;

	if (!ei->launching) {
		GnomeDesktopItem *desktop_item;

		ei->launching = TRUE;
		gtk_timeout_add (1000, cb_entry_info_reset, ei);

		desktop_item = gnome_desktop_item_new_from_file (entry->desktop_entry,
								 GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
								 NULL);
		if (desktop_item != NULL) {
			gnome_desktop_item_launch (desktop_item, NULL, 0, NULL);
			gnome_desktop_item_unref (desktop_item);
		}
	}
}

static void
select_entry (GnomeccCanvas *canvas, ControlCenterEntry *entry)
{
	GnomeccCanvasPrivate *priv;
	EntryInfo *ei = NULL;
	GtkAdjustment *pos;
	double affine[6];
	ControlCenterEntry *selected;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	selected = priv->selected;

	if (selected == entry)
		return;

	if (selected && selected->user_data)
		((EntryInfo *) selected->user_data)->selected = FALSE;
	setup_entry (canvas, selected);

	priv->selected = selected = entry;

	if (selected && selected->user_data)
		((EntryInfo *) selected->user_data)->selected = TRUE;
	setup_entry (canvas, selected);

	g_signal_emit (canvas, gnomecc_canvas_signals [SELECTION_CHANGED], 0,
		       (entry) ? entry->comment : NULL);

	if (!entry)
		return;

	ei = entry->user_data;
	gnome_canvas_item_i2c_affine (GNOME_CANVAS_ITEM (ei->group), affine);
	pos = gtk_layout_get_vadjustment (GTK_LAYOUT (ei->cover->canvas));

	if (affine[5] < pos->value)
		gtk_adjustment_set_value (pos, MAX (affine[5] - PAD, 0));
	else if ((affine[5] + priv->max_item_height) > (pos->value+pos->page_size))
		gtk_adjustment_set_value (pos, MAX (MIN (affine[5] + priv->max_item_height + PAD, pos->upper) - pos->page_size, 0));
}

static gboolean
cover_event (GnomeCanvasItem *item, GdkEvent *event, ControlCenterEntry *entry)
{
	EntryInfo *ei = entry->user_data;
	GnomeccCanvas *canvas = ei->canvas;

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		ei->highlighted = TRUE;
		setup_entry (canvas, entry); /* highlight even if it is already selected */
		return TRUE;
	case GDK_LEAVE_NOTIFY:
		ei->highlighted = FALSE;
		setup_entry (canvas, entry);
		return TRUE;
	case GDK_BUTTON_PRESS:
		if (single_click_activates ()) {
			activate_entry (entry);
		} else {
			select_entry (canvas, entry);
		}
		return TRUE;
	case GDK_2BUTTON_PRESS:
		activate_entry (entry);
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
cb_canvas_event (GnomeCanvasItem *item, GdkEvent *event, GnomeccCanvas *canvas)
{
	GnomeccCanvasPrivate *priv;
	EntryInfo *ei = NULL;
	gint n_category, n_entry;
	gint n_categories, n_entries;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	
	if (event->type == GDK_BUTTON_PRESS) {
		select_entry (canvas, NULL);
		return TRUE;
	}

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	if (priv->selected)
		ei = priv->selected->user_data;

	n_entry = 0;
	n_category = 0;
	n_categories = priv->info->n_categories;
	n_entries = priv->info->categories[ei->n_category]->n_entries;

	switch (event->key.keyval) {
	case GDK_KP_Right:
	case GDK_Right:
		if (ei) {
			n_entry = (priv->rtl) ? ei->n_entry - 1 : ei->n_entry + 1;
			n_category = ei->n_category;
		}
		break;
	case GDK_KP_Left:
	case GDK_Left:
		if (ei) {
			n_entry = (priv->rtl) ? ei->n_entry + 1 : ei->n_entry - 1;
			n_category = ei->n_category;
		}
		break;
	case GDK_KP_Down:
	case GDK_Down:
		if (ei) {
			n_category = ei->n_category;
			n_entry    = ei->n_entry;

			if (ei->n_entry + priv->items_per_row <
			    priv->info->categories[n_category]->n_entries)
				n_entry += priv->items_per_row;
		}
		break;
	case GDK_KP_Up:
	case GDK_Up:
		if (ei) {
			n_category = ei->n_category;
			n_entry    = ei->n_entry;

			if (ei->n_entry - priv->items_per_row >= 0)
				n_entry -= priv->items_per_row;
		}
		break;
	case GDK_Tab:
	case GDK_KP_Tab:
		if (ei) {
			n_entry = 0;
			n_category = ei->n_category + 1;

			if (n_category > priv->info->n_categories - 1)
				n_category = 0;
		}
		break;
	case GDK_ISO_Left_Tab:
		if (ei) {
			n_entry = 0;
			n_category = ei->n_category - 1;

			if (n_category < 0)
				n_category = priv->info->n_categories;
		}
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if (priv->selected)
			activate_entry (priv->selected);

		return TRUE;
		break;
	case GDK_Escape:
		gtk_main_quit ();
		return TRUE;
	case 'q':
	case 'Q':
		if (event->key.state == GDK_CONTROL_MASK) {
			gtk_main_quit ();
		}
		return TRUE;
	default:
		return FALSE;
	}

	n_category = CLAMP (n_category, 0, priv->info->n_categories - 1);
	n_entry = CLAMP (n_entry, 0, priv->info->categories[n_category]->n_entries - 1);

	select_entry (canvas, priv->info->categories[n_category]->entries[n_entry]);
	return TRUE;
}

static void
calculate_item_width (GnomeccCanvas *canvas, EntryInfo *ei)
{
	GnomeccCanvasPrivate *priv;
	PangoLayout *layout;
	PangoRectangle rectangle;

	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	layout = GNOME_CANVAS_TEXT (ei->text)->layout;

	pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_width (layout, -1);
	pango_layout_get_pixel_extents (layout, NULL, &rectangle);

	/* If its too big wrap at the max and regen to find the layout */
	if (rectangle.width > MAX_ITEM_WIDTH) {
		pango_layout_set_width (layout, MAX_ITEM_WIDTH * PANGO_SCALE);
		pango_layout_get_pixel_extents (layout, NULL, &rectangle);
		rectangle.width = MAX_ITEM_WIDTH;
	}

	ei->text_height = rectangle.height;

	priv->max_item_width = MAX (priv->max_item_width, rectangle.width);
}

static void
calculate_item_height (GnomeccCanvas *canvas, EntryInfo *ei)
{
	GnomeccCanvasPrivate *priv;
	gint item_height;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	if (ei->pixbuf)
		priv->max_icon_height = MAX (priv->max_icon_height, ei->icon_height);

	item_height = ei->icon_height + ei->text_height;
	priv->max_item_height = MAX (priv->max_item_height, item_height);
}

static void
calculate_sizes (GnomeccCanvas *canvas)
{
	GnomeccCanvasPrivate *priv;
	int i, j;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	priv->max_item_height = 0;
	priv->max_icon_height = 0;
	priv->max_item_width  = 0;

	for (i = 0; i < priv->info->n_categories; i++) {
		for (j = 0; j < priv->info->categories[i]->n_entries; j++) {
			EntryInfo *ei = priv->info->categories[i]->entries[j]->user_data;

			calculate_item_width  (canvas, ei);
			calculate_item_height (canvas, ei);
		}
	}
}

static void
gnome_canvas_item_move_absolute (GnomeCanvasItem *item, double dx, double dy)
{
	double translate[6];

	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	art_affine_translate (translate, dx, dy);

	gnome_canvas_item_affine_absolute (item, translate);
}

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value;
	new_value += 24 + (new_value >> 3);
	if (new_value > 255) {
		new_value = 255;
	}
	return (guchar) new_value;
}

static GdkPixbuf *
create_spotlight_pixbuf (GdkPixbuf* src)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = gdk_pixbuf_copy (src);

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}

	return dest;
}

static void
build_canvas (GnomeccCanvas *canvas)
{
	GnomeccCanvasPrivate *priv;
	GnomeCanvas *gcanvas;
	int i, j, index;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	gcanvas = GNOME_CANVAS (canvas);
	index = 0;

	priv->under_cover = gnome_canvas_item_new (gnome_canvas_root (gcanvas),
						   gnomecc_event_box_get_type(),
						   NULL);

	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (gnome_canvas_root (gcanvas)));
	g_signal_connect (gnome_canvas_root (gcanvas), "event",
			  G_CALLBACK (cb_canvas_event), canvas);

	for (i = 0; i < priv->info->n_categories; i++) {
		CategoryInfo *catinfo;

		if (priv->info->categories[i]->user_data == NULL)
			priv->info->categories[i]->user_data = g_new (CategoryInfo, 1);

		catinfo = priv->info->categories[i]->user_data;
		catinfo->group = NULL;
		catinfo->title = NULL;
		catinfo->line = NULL;

		catinfo->group =
			GNOME_CANVAS_GROUP (gnome_canvas_item_new (gnome_canvas_root (gcanvas),
								   gnome_canvas_group_get_type (),
								   NULL));
		gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (catinfo->group), 0, BORDERS);

		if (i > 0) {
			catinfo->line = gnome_canvas_item_new (catinfo->group,
				gnome_canvas_rect_get_type (),
				"x2", (double) priv->max_width - 2 * BORDERS,
				"y2", (double) LINE_HEIGHT,
				NULL);
		}

		catinfo->title = NULL;
		if (priv->info->categories[i] && (priv->info->n_categories != 1 || priv->info->categories[0]->real_category)) {
			char *label = g_strdup_printf ("<span weight=\"bold\">%s</span>", priv->info->categories[i]->title);
			catinfo->title = gnome_canvas_item_new (catinfo->group,
				gnome_canvas_text_get_type (),
				"text", priv->info->categories[i]->title,
				"markup", label,
				"anchor", GTK_ANCHOR_NW,
				NULL);
			g_free (label);
		}

		for (j = 0; j < priv->info->categories[i]->n_entries; j++) {
			EntryInfo *ei;

			if (priv->info->categories[i]->entries[j]->user_data == NULL)
				priv->info->categories[i]->entries[j]->user_data = g_new0 (EntryInfo, 1);

			ei = priv->info->categories[i]->entries[j]->user_data;

			ei->canvas = canvas;
			ei->group  = GNOME_CANVAS_GROUP (
				gnome_canvas_item_new (catinfo->group,
				gnome_canvas_group_get_type (),
				NULL));
			ei->selection = gnome_canvas_item_new (
				ei->group,
				GNOMECC_TYPE_ROUNDED_RECT,
				NULL);

			if (priv->info->categories[i]->entries[j]->title) {
				ei->text = gnome_canvas_item_new (ei->group,
					gnome_canvas_text_get_type (),
					"anchor", GTK_ANCHOR_NW,
					"justification", GTK_JUSTIFY_CENTER,
					"clip",	  TRUE,
					NULL);
				pango_layout_set_alignment (GNOME_CANVAS_TEXT (ei->text)->layout,
							    PANGO_ALIGN_CENTER);
				pango_layout_set_justify (GNOME_CANVAS_TEXT (ei->text)->layout,
							  FALSE);
				g_object_set (ei->text,
					      "text", priv->info->categories[i]->entries[j]->title,
					      NULL);
			} else
				ei->text = NULL;

			if (priv->info->categories[i]->entries[j]->icon_pixbuf) {
				GdkPixbuf *pixbuf = priv->info->categories[i]->entries[j]->icon_pixbuf;
				GdkPixbuf *highlight_pixbuf =
					create_spotlight_pixbuf (pixbuf);
				ei->icon_height = gdk_pixbuf_get_height (pixbuf);
				ei->icon_width  = gdk_pixbuf_get_width (pixbuf);
				ei->pixbuf = gnome_canvas_item_new (ei->group,
					gnome_canvas_pixbuf_get_type (),
					"pixbuf", pixbuf,
					NULL);
				g_object_unref (pixbuf);
				ei->highlight_pixbuf = gnome_canvas_item_new (ei->group,
					gnome_canvas_pixbuf_get_type (),
					"pixbuf", highlight_pixbuf,
					NULL);
				g_object_unref (highlight_pixbuf);
			} else {
				ei->pixbuf = NULL;
				ei->highlight_pixbuf = NULL;
			}

			ei->cover = gnome_canvas_item_new (ei->group,
							   gnomecc_event_box_get_type(),
							   NULL);
			calculate_item_width  (canvas, ei);
			calculate_item_height (canvas, ei);

			ei->n_category = i;
			ei->n_entry = j;
			ei->index = index;

			g_signal_connect (ei->cover, "event",
				G_CALLBACK (cover_event),
				priv->info->categories[i]->entries[j]);

			setup_entry (canvas, priv->info->categories[i]->entries[j]);
			index++;
		}
	}
}

static void
gnomecc_canvas_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	GnomeccCanvasPrivate *priv;

	priv = GNOMECC_CANVAS_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_INFO:
		priv->info = g_value_get_pointer (value);
		build_canvas (GNOMECC_CANVAS (object));
		break;
	}
}

static void
gnomecc_canvas_get_property (GObject      *object,
			     guint         prop_id,
			     GValue       *value,
			     GParamSpec   *pspec)
{
	GnomeccCanvasPrivate *priv;

	priv = GNOMECC_CANVAS_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_INFO:
		g_value_set_pointer (value, priv->info);
		break;
	}
}

static void
gnomecc_canvas_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (gnomecc_canvas_parent_class)->finalize)
		(* G_OBJECT_CLASS (gnomecc_canvas_parent_class)->finalize) (object);
}

static void
gnomecc_canvas_draw_background (GnomeCanvas *canvas, GdkDrawable *drawable,
				int x, int y, int width, int height)
{
	/* By default, we use the style base color. */
	gdk_gc_set_foreground (canvas->pixmap_gc,
			       &GTK_WIDGET (canvas)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle (drawable,
			    canvas->pixmap_gc,
			    TRUE,
			    0, 0,
			    width, height);
}

/* A category canvas item contains all the elements
 * for the category, as well as the title and a separator
 */
static void
relayout_category (GnomeccCanvas *canvas, CategoryInfo *catinfo,
		   gint vert_pos, gint *category_vert_pos)
{
	GnomeccCanvasPrivate *priv;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (catinfo->group),
					 0, vert_pos);

	if (catinfo->line) {
		gnome_canvas_item_move_absolute (catinfo->line, BORDERS, ABOVE_LINE_SPACING);

		gnome_canvas_item_set (catinfo->line,
				       "x2", (double) priv->max_width - 2 * BORDERS,
				       "y2", (double) LINE_HEIGHT,
				       NULL);
	}

	if (catinfo->title) {
		double text_height, text_width;

		g_object_get (catinfo->title,
			      "text_height", &text_height,
			      "text_width",  &text_width,
			      NULL);

		*category_vert_pos += text_height; /* move it down 1 line */
		gnome_canvas_item_move_absolute (catinfo->title,
						 (priv->rtl) ? priv->max_width - BORDERS - text_width : BORDERS,
						 *category_vert_pos);
		*category_vert_pos += text_height + text_height/2 + UNDER_TITLE_SPACING;
	}
}

static void
relayout_item (GnomeccCanvas *canvas, EntryInfo *ei,
	       gint category_horiz_pos, gint category_vert_pos)
{
	GnomeccCanvasPrivate *priv;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (ei->group),
					 category_horiz_pos,
					 category_vert_pos);

	gnome_canvas_item_set (ei->selection,
			       "x2", (double) priv->max_item_width + 2 * PAD,
			       "y2", (double) ei->text_height + 1, /* expand it down slightly */
			       NULL);

	gnome_canvas_item_move_absolute (ei->selection, -PAD, priv->max_icon_height);

	if (ei->text) {
		/* canvas asks layout for its extent, layout gives real
		 * size, not fixed width and drawing gets confused.
		 */
		gnome_canvas_item_set (ei->text,
				       "clip_width",  (double) priv->max_item_width,
				       "clip_height", (double) priv->max_item_height,
				       NULL);

		/* text is centered by pango */
		pango_layout_set_width (GNOME_CANVAS_TEXT (ei->text)->layout,
					(gint) priv->max_item_width * PANGO_SCALE);

		gnome_canvas_item_move_absolute (ei->text,
						 0, priv->max_icon_height);
	}

	if (ei->pixbuf) {
		/* manually center the icon */
		gnome_canvas_item_move_absolute (ei->pixbuf,
						 priv->max_item_width / 2 - ei->icon_width / 2, 0);
		gnome_canvas_item_move_absolute (ei->highlight_pixbuf,
						 priv->max_item_width / 2 - ei->icon_width / 2, 0);
	}

	/* cover the item */
	gnome_canvas_item_set (ei->cover,
			       "x2", (double) priv->max_item_width,
			       "y2", (double) priv->max_item_height,
			       NULL);
}

static void
relayout_canvas (GnomeccCanvas *canvas)
{
	gint cat, cat_count, entry, cat_entries;
	gint vert_pos, category_vert_pos, category_horiz_pos;
	gint real_width;
	GnomeccCanvasPrivate *priv;
	ControlCenterCategory *category;
	CategoryInfo *catinfo;
	EntryInfo *ei;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	real_width = priv->max_width - 2 * BORDERS + ITEMS_SEPARATION;
	priv->items_per_row = real_width / ((gint) priv->max_item_width + ITEMS_SEPARATION);

	cat_count = priv->info->n_categories;
	vert_pos = BORDERS;

	for (cat = 0; cat < cat_count; cat++) {
		category_vert_pos = 0;

		category_horiz_pos = (priv->rtl) ? priv->max_width - (gint) priv->max_item_width - BORDERS : BORDERS;

		category = priv->info->categories [cat];
		catinfo  = category->user_data;

		cat_entries = category->n_entries;

		relayout_category (canvas, catinfo, vert_pos, &category_vert_pos);
		category_vert_pos += UNDER_LINE_SPACING;

		for (entry = 0; entry < cat_entries; entry++) {
			/* we don't want the first item to wrap, it would
			   be too separated from the section title */
			if ((entry > 0) &&
			    ((priv->items_per_row == 0) ||
			     (priv->items_per_row > 0 && (entry % priv->items_per_row == 0)))) {

				category_horiz_pos = (priv->rtl) ?
					priv->max_width - (gint) priv->max_item_width - BORDERS : BORDERS;
				category_vert_pos  += (gint) priv->max_item_height + ITEMS_SEPARATION;
			}

			ei = category->entries[entry]->user_data;

			relayout_item (canvas, ei, category_horiz_pos, category_vert_pos);

			if (priv->rtl)
				category_horiz_pos -= (gint) priv->max_item_width + ITEMS_SEPARATION;
			else
				category_horiz_pos += (gint) priv->max_item_width + ITEMS_SEPARATION;
		}

		category_vert_pos += (gint) priv->max_item_height;
		vert_pos += category_vert_pos + ITEMS_SEPARATION;
	}

	/* substract the last ITEMS_SEPARATION to
	   adjust the canvas size a bit more */
	vert_pos -= ITEMS_SEPARATION;

	priv->height = MAX (vert_pos, priv->min_height);
	priv->width  = priv->max_width;
}

static void
gnomecc_canvas_size_allocate(GtkWidget *canvas, GtkAllocation *allocation)
{
	GnomeccCanvasPrivate *priv;

	if (GTK_WIDGET_CLASS (gnomecc_canvas_parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (gnomecc_canvas_parent_class)->size_allocate) (canvas, allocation);

	if (allocation->height == 1 || allocation->width == 1)
		return;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	priv->max_width = allocation->width;
	priv->min_height = allocation->height;

	relayout_canvas (GNOMECC_CANVAS (canvas));

	gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0, 0, priv->width - 1, priv->height - 1);
	g_object_set (priv->under_cover,
		      "x2", priv->width,
		      "y2", priv->height,
		      NULL);
}

static void
set_style (GnomeccCanvas *canvas, gboolean font_changed)
{
	int i, j;
	GnomeccCanvasPrivate *priv;
	GtkWidget *widget = GTK_WIDGET (canvas);

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	priv = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	for (i = 0; i < priv->info->n_categories; i++) {
		CategoryInfo *catinfo = priv->info->categories[i]->user_data;

		if (catinfo->line) {
			g_object_set (catinfo->line,
				      "fill_color_gdk", &widget->style->text_aa[GTK_STATE_NORMAL],
				      NULL);
		}
		if (catinfo->title) {
			g_object_set (catinfo->title,
				      "fill_color_gdk", &widget->style->text[GTK_STATE_NORMAL],
				      NULL);

			if (font_changed)
				g_object_set (catinfo->title,
					      "font", NULL,
					      NULL);
		}

		for (j = 0; j < priv->info->categories[i]->n_entries; j++) {
			ControlCenterEntry *entry = priv->info->categories[i]->entries[j];
			EntryInfo *entryinfo = entry->user_data;
			if (font_changed && entryinfo->text)
				g_object_set (entryinfo->text,
					      "font", NULL,
					      NULL);
			setup_entry (canvas, entry);
		}
	}

	if (font_changed) {
		calculate_sizes (canvas);
		relayout_canvas (canvas);
	}
}

static void
gnomecc_canvas_style_set (GtkWidget *canvas, GtkStyle *previous_style)
{
	if (!GTK_WIDGET_REALIZED (canvas))
		return;

	set_style (GNOMECC_CANVAS (canvas), (previous_style &&
					     canvas->style &&
					     !pango_font_description_equal (canvas->style->font_desc,
									    previous_style->font_desc)));
}

static void
gnomecc_canvas_realize (GtkWidget *canvas)
{
	if (GTK_WIDGET_CLASS (gnomecc_canvas_parent_class)->realize)
		(* GTK_WIDGET_CLASS (gnomecc_canvas_parent_class)->realize) (canvas);

	set_style (GNOMECC_CANVAS (canvas), FALSE);
}

GtkWidget*
gnomecc_canvas_new (ControlCenterInformation *info)
{
	return g_object_new (GNOMECC_TYPE_CANVAS,
			     "info", info,
			     NULL);
}

/* Accessibility support */
static gpointer accessible_parent_class;
static gpointer accessible_item_parent_class;

enum {
	ACTION_ACTIVATE,
	LAST_ACTION
};

typedef struct {
	AtkObject parent;

	ControlCenterEntry *entry;
	AtkStateSet *state_set;

	guint action_idle_handler;
} GnomeccCanvasItemAccessible;

typedef struct {
	AtkObjectClass parent_class;
} GnomeccCanvasItemAccessibleClass;

static const gchar *const action_names[] = 
{
	"activate",
	NULL
};

static const gchar *const action_descriptions[] =
{
	"Activate item",
	NULL
};

static void
gnomecc_canvas_item_accessible_get_extents (AtkComponent *component,
					    gint         *x,
					    gint         *y,
					    gint         *width,
					    gint         *height,
					    AtkCoordType  coord_type)
{
	GnomeccCanvasItemAccessible *item;
	GnomeccCanvas *canvas;
	GnomeCanvasItem *cover;
	AtkObject *parent_object;
	gint p_x, p_y;

	item = (GnomeccCanvasItemAccessible *) component;

	canvas = ((EntryInfo *) item->entry->user_data)->canvas;
	parent_object = gtk_widget_get_accessible (GTK_WIDGET (canvas));
	atk_component_get_position (ATK_COMPONENT (parent_object), &p_x, &p_y, coord_type);

	/* the cover pretty much represents the item size */
	cover = GNOME_CANVAS_ITEM (((EntryInfo *)item->entry->user_data)->cover);

	*x = p_x + cover->x1;
	*y = p_y + cover->y1;
	*width  = cover->x2 - cover->x1;
	*height = cover->y2 - cover->y1;
}

static void
atk_component_item_interface_init (AtkComponentIface *iface)
{
	iface->get_extents = gnomecc_canvas_item_accessible_get_extents;
}

static gboolean
idle_do_action (gpointer data)
{
	GnomeccCanvasItemAccessible *item;

	item = (GnomeccCanvasItemAccessible *) data;

	item->action_idle_handler = 0;
	activate_entry (item->entry);

	return FALSE;
}

static gboolean
gnomecc_canvas_item_accessible_action_do_action (AtkAction *action,
						 gint       index)
{
	GnomeccCanvasItemAccessible *item;

	if (index < 0 || index >= LAST_ACTION) 
		return FALSE;

	item = (GnomeccCanvasItemAccessible *) action;

	switch (index) {
	case ACTION_ACTIVATE:
		if (!item->action_idle_handler)
			item->action_idle_handler = g_idle_add (idle_do_action, item);
		break;
	default:
		g_assert_not_reached ();
		return FALSE;
	}        

	return TRUE;
}

static gint
gnomecc_canvas_item_accessible_action_get_n_actions (AtkAction *action)
{
        return LAST_ACTION;
}

static const gchar *
gnomecc_canvas_item_accessible_action_get_description (AtkAction *action,
                                                      gint       index)
{
	if (index < 0 || index >= LAST_ACTION) 
		return NULL;

	return action_descriptions[index];
}

static const gchar *
gnomecc_canvas_item_accessible_action_get_name (AtkAction *action,
                                               gint       index)
{
	if (index < 0 || index >= LAST_ACTION) 
		return NULL;

	return action_names[index];
}

static void
atk_action_item_interface_init (AtkActionIface *iface)
{
	iface->do_action = gnomecc_canvas_item_accessible_action_do_action;
	iface->get_n_actions = gnomecc_canvas_item_accessible_action_get_n_actions;
	iface->get_description = gnomecc_canvas_item_accessible_action_get_description;
	iface->get_name = gnomecc_canvas_item_accessible_action_get_name;
}

static gint
gnomecc_canvas_item_accessible_get_index_in_parent (AtkObject *object)
{
	GnomeccCanvasItemAccessible *item;

	item = (GnomeccCanvasItemAccessible *) object;

	return ((EntryInfo *) item->entry->user_data)->index;
}

static G_CONST_RETURN gchar*
gnomecc_canvas_item_accessible_get_name (AtkObject *object)
{
	GnomeccCanvasItemAccessible *item;

	if (object->name)
		return object->name;
	else {
		item = (GnomeccCanvasItemAccessible *) object;

		if (item->entry && item->entry->title)
			return item->entry->title;
		else
			return "Item with no description";
	}
}

static AtkObject*
gnomecc_canvas_item_accessible_get_parent (AtkObject *object)
{
	GnomeccCanvasItemAccessible *item;
	GnomeccCanvas *canvas;

	item = (GnomeccCanvasItemAccessible *) object;
	canvas = ((EntryInfo *) item->entry->user_data)->canvas;

	return gtk_widget_get_accessible (GTK_WIDGET (canvas));
}

static AtkStateSet*
gnomecc_canvas_item_accessible_ref_state_set (AtkObject *object)
{
	GnomeccCanvasItemAccessible *item;
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;

	item   = (GnomeccCanvasItemAccessible *) object;
	canvas = ((EntryInfo *) item->entry->user_data)->canvas;
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	if (item->entry == priv->selected)
		atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSED);
	else
		atk_state_set_remove_state (item->state_set, ATK_STATE_FOCUSED);

	return g_object_ref (item->state_set);
}

static void
gnomecc_canvas_item_accessible_class_init (AtkObjectClass *class)
{
	GObjectClass *object_class;

	accessible_item_parent_class = g_type_class_peek_parent (class);

	object_class = (GObjectClass *)class;

	class->get_index_in_parent = gnomecc_canvas_item_accessible_get_index_in_parent; 
	class->get_name = gnomecc_canvas_item_accessible_get_name;
	class->get_parent = gnomecc_canvas_item_accessible_get_parent; 
	class->ref_state_set = gnomecc_canvas_item_accessible_ref_state_set;
}

static void
gnomecc_canvas_item_accessible_object_init (GnomeccCanvasItemAccessible *item)
{
	item->state_set = atk_state_set_new ();

	atk_state_set_add_state (item->state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSABLE);
	atk_state_set_add_state (item->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (item->state_set, ATK_STATE_SELECTABLE);
	atk_state_set_add_state (item->state_set, ATK_STATE_VISIBLE);

	item->action_idle_handler = 0;
}

static GType
gnomecc_canvas_item_accessible_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GnomeccCanvasItemAccessibleClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) gnomecc_canvas_item_accessible_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (GnomeccCanvasItemAccessible), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) gnomecc_canvas_item_accessible_object_init, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_item_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_item_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (ATK_TYPE_OBJECT,
					       "GnomeccCanvasItemAccessible", &info, 0);

		g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
					     &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
					     &atk_action_info);
	}

	return type;
}

static gint
gnomecc_canvas_accessible_get_n_children (AtkObject *accessible)
{
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;
	gint i, count;

	widget = GTK_ACCESSIBLE (accessible)->widget;

	if (!widget)
		return 0;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	count  = 0;

	for (i = 0; i < priv->info->n_categories; i++)
		count += priv->info->categories[i]->n_entries;

	return count;
}

static ControlCenterEntry*
gnomecc_canvas_accessible_get_entry (GnomeccCanvas *canvas, gint index)
{
	GnomeccCanvasPrivate *priv;
	gint i, count;

	priv  = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	count = 0;

	for (i = 0; i < priv->info->n_categories; i++) {
		if (index >= count &&
		    index < count + priv->info->categories[i]->n_entries) {
			return priv->info->categories[i]->entries[index - count];
		} else
			count += priv->info->categories [i]->n_entries;
	}

	return NULL;
}

static AtkObject*
gnomecc_canvas_accessible_ref_child (AtkObject *accessible, gint index)
{
	GnomeccCanvasItemAccessible *item;
	ControlCenterEntry *entry;
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;
	AtkObject *object;

	widget = GTK_ACCESSIBLE (accessible)->widget;

	if (!widget)
		return NULL;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);
	entry  = gnomecc_canvas_accessible_get_entry (canvas, index);

	if (!entry)
		return NULL;

	object = g_hash_table_lookup (priv->accessible_children, &index);

	if (!object) {
		object = g_object_new (gnomecc_canvas_item_accessible_get_type (), NULL);
		item   = (GnomeccCanvasItemAccessible *) object;

		object->role = ATK_ROLE_ICON;
		item->entry  = entry;

		g_hash_table_insert (priv->accessible_children,
				     &((EntryInfo *) entry->user_data)->index, object);
	}

	return g_object_ref (object);
}

static void
gnomecc_canvas_accessible_initialize (AtkObject *accessible, gpointer data)
{
	if (ATK_OBJECT_CLASS (accessible_parent_class)->initialize)
		ATK_OBJECT_CLASS (accessible_parent_class)->initialize (accessible, data);

	accessible->role = ATK_ROLE_LAYERED_PANE;
}

static void
gnomecc_canvas_accessible_class_init (AtkObjectClass *class)
{
	GObjectClass *object_class;
	GtkAccessibleClass *accessible_class;

	accessible_parent_class = g_type_class_peek_parent (class);

	object_class = (GObjectClass *)class;
	accessible_class = (GtkAccessibleClass *)class;

	class->get_n_children = gnomecc_canvas_accessible_get_n_children;
	class->ref_child = gnomecc_canvas_accessible_ref_child;
	class->initialize = gnomecc_canvas_accessible_initialize;
}

static gboolean
gnomecc_canvas_accessible_add_selection (AtkSelection *selection, gint i)
{
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;
	ControlCenterEntry *entry;

	widget = GTK_ACCESSIBLE (selection)->widget;

	if (!widget)
		return FALSE;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	entry = gnomecc_canvas_accessible_get_entry (canvas, i);
	select_entry (canvas, entry);

	return TRUE;
}

static gboolean
gnomecc_canvas_accessible_clear_selection (AtkSelection *selection)
{
	GnomeccCanvas *canvas;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (selection)->widget;

	if (!widget)
		return FALSE;

	canvas = GNOMECC_CANVAS (widget);
	select_entry (canvas, NULL);

	return TRUE;
}

static AtkObject*
gnomecc_canvas_accessible_ref_selection (AtkSelection *selection, gint i)
{
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (selection)->widget;

	if (!widget)
		return NULL;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	if (priv->selected)
		return atk_object_ref_accessible_child (gtk_widget_get_accessible (widget),
							((EntryInfo *)priv->selected->user_data)->index);
	return NULL;
}

static gint
gnomecc_canvas_accessible_get_selection_count (AtkSelection *selection)
{
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (selection)->widget;

	if (!widget)
		return 0;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	return (priv->selected) ? 1 : 0;
}

static gboolean
gnomecc_canvas_accessible_is_child_selected (AtkSelection *selection, gint i)
{
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (selection)->widget;

	if (!widget)
		return FALSE;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	return (priv->selected == gnomecc_canvas_accessible_get_entry (canvas, i));
}

static gboolean
gnomecc_canvas_accessible_remove_selection (AtkSelection *selection, gint i)
{
	/* There can be only one item selected */
	return gnomecc_canvas_accessible_clear_selection (selection);
}

static gboolean
gnomecc_canvas_accessible_select_all_selection (AtkSelection *selection)
{
	/* This can't happen */
	return FALSE;
}

static void
gnomecc_canvas_accessible_selection_interface_init (AtkSelectionIface *iface)
{
	iface->add_selection = gnomecc_canvas_accessible_add_selection;
	iface->clear_selection = gnomecc_canvas_accessible_clear_selection;
	iface->ref_selection = gnomecc_canvas_accessible_ref_selection;
	iface->get_selection_count = gnomecc_canvas_accessible_get_selection_count;
	iface->is_child_selected = gnomecc_canvas_accessible_is_child_selected;
	iface->remove_selection = gnomecc_canvas_accessible_remove_selection;
	iface->select_all_selection = gnomecc_canvas_accessible_select_all_selection;
}

static AtkObject*
gnomecc_canvas_accessible_ref_accessible_at_point (AtkComponent *component,
						   gint          x,
						   gint          y,
						   AtkCoordType  coord_type)
{
	GtkWidget *widget;
	GnomeccCanvasPrivate *priv;
	GnomeccCanvas *canvas;
	EntryInfo *entry;
	gint x_pos, y_pos, x_w, y_w;
	gint i, j;

	widget = GTK_ACCESSIBLE (component)->widget;

	if (widget == NULL)
		return NULL;

	canvas = GNOMECC_CANVAS (widget);
	priv   = GNOMECC_CANVAS_GET_PRIVATE (canvas);

	atk_component_get_extents (component, &x_pos, &y_pos, NULL, NULL, coord_type);
	x_w = x - x_pos;
	y_w = y - y_pos;

	for (i = 0; i < priv->info->n_categories; i++) {
		for (j = 0; j < priv->info->categories[i]->n_entries; j++) {
			entry = (EntryInfo *) priv->info->categories[i]->entries[j];
			
			if (x_w > entry->cover->x1 &&
			    x_w < entry->cover->x2 &&
			    y_w > entry->cover->y1 &&
			    y_w < entry->cover->y2)
				return gnomecc_canvas_accessible_ref_child (ATK_OBJECT (component), entry->index);
		}
	}

	return NULL;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
	iface->ref_accessible_at_point = gnomecc_canvas_accessible_ref_accessible_at_point;
}

static GType
gnomecc_canvas_accessible_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GType derived_type;
	GTypeQuery query;
	GType derived_atk_type;

	if (type == 0) {
		static GTypeInfo info = {
			0, /* class size */
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) gnomecc_canvas_accessible_class_init,
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			0, /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo  atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_selection_info = {
			(GInterfaceInitFunc) gnomecc_canvas_accessible_selection_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		derived_type = g_type_parent (GNOMECC_TYPE_CANVAS);
		factory = atk_registry_get_factory (atk_get_default_registry (),
						    derived_type);

		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		info.class_size = query.class_size;
		info.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "GnomeccCanvasAccessible",
					       &info, 0);

		g_type_add_interface_static (type,
					     ATK_TYPE_COMPONENT,
					     &atk_component_info);
		g_type_add_interface_static (type,
					     ATK_TYPE_SELECTION,
					     &atk_selection_info);
	}

	return type;
}

static AtkObject*
gnomecc_canvas_accessible_new (GObject *object)
{
	AtkObject *accessible;

	accessible = g_object_new (gnomecc_canvas_accessible_get_type (), NULL);
	atk_object_initialize (accessible, object);

	return accessible;
}

static AtkObject*
gnomecc_canvas_accessible_factory_create_accessible (GObject *object)
{
	return gnomecc_canvas_accessible_new (object);
}

static void
gnomecc_canvas_accessible_factory_class_init (AtkObjectFactoryClass *class)
{
	class->create_accessible   = gnomecc_canvas_accessible_factory_create_accessible;
	class->get_accessible_type = gnomecc_canvas_accessible_get_type;
}

static GType
gnomecc_canvas_accessible_factory_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (AtkObjectFactoryClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gnomecc_canvas_accessible_factory_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (AtkObjectFactory),
			0,             /* n_preallocs */
			NULL, NULL
		};

		type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY,
					       "GnomeccCanvasAccessibleFactory",
					       &info, 0);
	}

	return type;
}

static AtkObject*
gnomecc_canvas_get_accessible (GtkWidget *widget)
{
	static gboolean already_here = FALSE;

	if (!already_here) {
		AtkObjectFactory *factory;
		AtkRegistry *registry;
		GType derived_type;
		GType derived_atk_type;

		already_here = TRUE;

		derived_type = g_type_parent (GNOMECC_TYPE_CANVAS);

		registry = atk_get_default_registry ();
		factory  = atk_registry_get_factory (registry, derived_type);

		derived_atk_type = atk_object_factory_get_accessible_type (factory);

		if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE)) {
			atk_registry_set_factory_type (registry,
						       GNOMECC_TYPE_CANVAS,
						       gnomecc_canvas_accessible_factory_get_type ());
		}
	}

	return (* GTK_WIDGET_CLASS (gnomecc_canvas_parent_class)->get_accessible) (widget);
}
