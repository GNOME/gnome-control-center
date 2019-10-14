/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-crop-area.h"

struct _CcCropArea {
        GtkDrawingArea parent_instance;

        GdkPixbuf *browse_pixbuf;
        GdkPixbuf *pixbuf;
        GdkPixbuf *color_shifted;
        gdouble scale;
        GdkRectangle image;
        GdkCursorType current_cursor;
        GdkRectangle crop;
        gint active_region;
        gint last_press_x;
        gint last_press_y;
        gint base_width;
        gint base_height;
        gdouble aspect;
};

G_DEFINE_TYPE (CcCropArea, cc_crop_area, GTK_TYPE_DRAWING_AREA);

static inline guchar
shift_color_byte (guchar b,
                  int    shift)
{
        return CLAMP(b + shift, 0, 255);
}

static void
shift_colors (GdkPixbuf *pixbuf,
              gint       red,
              gint       green,
              gint       blue,
              gint       alpha)
{
        gint x, y, offset, y_offset, rowstride, width, height;
        guchar *pixels;
        gint channels;

        width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);
        channels = gdk_pixbuf_get_n_channels (pixbuf);

        for (y = 0; y < height; y++) {
                y_offset = y * rowstride;
                for (x = 0; x < width; x++) {
                        offset = y_offset + x * channels;
                        if (red != 0)
                                pixels[offset] = shift_color_byte (pixels[offset], red);
                        if (green != 0)
                                pixels[offset + 1] = shift_color_byte (pixels[offset + 1], green);
                        if (blue != 0)
                                pixels[offset + 2] = shift_color_byte (pixels[offset + 2], blue);
                        if (alpha != 0 && channels >= 4)
                                pixels[offset + 3] = shift_color_byte (pixels[offset + 3], blue);
                }
        }
}

static void
update_pixbufs (CcCropArea *area)
{
        gint width;
        gint height;
        GtkAllocation allocation;
        gdouble scale;
        gint dest_width, dest_height;
        GtkWidget *widget;

        widget = GTK_WIDGET (area);
        gtk_widget_get_allocation (widget, &allocation);

        width = gdk_pixbuf_get_width (area->browse_pixbuf);
        height = gdk_pixbuf_get_height (area->browse_pixbuf);

        scale = allocation.height / (gdouble)height;
        if (scale * width > allocation.width)
                scale = allocation.width / (gdouble)width;

        dest_width = width * scale;
        dest_height = height * scale;

        if (area->pixbuf == NULL ||
            gdk_pixbuf_get_width (area->pixbuf) != allocation.width ||
            gdk_pixbuf_get_height (area->pixbuf) != allocation.height) {
                g_clear_object (&area->pixbuf);
                area->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                                     gdk_pixbuf_get_has_alpha (area->browse_pixbuf),
                                                     8,
                                                     dest_width, dest_height);
                gdk_pixbuf_fill (area->pixbuf, 0x0);

                gdk_pixbuf_scale (area->browse_pixbuf,
                                  area->pixbuf,
                                  0, 0,
                                  dest_width, dest_height,
                                  0, 0,
                                  scale, scale,
                                  GDK_INTERP_BILINEAR);

                g_clear_object (&area->color_shifted);
                area->color_shifted = gdk_pixbuf_copy (area->pixbuf);
                shift_colors (area->color_shifted, -32, -32, -32, 0);

                if (area->scale == 0.0) {
                        gdouble scale_to_80, scale_to_image, crop_scale;

                        /* Scale the crop rectangle to 80% of the area, or less to fit the image */
                        scale_to_80 = MIN ((gdouble)gdk_pixbuf_get_width (area->pixbuf) * 0.8 / area->base_width,
                                           (gdouble)gdk_pixbuf_get_height (area->pixbuf) * 0.8 / area->base_height);
                        scale_to_image = MIN ((gdouble)dest_width / area->base_width,
                                              (gdouble)dest_height / area->base_height);
                        crop_scale = MIN (scale_to_80, scale_to_image);

                        area->crop.width = crop_scale * area->base_width / scale;
                        area->crop.height = crop_scale * area->base_height / scale;
                        area->crop.x = (gdk_pixbuf_get_width (area->browse_pixbuf) - area->crop.width) / 2;
                        area->crop.y = (gdk_pixbuf_get_height (area->browse_pixbuf) - area->crop.height) / 2;
                }

                area->scale = scale;
                area->image.x = (allocation.width - dest_width) / 2;
                area->image.y = (allocation.height - dest_height) / 2;
                area->image.width = dest_width;
                area->image.height = dest_height;
        }
}

