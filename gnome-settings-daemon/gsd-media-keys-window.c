/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <glade/glade.h>

#include "gsd-media-keys-window.h"

#define DIALOG_TIMEOUT 2000     /* dialog timeout in ms */

static void     gsd_media_keys_window_class_init (GsdMediaKeysWindowClass *klass);
static void     gsd_media_keys_window_init       (GsdMediaKeysWindow      *fade);
static void     gsd_media_keys_window_finalize   (GObject                 *object);

#define GSD_MEDIA_KEYS_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_MEDIA_KEYS_WINDOW, GsdMediaKeysWindowPrivate))

struct GsdMediaKeysWindowPrivate
{
	guint                    is_composited : 1;
	guint                    hide_timeout_id;
	GsdMediaKeysWindowAction action;

	guint                    volume_muted : 1;
	int                      volume_level;

	GladeXML                *xml;
};

G_DEFINE_TYPE (GsdMediaKeysWindow, gsd_media_keys_window, GTK_TYPE_WINDOW)

static gboolean
hide_timeout (GsdMediaKeysWindow *window)
{
	gtk_widget_hide (GTK_WIDGET (window));

	return FALSE;
}

static void
remove_hide_timeout (GsdMediaKeysWindow *window)
{
        if (window->priv->hide_timeout_id != 0) {
                g_source_remove (window->priv->hide_timeout_id);
                window->priv->hide_timeout_id = 0;
        }
}

static void
add_hide_timeout (GsdMediaKeysWindow *window)
{
        window->priv->hide_timeout_id = g_timeout_add (DIALOG_TIMEOUT,
						       (GSourceFunc)hide_timeout,
						       window);
}

static void
update_window (GsdMediaKeysWindow *window)
{
	remove_hide_timeout (window);
	add_hide_timeout (window);

	if (window->priv->is_composited) {
		gtk_widget_queue_draw (GTK_WIDGET (window));
	}
}

static void
volume_controls_set_visible (GsdMediaKeysWindow *window,
			     gboolean            visible)
{
	GtkWidget *progress;

	if (window->priv->xml == NULL) {
		return;
	}

	progress = glade_xml_get_widget (window->priv->xml, "acme_volume_progressbar");
	if (progress == NULL) {
		return;
	}

	if (visible) {
		gtk_widget_show (progress);
	} else {
		gtk_widget_hide (progress);
	}
}

static void
window_set_icon_name (GsdMediaKeysWindow *window,
		      const char         *name)
{
	GtkWidget *image;

	if (window->priv->xml == NULL) {
		return;
	}

	image = glade_xml_get_widget (window->priv->xml, "acme_image");
	if (image == NULL) {
		return;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (image), name, GTK_ICON_SIZE_DIALOG);
}

static void
window_set_icon_file (GsdMediaKeysWindow *window,
		      const char         *path)
{
	GtkWidget *image;

	if (window->priv->xml == NULL) {
		return;
	}

	image = glade_xml_get_widget (window->priv->xml, "acme_image");
	if (image == NULL) {
		return;
	}

	gtk_image_set_from_file (GTK_IMAGE (image), path);
}

static void
action_changed (GsdMediaKeysWindow *window)
{
	if (! window->priv->is_composited) {
		switch (window->priv->action) {
		case GSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME:
			volume_controls_set_visible (window, TRUE);

			if (window->priv->volume_muted) {
				window_set_icon_name (window, "audio-volume-muted");
			} else {
				window_set_icon_name (window, "audio-volume-high");
			}

			break;
		case GSD_MEDIA_KEYS_WINDOW_ACTION_EJECT:
			volume_controls_set_visible (window, FALSE);
			window_set_icon_file (window, PIXMAPSDIR "/acme-eject.png");
			break;
		default:
			break;
		}
	}

	update_window (window);
}

static void
volume_level_changed (GsdMediaKeysWindow *window)
{
	update_window (window);

	if (! window->priv->is_composited) {
		GtkWidget *progress;
		double     fraction;

		if (window->priv->xml != NULL) {
			fraction = (double)window->priv->volume_level / 100.0;

			progress = glade_xml_get_widget (window->priv->xml, "acme_volume_progressbar");
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), fraction);
		}
	}
}

