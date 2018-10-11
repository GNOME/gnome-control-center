/*
 * Copyright (C) 2010 Intel, Inc
 *
 * Portions from Ubiquity, Copyright (C) 2009 Canonical Ltd.
 * Written by Evan Dandrea <evand@ubuntu.com>
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-timezone-map.h"
#include <math.h>
#include <string.h>
#include "tz.h"

#define PIN_HOT_POINT_X 8
#define PIN_HOT_POINT_Y 15

#define DATETIME_RESOURCE_PATH "/org/gnome/control-center/datetime"

typedef struct
{
  gdouble offset;
  guchar red;
  guchar green;
  guchar blue;
  guchar alpha;
} CcTimezoneMapOffset;

struct _CcTimezoneMap
{
  GtkWidget parent_instance;

  GdkPixbuf *orig_background;
  GdkPixbuf *orig_background_dim;
  GdkPixbuf *orig_color_map;

  GdkPixbuf *background;
  GdkPixbuf *color_map;
  GdkPixbuf *pin;

  guchar *visible_map_pixels;
  gint visible_map_rowstride;

  gdouble selected_offset;

  TzDB *tzdb;
  TzLocation *location;

  gchar *bubble_text;
};

G_DEFINE_TYPE (CcTimezoneMap, cc_timezone_map, GTK_TYPE_WIDGET)

enum
{
  LOCATION_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


static CcTimezoneMapOffset color_codes[] =
{
    {-11.0, 43, 0, 0, 255 },
    {-10.0, 85, 0, 0, 255 },
    {-9.5, 102, 255, 0, 255 },
    {-9.0, 128, 0, 0, 255 },
    {-8.0, 170, 0, 0, 255 },
    {-7.0, 212, 0, 0, 255 },
    {-6.0, 255, 0, 1, 255 }, // north
    {-6.0, 255, 0, 0, 255 }, // south
    {-5.0, 255, 42, 42, 255 },
    {-4.5, 192, 255, 0, 255 },
    {-4.0, 255, 85, 85, 255 },
    {-3.5, 0, 255, 0, 255 },
    {-3.0, 255, 128, 128, 255 },
    {-2.0, 255, 170, 170, 255 },
    {-1.0, 255, 213, 213, 255 },
    {0.0, 43, 17, 0, 255 },
    {1.0, 85, 34, 0, 255 },
    {2.0, 128, 51, 0, 255 },
    {3.0, 170, 68, 0, 255 },
    {3.5, 0, 255, 102, 255 },
    {4.0, 212, 85, 0, 255 },
    {4.5, 0, 204, 255, 255 },
    {5.0, 255, 102, 0, 255 },
    {5.5, 0, 102, 255, 255 },
    {5.75, 0, 238, 207, 247 },
    {6.0, 255, 127, 42, 255 },
    {6.5, 204, 0, 254, 254 },
    {7.0, 255, 153, 85, 255 },
    {8.0, 255, 179, 128, 255 },
    {9.0, 255, 204, 170, 255 },
    {9.5, 170, 0, 68, 250 },
    {10.0, 255, 230, 213, 255 },
    {10.5, 212, 124, 21, 250 },
    {11.0, 212, 170, 0, 255 },
    {11.5, 249, 25, 87, 253 },
    {12.0, 255, 204, 0, 255 },
    {12.75, 254, 74, 100, 248 },
    {13.0, 255, 85, 153, 250 },
    {-100, 0, 0, 0, 0 }
};


static void
cc_timezone_map_dispose (GObject *object)
{
  CcTimezoneMap *self = CC_TIMEZONE_MAP (object);

  g_clear_object (&self->orig_background);
  g_clear_object (&self->orig_background_dim);
  g_clear_object (&self->orig_color_map);
  g_clear_object (&self->background);
  g_clear_object (&self->pin);
  g_clear_pointer (&self->bubble_text, g_free);

  if (self->color_map)
    {
      g_clear_object (&self->color_map);

      self->visible_map_pixels = NULL;
      self->visible_map_rowstride = 0;
    }

  G_OBJECT_CLASS (cc_timezone_map_parent_class)->dispose (object);
}

static void
cc_timezone_map_finalize (GObject *object)
{
  CcTimezoneMap *self = CC_TIMEZONE_MAP (object);

  g_clear_pointer (&self->tzdb, tz_db_free);

  G_OBJECT_CLASS (cc_timezone_map_parent_class)->finalize (object);
}

/* GtkWidget functions */
static void
cc_timezone_map_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  gint size;

  size = gdk_pixbuf_get_width (map->orig_background);

  if (minimum != NULL)
    *minimum = size;
  if (natural != NULL)
    *natural = size;
}