static void
crop_to_widget (CcCropArea    *area,
                GdkRectangle  *crop)
{
        crop->x = area->image.x + area->crop.x * area->scale;
        crop->y = area->image.y + area->crop.y * area->scale;
        crop->width = area->crop.width * area->scale;
        crop->height = area->crop.height * area->scale;
}

typedef enum {
        OUTSIDE,
        INSIDE,
        TOP,
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        LEFT,
        RIGHT
} Location;

static gboolean
cc_crop_area_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
        GdkRectangle crop;
        gint width, height, ix, iy;
        CcCropArea *uarea = CC_CROP_AREA (widget);

        if (uarea->browse_pixbuf == NULL)
                return FALSE;

        update_pixbufs (uarea);

        width = gdk_pixbuf_get_width (uarea->pixbuf);
        height = gdk_pixbuf_get_height (uarea->pixbuf);
        crop_to_widget (uarea, &crop);

        ix = uarea->image.x;
        iy = uarea->image.y;

        gdk_cairo_set_source_pixbuf (cr, uarea->color_shifted, ix, iy);
        cairo_rectangle (cr, ix, iy, width, crop.y - iy);
        cairo_rectangle (cr, ix, crop.y, crop.x - ix, crop.height);
        cairo_rectangle (cr, crop.x + crop.width, crop.y, width - crop.width - (crop.x - ix), crop.height);
        cairo_rectangle (cr, ix, crop.y + crop.height, width, height - crop.height - (crop.y - iy));
        cairo_fill (cr);

        gdk_cairo_set_source_pixbuf (cr, uarea->pixbuf, ix, iy);
        cairo_rectangle (cr, crop.x, crop.y, crop.width, crop.height);
        cairo_fill (cr);

        if (uarea->active_region != OUTSIDE) {
                gint x1, x2, y1, y2;
                cairo_set_source_rgb (cr, 1, 1, 1);
                cairo_set_line_width (cr, 1.0);
                x1 = crop.x + crop.width / 3.0;
                x2 = crop.x + 2 * crop.width / 3.0;
                y1 = crop.y + crop.height / 3.0;
                y2 = crop.y + 2 * crop.height / 3.0;

                cairo_move_to (cr, x1 + 0.5, crop.y);
                cairo_line_to (cr, x1 + 0.5, crop.y + crop.height);

                cairo_move_to (cr, x2 + 0.5, crop.y);
                cairo_line_to (cr, x2 + 0.5, crop.y + crop.height);

                cairo_move_to (cr, crop.x, y1 + 0.5);
                cairo_line_to (cr, crop.x + crop.width, y1 + 0.5);

                cairo_move_to (cr, crop.x, y2 + 0.5);
                cairo_line_to (cr, crop.x + crop.width, y2 + 0.5);
                cairo_stroke (cr);
        }

        cairo_set_source_rgb (cr,  0, 0, 0);
        cairo_set_line_width (cr, 1.0);
        cairo_rectangle (cr,
                         crop.x + 0.5,
                         crop.y + 0.5,
                         crop.width - 1.0,
                         crop.height - 1.0);
        cairo_stroke (cr);

        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_set_line_width (cr, 2.0);
        cairo_rectangle (cr,
                         crop.x + 2.0,
                         crop.y + 2.0,
                         crop.width - 4.0,
                         crop.height - 4.0);
        cairo_stroke (cr);

        return FALSE;
}

typedef enum {
        BELOW,
        LOWER,
        BETWEEN,
        UPPER,
        ABOVE
} Range;

static Range
find_range (gint x,
            gint min,
            gint max)
{
        gint tolerance = 12;

        if (x < min - tolerance)
                return BELOW;
        if (x <= min + tolerance)
                return LOWER;
        if (x < max - tolerance)
                return BETWEEN;
        if (x <= max + tolerance)
                return UPPER;
        return ABOVE;
}

static Location
find_location (GdkRectangle *rect,
               gint          x,
               gint          y)
{
        Range x_range, y_range;
        Location location[5][5] = {
                { OUTSIDE, OUTSIDE,     OUTSIDE, OUTSIDE,      OUTSIDE },
                { OUTSIDE, TOP_LEFT,    TOP,     TOP_RIGHT,    OUTSIDE },
                { OUTSIDE, LEFT,        INSIDE,  RIGHT,        OUTSIDE },
                { OUTSIDE, BOTTOM_LEFT, BOTTOM,  BOTTOM_RIGHT, OUTSIDE },
                { OUTSIDE, OUTSIDE,     OUTSIDE, OUTSIDE,      OUTSIDE }
        };

        x_range = find_range (x, rect->x, rect->x + rect->width);
        y_range = find_range (y, rect->y, rect->y + rect->height);

        return location[y_range][x_range];
}

