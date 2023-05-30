/*
 * Copyright 2021  Red Hat, Inc,
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
 * - Niels De Graef <nielsdg@redhat.com>
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
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gsk/gl/gskglrenderer.h>

#include "cc-crop-area.h"

/**
 * CcCropArea:
 *
 * A widget that shows a [iface@Gdk.Paintable] and allows the user specify a
 * cropping rectangle to effectively crop to that given area.
 */

/* Location of the cursor relative to the cropping rectangle/circle */
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

struct _CcCropArea {
    GtkWidget parent_instance;

    GdkPaintable *paintable;

    double scale; /* scale factor to go from paintable size to widget size */

    const char *current_cursor;
    Location active_region;
    double drag_offx;
    double drag_offy;

    /* In source coordinates. See get_scaled_crop() for widget coordinates */
    GdkRectangle crop;

    /* In widget coordinates */
    GdkRectangle image;
    int min_crop_width;
    int min_crop_height;
};

G_DEFINE_TYPE (CcCropArea, cc_crop_area, GTK_TYPE_WIDGET);

static void
update_image_and_crop (CcCropArea *area)
{
    GtkAllocation allocation;
    int width, height;
    int dest_width, dest_height;
    double scale;

    if (area->paintable == NULL)
        return;

    gtk_widget_get_allocation (GTK_WIDGET (area), &allocation);

    /* Get the size of the paintable */
    width = gdk_paintable_get_intrinsic_width (area->paintable);
    height = gdk_paintable_get_intrinsic_height (area->paintable);

    /* Find out the scale to convert to widget width/height */
    scale = allocation.height / (double) height;
    if (scale * width > allocation.width)
        scale = allocation.width / (double) width;

    dest_width = width * scale;
    dest_height = height * scale;

    if (area->scale == 0.0) {
        double scale_to_80, scale_to_image, crop_scale;

        /* Start with a crop area of 80% of the area, unless it's larger than min_size */
        scale_to_80 = MIN ((double) dest_width * 0.8, (double) dest_height * 0.8);
        scale_to_image = MIN ((double) area->min_crop_width, (double) area->min_crop_height);
        crop_scale = MAX (scale_to_80, scale_to_image);

        /* Divide by `scale` to get back to paintable coordinates */
        area->crop.width = crop_scale / scale;
        area->crop.height = crop_scale / scale;
        area->crop.x = (width - area->crop.width) / 2;
        area->crop.y = (height - area->crop.height) / 2;
    }

    area->scale = scale;
    area->image.x = (allocation.width - dest_width) / 2;
    area->image.y = (allocation.height - dest_height) / 2;
    area->image.width = dest_width;
    area->image.height = dest_height;
}

/* Returns area->crop in widget coordinates (vs paintable coordsinates) */
static void
get_scaled_crop (CcCropArea    *area,
               GdkRectangle  *crop)
{
    crop->x = area->image.x + area->crop.x * area->scale;
    crop->y = area->image.y + area->crop.y * area->scale;
    crop->width = area->crop.width * area->scale;
    crop->height = area->crop.height * area->scale;
}

typedef enum {
    BELOW,
    LOWER,
    BETWEEN,
    UPPER,
    ABOVE
} Range;

static Range
find_range (int x,
            int min,
            int max)
{
    int tolerance = 12;

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

/* Finds the location of (@x, @y) relative to the crop @rect */
static Location
find_location (GdkRectangle *rect,
               int           x,
               int           y)
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
               int         x,
               int         y)
{
    const char *cursor_type;
    GdkRectangle crop;
    int region;

    region = area->active_region;
    if (region == OUTSIDE) {
        get_scaled_crop (area, &crop);
        region = find_location (&crop, x, y);
    }

    switch (region) {
    case OUTSIDE:
        cursor_type = "default";
        break;
    case TOP_LEFT:
        cursor_type = "nw-resize";
        break;
    case TOP:
        cursor_type = "n-resize";
        break;
    case TOP_RIGHT:
        cursor_type = "ne-resize";
        break;
    case LEFT:
        cursor_type = "w-resize";
        break;
    case INSIDE:
        cursor_type = "move";
        break;
    case RIGHT:
        cursor_type = "e-resize";
        break;
    case BOTTOM_LEFT:
        cursor_type = "sw-resize";
        break;
    case BOTTOM:
        cursor_type = "s-resize";
        break;
    case BOTTOM_RIGHT:
        cursor_type = "se-resize";
        break;
    default:
        g_assert_not_reached ();
    }

    if (cursor_type != area->current_cursor) {
        GtkNative *native;
        g_autoptr (GdkCursor) cursor = NULL;

        native = gtk_widget_get_native (GTK_WIDGET (area));
        if (!native) {
            g_warning ("Can't adjust cursor: no GtkNative found");
            return;
        }
        cursor = gdk_cursor_new_from_name (cursor_type, NULL);
        gdk_surface_set_cursor (gtk_native_get_surface (native), cursor);
        area->current_cursor = cursor_type;
    }
}