static void
cc_timezone_map_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  gint size;

  size = gdk_pixbuf_get_height (map->orig_background);

  if (minimum != NULL)
    *minimum = size;
  if (natural != NULL)
    *natural = size;
}

static void
cc_timezone_map_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  GdkPixbuf *pixbuf;

  if (map->background)
    g_object_unref (map->background);

  if (!gtk_widget_is_sensitive (widget))
    pixbuf = map->orig_background_dim;
  else
    pixbuf = map->orig_background;

  map->background = gdk_pixbuf_scale_simple (pixbuf,
                                             allocation->width,
                                             allocation->height,
                                             GDK_INTERP_BILINEAR);

  if (map->color_map)
    g_object_unref (map->color_map);

  map->color_map = gdk_pixbuf_scale_simple (map->orig_color_map,
                                            allocation->width,
                                            allocation->height,
                                            GDK_INTERP_BILINEAR);

  map->visible_map_pixels = gdk_pixbuf_get_pixels (map->color_map);
  map->visible_map_rowstride = gdk_pixbuf_get_rowstride (map->color_map);

  GTK_WIDGET_CLASS (cc_timezone_map_parent_class)->size_allocate (widget,
                                                                  allocation);
}

static void
cc_timezone_map_realize (GtkWidget *widget)
{
  GdkWindowAttr attr = { 0, };
  GtkAllocation allocation;
  GdkWindow *window;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.width = allocation.width;
  attr.height = allocation.height;
  attr.x = allocation.x;
  attr.y = allocation.y;
  attr.event_mask = gtk_widget_get_events (widget)
                                 | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

  window = gdk_window_new (gtk_widget_get_parent_window (widget), &attr,
                           GDK_WA_X | GDK_WA_Y);

  gdk_window_set_user_data (window, widget);
  gtk_widget_set_window (widget, window);
}


static gdouble
convert_longitude_to_x (gdouble longitude, gint map_width)
{
  const gdouble xdeg_offset = -6;
  gdouble x;

  x = (map_width * (180.0 + longitude) / 360.0)
    + (map_width * xdeg_offset / 180.0);

  return x;
}

static gdouble
radians (gdouble degrees)
{
  return (degrees / 360.0) * G_PI * 2;
}

static gdouble
convert_latitude_to_y (gdouble latitude, gdouble map_height)
{
  gdouble bottom_lat = -59;
  gdouble top_lat = 81;
  gdouble top_per, y, full_range, top_offset, map_range;

  top_per = top_lat / 180.0;
  y = 1.25 * log (tan (G_PI_4 + 0.4 * radians (latitude)));
  full_range = 4.6068250867599998;
  top_offset = full_range * top_per;
  map_range = fabs (1.25 * log (tan (G_PI_4 + 0.4 * radians (bottom_lat))) - top_offset);
  y = fabs (y - top_offset);
  y = y / map_range;
  y = y * map_height;
  return y;
}

static void
draw_text_bubble (cairo_t *cr,
                  GtkWidget *widget,
                  gdouble pointx,
                  gdouble pointy)
{
  static const double corner_radius = 9.0;
  static const double margin_top = 12.0;
  static const double margin_bottom = 12.0;
  static const double margin_left = 24.0;
  static const double margin_right = 24.0;

  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  GtkAllocation alloc;
  PangoLayout *layout;
  PangoRectangle text_rect;
  double x;
  double y;
  double width;
  double height;

  if (!map->bubble_text)
    return;

  gtk_widget_get_allocation (widget, &alloc);
  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* Layout the text */
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  pango_layout_set_spacing (layout, 3);
  pango_layout_set_markup (layout, map->bubble_text, -1);

  pango_layout_get_pixel_extents (layout, NULL, &text_rect);

  /* Calculate the bubble size based on the text layout size */
  width = text_rect.width + margin_left + margin_right;
  height = text_rect.height + margin_top + margin_bottom;

  if (pointx < alloc.width / 2)
    x = pointx + 25;
  else
    x = pointx - width - 25;

  y = pointy - height / 2;

  /* Make sure it fits in the visible area */
  x = CLAMP (x, 0, alloc.width - width);
  y = CLAMP (y, 0, alloc.height - height);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  /* Draw the bubble */
  cairo_new_sub_path (cr);
  cairo_arc (cr, width - corner_radius, corner_radius, corner_radius, radians (-90), radians (0));
  cairo_arc (cr, width - corner_radius, height - corner_radius, corner_radius, radians (0), radians (90));
  cairo_arc (cr, corner_radius, height - corner_radius, corner_radius, radians (90), radians (180));
  cairo_arc (cr, corner_radius, corner_radius, corner_radius, radians (180), radians (270));
  cairo_close_path (cr);

  cairo_set_source_rgba (cr, 0.2, 0.2, 0.2, 0.7);
  cairo_fill (cr);

  /* And finally draw the text */
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_move_to (cr, margin_left, margin_top);
  pango_cairo_show_layout (cr, layout);

  g_object_unref (layout);
  cairo_restore (cr);
}

