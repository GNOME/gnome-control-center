/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <william.jon.mccann@gmail.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "gvc-level-bar.h"

#define NUM_BOXES 30

#define MIN_HORIZONTAL_BAR_WIDTH   150
#define HORIZONTAL_BAR_HEIGHT      6
#define VERTICAL_BAR_WIDTH         6
#define MIN_VERTICAL_BAR_HEIGHT    400

typedef struct {
        int          peak_num;
        int          max_peak_num;

        GdkRectangle area;
        int          delta;
        int          box_width;
        int          box_height;
        int          box_radius;
        double       bg_r;
        double       bg_g;
        double       bg_b;
        double       bdr_r;
        double       bdr_g;
        double       bdr_b;
        double       fl_r;
        double       fl_g;
        double       fl_b;
} LevelBarLayout;

struct _GvcLevelBar
{
        GtkWidget      parent_instance;

        GtkOrientation orientation;
        GtkAdjustment *peak_adjustment;
        GtkAdjustment *rms_adjustment;
        GvcLevelScale  scale;
        gdouble        peak_fraction;
        gdouble        rms_fraction;
        gdouble        max_peak;
        guint          max_peak_id;
        LevelBarLayout layout;
};

enum
{
        PROP_0,
        PROP_PEAK_ADJUSTMENT,
        PROP_RMS_ADJUSTMENT,
        PROP_SCALE,
        PROP_ORIENTATION,
};

static void     gvc_level_bar_class_init (GvcLevelBarClass *klass);
static void     gvc_level_bar_init       (GvcLevelBar      *level_bar);
static void     gvc_level_bar_finalize   (GObject            *object);

G_DEFINE_TYPE (GvcLevelBar, gvc_level_bar, GTK_TYPE_WIDGET)

#define check_rectangle(rectangle1, rectangle2)                          \
        {                                                                \
                /* .x and .y are always 0 */                             \
                if (rectangle1.width  != rectangle2.width)  return TRUE; \
                if (rectangle1.height != rectangle2.height) return TRUE; \
        }

static gboolean
layout_changed (LevelBarLayout *layout1,
                LevelBarLayout *layout2)
{
        check_rectangle (layout1->area, layout2->area);
        if (layout1->delta != layout2->delta) return TRUE;
        if (layout1->peak_num != layout2->peak_num) return TRUE;
        if (layout1->max_peak_num != layout2->max_peak_num) return TRUE;
        if (layout1->bg_r != layout2->bg_r
            || layout1->bg_g != layout2->bg_g
            || layout1->bg_b != layout2->bg_b)
                return TRUE;
        if (layout1->bdr_r != layout2->bdr_r
            || layout1->bdr_g != layout2->bdr_g
            || layout1->bdr_b != layout2->bdr_b)
                return TRUE;
        if (layout1->fl_r != layout2->fl_r
            || layout1->fl_g != layout2->fl_g
            || layout1->fl_b != layout2->fl_b)
                return TRUE;

        return FALSE;
}

static gdouble
fraction_from_adjustment (GvcLevelBar   *bar,
                          GtkAdjustment *adjustment)
{
        gdouble level;
        gdouble fraction;
        gdouble min;
        gdouble max;

        level = gtk_adjustment_get_value (adjustment);

        min = gtk_adjustment_get_lower (adjustment);
        max = gtk_adjustment_get_upper (adjustment);

        switch (bar->scale) {
        case GVC_LEVEL_SCALE_LINEAR:
                fraction = (level - min) / (max - min);
                break;
        case GVC_LEVEL_SCALE_LOG:
                fraction = log10 ((level - min + 1) / (max - min + 1));
                break;
        default:
                g_assert_not_reached ();
        }

        return fraction;
}

static gboolean
reset_max_peak (GvcLevelBar *bar)
{
        gdouble min;

        min = gtk_adjustment_get_lower (bar->peak_adjustment);
        bar->max_peak = min;
        bar->layout.max_peak_num = 0;
        gtk_widget_queue_draw (GTK_WIDGET (bar));
        bar->max_peak_id = 0;
        return FALSE;
}

