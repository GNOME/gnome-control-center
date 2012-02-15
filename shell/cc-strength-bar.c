/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2010-2012 Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by:
 *      Matthias Clasen <mclasen@redhat.com>
 *      Richard Hughes <richard@hughsie.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-strength-bar.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_STRENGTH_BAR, CcStrengthBarPrivate))

struct _CcStrengthBarPrivate
{
        gdouble fraction;
        gint segments;
};

enum {
        PROP_0,
        PROP_FRACTION,
        PROP_SEGMENTS
};

G_DEFINE_TYPE (CcStrengthBar, cc_strength_bar, GTK_TYPE_WIDGET);


static void
cc_strength_bar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        CcStrengthBar *bar = CC_STRENGTH_BAR (object);

        switch (prop_id) {
        case PROP_FRACTION:
                cc_strength_bar_set_fraction (bar, g_value_get_double (value));
                break;
        case PROP_SEGMENTS:
                cc_strength_bar_set_segments (bar, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_strength_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        CcStrengthBar *bar = CC_STRENGTH_BAR (object);

        switch (prop_id) {
        case PROP_FRACTION:
                g_value_set_double (value, cc_strength_bar_get_fraction (bar));
                break;
        case PROP_SEGMENTS:
                g_value_set_int (value, cc_strength_bar_get_segments (bar));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
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
                        cairo_curve_to (cr, x0 , y0 + radius, x0, y0, x0 + radius, y0);
                        cairo_line_to (cr, x1 - radius, y0);
                        cairo_curve_to (cr, x1 - radius, y0, x1, y0, x1, y0 + radius);
                        cairo_line_to (cr, x1, y1 - radius);
                        cairo_curve_to (cr, x1, y1 - radius, x1, y1, x1 - radius, y1);
                        cairo_line_to (cr, x0 + radius, y1);
                        cairo_curve_to (cr, x0 + radius, y1, x0, y1, x0, y1 - radius);
                }
        }

        cairo_close_path (cr);
}

static gboolean
cc_strength_bar_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
        CcStrengthBar *bar = CC_STRENGTH_BAR (widget);
        const gint padding_end = 3;     /* this is the padding at either end */
        const gint padding_x = 2;       /* this is the padding between elements */
        gboolean ret;
        GdkRGBA color;
        GdkRGBA color2;
        gdouble section_width;
        gdouble this_width;
        gint count;
        gint last_x;
        gint width, height;
        GtkStyleContext *context;
        guint i;

        context = gtk_widget_get_style_context (widget);
        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

        /* clip */
        cairo_save (cr);
        cairo_rectangle (cr,
                         0, 0,
                         width, height);
        cairo_clip (cr);

        /* get gutter color */
        ret = gtk_style_context_lookup_color (context,
                                              "@base_color",
                                              &color);
        if (!ret) {
                /* fall back to white */
                color.alpha = 1.0f;
                color.red = 1.0;
                color.green = 1.0;
                color.blue = 1.0;
        }

        /* border */
        cairo_set_line_width (cr, 1);
        gdk_cairo_set_source_rgba (cr, &color);
        curved_rectangle (cr,
                         0.5, 0.5,
                         width - 1, height - 1, 4);
        cairo_fill_preserve (cr);
        ret = gtk_style_context_lookup_color (context,
                                              "@borders",
                                              &color);
        if (!ret) {
                /* fall back to black */
                color.alpha = 1.0f;
                color.red = 0.0;
                color.green = 0.0;
                color.blue = 0.0;
        }
        gdk_cairo_set_source_rgba (cr, &color);
        cairo_stroke (cr);

        /* get color of bar */
        if (bar->priv->fraction < 0.25) {
                ret = gtk_style_context_lookup_color (context,
                                                      "warning_color",
                                                      &color);
                if (!ret) {
                        /* fall back to red */
                        color.alpha = 1.0f;
                        color.red = 0.91;
                        color.green = 0.16;
                        color.blue = 0.16;
                }
        } else if (bar->priv->fraction > 0.75) {
                ret = gtk_style_context_lookup_color (context,
                                                      "success_color",
                                                      &color);
                if (!ret) {
                        /* fall back to green */
                        color.alpha = 1.0f;
                        color.red = 0.45;
                        color.green = 0.82;
                        color.blue = 0.09;
                }
        } else {
                ret = gtk_style_context_lookup_color (context,
                                                      "selected_bg_color",
                                                      &color);
                if (!ret) {
                        /* fall back to blue */
                        color.alpha = 1.0f;
                        color.red = 0.44;
                        color.green = 0.62;
                        color.blue = 0.80;
                }
        }

        /* make darker background color */
        color2.alpha = 1.0f;
        color2.red = color.red / 1.5;
        color2.green = color.green / 1.5;
        color2.blue = color.blue / 1.5;

        /* nothing hardcoded, so set a number based on the width */
        if (bar->priv->segments == -1)
                bar->priv->segments = width / 30;

        /* print the bar */
        section_width = (width - (padding_end * 2) + 2) / (gdouble) bar->priv->segments;
        count = bar->priv->fraction * (gdouble) bar->priv->segments;
        last_x = -padding_x + padding_end;
        for (i = 0; i < count; i++) {

                /* do this for each segment as we're clipping to integers
                 * do avoid antialiased lines */
                this_width = (section_width * (i + 1)) - last_x - padding_x;

                /* lighter line */
                cairo_set_line_width (cr, 1);
                gdk_cairo_set_source_rgba (cr, &color);
                curved_rectangle (cr,
                                 last_x + padding_x + 0.5, 3.0 + 0.5,
                                 (gint) this_width, height - 7, 4);
                cairo_fill (cr);

                /* darker line */
                gdk_cairo_set_source_rgba (cr, &color2);
                curved_rectangle (cr,
                                 last_x + padding_x + 0.5, 3.0 + 0.5,
                                 (gint) this_width, height - 7, 4);
                cairo_stroke (cr);

                /* square off except the start */
                if (i != 0) {
                        cairo_move_to (cr,
                                       last_x + padding_x + 0.5,
                                       3.0 + 0.5);
                        cairo_line_to (cr,
                                       last_x + padding_x + 0.5,
                                       height - 3);
                        cairo_stroke (cr);
                }
                
                /* square off except the end */
                if (i != bar->priv->segments - 1) {
                        cairo_move_to (cr,
                                       last_x + padding_x + (gint) this_width + 0.5,
                                       3.0 + 0.5);
                        cairo_line_to (cr,
                                       last_x + padding_x + (gint) this_width + 0.5,
                                       height - 3);
                        cairo_stroke (cr);
                }

                /* increment counter using integers */
                last_x += (gint) this_width + padding_x;
        }

        cairo_restore (cr); /* undo clip */

        return FALSE;
}