static void
update_cursor (CcCropArea *area,
               gint           x,
               gint           y)
{
        gint cursor_type;
        GdkRectangle crop;
        gint region;

        region = area->active_region;
        if (region == OUTSIDE) {
                crop_to_widget (area, &crop);
                region = find_location (&crop, x, y);
        }

        switch (region) {
        case OUTSIDE:
                cursor_type = GDK_LEFT_PTR;
                break;
        case TOP_LEFT:
                cursor_type = GDK_TOP_LEFT_CORNER;
                break;
        case TOP:
                cursor_type = GDK_TOP_SIDE;
                break;
        case TOP_RIGHT:
                cursor_type = GDK_TOP_RIGHT_CORNER;
                break;
        case LEFT:
                cursor_type = GDK_LEFT_SIDE;
                break;
        case INSIDE:
                cursor_type = GDK_FLEUR;
                break;
        case RIGHT:
                cursor_type = GDK_RIGHT_SIDE;
                break;
        case BOTTOM_LEFT:
                cursor_type = GDK_BOTTOM_LEFT_CORNER;
                break;
        case BOTTOM:
                cursor_type = GDK_BOTTOM_SIDE;
                break;
        case BOTTOM_RIGHT:
                cursor_type = GDK_BOTTOM_RIGHT_CORNER;
                break;
	default:
		g_assert_not_reached ();
        }

        if (cursor_type != area->current_cursor) {
                GdkCursor *cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (area)),
                                                                cursor_type);
                gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (area)), cursor);
                g_object_unref (cursor);
                area->current_cursor = cursor_type;
        }
}

static int
eval_radial_line (gdouble center_x, gdouble center_y,
                  gdouble bounds_x, gdouble bounds_y,
                  gdouble user_x)
{
        gdouble decision_slope;
        gdouble decision_intercept;

        decision_slope = (bounds_y - center_y) / (bounds_x - center_x);
        decision_intercept = -(decision_slope * bounds_x);

        return (int) (decision_slope * user_x + decision_intercept);
}