static void
bar_calc_layout (GvcLevelBar *bar)
{
        GdkColor color;
        int      peak_level;
        int      max_peak_level;
        GtkAllocation allocation;
        GtkStyle *style;

        gtk_widget_get_allocation (GTK_WIDGET (bar), &allocation);
        bar->layout.area.width = allocation.width - 2;
        bar->layout.area.height = allocation.height - 2;

        style = gtk_widget_get_style (GTK_WIDGET (bar));
        color = style->bg [GTK_STATE_NORMAL];
        bar->layout.bg_r = (float)color.red / 65535.0;
        bar->layout.bg_g = (float)color.green / 65535.0;
        bar->layout.bg_b = (float)color.blue / 65535.0;
        color = style->dark [GTK_STATE_NORMAL];
        bar->layout.bdr_r = (float)color.red / 65535.0;
        bar->layout.bdr_g = (float)color.green / 65535.0;
        bar->layout.bdr_b = (float)color.blue / 65535.0;
        color = style->bg [GTK_STATE_SELECTED];
        bar->layout.fl_r = (float)color.red / 65535.0;
        bar->layout.fl_g = (float)color.green / 65535.0;
        bar->layout.fl_b = (float)color.blue / 65535.0;

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                peak_level = bar->peak_fraction * bar->layout.area.height;
                max_peak_level = bar->max_peak * bar->layout.area.height;

                bar->layout.delta = bar->layout.area.height / NUM_BOXES;
                bar->layout.area.x = 0;
                bar->layout.area.y = 0;
                bar->layout.box_height = bar->layout.delta / 2;
                bar->layout.box_width = bar->layout.area.width;
                bar->layout.box_radius = bar->layout.box_width / 2;
        } else {
                peak_level = bar->peak_fraction * bar->layout.area.width;
                max_peak_level = bar->max_peak * bar->layout.area.width;

                bar->layout.delta = bar->layout.area.width / NUM_BOXES;
                bar->layout.area.x = 0;
                bar->layout.area.y = 0;
                bar->layout.box_width = bar->layout.delta / 2;
                bar->layout.box_height = bar->layout.area.height;
                bar->layout.box_radius = bar->layout.box_height / 2;
        }

        /* This can happen if the level bar isn't realized */
        if (bar->layout.delta == 0)
                return;

        bar->layout.peak_num = peak_level / bar->layout.delta;
        bar->layout.max_peak_num = max_peak_level / bar->layout.delta;
}

static void
update_peak_value (GvcLevelBar *bar)
{
        gdouble        val;
        LevelBarLayout layout;

        layout = bar->layout;

        val = fraction_from_adjustment (bar, bar->peak_adjustment);
        bar->peak_fraction = val;

        if (val > bar->max_peak) {
                if (bar->max_peak_id > 0) {
                        g_source_remove (bar->max_peak_id);
                }
                bar->max_peak_id = g_timeout_add_seconds (1, (GSourceFunc)reset_max_peak, bar);
                bar->max_peak = val;
        }

        bar_calc_layout (bar);

        if (layout_changed (&bar->layout, &layout)) {
                gtk_widget_queue_draw (GTK_WIDGET (bar));
        }
}

static void
update_rms_value (GvcLevelBar *bar)
{
        gdouble val;

        val = fraction_from_adjustment (bar, bar->rms_adjustment);
        bar->rms_fraction = val;
}

GtkOrientation
gvc_level_bar_get_orientation (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), 0);
        return bar->orientation;
}

void
gvc_level_bar_set_orientation (GvcLevelBar   *bar,
                               GtkOrientation orientation)
{
        g_return_if_fail (GVC_IS_LEVEL_BAR (bar));

        if (orientation != bar->orientation) {
                bar->orientation = orientation;
                gtk_widget_queue_draw (GTK_WIDGET (bar));
                g_object_notify (G_OBJECT (bar), "orientation");
        }
}

