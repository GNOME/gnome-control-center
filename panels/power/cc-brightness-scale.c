/* cc-brightness-scale.c
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010,2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 System76, Inc.
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-brightness-scale.h"
#include "shell/cc-object-storage.h"

struct _CcBrightnessScale {
  GtkScale         parent_instance;

  GCancellable    *cancellable;
  BrightnessDevice device;
  gboolean         has_brightness;
  GDBusProxy      *proxy;
  gboolean         setting_brightness;
};

enum
{
  PROP_0,
  PROP_HAS_BRIGHTNESS,
  PROP_DEVICE,
};

G_DEFINE_TYPE (CcBrightnessScale, cc_brightness_scale, GTK_TYPE_SCALE)

static void
set_brightness_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) result = NULL;
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);

  result = g_dbus_proxy_call_finish (proxy, res, &error);
  if (result == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error setting brightness: %s\n", error->message);
      return;
    }

  CcBrightnessScale *self = CC_BRIGHTNESS_SCALE (user_data);

  /* not setting, so pay attention to changed signals */
  self->setting_brightness = FALSE;
}

static void
brightness_slider_value_changed_cb (CcBrightnessScale *self, GtkRange *range)
{
  guint percentage;
  g_autoptr(GVariant) variant = NULL;

  percentage = (guint) gtk_range_get_value (range);

  /* do not loop */
  if (self->setting_brightness)
    return;

  self->setting_brightness = TRUE;

  if (self->device == BRIGHTNESS_DEVICE_KBD)
    variant = g_variant_new_parsed ("('org.gnome.SettingsDaemon.Power.Keyboard',"
                                    "'Brightness', %v)",
                                    g_variant_new_int32 (percentage));
  else
    variant = g_variant_new_parsed ("('org.gnome.SettingsDaemon.Power.Screen',"
                                    "'Brightness', %v)",
                                    g_variant_new_int32 (percentage));

  /* push this to g-s-d */
  g_dbus_proxy_call (self->proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_ref_sink (variant),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     set_brightness_cb,
                     self);
}

static void
sync_brightness (CcBrightnessScale *self)
{
  g_autoptr(GVariant) result = NULL;
  gint brightness;
  GtkRange *range;

  result = g_dbus_proxy_get_cached_property (self->proxy, "Brightness");

  if (result)
    {
      /* set the slider */
      brightness = g_variant_get_int32 (result);
      self->has_brightness = brightness >= 0.0;
    }
  else
    {
      self->has_brightness = FALSE;
    }

  g_object_notify (G_OBJECT (self), "has-brightness");

  if (self->has_brightness)
    {
      range = GTK_RANGE (self);
      self->setting_brightness = TRUE;
      gtk_range_set_value (range, brightness);
      self->setting_brightness = FALSE;
    }
}

static void
got_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  CcBrightnessScale *self;
  GDBusProxy *proxy;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating proxy: %s\n", error->message);
      return;
    }

  self = CC_BRIGHTNESS_SCALE (user_data);
  self->proxy = proxy;

  g_signal_connect_object (proxy, "g-properties-changed",
                           G_CALLBACK (sync_brightness), self, G_CONNECT_SWAPPED);

  sync_brightness (self);
}

static void
cc_brightness_scale_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CcBrightnessScale *self;

  self = CC_BRIGHTNESS_SCALE (object);

  switch (prop_id) {
  case PROP_HAS_BRIGHTNESS:
    g_value_set_boolean (value, self->has_brightness);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_brightness_scale_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcBrightnessScale *self;

  self = CC_BRIGHTNESS_SCALE (object);

  switch (prop_id) {
  case PROP_DEVICE:
    self->device = g_value_get_enum (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_brightness_scale_constructed (GObject *object)
{
  CcBrightnessScale *self;
  const gchar *interface;

  G_OBJECT_CLASS (cc_brightness_scale_parent_class)->constructed (object);

  self = CC_BRIGHTNESS_SCALE (object);

  self->cancellable = g_cancellable_new();

  g_signal_connect_object (GTK_SCALE (self), "value-changed",
                           G_CALLBACK (brightness_slider_value_changed_cb), self, G_CONNECT_SWAPPED);

  if (self->device == BRIGHTNESS_DEVICE_KBD)
    interface = "org.gnome.SettingsDaemon.Power.Keyboard";
  else
    interface = "org.gnome.SettingsDaemon.Power.Screen";

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Power",
                                       "/org/gnome/SettingsDaemon/Power",
                                       interface,
                                       self->cancellable,
                                       got_proxy_cb,
                                       self);

  gtk_range_set_range (GTK_RANGE (self), 0, 100);
  gtk_range_set_increments (GTK_RANGE (self), 1, 10);
  gtk_range_set_round_digits (GTK_RANGE (self), 0);
  gtk_scale_set_draw_value (GTK_SCALE (self), FALSE);
}

static void
cc_brightness_scale_finalize (GObject *object)
{
  CcBrightnessScale *self = CC_BRIGHTNESS_SCALE (object);

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (cc_brightness_scale_parent_class)->finalize (object);
}

void
cc_brightness_scale_class_init (CcBrightnessScaleClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_brightness_scale_get_property;
  object_class->set_property = cc_brightness_scale_set_property;
  object_class->constructed = cc_brightness_scale_constructed;
  object_class->finalize = cc_brightness_scale_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DEVICE,
                                   g_param_spec_enum ("device",
                                                      "device",
                                                      "device",
                                                      brightness_device_get_type(),
                                                      BRIGHTNESS_DEVICE_SCREEN,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_HAS_BRIGHTNESS,
                                   g_param_spec_boolean ("has-brightness",
                                                         "has brightness",
                                                         "has brightness",
                                                         FALSE,
                                                         G_PARAM_READABLE));
}

static void
cc_brightness_scale_init (CcBrightnessScale *self)
{
}


gboolean
cc_brightness_scale_get_has_brightness (CcBrightnessScale *self)
{
  g_return_val_if_fail (CC_IS_BRIGHTNESS_SCALE (self), FALSE);

  return self->has_brightness;
}