static int
eval_radial_line (double center_x, double center_y,
                  double bounds_x, double bounds_y,
                  double user_x)
{
    double decision_slope;
    double decision_intercept;

    decision_slope = (bounds_y - center_y) / (bounds_x - center_x);
    decision_intercept = -(decision_slope * bounds_x);

    return (int) (decision_slope * user_x + decision_intercept);
}

static gboolean
on_motion (CcCropArea *area,
           double      event_x,
           double      event_y)
{
    if (area->paintable == NULL)
        return FALSE;

    update_cursor (area, event_x, event_y);

    return FALSE;
}

static void
on_leave (CcCropArea *area)
{
    if (area->paintable == NULL)
        return;

    /* Restore 'default' cursor */
    update_cursor (area, 0, 0);
}

static void
on_drag_begin (CcCropArea     *area,
               double          start_x,
               double          start_y)
{
    GdkRectangle crop;

    if (area->paintable == NULL)
        return;

    update_cursor (area, start_x, start_y);

    get_scaled_crop (area, &crop);

    area->active_region = find_location (&crop, start_x, start_y);

    area->drag_offx = 0.0;
    area->drag_offy = 0.0;
}

static void
on_drag_update (CcCropArea     *area,
                double          offset_x,
                double          offset_y,
                GtkGestureDrag *gesture)
{
    double start_x, start_y;
    int x, y, delta_x, delta_y;
    int width, height;
    int adj_width, adj_height;
    int pb_width, pb_height;
    int left, right, top, bottom;
    double new_width, new_height;
    double center_x, center_y;
    int min_width, min_height;

    pb_width = gdk_paintable_get_intrinsic_width (area->paintable);
    pb_height = gdk_paintable_get_intrinsic_height (area->paintable);

    gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

    /* Get the x, y, dx, dy in paintable coords */
    x = (start_x + offset_x - area->image.x) / area->scale;
    y = (start_y + offset_y - area->image.y) / area->scale;
    delta_x = (offset_x - area->drag_offx) / area->scale;
    delta_y = (offset_y - area->drag_offy) / area->scale;

    /* Helper variables */
    left = area->crop.x;
    right = area->crop.x + area->crop.width - 1;
    top = area->crop.y;
    bottom = area->crop.y + area->crop.height - 1;

    center_x = (left + right) / 2.0;
    center_y = (top + bottom) / 2.0;

    /* What we have to do depends on where the user started dragging */
    switch (area->active_region) {
    case INSIDE:
        width = right - left + 1;
        height = bottom - top + 1;

        left = MAX (left + delta_x, 0);
        right = MIN (right + delta_x, pb_width);
        top = MAX (top + delta_y, 0);
        bottom = MIN (bottom + delta_y, pb_height);

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
        if (y < eval_radial_line (center_x, center_y, left, top, x)) {
            top = y;
            new_width = bottom - top;
            left = right - new_width;
        } else {
            left = x;
            new_height = right - left;
            top = bottom - new_height;
        }
        break;

    case TOP:
        top = y;
        new_width = bottom - top;
        right = left + new_width;
        break;

    case TOP_RIGHT:
        if (y < eval_radial_line (center_x, center_y, right, top, x)) {
            top = y;
            new_width = bottom - top;
            right = left + new_width;
        } else {
            right = x;
            new_height = right - left;
            top = bottom - new_height;
        }
        break;

    case LEFT:
        left = x;
        new_height = right - left;
        bottom = top + new_height;
        break;

    case BOTTOM_LEFT:
        if (y < eval_radial_line (center_x, center_y, left, bottom, x)) {
            left = x;
            new_height = right - left;
            bottom = top + new_height;
        } else {
            bottom = y;
            new_width = bottom - top;
            left = right - new_width;
        }
        break;

    case RIGHT:
        right = x;
        new_height = right - left;
        bottom = top + new_height;
        break;

    case BOTTOM_RIGHT:
        if (y < eval_radial_line (center_x, center_y, right, bottom, x)) {
            right = x;
            new_height = right - left;
            bottom = top + new_height;
        } else {
            bottom = y;
            new_width = bottom - top;
            right = left + new_width;
        }
        break;

    case BOTTOM:
        bottom = y;
        new_width = bottom - top;
        right= left + new_width;
        break;

    default:
        return;
    }

    min_width = area->min_crop_width / area->scale;
    min_height = area->min_crop_height / area->scale;

    width = right - left + 1;
    height = bottom - top + 1;
    if (left < 0 || top < 0 ||
        right > pb_width || bottom > pb_height ||
        width < min_width || height < min_height) {
        left = area->crop.x;
        right = area->crop.x + area->crop.width - 1;
        top = area->crop.y;
        bottom = area->crop.y + area->crop.height - 1;
    }

    area->crop.x = left;
    area->crop.y = top;
    area->crop.width = right - left + 1;
    area->crop.height = bottom - top + 1;

    area->drag_offx = offset_x;
    area->drag_offy = offset_y;

    gtk_widget_queue_draw (GTK_WIDGET (area));
}