static void
on_peak_adjustment_value_changed (GtkAdjustment *adjustment,
                                  GvcLevelBar   *bar)
{
        update_peak_value (bar);
}

static void
on_rms_adjustment_value_changed (GtkAdjustment *adjustment,
                                 GvcLevelBar   *bar)
{
        update_rms_value (bar);
}

void
gvc_level_bar_set_peak_adjustment (GvcLevelBar   *bar,
                                   GtkAdjustment *adjustment)
{
        g_return_if_fail (GVC_LEVEL_BAR (bar));
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

        if (bar->peak_adjustment != NULL) {
                g_signal_handlers_disconnect_by_func (bar->peak_adjustment,
                                                      G_CALLBACK (on_peak_adjustment_value_changed),
                                                      bar);
                g_object_unref (bar->peak_adjustment);
        }

        bar->peak_adjustment = g_object_ref_sink (adjustment);

        g_signal_connect (bar->peak_adjustment,
                          "value-changed",
                          G_CALLBACK (on_peak_adjustment_value_changed),
                          bar);

        update_peak_value (bar);

        g_object_notify (G_OBJECT (bar), "peak-adjustment");
}

void
gvc_level_bar_set_rms_adjustment (GvcLevelBar   *bar,
                                  GtkAdjustment *adjustment)
{
        g_return_if_fail (GVC_LEVEL_BAR (bar));
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

        if (bar->rms_adjustment != NULL) {
                g_signal_handlers_disconnect_by_func (bar->peak_adjustment,
                                                      G_CALLBACK (on_rms_adjustment_value_changed),
                                                      bar);
                g_object_unref (bar->rms_adjustment);
        }

        bar->rms_adjustment = g_object_ref_sink (adjustment);


        g_signal_connect (bar->peak_adjustment,
                          "value-changed",
                          G_CALLBACK (on_peak_adjustment_value_changed),
                          bar);

        update_rms_value (bar);

        g_object_notify (G_OBJECT (bar), "rms-adjustment");
}

GtkAdjustment *
gvc_level_bar_get_peak_adjustment (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), NULL);

        return bar->peak_adjustment;
}

GtkAdjustment *
gvc_level_bar_get_rms_adjustment (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), NULL);

        return bar->rms_adjustment;
}

void
gvc_level_bar_set_scale (GvcLevelBar  *bar,
                         GvcLevelScale scale)
{
        g_return_if_fail (GVC_IS_LEVEL_BAR (bar));

        if (scale != bar->scale) {
                bar->scale = scale;

                update_peak_value (bar);
                update_rms_value (bar);

                g_object_notify (G_OBJECT (bar), "scale");
        }
}