static void
volume_muted_changed (GsdMediaKeysWindow *window)
{
	update_window (window);

	if (! window->priv->is_composited) {
		if (window->priv->volume_muted) {
			window_set_icon_name (window, "audio-volume-muted");
		} else {
			window_set_icon_name (window, "audio-volume-high");
		}
	}
}

void
gsd_media_keys_window_set_action (GsdMediaKeysWindow      *window,
				  GsdMediaKeysWindowAction action)
{
        g_return_if_fail (GSD_IS_MEDIA_KEYS_WINDOW (window));

	if (window->priv->action != action) {
		window->priv->action = action;
		action_changed (window);
	}
}

void
gsd_media_keys_window_set_volume_muted (GsdMediaKeysWindow *window,
					gboolean            muted)
{
        g_return_if_fail (GSD_IS_MEDIA_KEYS_WINDOW (window));

	if (window->priv->volume_muted != muted) {
		window->priv->volume_muted = muted;
		volume_muted_changed (window);
	}
}

void
gsd_media_keys_window_set_volume_level (GsdMediaKeysWindow *window,
					int                 level)
{
        g_return_if_fail (GSD_IS_MEDIA_KEYS_WINDOW (window));

	if (window->priv->volume_level != level) {
		window->priv->volume_level = level;
		volume_level_changed (window);
	}
}

static void
curved_rectangle (cairo_t *cr,
		  double   x0,
		  double   y0,
		  double   width,
		  double   height,
		  double   radius)
{
	double x1;
	double y1;

	x1 = x0 + width;
	y1 = y0 + height;

	if (!width || !height) {
		return;
	}

	if (width / 2 < radius) {
		if (height / 2 < radius) {
			cairo_move_to  (cr, x0, (y0 + y1) / 2);
			cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1) / 2, y0);
			cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
			cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
			cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
		} else {
			cairo_move_to  (cr, x0, y0 + radius);
			cairo_curve_to (cr, x0, y0, x0, y0, (x0 + x1) / 2, y0);
			cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
			cairo_line_to (cr, x1, y1 - radius);
			cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
			cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
		}
	} else {
		if (height / 2 < radius) {
			cairo_move_to  (cr, x0, (y0 + y1) / 2);
			cairo_curve_to (cr, x0, y0, x0 , y0, x0 + radius, y0);
			cairo_line_to (cr, x1 - radius, y0);
			cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
			cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
			cairo_line_to (cr, x0 + radius, y1);
			cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
		} else {
			cairo_move_to  (cr, x0, y0 + radius);
			cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
			cairo_line_to (cr, x1 - radius, y0);
			cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
			cairo_line_to (cr, x1, y1 - radius);
			cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
			cairo_line_to (cr, x0 + radius, y1);
			cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
		}
	}

	cairo_close_path (cr);
}

static void
draw_action_eject (GsdMediaKeysWindow *window,
		   cairo_t            *cr)
{
	int window_width;
	int window_height;
	int width;
	int height;
	int x0;
	int y0;
	int box_height;
	int tri_height;
	int separation;

	gtk_window_get_size (GTK_WINDOW (window), &window_width, &window_height);

	width = window_width * 0.5;
	height = window_height * 0.5;
	x0 = (window_width - width) / 2;
	y0 = (window_height - height) / 2;
	box_height = height * 0.2;
	separation = box_height / 3;
	tri_height = height - box_height - separation;

	/* draw eject symbol */
	cairo_rectangle (cr, x0, y0 + height - box_height, width, box_height);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (cr);

	cairo_move_to (cr, x0, y0 + tri_height);
	cairo_rel_line_to (cr, width, 0);
	cairo_rel_line_to (cr, -width / 2, -tri_height);
	cairo_rel_line_to (cr, -width / 2, tri_height);
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (cr);
}