static gboolean
cc_crop_area_motion_notify_event (GtkWidget      *widget,
                                  GdkEventMotion *event)
{
        CcCropArea *area = CC_CROP_AREA (widget);
        gint x, y;
        gint delta_x, delta_y;
        gint width, height;
        gint adj_width, adj_height;
        gint pb_width, pb_height;
        GdkRectangle damage;
        gint left, right, top, bottom;
        gdouble new_width, new_height;
        gdouble center_x, center_y;
        gint min_width, min_height;

        if (area->browse_pixbuf == NULL)
                return FALSE;

        update_cursor (area, event->x, event->y);

        crop_to_widget (area, &damage);
        gtk_widget_queue_draw_area (widget,
                                    damage.x - 1, damage.y - 1,
                                    damage.width + 2, damage.height + 2);

        pb_width = gdk_pixbuf_get_width (area->browse_pixbuf);
        pb_height = gdk_pixbuf_get_height (area->browse_pixbuf);

        x = (event->x - area->image.x) / area->scale;
        y = (event->y - area->image.y) / area->scale;

        delta_x = x - area->last_press_x;
        delta_y = y - area->last_press_y;
        area->last_press_x = x;
        area->last_press_y = y;

        left = area->crop.x;
        right = area->crop.x + area->crop.width - 1;
        top = area->crop.y;
        bottom = area->crop.y + area->crop.height - 1;

        center_x = (left + right) / 2.0;
        center_y = (top + bottom) / 2.0;

        switch (area->active_region) {
        case INSIDE:
                width = right - left + 1;
                height = bottom - top + 1;

                left += delta_x;
                right += delta_x;
                top += delta_y;
                bottom += delta_y;

                if (left < 0)
                        left = 0;
                if (top < 0)
                        top = 0;
                if (right > pb_width)
                        right = pb_width;
                if (bottom > pb_height)
                        bottom = pb_height;

                adj_width = right - left + 1;
                adj_height = bottom - top + 1;
                if (adj_width != width) {
                        if (delta_x < 0)
                                right = left + width - 1;
                        else
                                left = right - width + 1;
                }
                if (adj_height != height) {
                        if (delta_y < 0)
                                bottom = top + height - 1;
                        else
                                top = bottom - height + 1;
                }

                break;

        case TOP_LEFT:
                if (area->aspect < 0) {
                        top = y;
                        left = x;
                }
                else if (y < eval_radial_line (center_x, center_y, left, top, x)) {
                        top = y;
                        new_width = (bottom - top) * area->aspect;
                        left = right - new_width;
                }
                else {
                        left = x;
                        new_height = (right - left) / area->aspect;
                        top = bottom - new_height;
                }
                break;

        case TOP:
                top = y;
                if (area->aspect > 0) {
                        new_width = (bottom - top) * area->aspect;
                        right = left + new_width;
                }
                break;

        case TOP_RIGHT:
                if (area->aspect < 0) {
                        top = y;
                        right = x;
                }
                else if (y < eval_radial_line (center_x, center_y, right, top, x)) {
                        top = y;
                        new_width = (bottom - top) * area->aspect;
                        right = left + new_width;
                }
                else {
                        right = x;
                        new_height = (right - left) / area->aspect;
                        top = bottom - new_height;
                }
                break;

        case LEFT:
                left = x;
                if (area->aspect > 0) {
                        new_height = (right - left) / area->aspect;
                        bottom = top + new_height;
                }
                break;

        case BOTTOM_LEFT:
                if (area->aspect < 0) {
                        bottom = y;
                        left = x;
                }
                else if (y < eval_radial_line (center_x, center_y, left, bottom, x)) {
                        left = x;
                        new_height = (right - left) / area->aspect;
                        bottom = top + new_height;
                }
                else {
                        bottom = y;
                        new_width = (bottom - top) * area->aspect;
                        left = right - new_width;
                }
                break;

        case RIGHT:
                right = x;
                if (area->aspect > 0) {
                        new_height = (right - left) / area->aspect;
                        bottom = top + new_height;
                }
                break;

        case BOTTOM_RIGHT:
                if (area->aspect < 0) {
                        bottom = y;
                        right = x;
                }
                else if (y < eval_radial_line (center_x, center_y, right, bottom, x)) {
                        right = x;
                        new_height = (right - left) / area->aspect;
                        bottom = top + new_height;
                }
                else {
                        bottom = y;
                        new_width = (bottom - top) * area->aspect;
                        right = left + new_width;
                }
                break;

        case BOTTOM:
                bottom = y;
                if (area->aspect > 0) {
                        new_width = (bottom - top) * area->aspect;
                        right= left + new_width;
                }
                break;

        default:
                return FALSE;
        }

        min_width = area->base_width / area->scale;
        min_height = area->base_height / area->scale;

        width = right - left + 1;
        height = bottom - top + 1;
        if (area->aspect < 0) {
                if (left < 0)
                        left = 0;
                if (top < 0)
                        top = 0;
                if (right > pb_width)
                        right = pb_width;
                if (bottom > pb_height)
                        bottom = pb_height;

                width = right - left + 1;
                height = bottom - top + 1;

                switch (area->active_region) {
                case LEFT:
                case TOP_LEFT:
                case BOTTOM_LEFT:
                        if (width < min_width)
                                left = right - min_width;
                        break;
                case RIGHT:
                case TOP_RIGHT:
                case BOTTOM_RIGHT:
                        if (width < min_width)
                                right = left + min_width;
                        break;

                default: ;
                }

                switch (area->active_region) {
                case TOP:
                case TOP_LEFT:
                case TOP_RIGHT:
                        if (height < min_height)
                                top = bottom - min_height;
                        break;
                case BOTTOM:
                case BOTTOM_LEFT:
                case BOTTOM_RIGHT:
                        if (height < min_height)
                                bottom = top + min_height;
                        break;

                default: ;
                }
        }
        else {
                if (left < 0 || top < 0 ||
                    right > pb_width || bottom > pb_height ||
                    width < min_width || height < min_height) {
                        left = area->crop.x;
                        right = area->crop.x + area->crop.width - 1;
                        top = area->crop.y;
                        bottom = area->crop.y + area->crop.height - 1;
                }
        }

        area->crop.x = left;
        area->crop.y = top;
        area->crop.width = right - left + 1;
        area->crop.height = bottom - top + 1;

        crop_to_widget (area, &damage);
        gtk_widget_queue_draw_area (widget,
                                    damage.x - 1, damage.y - 1,
                                    damage.width + 2, damage.height + 2);

        return FALSE;
}

static gboolean
cc_crop_area_button_press_event (GtkWidget      *widget,
                                 GdkEventButton *event)
{
        CcCropArea *area = CC_CROP_AREA (widget);
        GdkRectangle crop;

        if (area->browse_pixbuf == NULL)
                return FALSE;

        crop_to_widget (area, &crop);

        area->last_press_x = (event->x - area->image.x) / area->scale;
        area->last_press_y = (event->y - area->image.y) / area->scale;
        area->active_region = find_location (&crop, event->x, event->y);

        gtk_widget_queue_draw_area (widget,
                                    crop.x - 1, crop.y - 1,
                                    crop.width + 2, crop.height + 2);

        return FALSE;
}