static void
gvc_level_bar_set_property (GObject       *object,
                              guint          prop_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
        GvcLevelBar *self = GVC_LEVEL_BAR (object);

        switch (prop_id) {
        case PROP_SCALE:
                gvc_level_bar_set_scale (self, g_value_get_int (value));
                break;
        case PROP_ORIENTATION:
                gvc_level_bar_set_orientation (self, g_value_get_enum (value));
                break;
        case PROP_PEAK_ADJUSTMENT:
                gvc_level_bar_set_peak_adjustment (self, g_value_get_object (value));
                break;
        case PROP_RMS_ADJUSTMENT:
                gvc_level_bar_set_rms_adjustment (self, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_level_bar_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
        GvcLevelBar *self = GVC_LEVEL_BAR (object);

        switch (prop_id) {
        case PROP_SCALE:
                g_value_set_int (value, self->scale);
                break;
        case PROP_ORIENTATION:
                g_value_set_enum (value, self->orientation);
                break;
        case PROP_PEAK_ADJUSTMENT:
                g_value_set_object (value, self->peak_adjustment);
                break;
        case PROP_RMS_ADJUSTMENT:
                g_value_set_object (value, self->rms_adjustment);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gvc_level_bar_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_params)
{
        return G_OBJECT_CLASS (gvc_level_bar_parent_class)->constructor (type, n_construct_properties, construct_params);
}

static void
gvc_level_bar_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
        GvcLevelBar *bar = GVC_LEVEL_BAR (widget);

        switch (bar->orientation) {
        case GTK_ORIENTATION_VERTICAL:
                requisition->width = VERTICAL_BAR_WIDTH;
                requisition->height = MIN_VERTICAL_BAR_HEIGHT;
                break;
        case GTK_ORIENTATION_HORIZONTAL:
                requisition->width = MIN_HORIZONTAL_BAR_WIDTH;
                requisition->height = HORIZONTAL_BAR_HEIGHT;
                break;
        default:
                g_assert_not_reached ();
                break;
        }
}

static void
gvc_level_bar_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
        GtkRequisition requisition;

        gvc_level_bar_size_request (widget, &requisition);

        if (minimum != NULL) {
                *minimum = requisition.width;
        }
        if (natural != NULL) {
                *natural = requisition.width;
        }
}

static void
gvc_level_bar_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
        GtkRequisition requisition;

        gvc_level_bar_size_request (widget, &requisition);

        if (minimum != NULL) {
                *minimum = requisition.height;
        }
        if (natural != NULL) {
                *natural = requisition.height;
        }
}

static void
gvc_level_bar_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
        GvcLevelBar *bar;

        g_return_if_fail (GVC_IS_LEVEL_BAR (widget));
        g_return_if_fail (allocation != NULL);

        bar = GVC_LEVEL_BAR (widget);

        /* FIXME: add height property, labels, etc */
        GTK_WIDGET_CLASS (gvc_level_bar_parent_class)->size_allocate (widget, allocation);

        gtk_widget_set_allocation (widget, allocation);
        gtk_widget_get_allocation (widget, allocation);

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                allocation->height = MIN (allocation->height, MIN_VERTICAL_BAR_HEIGHT);
                allocation->width = MAX (allocation->width, VERTICAL_BAR_WIDTH);
        } else {
                allocation->width = MIN (allocation->width, MIN_HORIZONTAL_BAR_WIDTH);
                allocation->height = MAX (allocation->height, HORIZONTAL_BAR_HEIGHT);
        }

        bar_calc_layout (bar);
}

static void
curved_rectangle (cairo_t *cr,
                  double   _x0,
                  double   _y0,
                  double   width,
                  double   height,
                  double   radius)
{
        double x1;
        double _y1;

        x1 = _x0 + width;
        _y1 = _y0 + height;

        if (!width || !height) {
                return;
        }

        if (width / 2 < radius) {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, _x0, (_y0 + _y1) / 2);
                        cairo_curve_to (cr, _x0 ,_y0, _x0, _y0, (_x0 + x1) / 2, _y0);
                        cairo_curve_to (cr, x1, _y0, x1, _y0, x1, (_y0 + _y1) / 2);
                        cairo_curve_to (cr, x1, _y1, x1, _y1, (x1 + _x0) / 2, _y1);
                        cairo_curve_to (cr, _x0, _y1, _x0, _y1, _x0, (_y0 + _y1) / 2);
                } else {
                        cairo_move_to  (cr, _x0, _y0 + radius);
                        cairo_curve_to (cr, _x0, _y0, _x0, _y0, (_x0 + x1) / 2, _y0);
                        cairo_curve_to (cr, x1, _y0, x1, _y0, x1, _y0 + radius);
                        cairo_line_to (cr, x1, _y1 - radius);
                        cairo_curve_to (cr, x1, _y1, x1, _y1, (x1 + _x0) / 2, _y1);
                        cairo_curve_to (cr, _x0, _y1, _x0, _y1, _x0, _y1 - radius);
                }
        } else {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, _x0, (_y0 + _y1) / 2);
                        cairo_curve_to (cr, _x0, _y0, _x0 , _y0, _x0 + radius, _y0);
                        cairo_line_to (cr, x1 - radius, _y0);
                        cairo_curve_to (cr, x1, _y0, x1, _y0, x1, (_y0 + _y1) / 2);
                        cairo_curve_to (cr, x1, _y1, x1, _y1, x1 - radius, _y1);
                        cairo_line_to (cr, _x0 + radius, _y1);
                        cairo_curve_to (cr, _x0, _y1, _x0, _y1, _x0, (_y0 + _y1) / 2);
                } else {
                        cairo_move_to  (cr, _x0, _y0 + radius);
                        cairo_curve_to (cr, _x0 , _y0, _x0 , _y0, _x0 + radius, _y0);
                        cairo_line_to (cr, x1 - radius, _y0);
                        cairo_curve_to (cr, x1, _y0, x1, _y0, x1, _y0 + radius);
                        cairo_line_to (cr, x1, _y1 - radius);
                        cairo_curve_to (cr, x1, _y1, x1, _y1, x1 - radius, _y1);
                        cairo_line_to (cr, _x0 + radius, _y1);
                        cairo_curve_to (cr, _x0, _y1, _x0, _y1, _x0, _y1 - radius);
                }
        }

        cairo_close_path (cr);
}

