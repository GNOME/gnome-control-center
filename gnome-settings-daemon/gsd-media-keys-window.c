/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
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
#define DIALOG_FADE_TIMEOUT 1500 /* timeout before fade starts */
#define FADE_TIMEOUT 10        /* timeout in ms between each frame of the fade */

#define BG_ALPHA 0.50
#define FG_ALPHA 1.00

static void     gsd_media_keys_window_class_init (GsdMediaKeysWindowClass *klass);
static void     gsd_media_keys_window_init       (GsdMediaKeysWindow      *fade);
static void     gsd_media_keys_window_finalize   (GObject                 *object);

#define GSD_MEDIA_KEYS_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_MEDIA_KEYS_WINDOW, GsdMediaKeysWindowPrivate))

struct GsdMediaKeysWindowPrivate
{
	guint                    is_composited : 1;
	guint                    hide_timeout_id;
	guint                    fade_timeout_id;
	double                   fade_out_alpha;
	GsdMediaKeysWindowAction action;

	guint                    volume_muted : 1;
	int                      volume_level;

	GladeXML                *xml;
};

G_DEFINE_TYPE (GsdMediaKeysWindow, gsd_media_keys_window, GTK_TYPE_WINDOW)

static gboolean
fade_timeout (GsdMediaKeysWindow *window)
{
	if (window->priv->fade_out_alpha <= 0.0) {
		gtk_widget_hide (GTK_WIDGET (window));

		/* Reset it for the next time */
		window->priv->fade_out_alpha = 1.0;
		window->priv->fade_timeout_id = 0;

		return FALSE;
	} else {
		GdkRectangle rect;
		GtkWidget *win = GTK_WIDGET (window);

		window->priv->fade_out_alpha -= 0.10;

		rect.x = 0;
		rect.y = 0;
		rect.width = win->allocation.width;
		rect.height = win->allocation.height;

		gdk_window_invalidate_rect (win->window, &rect, FALSE);
	}

	return TRUE;
}

static gboolean
hide_timeout (GsdMediaKeysWindow *window)
{
	if (window->priv->is_composited) {
		window->priv->hide_timeout_id = 0;
		window->priv->fade_timeout_id = g_timeout_add (FADE_TIMEOUT,
							       (GSourceFunc) fade_timeout,
							       window);
	} else {
		gtk_widget_hide (GTK_WIDGET (window));
	}

	return FALSE;
}

static void
remove_hide_timeout (GsdMediaKeysWindow *window)
{
        if (window->priv->hide_timeout_id != 0) {
                g_source_remove (window->priv->hide_timeout_id);
                window->priv->hide_timeout_id = 0;
        }

	if (window->priv->fade_timeout_id != 0) {
		g_source_remove (window->priv->fade_timeout_id);
		window->priv->fade_timeout_id = 0;
	}
}

static void
add_hide_timeout (GsdMediaKeysWindow *window)
{
	int timeout;

	if (window->priv->is_composited) {
		timeout = DIALOG_FADE_TIMEOUT;
	} else {
		timeout = DIALOG_TIMEOUT;
	}
	window->priv->hide_timeout_id = g_timeout_add (timeout,
						       (GSourceFunc) hide_timeout,
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
			window_set_icon_file (window, GNOMECC_PIXMAPS_DIR "/acme-eject.png");
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

static GdkPixbuf *
load_pixbuf (GsdMediaKeysWindow *window,
	     const char         *name,
	     int                 icon_size)
{
	GtkIconTheme *theme;
	GdkPixbuf    *pixbuf;

	if (window != NULL && gtk_widget_has_screen (GTK_WIDGET (window))) {
		theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)));
	} else {
		theme = gtk_icon_theme_get_default ();
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   name,
					   icon_size,
					   GTK_ICON_LOOKUP_FORCE_SVG,
					   NULL);

	/* make sure the pixbuf is close to the requested size
	 * this is necessary because GTK_ICON_LOOKUP_FORCE_SVG
	 * seems to be broken */
	if (pixbuf != NULL) {
		int width;

		width = gdk_pixbuf_get_width (pixbuf);
		if (width < (float)icon_size * 0.75) {
			g_object_unref (pixbuf);
			pixbuf = NULL;
		}
	}

	return pixbuf;
}