static gboolean
cc_timezone_map_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  g_autoptr(GdkPixbuf) orig_hilight = NULL;
  GtkAllocation alloc;
  g_autofree gchar *file = NULL;
  g_autoptr(GError) err = NULL;
  gdouble pointx, pointy;
  char buf[16];

  gtk_widget_get_allocation (widget, &alloc);

  /* paint background */
  gdk_cairo_set_source_pixbuf (cr, map->background, 0, 0);
  cairo_paint (cr);

  /* paint hilight */
  if (gtk_widget_is_sensitive (widget))
    {
      file = g_strdup_printf (DATETIME_RESOURCE_PATH "/timezone_%s.png",
                              g_ascii_formatd (buf, sizeof (buf),
                                               "%g", map->selected_offset));
    }
  else
    {
      file = g_strdup_printf (DATETIME_RESOURCE_PATH "/timezone_%s_dim.png",
                              g_ascii_formatd (buf, sizeof (buf),
                                               "%g", map->selected_offset));

    }

  orig_hilight = gdk_pixbuf_new_from_resource (file, &err);

  if (!orig_hilight)
    {
      g_warning ("Could not load hilight: %s",
                 (err) ? err->message : "Unknown Error");
    }
  else
    {
      g_autoptr(GdkPixbuf) hilight = NULL;

      hilight = gdk_pixbuf_scale_simple (orig_hilight, alloc.width,
                                         alloc.height, GDK_INTERP_BILINEAR);
      gdk_cairo_set_source_pixbuf (cr, hilight, 0, 0);

      cairo_paint (cr);
    }

  if (map->location)
    {
      pointx = convert_longitude_to_x (map->location->longitude, alloc.width);
      pointy = convert_latitude_to_y (map->location->latitude, alloc.height);

      pointx = CLAMP (floor (pointx), 0, alloc.width);
      pointy = CLAMP (floor (pointy), 0, alloc.height);

      draw_text_bubble (cr, widget, pointx, pointy);

      if (map->pin)
        {
          gdk_cairo_set_source_pixbuf (cr, map->pin,
                                       pointx - PIN_HOT_POINT_X,
                                       pointy - PIN_HOT_POINT_Y);
          cairo_paint (cr);
        }
    }

  return TRUE;
}

static void
update_cursor (GtkWidget *widget)
{
  GdkWindow *window;
  g_autoptr(GdkCursor) cursor = NULL;

  if (!gtk_widget_get_realized (widget))
    return;

  if (gtk_widget_is_sensitive (widget))
    {
      GdkDisplay *display;
      display = gtk_widget_get_display (widget);
      cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
    }

  window = gtk_widget_get_window (widget);
  gdk_window_set_cursor (window, cursor);
}

static void
cc_timezone_map_state_flags_changed (GtkWidget     *widget,
                                     GtkStateFlags  prev_state)
{
  update_cursor (widget);

  if (GTK_WIDGET_CLASS (cc_timezone_map_parent_class)->state_flags_changed)
    GTK_WIDGET_CLASS (cc_timezone_map_parent_class)->state_flags_changed (widget, prev_state);
}


static void
cc_timezone_map_class_init (CcTimezoneMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_timezone_map_dispose;
  object_class->finalize = cc_timezone_map_finalize;

  widget_class->get_preferred_width = cc_timezone_map_get_preferred_width;
  widget_class->get_preferred_height = cc_timezone_map_get_preferred_height;
  widget_class->size_allocate = cc_timezone_map_size_allocate;
  widget_class->realize = cc_timezone_map_realize;
  widget_class->draw = cc_timezone_map_draw;
  widget_class->state_flags_changed = cc_timezone_map_state_flags_changed;

  signals[LOCATION_CHANGED] = g_signal_new ("location-changed",
                                            CC_TYPE_TIMEZONE_MAP,
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__POINTER,
                                            G_TYPE_NONE, 1,
                                            G_TYPE_POINTER);
}