static void
cc_strength_bar_class_init (CcStrengthBarClass *class)
{
        GObjectClass *gobject_class;
        GtkWidgetClass *widget_class;

        gobject_class = (GObjectClass*)class;
        widget_class = (GtkWidgetClass*)class;

        gobject_class->set_property = cc_strength_bar_set_property;
        gobject_class->get_property = cc_strength_bar_get_property;

        widget_class->draw = cc_strength_bar_draw;

         g_object_class_install_property (gobject_class,
                                          PROP_FRACTION,
                                          g_param_spec_double ("fraction",
                                                               "Fraction",
                                                               "Fraction",
                                                               0.0, 1.0, 0.0,
                                                               G_PARAM_READWRITE));
         g_object_class_install_property (gobject_class,
                                          PROP_SEGMENTS,
                                          g_param_spec_int ("segments",
                                                            "Segments",
                                                            "Segments",
                                                            -1, 20, -1,
                                                            G_PARAM_READWRITE));

        g_type_class_add_private (class, sizeof (CcStrengthBarPrivate));
}

static void
cc_strength_bar_init (CcStrengthBar *bar)
{
        gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
        gtk_widget_set_size_request (GTK_WIDGET (bar), 120, 9);

        bar->priv = GET_PRIVATE (bar);
        bar->priv->fraction = 0.0;
        bar->priv->segments = -1;
}

GtkWidget *
cc_strength_bar_new (void)
{
        return (GtkWidget*) g_object_new (UM_TYPE_STRENGTH_BAR, NULL);
}

void
cc_strength_bar_set_fraction (CcStrengthBar *bar,
                              gdouble        fraction)
{
        bar->priv->fraction = fraction;

        g_object_notify (G_OBJECT (bar), "fraction");

        if (gtk_widget_is_drawable (GTK_WIDGET (bar)))
                gtk_widget_queue_draw (GTK_WIDGET (bar));
}

gdouble
cc_strength_bar_get_fraction (CcStrengthBar *bar)
{
        return bar->priv->fraction;
}

gint
cc_strength_bar_get_segments (CcStrengthBar *bar)
{
        return bar->priv->segments;
}

void
cc_strength_bar_set_segments (CcStrengthBar *bar,
                              gint           segments)
{
        bar->priv->segments = segments;

        g_object_notify (G_OBJECT (bar), "segments");

        if (gtk_widget_is_drawable (GTK_WIDGET (bar)))
                gtk_widget_queue_draw (GTK_WIDGET (bar));
}