static int
gvc_level_bar_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
        GvcLevelBar     *bar;

        g_return_val_if_fail (GVC_IS_LEVEL_BAR (widget), FALSE);

        bar = GVC_LEVEL_BAR (widget);

        cairo_save (cr);

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                int i;
                int by;

                for (i = 0; i < NUM_BOXES; i++) {
                        by = i * bar->layout.delta;
                        curved_rectangle (cr,
                                          bar->layout.area.x + 0.5,
                                          by + 0.5,
                                          bar->layout.box_width - 1,
                                          bar->layout.box_height - 1,
                                          bar->layout.box_radius);
                        if ((bar->layout.max_peak_num - 1) == i) {
                                /* fill peak foreground */
                                cairo_set_source_rgb (cr, bar->layout.fl_r, bar->layout.fl_g, bar->layout.fl_b);
                                cairo_fill_preserve (cr);
                        } else if ((bar->layout.peak_num - 1) >= i) {
                                /* fill background */
                                cairo_set_source_rgb (cr, bar->layout.bg_r, bar->layout.bg_g, bar->layout.bg_b);
                                cairo_fill_preserve (cr);
                                /* fill foreground */
                                cairo_set_source_rgba (cr, bar->layout.fl_r, bar->layout.fl_g, bar->layout.fl_b, 0.5);
                                cairo_fill_preserve (cr);
                        } else {
                                /* fill background */
                                cairo_set_source_rgb (cr, bar->layout.bg_r, bar->layout.bg_g, bar->layout.bg_b);
                                cairo_fill_preserve (cr);
                        }

                        /* stroke border */
                        cairo_set_source_rgb (cr, bar->layout.bdr_r, bar->layout.bdr_g, bar->layout.bdr_b);
                        cairo_set_line_width (cr, 1);
                        cairo_stroke (cr);
                }

        } else {
                int i;
                int bx;

                if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
                        cairo_scale (cr, -1, 1);
                        cairo_translate (cr, -gtk_widget_get_allocated_width (widget), 0);
                }

                for (i = 0; i < NUM_BOXES; i++) {
                        bx = i * bar->layout.delta;
                        curved_rectangle (cr,
                                          bx + 0.5,
                                          bar->layout.area.y + 0.5,
                                          bar->layout.box_width - 1,
                                          bar->layout.box_height - 1,
                                          bar->layout.box_radius);

                        if ((bar->layout.max_peak_num - 1) == i) {
                                /* fill peak foreground */
                                cairo_set_source_rgb (cr, bar->layout.fl_r, bar->layout.fl_g, bar->layout.fl_b);
                                cairo_fill_preserve (cr);
                        } else if ((bar->layout.peak_num - 1) >= i) {
                                /* fill background */
                                cairo_set_source_rgb (cr, bar->layout.bg_r, bar->layout.bg_g, bar->layout.bg_b);
                                cairo_fill_preserve (cr);
                                /* fill foreground */
                                cairo_set_source_rgba (cr, bar->layout.fl_r, bar->layout.fl_g, bar->layout.fl_b, 0.5);
                                cairo_fill_preserve (cr);
                        } else {
                                /* fill background */
                                cairo_set_source_rgb (cr, bar->layout.bg_r, bar->layout.bg_g, bar->layout.bg_b);
                                cairo_fill_preserve (cr);
                        }

                        /* stroke border */
                        cairo_set_source_rgb (cr, bar->layout.bdr_r, bar->layout.bdr_g, bar->layout.bdr_b);
                        cairo_set_line_width (cr, 1);
                        cairo_stroke (cr);
                }
        }

        cairo_restore (cr);

        return FALSE;
}