static gint
sort_locations (TzLocation *a,
                TzLocation *b)
{
  if (a->dist > b->dist)
    return 1;

  if (a->dist < b->dist)
    return -1;

  return 0;
}

static void
set_location (CcTimezoneMap *map,
              TzLocation    *location)
{
  g_autoptr(TzInfo) info = NULL;

  map->location = location;

  info = tz_info_from_location (map->location);

  map->selected_offset = tz_location_get_utc_offset (map->location)
    / (60.0*60.0) + ((info->daylight) ? -1.0 : 0.0);

  g_signal_emit (map, signals[LOCATION_CHANGED], 0, map->location);
}

static gboolean
button_press_event (CcTimezoneMap  *map,
                    GdkEventButton *event)
{
  gint x, y;
  guchar r, g, b, a;
  guchar *pixels;
  gint rowstride;
  gint i;

  const GPtrArray *array;
  gint width, height;
  GList *distances = NULL;
  GtkAllocation alloc;

  x = event->x;
  y = event->y;


  rowstride = map->visible_map_rowstride;
  pixels = map->visible_map_pixels;

  r = pixels[(rowstride * y + x * 4)];
  g = pixels[(rowstride * y + x * 4) + 1];
  b = pixels[(rowstride * y + x * 4) + 2];
  a = pixels[(rowstride * y + x * 4) + 3];


  for (i = 0; color_codes[i].offset != -100; i++)
    {
       if (color_codes[i].red == r && color_codes[i].green == g
           && color_codes[i].blue == b && color_codes[i].alpha == a)
         {
           map->selected_offset = color_codes[i].offset;
         }
    }

  gtk_widget_queue_draw (GTK_WIDGET (map));

  /* work out the co-ordinates */

  array = tz_get_locations (map->tzdb);

  gtk_widget_get_allocation (GTK_WIDGET (map), &alloc);
  width = alloc.width;
  height = alloc.height;

  for (i = 0; i < array->len; i++)
    {
      gdouble pointx, pointy, dx, dy;
      TzLocation *loc = array->pdata[i];

      pointx = convert_longitude_to_x (loc->longitude, width);
      pointy = convert_latitude_to_y (loc->latitude, height);

      dx = pointx - x;
      dy = pointy - y;

      loc->dist = dx * dx + dy * dy;
      distances = g_list_prepend (distances, loc);

    }
  distances = g_list_sort (distances, (GCompareFunc) sort_locations);


  set_location (map, (TzLocation*) distances->data);

  g_list_free (distances);

  return TRUE;
}

static void
cc_timezone_map_init (CcTimezoneMap *map)
{
  GError *err = NULL;

  map->orig_background = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/bg.png",
                                                       &err);

  if (!map->orig_background)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->orig_background_dim = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/bg_dim.png",
                                                           &err);

  if (!map->orig_background_dim)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->orig_color_map = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/cc.png",
                                                      &err);
  if (!map->orig_color_map)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->pin = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/pin.png",
                                           &err);
  if (!map->pin)
    {
      g_warning ("Could not load pin icon: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->tzdb = tz_load_db ();

  g_signal_connect_object (map, "button-press-event", G_CALLBACK (button_press_event), map, G_CONNECT_SWAPPED);
}

CcTimezoneMap *
cc_timezone_map_new (void)
{
  return g_object_new (CC_TYPE_TIMEZONE_MAP, NULL);
}

gboolean
cc_timezone_map_set_timezone (CcTimezoneMap *map,
                              const gchar   *timezone)
{
  GPtrArray *locations;
  guint i;
  g_autofree gchar *real_tz = NULL;
  gboolean ret;

  real_tz = tz_info_get_clean_name (map->tzdb, timezone);

  locations = tz_get_locations (map->tzdb);
  ret = FALSE;

  for (i = 0; i < locations->len; i++)
    {
      TzLocation *loc = locations->pdata[i];

      if (!g_strcmp0 (loc->zone, real_tz ? real_tz : timezone))
        {
          set_location (map, loc);
          ret = TRUE;
          break;
        }
    }

  if (ret)
    gtk_widget_queue_draw (GTK_WIDGET (map));

  return ret;
}

void
cc_timezone_map_set_bubble_text (CcTimezoneMap *map,
                                 const gchar   *text)
{
  g_free (map->bubble_text);
  map->bubble_text = g_strdup (text);

  gtk_widget_queue_draw (GTK_WIDGET (map));
}

TzLocation *
cc_timezone_map_get_location (CcTimezoneMap *map)
{
  return map->location;
}