static gboolean
render_eject (GsdMediaKeysWindow *window,
	      cairo_t            *cr,
	      double              x0,
	      double              y0,
	      double              width,
	      double              height)
{
	GdkPixbuf  *pixbuf;
	int         icon_size;
	const char *icon_name;

	icon_name = "media-eject";

	icon_size = (int)width;

	pixbuf = load_pixbuf (window, icon_name, icon_size);

	if (pixbuf == NULL) {
		return FALSE;
	}

	gdk_cairo_set_source_pixbuf (cr, pixbuf, x0, y0);
	cairo_paint_with_alpha (cr, FG_ALPHA);

	g_object_unref (pixbuf);

	return TRUE;
}

static void
draw_eject (cairo_t *cr,
	    double   x0,
	    double   y0,
	    double   width,
	    double   height)
{
	int box_height;
	int tri_height;
	int separation;

	box_height = height * 0.2;
	separation = box_height / 3;
	tri_height = height - box_height - separation;

	cairo_rectangle (cr, x0, y0 + height - box_height, width, box_height);

	cairo_move_to (cr, x0, y0 + tri_height);
	cairo_rel_line_to (cr, width, 0);
	cairo_rel_line_to (cr, -width / 2, -tri_height);
	cairo_rel_line_to (cr, -width / 2, tri_height);
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, FG_ALPHA);
	cairo_fill_preserve (cr);

	cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, FG_ALPHA / 2);
	cairo_set_line_width (cr, 2);
	cairo_stroke (cr);
}

static void
draw_action_eject (GsdMediaKeysWindow *window,
		   cairo_t            *cr)
{
	int      window_width;
	int      window_height;
	double   width;
	double   height;
	double   x0;
	double   y0;
	gboolean res;

	gtk_window_get_size (GTK_WINDOW (window), &window_width, &window_height);

	width = window_width * 0.65;
	height = window_height * 0.65;
	x0 = (window_width - width) / 2;
	y0 = (window_height - height) / 2;

#if 0
	g_message ("eject box: w=%f h=%f x0=%f y0=%f",
		   width,
		   height,
		   x0,
		   y0);
#endif

	res = render_eject (window,
			    cr,
			    x0, y0,
			    width, height);
	if (! res) {
		/* draw eject symbol */
		draw_eject (cr, x0, y0, width, height);
	}
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

		angle1 = -M_PI / 4;
		angle2 = M_PI / 4;

		radius = (i + 1) * (max_radius / n_waves);
		cairo_arc (cr, cx, cy, radius, angle1, angle2);
		cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, FG_ALPHA / 2);
		cairo_set_line_width (cr, 14);
		cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
		cairo_stroke_preserve (cr);

		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, FG_ALPHA);
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
	double x0;
	double y0;

	box_width = width / 3;
	box_height = height / 3;

	x0 = cx - (width / 2) + box_width;
	y0 = cy - box_height / 2;

	cairo_move_to (cr, x0, y0);
	cairo_rel_line_to (cr, - box_width, 0);
	cairo_rel_line_to (cr, 0, box_height);
	cairo_rel_line_to (cr, box_width, 0);

	cairo_line_to (cr, cx + box_width, cy + height / 2);
	cairo_rel_line_to (cr, 0, -height);
	cairo_line_to (cr, x0, y0);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, FG_ALPHA);
	cairo_fill_preserve (cr);

	cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, FG_ALPHA / 2);
	cairo_set_line_width (cr, 2);
	cairo_stroke (cr);
}