static void
draw_waves (cairo_t *cr,
	    double   cx,
	    double   cy,
	    double   max_radius)
{
	int    n_waves;
	int    i;

	n_waves = 3;

	for (i = 0; i < n_waves; i++) {
		double angle1;
		double angle2;
		double radius;

		angle1 = -M_PI / 3;
		angle2 = M_PI / 3;

		radius = (i + 1) * (max_radius / n_waves);
		cairo_arc (cr, cx, cy, radius, angle1, angle2);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
		cairo_set_line_width (cr, 10);
		cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
		cairo_stroke (cr);
	}
}

static void
draw_speaker (cairo_t *cr,
	      double   cx,
	      double   cy,
	      double   width,
	      double   height)
{
	double box_width;
	double box_height;

	box_width = width / 3;
	box_height = height / 3;

	cairo_rectangle (cr, cx - box_width / 2, cy - box_height / 2, box_width, box_height);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (cr);

	cairo_move_to (cr, cx, cy);
	cairo_rel_line_to (cr, width / 2, -height / 2);
	cairo_rel_line_to (cr, 0, height);
	cairo_rel_line_to (cr, -width / 2, -height / 2);
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (cr);
}

static void
draw_volume_boxes (cairo_t *cr,
		   double   percentage,
		   double   x0,
		   double   y0,
		   double   width,
		   double   height)
{
	gdouble x1;

	x1 = width * percentage;

	cairo_rectangle (cr, x0, y0, width, height);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 1.0);
	cairo_fill (cr);

	cairo_rectangle (cr, x0, y0, x1, height);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (cr);
}

static void
draw_action_volume (GsdMediaKeysWindow *window,
		    cairo_t            *cr)
{
	int window_width;
	int window_height;
	int width;
	int height;
	double speaker_width;
	double speaker_height;
	double speaker_cx;
	double speaker_cy;
	double wave_x0;
	double wave_y0;
	double wave_radius;
	double box_x0;
	double box_y0;
	double box_width;
	double box_height;

	gtk_window_get_size (GTK_WINDOW (window), &window_width, &window_height);

	width = window_width * 0.5;
	height = window_height * 0.5;

	speaker_width = width * 0.75;
	speaker_height = height * 0.75;
	speaker_cx = window_width / 4;
	speaker_cy = window_height / 3;

	wave_x0 = window_width / 2;
	wave_y0 = speaker_cy;
	wave_radius = width / 2;

	box_x0 = (window_width - width) / 2;
	box_y0 = window_width - (window_width - width) / 2;
	box_width = width;
	box_height = height * 0.1;

	/* draw speaker symbol */
	draw_speaker (cr, speaker_cx, speaker_cy, speaker_width, speaker_height);

	/* draw sound waves */
	if (! window->priv->volume_muted) {
		draw_waves (cr, wave_x0, wave_y0, wave_radius);
	}

	/* draw volume meter */
	draw_volume_boxes (cr,
			   (double)window->priv->volume_level / 100.0,
			   box_x0,
			   box_y0,
			   box_width,
			   box_height);
}

static void
draw_action (GsdMediaKeysWindow *window,
	     cairo_t            *cr)
{
	switch (window->priv->action) {
	case GSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME:
		draw_action_volume (window, cr);
		break;
	case GSD_MEDIA_KEYS_WINDOW_ACTION_EJECT:
		draw_action_eject (window, cr);
		break;
	default:
		break;
	}
}

static gboolean
on_expose_event (GtkWidget          *widget,
		 GdkEventExpose     *event,
		 GsdMediaKeysWindow *window)
{
	cairo_t         *context;
	cairo_t         *cr;
	cairo_surface_t *surface;
	int              width;
	int              height;

	context = gdk_cairo_create (GTK_WIDGET (window)->window);

	cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);
	gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

	surface = cairo_surface_create_similar (cairo_get_target (context),
						CAIRO_CONTENT_COLOR_ALPHA,
						width,
						height);

	if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
		goto done;
	}

	cr = cairo_create (surface);
	if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
		goto done;
	}
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_paint (cr);

	/* draw a box */
	curved_rectangle (cr, 0, 0, width, height, 50);
	cairo_set_source_rgba (cr, 0.2, 0.2, 0.2, 0.5);
	cairo_fill (cr);

	/* draw action */
	draw_action (window, cr);

	cairo_destroy (cr);

	cairo_set_source_surface (context, surface, 0, 0);
	cairo_paint (context);

 done:
	cairo_destroy (context);

	return FALSE;
}

