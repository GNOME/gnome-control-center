/* cc-background-preview.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "cc-background-preview.h"

struct _CcBackgroundPreview
{
  GtkWidget         parent;

  GtkImage         *animated_background_icon;
  GtkLabel         *desktop_clock_label;
  GtkFrame         *desktop_frame;
  GtkDrawingArea   *drawing_area;
  GtkFrame         *lock_frame;
  GtkLabel         *lock_screen_label;
  GtkWidget        *overlay;
  GtkStack         *stack;

  GnomeDesktopThumbnailFactory *thumbnail_factory;

  CcBackgroundItem *item;
  GSettings        *desktop_settings;

  guint             lock_screen_time_timeout_id;
  gboolean          is_lock_screen;
  GDateTime        *previous_time;
  gboolean          is_24h_format;
};

G_DEFINE_TYPE (CcBackgroundPreview, cc_background_preview, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_IS_LOCK_SCREEN,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/* Auxiliary methods */

static void
update_lock_screen_label (CcBackgroundPreview *self,
                          gboolean             force)
{
  g_autoptr(GDateTime) now = NULL;
  g_autofree gchar *label = NULL;

  now = g_date_time_new_now_local ();

  /* Don't update the label if the hour:minute pair did not change */
  if (!force && self->previous_time &&
      g_date_time_get_hour (now) == g_date_time_get_hour (self->previous_time) &&
      g_date_time_get_minute (now) == g_date_time_get_minute (self->previous_time))
    {
      return;
    }

  if (self->is_24h_format)
    label = g_date_time_format (now, "%R");
  else
    label = g_date_time_format (now, "%I:%M %p");

  gtk_label_set_label (self->lock_screen_label, label);
  gtk_label_set_label (self->desktop_clock_label, label);

  g_clear_pointer (&self->previous_time, g_date_time_unref);
  self->previous_time = g_steal_pointer (&now);
}

static void
update_clock_format (CcBackgroundPreview *self)
{
  g_autofree gchar *clock_format = NULL;
  gboolean is_24h_format;

  clock_format = g_settings_get_string (self->desktop_settings, "clock-format");
  is_24h_format = g_strcmp0 (clock_format, "24h") == 0;

  if (is_24h_format != self->is_24h_format)
    {
      self->is_24h_format = is_24h_format;
      update_lock_screen_label (self, TRUE);
    }
}

static void
load_custom_css (CcBackgroundPreview *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  /* use custom CSS */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/background/preview.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

}

static gboolean
update_clock_cb (gpointer data)
{
  CcBackgroundPreview *self = data;

  update_lock_screen_label (self, FALSE);

  return G_SOURCE_CONTINUE;
}

static void
start_monitor_time (CcBackgroundPreview *self)
{
  if (self->lock_screen_time_timeout_id > 0)
    return;

  self->lock_screen_time_timeout_id = g_timeout_add_seconds (1,
                                                             update_clock_cb,
                                                             self);
}

static void
stop_monitor_time (CcBackgroundPreview *self)
{
  if (self->lock_screen_time_timeout_id > 0)
    {
      g_source_remove (self->lock_screen_time_timeout_id);
      self->lock_screen_time_timeout_id = 0;
    }
}


/* Callbacks */

static void
draw_preview_func (GtkDrawingArea *drawing_area,
                   cairo_t        *cr,
                   gint            width,
                   gint            height,
                   gpointer        user_data)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (user_data);
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  gint scale_factor;

  if (!self->item)
    return;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (drawing_area));
  pixbuf = cc_background_item_get_frame_thumbnail (self->item,
                                                   self->thumbnail_factory,
                                                   width,
                                                   height,
                                                   scale_factor,
                                                   0,
                                                   TRUE);


  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
}

/* GObject overrides */