static void
gvc_level_bar_class_init (GvcLevelBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->constructor = gvc_level_bar_constructor;
        object_class->finalize = gvc_level_bar_finalize;
        object_class->set_property = gvc_level_bar_set_property;
        object_class->get_property = gvc_level_bar_get_property;

        widget_class->draw = gvc_level_bar_draw;
        widget_class->get_preferred_width = gvc_level_bar_get_preferred_width;
        widget_class->get_preferred_height = gvc_level_bar_get_preferred_height;
        widget_class->size_allocate = gvc_level_bar_size_allocate;

        g_object_class_install_property (object_class,
                                         PROP_ORIENTATION,
                                         g_param_spec_enum ("orientation",
                                                            "Orientation",
                                                            "The orientation of the bar",
                                                            GTK_TYPE_ORIENTATION,
                                                            GTK_ORIENTATION_HORIZONTAL,
                                                            G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_PEAK_ADJUSTMENT,
                                         g_param_spec_object ("peak-adjustment",
                                                              "Peak Adjustment",
                                                              "The GtkAdjustment that contains the current peak value",
                                                              GTK_TYPE_ADJUSTMENT,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_RMS_ADJUSTMENT,
                                         g_param_spec_object ("rms-adjustment",
                                                              "RMS Adjustment",
                                                              "The GtkAdjustment that contains the current rms value",
                                                              GTK_TYPE_ADJUSTMENT,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_SCALE,
                                         g_param_spec_int ("scale",
                                                           "Scale",
                                                           "Scale",
                                                           0,
                                                           G_MAXINT,
                                                           GVC_LEVEL_SCALE_LINEAR,
                                                           G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}

static void
gvc_level_bar_init (GvcLevelBar *bar)
{
        bar->peak_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                         0.0,
                                                                         1.0,
                                                                         0.05,
                                                                         0.1,
                                                                         0.1));
        g_object_ref_sink (bar->peak_adjustment);
        g_signal_connect (bar->peak_adjustment,
                          "value-changed",
                          G_CALLBACK (on_peak_adjustment_value_changed),
                          bar);

        bar->rms_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                        0.0,
                                                                        1.0,
                                                                        0.05,
                                                                        0.1,
                                                                        0.1));
        g_object_ref_sink (bar->rms_adjustment);
        g_signal_connect (bar->rms_adjustment,
                          "value-changed",
                          G_CALLBACK (on_rms_adjustment_value_changed),
                          bar);

        gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
}

static void
gvc_level_bar_finalize (GObject *object)
{
        GvcLevelBar *bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_LEVEL_BAR (object));

        bar = GVC_LEVEL_BAR (object);

        if (bar->max_peak_id > 0) {
                g_source_remove (bar->max_peak_id);
        }

        G_OBJECT_CLASS (gvc_level_bar_parent_class)->finalize (object);
}

GtkWidget *
gvc_level_bar_new (void)
{
        GObject *bar;
        bar = g_object_new (GVC_TYPE_LEVEL_BAR,
                            NULL);
        return GTK_WIDGET (bar);
}