static gboolean
cc_crop_area_button_release_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
        CcCropArea *area = CC_CROP_AREA (widget);
        GdkRectangle crop;

        if (area->browse_pixbuf == NULL)
                return FALSE;

        crop_to_widget (area, &crop);

        area->last_press_x = -1;
        area->last_press_y = -1;
        area->active_region = OUTSIDE;

        gtk_widget_queue_draw_area (widget,
                                    crop.x - 1, crop.y - 1,
                                    crop.width + 2, crop.height + 2);

        return FALSE;
}

static void
cc_crop_area_set_size_request (CcCropArea *area)
{
        gtk_widget_set_size_request (GTK_WIDGET (area),
                                     area->base_width,
                                     area->base_height);
}

static void
cc_crop_area_finalize (GObject *object)
{
        CcCropArea *area = CC_CROP_AREA (object);

        g_clear_object (&area->browse_pixbuf);
        g_clear_object (&area->pixbuf);
        g_clear_object (&area->color_shifted);

        G_OBJECT_CLASS (cc_crop_area_parent_class)->finalize (object);
}

static void
cc_crop_area_class_init (CcCropAreaClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = cc_crop_area_finalize;
        widget_class->draw = cc_crop_area_draw;
        widget_class->button_press_event = cc_crop_area_button_press_event;
        widget_class->button_release_event = cc_crop_area_button_release_event;
        widget_class->motion_notify_event = cc_crop_area_motion_notify_event;
}

static void
cc_crop_area_init (CcCropArea *area)
{
        gtk_widget_add_events (GTK_WIDGET (area), GDK_POINTER_MOTION_MASK |
                               GDK_BUTTON_PRESS_MASK |
                               GDK_BUTTON_RELEASE_MASK);

        area->scale = 0.0;
        area->image.x = 0;
        area->image.y = 0;
        area->image.width = 0;
        area->image.height = 0;
        area->active_region = OUTSIDE;
        area->base_width = 48;
        area->base_height = 48;
        area->aspect = 1;

        cc_crop_area_set_size_request (area);
}

GtkWidget *
cc_crop_area_new (void)
{
        return g_object_new (CC_TYPE_CROP_AREA, NULL);
}

GdkPixbuf *
cc_crop_area_get_picture (CcCropArea *area)
{
        gint width, height;

        width = gdk_pixbuf_get_width (area->browse_pixbuf);
        height = gdk_pixbuf_get_height (area->browse_pixbuf);
        width = MIN (area->crop.width, width - area->crop.x);
        height = MIN (area->crop.height, height - area->crop.y);

        return gdk_pixbuf_new_subpixbuf (area->browse_pixbuf,
                                         area->crop.x,
                                         area->crop.y,
                                         width, height);
}

void
cc_crop_area_set_picture (CcCropArea *area,
                          GdkPixbuf  *pixbuf)
{
        int width;
        int height;

        if (area->browse_pixbuf) {
                g_object_unref (area->browse_pixbuf);
                area->browse_pixbuf = NULL;
        }
        if (pixbuf) {
                area->browse_pixbuf = g_object_ref (pixbuf);
                width = gdk_pixbuf_get_width (pixbuf);
                height = gdk_pixbuf_get_height (pixbuf);
        } else {
                width = 0;
                height = 0;
        }

        area->crop.width = 2 * area->base_width;
        area->crop.height = 2 * area->base_height;
        area->crop.x = (width - area->crop.width) / 2;
        area->crop.y = (height - area->crop.height) / 2;

        area->scale = 0.0;
        area->image.x = 0;
        area->image.y = 0;
        area->image.width = 0;
        area->image.height = 0;

        gtk_widget_queue_draw (GTK_WIDGET (area));
}

void
cc_crop_area_set_min_size (CcCropArea *area,
                           gint        width,
                           gint        height)
{
        area->base_width = width;
        area->base_height = height;

        cc_crop_area_set_size_request (area);

        if (area->aspect > 0) {
                area->aspect = area->base_width / (gdouble)area->base_height;
        }
}

void
cc_crop_area_set_constrain_aspect (CcCropArea *area,
                                   gboolean    constrain)
{
        if (constrain) {
                area->aspect = area->base_width / (gdouble)area->base_height;
        }
        else {
                area->aspect = -1;
        }
}

