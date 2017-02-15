/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <colord.h>

#include "cc-night-light-widget.h"
#include "cc-display-resources.h"

struct _CcNightLightWidget {
  GtkDrawingArea   parent;
  gdouble          to;
  gdouble          from;
  gdouble          now;
  cairo_surface_t *surface_sunrise;
  cairo_surface_t *surface_sunset;
  CcNightLightWidgetMode mode;
};

G_DEFINE_TYPE (CcNightLightWidget, cc_night_light_widget, GTK_TYPE_DRAWING_AREA);

static gboolean cc_night_light_widget_draw (GtkWidget *widget, cairo_t *cr);

void
cc_night_light_widget_set_to (CcNightLightWidget *self, gdouble to)
{
  self->to = to;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
cc_night_light_widget_set_from (CcNightLightWidget *self, gdouble from)
{
  self->from = from;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
cc_night_light_widget_set_now (CcNightLightWidget *self, gdouble now)
{
  self->now = now;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
cc_night_light_widget_set_mode (CcNightLightWidget *self,
                                CcNightLightWidgetMode mode)
{
  self->mode = mode;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
cc_night_light_widget_finalize (GObject *object)
{
  CcNightLightWidget *self = CC_NIGHT_LIGHT_WIDGET (object);

  g_clear_pointer (&self->surface_sunrise, (GDestroyNotify) cairo_surface_destroy);
  g_clear_pointer (&self->surface_sunset, (GDestroyNotify) cairo_surface_destroy);

  G_OBJECT_CLASS (cc_night_light_widget_parent_class)->finalize (object);
}

static void
cc_night_light_widget_class_init (CcNightLightWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->finalize = cc_night_light_widget_finalize;
  widget_class->draw = cc_night_light_widget_draw;
}

static cairo_status_t
_png_read_func (void *closure, unsigned char *data, unsigned int length)
{
  GInputStream *stream = G_INPUT_STREAM (closure);
  gssize read;
  g_autoptr(GError) error = NULL;
  read = g_input_stream_read (stream, data, (gsize) length, NULL, &error);
  if (read < 0)
    {
      g_warning ("failed to read form stream: %s", error->message);
      return CAIRO_STATUS_READ_ERROR;
    }
  return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
read_surface_from_resource (const gchar *path)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GInputStream) stream = NULL;
  stream = g_resource_open_stream (cc_display_get_resource (), path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  if (stream == NULL)
    {
      g_error ("failed to load PNG data: %s", error->message);
      return NULL;
    }
  return cairo_image_surface_create_from_png_stream (_png_read_func, stream);
}

static void
cc_night_light_widget_init (CcNightLightWidget *self)
{
  self->to = 8;
  self->from = 16;
  self->now = 11;
  self->surface_sunrise = read_surface_from_resource ("/org/gnome/control-center/display/sunrise.png");
  self->surface_sunset = read_surface_from_resource ("/org/gnome/control-center/display/sunset.png");
}

static gboolean
is_frac_day_between (gdouble value, gdouble to, gdouble from)
{
  /* wraparound to the next day */
  if (from < to)
    from += 24;

  /* wraparound to the previous day */
  if (value < from && value < to)
    value += 24;

  /* test limits */
  return value > to && value <= from;
}


static void
rounded_rectangle (cairo_t *cr,
                   gdouble  x,
                   gdouble  y,
                   gdouble  radius,
                   gdouble  width,
                   gdouble  height)
{
  gdouble degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             x + width - radius,
             y + radius,
             radius,
             -90 * degrees,
             0 * degrees);
  cairo_arc (cr,
             x + width - radius,
             y + height - radius,
             radius,
             0 * degrees,
             90 * degrees);
  cairo_arc (cr,
             x + radius,
             y + height - radius,
             radius,
             90 * degrees,
             180 * degrees);
  cairo_arc (cr,
             x + radius,
             y + radius,
             radius,
             180 * degrees,
             270 * degrees);
  cairo_close_path (cr);
}

static gboolean
cc_night_light_widget_draw (GtkWidget *widget, cairo_t *cr)
{
  CdColorRGB color;
  CdColorRGB color_temperature;
  CdColorRGB color_unity;
  GtkAllocation rect;
  const guint arrow_sz = 5; /* px */
  const guint icon_sz = 16; /* px */
  const guint pad_upper_sz = 6; /* px */
  const guint pad_lower_sz = 4; /* px */
  guint line_x = 0; /* px */
  guint bar_sz;

  CcNightLightWidget *self = (CcNightLightWidget*) widget;
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (CC_IS_NIGHT_LIGHT_WIDGET (self), FALSE);

  cd_color_rgb_set (&color_temperature, 0.992, 0.796, 0.612);
  cd_color_rgb_set (&color_unity, 0.773, 0.862, 0.953);

  /*
   *      /
   *      |  icon_sz
   *      \
   *            <- pad_upper_sz
   *      /
   *      |  bar_sz (calculated)
   *      \
   *            <- pad_lower_sz
   *      /
   *      |  arrow_sz
   *      \
   */
  gtk_widget_get_allocation (widget, &rect);
  bar_sz = rect.height - (icon_sz + pad_upper_sz + pad_lower_sz + arrow_sz);

  /* clip to a rounded rectangle */
  cairo_save (cr);
  cairo_set_line_width (cr, 1);
  rounded_rectangle (cr, 0, icon_sz + pad_upper_sz, 6,
                     rect.width, bar_sz);
  cairo_clip (cr);

  /* draw each color line */
  gdouble subsect = 24.f / (gdouble) rect.width;
  if (gtk_widget_is_sensitive (widget))
    {
      cairo_set_line_width (cr, 1);
      for (guint x = 0; x < rect.width; x += 1)
        {
          gdouble frac_hour = subsect * x;
          if (is_frac_day_between (frac_hour, self->to - 1, self->to))
            {
              gdouble frac = 1.f - (self->to - frac_hour);
              cd_color_rgb_interpolate (&color_temperature,
                                        &color_unity,
                                        frac,
                                        &color);
            }
          else if (is_frac_day_between (frac_hour, self->from - 1, self->from))
            {
              gdouble frac = self->from - frac_hour;
              cd_color_rgb_interpolate (&color_temperature,
                                        &color_unity,
                                        frac,
                                        &color);
            }
          else if (is_frac_day_between (frac_hour, self->to, self->from))
            {
              cd_color_rgb_copy (&color_unity, &color);
            }
          else
            {
              cd_color_rgb_copy (&color_temperature, &color);
            }
          cairo_set_source_rgb (cr, color.R, color.G, color.B);
          cairo_move_to (cr, x + 0.5, icon_sz + pad_upper_sz);
          cairo_line_to (cr, x + 0.5, icon_sz + pad_upper_sz + bar_sz);
          cairo_stroke (cr);
        }
    }
  else
    {
      rounded_rectangle (cr, 0, icon_sz + pad_upper_sz, 6,
                         rect.width, bar_sz);
      cairo_set_source_rgb (cr, 0.95f, 0.95f, 0.95f);
      cairo_fill (cr);
    }

  /* apply border */
  rounded_rectangle (cr, 0, icon_sz + pad_upper_sz, 6,
                     rect.width, bar_sz);
  cairo_set_source_rgb (cr, 0.65, 0.65, 0.65);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
  cairo_restore (cr);

  /* apply arrow */
  if (gtk_widget_is_sensitive (widget))
    {
      line_x = self->now / subsect;
      cairo_move_to (cr,
                     line_x - arrow_sz + 0.5,
                     icon_sz + pad_upper_sz + bar_sz + pad_lower_sz + arrow_sz);
      cairo_line_to (cr,
                     line_x + arrow_sz + 0.5,
                     icon_sz + pad_upper_sz + bar_sz + pad_lower_sz + arrow_sz);
      cairo_line_to (cr,
                     line_x + 0.5,
                     icon_sz + pad_upper_sz + bar_sz + pad_lower_sz);
      cairo_close_path (cr);
      cairo_set_source_rgb (cr, 0.333, 0.333, 0.333);
      cairo_fill (cr);
    }

  /* draw icons */
  if (gtk_widget_is_sensitive (widget) &&
      self->mode == CC_NIGHT_LIGHT_WIDGET_MODE_AUTOMATIC)
    {
      if (self->to <= 0)
        line_x = rect.width - icon_sz;
      else
        line_x = MIN (MAX ((self->to / subsect) - (icon_sz / 2), 0), rect.width - icon_sz);
      cairo_set_source_surface (cr, self->surface_sunrise, line_x, 0);
      cairo_paint (cr);
      if (self->from <= 0)
        line_x = rect.width - icon_sz;
      else
        line_x = MIN (MAX ((self->from / subsect) - (icon_sz / 2), 0), rect.width - icon_sz);
      cairo_set_source_surface (cr, self->surface_sunset, line_x, 0);
      cairo_paint (cr);
    }

  return FALSE;
}

GtkWidget *
cc_night_light_widget_new (void)
{
  return g_object_new (CC_TYPE_NIGHT_LIGHT_WIDGET, NULL);
}