static void
cc_background_preview_dispose (GObject *object)
{
  CcBackgroundPreview *self = (CcBackgroundPreview *)object;

  g_clear_pointer (&self->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (cc_background_preview_parent_class)->dispose (object);
}

static void
cc_background_preview_finalize (GObject *object)
{
  CcBackgroundPreview *self = (CcBackgroundPreview *)object;

  g_clear_object (&self->desktop_settings);
  g_clear_object (&self->item);
  g_clear_object (&self->thumbnail_factory);

  g_clear_pointer (&self->previous_time, g_date_time_unref);

  stop_monitor_time (self);

  G_OBJECT_CLASS (cc_background_preview_parent_class)->finalize (object);
}

static void
cc_background_preview_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IS_LOCK_SCREEN:
      g_value_set_boolean (value, self->is_lock_screen);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_preview_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IS_LOCK_SCREEN:
      self->is_lock_screen = g_value_get_boolean (value);
      gtk_stack_set_visible_child (self->stack,
                                   self->is_lock_screen ? GTK_WIDGET (self->lock_frame) : GTK_WIDGET (self->desktop_frame));
      break;

    case PROP_ITEM:
      cc_background_preview_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
cc_background_preview_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static gfloat
get_primary_monitor_aspect_ratio (void)
{
  GdkDisplay *display;
  GListModel *monitors;
  gfloat aspect_ratio;

  display = gdk_display_get_default ();
  aspect_ratio = 16.0 / 9.0;

  monitors = gdk_display_get_monitors (display);
  if (monitors)
    {
      g_autoptr(GdkMonitor) primary_monitor = NULL;
      GdkRectangle monitor_layout;

      primary_monitor = g_list_model_get_item (monitors, 0);
      gdk_monitor_get_geometry (primary_monitor, &monitor_layout);
      aspect_ratio = monitor_layout.width / (gfloat) monitor_layout.height;
    }

  return aspect_ratio;
}

static void
cc_background_preview_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               gint            for_size,
                               gint           *minimum,
                               gint           *natural,
                               gint           *minimum_baseline,
                               gint           *natural_baseline)
{
  CcBackgroundPreview *self = (CcBackgroundPreview *)widget;
  gint child_min, child_nat;
  gfloat aspect_ratio;

  aspect_ratio = get_primary_monitor_aspect_ratio ();

  gtk_widget_measure (self->overlay,
                      orientation,
                      for_size,
                      &child_min,
                      &child_nat,
                      NULL, NULL);

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      *minimum = MAX (2, child_min * aspect_ratio);
      *natural = MAX (2, child_nat * aspect_ratio);
      break;

    case GTK_ORIENTATION_VERTICAL:
      *minimum = MAX (2, for_size / aspect_ratio);
      *natural = MAX (2, for_size / aspect_ratio);
      break;
    }
}

static void
cc_background_preview_size_allocate (GtkWidget *widget,
                                     gint       width,
                                     gint       height,
                                     gint       baseline)
{
  CcBackgroundPreview *self = CC_BACKGROUND_PREVIEW (widget);

  gtk_widget_allocate (self->overlay, width, height, baseline, NULL);
}

static void
cc_background_preview_class_init (CcBackgroundPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_background_preview_dispose;
  object_class->finalize = cc_background_preview_finalize;
  object_class->get_property = cc_background_preview_get_property;
  object_class->set_property = cc_background_preview_set_property;

  widget_class->get_request_mode = cc_background_preview_get_request_mode;
  widget_class->measure = cc_background_preview_measure;
  widget_class->size_allocate = cc_background_preview_size_allocate;

  properties[PROP_IS_LOCK_SCREEN] = g_param_spec_boolean ("is-lock-screen",
                                                          "Lock screen",
                                                          "Whether the preview is of the lock screen",
                                                          FALSE,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ITEM] = g_param_spec_object ("item",
                                               "Item",
                                               "Background item",
                                               CC_TYPE_BACKGROUND_ITEM,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-preview.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, animated_background_icon);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, desktop_clock_label);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, desktop_frame);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, drawing_area);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, lock_frame);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, lock_screen_label);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, overlay);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPreview, stack);
}

static void
cc_background_preview_init (CcBackgroundPreview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  self->desktop_settings = g_settings_new ("org.gnome.desktop.interface");

  g_signal_connect_object (self->desktop_settings,
                           "changed::clock-format",
                           G_CALLBACK (update_clock_format),
                           self,
                           G_CONNECT_SWAPPED);

  update_clock_format (self);
  load_custom_css (self);
  start_monitor_time (self);
}

CcBackgroundItem*
cc_background_preview_get_item (CcBackgroundPreview *self)
{
  g_return_val_if_fail (CC_IS_BACKGROUND_PREVIEW (self), NULL);

  return self->item;
}

void
cc_background_preview_set_item (CcBackgroundPreview *self,
                                CcBackgroundItem    *item)
{
  g_return_if_fail (CC_IS_BACKGROUND_PREVIEW (self));
  g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

  if (!g_set_object (&self->item, item))
    return;

  gtk_widget_set_visible (GTK_WIDGET (self->animated_background_icon),
                          cc_background_item_changes_with_time (item));

  gtk_drawing_area_set_draw_func (self->drawing_area, draw_preview_func, self, NULL);
  gtk_widget_queue_draw (GTK_WIDGET (self->drawing_area));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}