static gboolean
render_speaker (GsdMediaKeysWindow *window,
		cairo_t            *cr,
		double              x0,
		double              y0,
		double              width,
		double              height)
{
	GdkPixbuf         *pixbuf;
	int                icon_size;
	int                n;
	static const char *icon_names[] = {
		"audio-volume-muted",
		"audio-volume-low",
		"audio-volume-medium",
		"audio-volume-high",
		NULL
	};

	if (window->priv->volume_muted) {
		n = 0;
	} else {
		/* select image */
		n = 3 * window->priv->volume_level / 100 + 1;
		if (n < 1) {
			n = 1;
		} else if (n > 3) {
			n = 3;
		}
	}

	icon_size = (int)width;

	pixbuf = load_pixbuf (window, icon_names[n], icon_size);

	if (pixbuf == NULL) {
		return FALSE;
	}

	gdk_cairo_set_source_pixbuf (cr, pixbuf, x0, y0);
	cairo_paint_with_alpha (cr, FG_ALPHA);

	g_object_unref (pixbuf);

	return TRUE;
}

static void
draw_volume_boxes (GsdMediaKeysWindow *window,
		   cairo_t            *cr,
		   double              percentage,
		   double              x0,
		   double              y0,
		   double              width,
		   double              height)
{
	gdouble  x1;
	GdkColor color;
	double   r, g, b;

	x1 = width * percentage;

	/* bar background */
	color = GTK_WIDGET (window)->style->dark [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x0, y0, width, height);
	cairo_set_source_rgba (cr, r, g, b, FG_ALPHA);
	cairo_fill (cr);

	/* bar border */
	color = GTK_WIDGET (window)->style->dark [GTK_STATE_SELECTED];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x0, y0, width, height);
	cairo_set_source_rgba (cr, r, g, b, FG_ALPHA);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	/* bar progress */
	color = GTK_WIDGET (window)->style->bg [GTK_STATE_SELECTED];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x0, y0, x1, height);
	cairo_set_source_rgba (cr, r, g, b, FG_ALPHA);
	cairo_fill (cr);
}