static void
on_drag_end (CcCropArea     *area,
             double          offset_x,
             double          offset_y)
{
    area->active_region = OUTSIDE;
    area->drag_offx = 0.0;
    area->drag_offy = 0.0;
}

static void
on_drag_cancel (CcCropArea       *area,
                GdkEventSequence *sequence)
{
    area->active_region = OUTSIDE;
    area->drag_offx = 0;
    area->drag_offy = 0;
}

static void
cc_crop_area_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
    CcCropArea *area = CC_CROP_AREA (widget);
    cairo_t *cr;
    GdkRectangle crop;

    if (area->paintable == NULL)
        return;

    update_image_and_crop (area);


    gtk_snapshot_save (snapshot);

    /* First draw the picture */
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (area->image.x, area->image.y));

    gdk_paintable_snapshot (area->paintable, snapshot, area->image.width, area->image.height);

    /* Draw the cropping UI on top with cairo */
    cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, area->image.width, area->image.height));

    get_scaled_crop (area, &crop);
    crop.x -= area->image.x;
    crop.y -= area->image.y;

    /* Draw the circle */
    cairo_save (cr);
    cairo_arc (cr, crop.x + crop.width / 2, crop.y + crop.width / 2, crop.width / 2, 0, 2 * G_PI);
    cairo_rectangle (cr, 0, 0, area->image.width, area->image.height);
    cairo_set_source_rgba (cr, 0, 0, 0, 0.4);
    cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill (cr);
    cairo_restore (cr);

    /* draw the four corners */
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_line_width (cr, 4.0);

    /* top left corner */
    cairo_move_to (cr, crop.x + 15, crop.y);
    cairo_line_to (cr, crop.x, crop.y);
    cairo_line_to (cr, crop.x, crop.y + 15);
    /* top right corner */
    cairo_move_to (cr, crop.x + crop.width - 15, crop.y);
    cairo_line_to (cr, crop.x + crop.width, crop.y);
    cairo_line_to (cr, crop.x + crop.width, crop.y + 15);
    /* bottom right corner */
    cairo_move_to (cr, crop.x + crop.width - 15, crop.y + crop.height);
    cairo_line_to (cr, crop.x + crop.width, crop.y + crop.height);
    cairo_line_to (cr, crop.x + crop.width, crop.y + crop.height - 15);
    /* bottom left corner */
    cairo_move_to (cr, crop.x + 15, crop.y + crop.height);
    cairo_line_to (cr, crop.x, crop.y + crop.height);
    cairo_line_to (cr, crop.x, crop.y + crop.height - 15);

    cairo_stroke (cr);

    gtk_snapshot_restore (snapshot);
}

static void
cc_crop_area_finalize (GObject *object)
{
    CcCropArea *area = CC_CROP_AREA (object);

    g_clear_object (&area->paintable);
}

static void
cc_crop_area_class_init (CcCropAreaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = cc_crop_area_finalize;

    widget_class->snapshot = cc_crop_area_snapshot;
}