static void
gsd_media_keys_window_real_show (GtkWidget *widget)
{
	GsdMediaKeysWindow *window;

	if (GTK_WIDGET_CLASS (gsd_media_keys_window_parent_class)->show) {
		GTK_WIDGET_CLASS (gsd_media_keys_window_parent_class)->show (widget);
	}

	window = GSD_MEDIA_KEYS_WINDOW (widget);
	remove_hide_timeout (window);
	add_hide_timeout (window);
}

static void
gsd_media_keys_window_real_hide (GtkWidget *widget)
{
	GsdMediaKeysWindow *window;

	if (GTK_WIDGET_CLASS (gsd_media_keys_window_parent_class)->hide) {
		GTK_WIDGET_CLASS (gsd_media_keys_window_parent_class)->hide (widget);
	}

	window = GSD_MEDIA_KEYS_WINDOW (widget);
	remove_hide_timeout (window);
}

static void
gsd_media_keys_window_class_init (GsdMediaKeysWindowClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize	   = gsd_media_keys_window_finalize;

	widget_class->show	   = gsd_media_keys_window_real_show;
	widget_class->hide	   = gsd_media_keys_window_real_hide;

	g_type_class_add_private (klass, sizeof (GsdMediaKeysWindowPrivate));
}

static void
initialize_alpha_mode (GsdMediaKeysWindow *window)
{
	GdkScreen   *screen;
	GdkColormap *colormap;

	screen = gtk_widget_get_screen (GTK_WIDGET (window));
	colormap = gdk_screen_get_rgba_colormap (screen);
	if (colormap != NULL && gdk_screen_is_composited (screen)) {
		gtk_widget_set_colormap (GTK_WIDGET (window), colormap);
		window->priv->is_composited = TRUE;
	} else {
		window->priv->is_composited = FALSE;
	}
}

static void
gsd_media_keys_window_init (GsdMediaKeysWindow *window)
{
	window->priv = GSD_MEDIA_KEYS_WINDOW_GET_PRIVATE (window);

	initialize_alpha_mode (window);

	if (window->priv->is_composited) {
		gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
		gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

		gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
		g_signal_connect (window, "expose-event", G_CALLBACK (on_expose_event), window);
	} else {
		GtkWidget *vbox;

		window->priv->xml = glade_xml_new (DATADIR "/control-center-2.0/interfaces/acme.glade", "acme_vbox", NULL);

		vbox = glade_xml_get_widget (window->priv->xml, "acme_vbox");
		if (vbox != NULL) {
			gtk_container_add (GTK_CONTAINER (window), vbox);
			gtk_widget_show_all (vbox);
		}
	}
}

static void
gsd_media_keys_window_finalize (GObject *object)
{
        GsdMediaKeysWindow *window;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_MEDIA_KEYS_WINDOW (object));

        window = GSD_MEDIA_KEYS_WINDOW (object);

        g_return_if_fail (window->priv != NULL);

	if (window->priv->xml != NULL) {
		g_object_unref (window->priv->xml);
	}

        G_OBJECT_CLASS (gsd_media_keys_window_parent_class)->finalize (object);
}

GtkWidget *
gsd_media_keys_window_new (void)
{
        GObject *object;

        object = g_object_new (GSD_TYPE_MEDIA_KEYS_WINDOW,
			       "type", GTK_WINDOW_POPUP,
			       "skip-taskbar-hint", TRUE,
			       "skip-pager-hint", TRUE,
			       "set-focus-on-map", FALSE,
			       NULL);

        return GTK_WIDGET (object);
}