static void
draw_action_volume (GsdMediaKeysWindow *window,
		    cairo_t            *cr)
{
	int window_width;
	int window_height;
	double icon_box_width;
	double icon_box_height;
	double icon_box_x0;
	double icon_box_y0;
	double volume_box_x0;
	double volume_box_y0;
	double volume_box_width;
	double volume_box_height;
	gboolean res;

	gtk_window_get_size (GTK_WINDOW (window), &window_width, &window_height);

	icon_box_width = window_width * 0.65;
	icon_box_height = window_height * 0.65;
	volume_box_width = icon_box_width;
	volume_box_height = window_height * 0.05;

	icon_box_x0 = (window_width - icon_box_width) / 2;
	icon_box_y0 = (window_height - icon_box_height - volume_box_height) / 2;
	volume_box_x0 = icon_box_x0;
	volume_box_y0 = icon_box_height + icon_box_y0;

#if 0
	g_message ("icon box: w=%f h=%f x0=%f y0=%f",
		   icon_box_width,
		   icon_box_height,
		   icon_box_x0,
		   icon_box_y0);
	g_message ("volume box: w=%f h=%f x0=%f y0=%f",
		   volume_box_width,
		   volume_box_height,
		   volume_box_x0,
		   volume_box_y0);
#endif

	res = render_speaker (window,
			      cr,
			      icon_box_x0, icon_box_y0,
			      icon_box_width, icon_box_height);
	if (! res) {
		double speaker_width;
		double speaker_height;
		double speaker_cx;
		double speaker_cy;
		double wave_x0;
		double wave_y0;
		double wave_radius;

		speaker_width = icon_box_width * 0.5;
		speaker_height = icon_box_height * 0.75;
		speaker_cx = icon_box_x0 + speaker_width / 2;
		speaker_cy = icon_box_y0 + speaker_height / 2;

		wave_x0 = window_width / 2;
		wave_y0 = speaker_cy;
		wave_radius = icon_box_width / 2;

#if 0
		g_message ("speaker box: w=%f h=%f cx=%f cy=%f",
			   speaker_width,
			   speaker_height,
			   speaker_cx,
			   speaker_cy);
#endif

		/* draw speaker symbol */
		draw_speaker (cr, speaker_cx, speaker_cy, speaker_width, speaker_height);
		/* draw sound waves */
		if (! window->priv->volume_muted) {
			draw_waves (cr, wave_x0, wave_y0, wave_radius);
		}
	}

	/* draw volume meter */
	draw_volume_boxes (window,
			   cr,
			   (double)window->priv->volume_level / 100.0,
			   volume_box_x0,
			   volume_box_y0,
			   volume_box_width,
			   volume_box_height);
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
	GdkColor         color;
	double           r, g, b;

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
	curved_rectangle (cr, 0, 0, width, height, height / 10);
	color = GTK_WIDGET (window)->style->bg [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_set_source_rgba (cr, r, g, b, BG_ALPHA);
	cairo_fill_preserve (cr);

	color = GTK_WIDGET (window)->style->fg [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_set_source_rgba (cr, r, g, b, BG_ALPHA);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	/* draw action */
	draw_action (window, cr);

	cairo_destroy (cr);

	/* Make sure we have a transparent background */
	cairo_rectangle (context, 0, 0, width, height);
	cairo_set_source_rgba (context, 0.0, 0.0, 0.0, 0.0);
	cairo_fill (context);

	cairo_set_source_surface (context, surface, 0, 0);
	cairo_paint_with_alpha (context, window->priv->fade_out_alpha);

 done:
	if (surface != NULL) {
		cairo_surface_destroy (surface);
	}
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

gboolean
gsd_media_keys_window_is_valid (GsdMediaKeysWindow *window)
{
	GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));
	return gdk_screen_is_composited (screen) == window->priv->is_composited;
}

static void
initialize_alpha_mode (GsdMediaKeysWindow *window, GdkScreen *screen)
{
	GdkColormap *colormap = NULL;

	if (gdk_screen_is_composited (screen)) {
	 	colormap = gdk_screen_get_rgba_colormap (screen);
	}

	if (colormap != NULL) {
		gtk_widget_set_colormap (GTK_WIDGET (window), colormap);
		window->priv->is_composited = TRUE;
	} else {
		window->priv->is_composited = FALSE;
	}
}

static void
gsd_media_keys_window_init (GsdMediaKeysWindow *window)
{
	GdkScreen *screen;

	window->priv = GSD_MEDIA_KEYS_WINDOW_GET_PRIVATE (window);

	screen = gtk_widget_get_screen (GTK_WIDGET (window));

	initialize_alpha_mode (window, screen);

	if (window->priv->is_composited) {
		gdouble scalew, scaleh, scale;
		gint size;

		gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
		gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

		/* assume 100x100 on a 800x600 display and scale from there */
		scalew = gdk_screen_get_width (screen) / 800.0;
		scaleh = gdk_screen_get_height (screen) / 600.0;
		scale = MIN (scalew, scaleh);
		size = 100 * MAX (1, scale);

		gtk_window_set_default_size (GTK_WINDOW (window), size, size);
		g_signal_connect (window, "expose-event", G_CALLBACK (on_expose_event), window);

		window->priv->fade_out_alpha = 1.0;
	} else {
		GtkWidget *frame;

		window->priv->xml = glade_xml_new (GNOMECC_GLADE_DIR "/acme.glade", "acme_frame", NULL);

		frame = glade_xml_get_widget (window->priv->xml, "acme_frame");
		if (frame != NULL) {
			gtk_container_add (GTK_CONTAINER (window), frame);
			gtk_widget_show_all (frame);
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
			       "focus-on-map", FALSE,
			       NULL);

        return GTK_WIDGET (object);
}