static void
cc_crop_area_init (CcCropArea *area)
{
    GtkGesture *gesture;
    GtkEventController *controller;

    /* Add handlers for dragging */
    gesture = gtk_gesture_drag_new ();
    g_signal_connect_swapped (gesture, "drag-begin", G_CALLBACK (on_drag_begin), area);
    g_signal_connect_swapped (gesture, "drag-update", G_CALLBACK (on_drag_update), area);
    g_signal_connect_swapped (gesture, "drag-end", G_CALLBACK (on_drag_end), area);
    g_signal_connect_swapped (gesture, "cancel", G_CALLBACK (on_drag_cancel), area);
    gtk_widget_add_controller (GTK_WIDGET (area), GTK_EVENT_CONTROLLER (gesture));

    /* Add handlers for motion events */
    controller = gtk_event_controller_motion_new ();
    g_signal_connect_swapped (controller, "motion", G_CALLBACK (on_motion), area);
    g_signal_connect_swapped (controller, "leave", G_CALLBACK (on_leave), area);
    gtk_widget_add_controller (GTK_WIDGET (area), GTK_EVENT_CONTROLLER (controller));

    area->scale = 0.0;
    area->image.x = 0;
    area->image.y = 0;
    area->image.width = 0;
    area->image.height = 0;
    area->active_region = OUTSIDE;
    area->min_crop_width = 48;
    area->min_crop_height = 48;

    gtk_widget_set_size_request (GTK_WIDGET (area), 48, 48);
}

GtkWidget *
cc_crop_area_new (void)
{
    return g_object_new (CC_TYPE_CROP_AREA, NULL);
}

/**
 * cc_crop_area_create_pixbuf:
 * @area: A crop area
 *
 * Renders the area's paintable, with the cropping applied by the user, into a
 * GdkPixbuf.
 *
 * Returns: (transfer full): The cropped picture
 */
GdkPixbuf *
cc_crop_area_create_pixbuf (CcCropArea *area)
{
    g_autoptr (GtkSnapshot) snapshot = NULL;
    g_autoptr (GskRenderNode) node = NULL;
    g_autoptr (GskRenderer) renderer = NULL;
    g_autoptr (GdkTexture) texture = NULL;
    g_autoptr (GError) error = NULL;
    graphene_rect_t viewport;

    g_return_val_if_fail (CC_IS_CROP_AREA (area), NULL);

    snapshot = gtk_snapshot_new ();
    gdk_paintable_snapshot (area->paintable, snapshot,
                            gdk_paintable_get_intrinsic_width (area->paintable),
                            gdk_paintable_get_intrinsic_height (area->paintable));
    node = gtk_snapshot_free_to_node (g_steal_pointer (&snapshot));

    renderer = gsk_gl_renderer_new ();
    if (!gsk_renderer_realize (renderer, NULL, &error)) {
        g_warning ("Couldn't realize GL renderer: %s", error->message);
        return NULL;
    }
    viewport = GRAPHENE_RECT_INIT (area->crop.x, area->crop.y,
                                   area->crop.width, area->crop.height);
    texture = gsk_renderer_render_texture (renderer, node, &viewport);
    gsk_renderer_unrealize (renderer);

    return gdk_pixbuf_get_from_texture (texture);
}

/**
 * cc_crop_area_get_paintable:
 * @area: A crop area
 *
 * Returns the area's paintable, unmodified.
 *
 * Returns: (transfer none) (nullable): The paintable which the user can crop
 */
GdkPaintable *
cc_crop_area_get_paintable (CcCropArea *area)
{
    g_return_val_if_fail (CC_IS_CROP_AREA (area), NULL);

    return area->paintable;
}

void
cc_crop_area_set_paintable (CcCropArea   *area,
                            GdkPaintable *paintable)
{
    g_return_if_fail (CC_IS_CROP_AREA (area));
    g_return_if_fail (GDK_IS_PAINTABLE (paintable));

    g_set_object (&area->paintable, paintable);

    area->scale = 0.0;
    area->image.x = 0;
    area->image.y = 0;
    area->image.width = 0;
    area->image.height = 0;

    gtk_widget_queue_draw (GTK_WIDGET (area));
}

/**
 * cc_crop_area_set_min_size:
 * @area: A crop widget
 * @width: The minimal width
 * @height: The minimal height
 *
 * Sets the minimal size of the crop rectangle (in paintable coordinates)
 */
void
cc_crop_area_set_min_size (CcCropArea *area,
                           int         width,
                           int         height)
{
    g_return_if_fail (CC_IS_CROP_AREA (area));

    area->min_crop_width = width;
    area->min_crop_height = height;

    gtk_widget_set_size_request (GTK_WIDGET (area),
                                 area->min_crop_width,
                                 area->min_crop_height);
}
