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

  GdkTexture *orig_background;
  GdkTexture *orig_background_dim;

  GdkTexture *background;
  GdkTexture *pin;

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

static GdkTexture *
texture_from_resource (const gchar  *resource_path,
                       GError      **error)
{
  g_autofree gchar *full_path = g_strdup_printf ("resource://%s", resource_path);
  g_autoptr(GFile) file = g_file_new_for_uri (full_path);
  g_autoptr(GdkTexture) texture = gdk_texture_new_from_file (file, error);

  return g_steal_pointer (&texture);
}

static void
cc_timezone_map_dispose (GObject *object)
{
  CcTimezoneMap *self = CC_TIMEZONE_MAP (object);

  g_clear_object (&self->orig_background);
  g_clear_object (&self->orig_background_dim);
  g_clear_object (&self->background);
  g_clear_object (&self->pin);
  g_clear_pointer (&self->bubble_text, g_free);

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
cc_timezone_map_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         gint            for_size,
                         gint           *minimum,
                         gint           *natural,
                         gint           *minimum_baseline,
                         gint           *natural_baseline)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  gint size;

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      size = gdk_texture_get_width (map->orig_background);
      break;

    case GTK_ORIENTATION_VERTICAL:
      size = gdk_texture_get_height (map->orig_background);
      break;
    }

  if (minimum != NULL)
    *minimum = size;
  if (natural != NULL)
    *natural = size;
}

static void
cc_timezone_map_size_allocate (GtkWidget *widget,
                               gint       width,
                               gint       height,
                               gint       baseline)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  GdkTexture *texture;

  if (!gtk_widget_is_sensitive (widget))
    texture = map->orig_background_dim;
  else
    texture = map->orig_background;

  g_clear_object (&map->background);
  map->background = g_object_ref (texture);

  GTK_WIDGET_CLASS (cc_timezone_map_parent_class)->size_allocate (widget,
                                                                  width,
                                                                  height,
                                                                  baseline);
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
draw_text_bubble (CcTimezoneMap *map,
                  GtkSnapshot   *snapshot,
                  gint           width,
                  gint           height,
                  gdouble        pointx,
                  gdouble        pointy)
{
  static const double corner_radius = 9.0;
  static const double margin_top = 12.0;
  static const double margin_bottom = 12.0;
  static const double margin_left = 24.0;
  static const double margin_right = 24.0;

  GskRoundedRect rounded_rect;
  PangoRectangle text_rect;
  PangoLayout *layout;
  GdkRGBA rgba;
  double x;
  double y;
  double bubble_width;
  double bubble_height;

  if (!map->bubble_text)
    return;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (map), NULL);

  /* Layout the text */
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  pango_layout_set_spacing (layout, 3);
  pango_layout_set_markup (layout, map->bubble_text, -1);

  pango_layout_get_pixel_extents (layout, NULL, &text_rect);

  /* Calculate the bubble size based on the text layout size */
  bubble_width = text_rect.width + margin_left + margin_right;
  bubble_height = text_rect.height + margin_top + margin_bottom;

  if (pointx < width / 2)
    x = pointx + 25;
  else
    x = pointx - bubble_width - 25;

  y = pointy - bubble_height / 2;

  /* Make sure it fits in the visible area */
  x = CLAMP (x, 0, width - bubble_width);
  y = CLAMP (y, 0, height - bubble_height);

  gtk_snapshot_save (snapshot);

  gsk_rounded_rect_init (&rounded_rect,
                         &GRAPHENE_RECT_INIT (x, y, bubble_width, bubble_height),
                         &GRAPHENE_SIZE_INIT (corner_radius, corner_radius),
                         &GRAPHENE_SIZE_INIT (corner_radius, corner_radius),
                         &GRAPHENE_SIZE_INIT (corner_radius, corner_radius),
                         &GRAPHENE_SIZE_INIT (corner_radius, corner_radius));

  gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);

  rgba = (GdkRGBA) {
    .red = 0.2,
    .green = 0.2,
    .blue = 0.2,
    .alpha = 0.7,
  };
  gtk_snapshot_append_color (snapshot,
                             &rgba,
                             &GRAPHENE_RECT_INIT (x, y, bubble_width, bubble_height));


  rgba = (GdkRGBA) {
    .red = 1.0,
    .green = 1.0,
    .blue = 1.0,
    .alpha = 1.0,
  };
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + margin_left, y + margin_top));
  gtk_snapshot_append_layout (snapshot, layout, &rgba);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);

  g_object_unref (layout);
}

static void
cc_timezone_map_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  gdouble pointx, pointy;
  gint width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  /* paint background */
  gtk_snapshot_append_texture (snapshot,
                               map->background,
                               &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (map->location)
    {
      pointx = convert_longitude_to_x (map->location->longitude, width);
      pointy = convert_latitude_to_y (map->location->latitude, height);

      pointx = CLAMP (floor (pointx), 0, width);
      pointy = CLAMP (floor (pointy), 0, height);

      draw_text_bubble (map, snapshot, width, height, pointx, pointy);

      if (map->pin)
        {
          gtk_snapshot_append_texture (snapshot,
                                       map->pin,
                                       &GRAPHENE_RECT_INIT (pointx - PIN_HOT_POINT_X,
                                                            pointy - PIN_HOT_POINT_Y,
                                                            gdk_texture_get_width (map->pin),
                                                            gdk_texture_get_height (map->pin)));
        }
    }
}

static void
update_cursor (GtkWidget *widget)
{
  const gchar *cursor_name = NULL;

  if (!gtk_widget_get_realized (widget))
    return;

  if (gtk_widget_is_sensitive (widget))
    cursor_name = "pointer";

  gtk_widget_set_cursor_from_name (widget, cursor_name);
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

  widget_class->measure = cc_timezone_map_measure;
  widget_class->size_allocate = cc_timezone_map_size_allocate;
  widget_class->snapshot = cc_timezone_map_snapshot;
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

  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_signal_emit (map, signals[LOCATION_CHANGED], 0, map->location);
}

static gboolean
map_clicked_cb (GtkGestureClick *self,
                gint             n_press,
                gdouble          x,
                gdouble          y,
                CcTimezoneMap   *map)
{
  const GPtrArray *array;
  gint width, height;
  GList *distances = NULL;
  gint i;

  /* work out the coordinates */

  array = tz_get_locations (map->tzdb);

  width = gtk_widget_get_width (GTK_WIDGET (map));
  height = gtk_widget_get_height (GTK_WIDGET (map));

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
  GtkGesture *click_gesture;
  GError *err = NULL;

  map->orig_background = texture_from_resource (DATETIME_RESOURCE_PATH "/bg.png", &err);
  if (!map->orig_background)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->orig_background_dim = texture_from_resource (DATETIME_RESOURCE_PATH "/bg_dim.png", &err);
  if (!map->orig_background_dim)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->pin = texture_from_resource (DATETIME_RESOURCE_PATH "/pin.png", &err);
  if (!map->pin)
    {
      g_warning ("Could not load pin icon: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  map->tzdb = tz_load_db ();

  click_gesture = gtk_gesture_click_new ();
  g_signal_connect (click_gesture, "pressed", G_CALLBACK (map_clicked_cb), map);
  gtk_widget_add_controller (GTK_WIDGET (map), GTK_EVENT_CONTROLLER (click_gesture));
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
