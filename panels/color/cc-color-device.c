/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-color-common.h"
#include "cc-color-device.h"

struct _CcColorDevice
{
  AdwActionRow parent_instance;

  CdDevice    *device;
  gboolean     expanded;
  gchar       *sortable;
  GtkWidget   *widget_description;
  GtkWidget   *widget_button;
  GtkWidget   *widget_switch;
  GtkWidget   *widget_nocalib;
};

G_DEFINE_TYPE (CcColorDevice, cc_color_device, ADW_TYPE_ACTION_ROW)

enum
{
  SIGNAL_EXPANDED_CHANGED,
  SIGNAL_LAST
};

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

static void
cc_color_device_refresh (CcColorDevice *self)
{
  g_autofree gchar *title = NULL;
  g_autoptr(GPtrArray) profiles = NULL;
  g_autofree gchar *name1 = NULL;
  g_autofree gchar *name2 = NULL;

  /* add switch and expander if there are profiles, otherwise use a label */
  profiles = cd_device_get_profiles (self->device);
  if (profiles == NULL)
    return;

  title = cc_color_device_get_title (self->device);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);

  gtk_widget_set_visible (self->widget_switch, profiles->len > 0);
  gtk_widget_set_visible (self->widget_button, profiles->len > 0);
  gtk_button_set_icon_name (GTK_BUTTON (self->widget_button),
                            self->expanded ? "pan-down-symbolic" : "pan-end-symbolic");
  gtk_widget_set_visible (self->widget_nocalib, profiles->len == 0);
  gtk_widget_set_sensitive (self->widget_button, cd_device_get_enabled (self->device));
  gtk_switch_set_active (GTK_SWITCH (self->widget_switch),
                         cd_device_get_enabled (self->device));

  name1 = g_strdup_printf (_("Enable color management for %s"), title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->widget_switch),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, name1,
                                  -1);

  name2 = g_strdup_printf (_("Show color profiles for %s"), title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->widget_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, name2,
                                  -1);
}

CdDevice *
cc_color_device_get_device (CcColorDevice *self)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (self), NULL);
  return self->device;
}

const gchar *
cc_color_device_get_sortable (CcColorDevice *self)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (self), NULL);
  return self->sortable;
}

static void
cc_color_device_get_property (GObject *object, guint param_id,
                              GValue *value, GParamSpec *pspec)
{
  CcColorDevice *self = CC_COLOR_DEVICE (object);
  switch (param_id)
    {
      case PROP_DEVICE:
        g_value_set_object (value, self->device);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_device_set_property (GObject *object, guint param_id,
                              const GValue *value, GParamSpec *pspec)
{
  CcColorDevice *self = CC_COLOR_DEVICE (object);

  switch (param_id)
    {
      case PROP_DEVICE:
        self->device = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_device_finalize (GObject *object)
{
  CcColorDevice *self = CC_COLOR_DEVICE (object);

  g_free (self->sortable);
  g_object_unref (self->device);

  G_OBJECT_CLASS (cc_color_device_parent_class)->finalize (object);
}

void
cc_color_device_set_expanded (CcColorDevice *self,
                              gboolean expanded)
{
  /* same as before */
  if (self->expanded == expanded)
    return;

  /* refresh */
  self->expanded = expanded;
  g_signal_emit (self,
                 signals[SIGNAL_EXPANDED_CHANGED], 0,
                 self->expanded);
  cc_color_device_refresh (self);
}

static void
cc_color_device_notify_enable_device_cb (CcColorDevice *self)
{
  gboolean enable;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  enable = gtk_switch_get_active (GTK_SWITCH (self->widget_switch));
  g_debug ("Set %s to %i", cd_device_get_id (self->device), enable);
  ret = cd_device_set_enabled_sync (self->device,
                                    enable, NULL, &error);
  if (!ret)
    g_warning ("failed to %s to the device: %s",
               enable ? "enable" : "disable", error->message);

  /* if expanded, close */
  cc_color_device_set_expanded (self, FALSE);
}

static void
cc_color_device_changed_cb (CcColorDevice *self)
{
  cc_color_device_refresh (self);
}

static void
cc_color_device_clicked_expander_cb (CcColorDevice *self)
{
  self->expanded = !self->expanded;
  cc_color_device_refresh (self);
  g_signal_emit (self, signals[SIGNAL_EXPANDED_CHANGED], 0,
                 self->expanded);
}

static void
cc_color_device_constructed (GObject *object)
{
  CcColorDevice *self = CC_COLOR_DEVICE (object);
  g_autofree gchar *sortable_tmp = NULL;

  /* watch the device for changes */
  g_signal_connect_object (self->device, "changed",
                           G_CALLBACK (cc_color_device_changed_cb), self, G_CONNECT_SWAPPED);

  /* calculate sortable -- FIXME: we have to hack this as EggListBox
   * does not let us specify a GtkSortType:
   * https://bugzilla.gnome.org/show_bug.cgi?id=691341 */
  sortable_tmp = cc_color_device_get_sortable_base (self->device);
  self->sortable = g_strdup_printf ("%sXX", sortable_tmp);

  cc_color_device_refresh (self);

  /* watch to see if the user flicked the switch */
  g_signal_connect_object (self->widget_switch, "notify::active",
                           G_CALLBACK (cc_color_device_notify_enable_device_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
cc_color_device_class_init (CcColorDeviceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = cc_color_device_get_property;
  object_class->set_property = cc_color_device_set_property;
  object_class->constructed = cc_color_device_constructed;
  object_class->finalize = cc_color_device_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/color/cc-color-device.ui");

  gtk_widget_class_bind_template_child (widget_class, CcColorDevice, widget_nocalib);
  gtk_widget_class_bind_template_child (widget_class, CcColorDevice, widget_switch);
  gtk_widget_class_bind_template_child (widget_class, CcColorDevice, widget_button);

  gtk_widget_class_bind_template_callback (widget_class, cc_color_device_clicked_expander_cb);

  g_object_class_install_property (object_class, PROP_DEVICE,
                                   g_param_spec_object ("device", NULL,
                                                        NULL,
                                                        CD_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  signals [SIGNAL_EXPANDED_CHANGED] =
    g_signal_new ("expanded-changed",
            G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
cc_color_device_init (CcColorDevice *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
cc_color_device_new (CdDevice *device)
{
  return g_object_new (CC_TYPE_COLOR_DEVICE,
                       "device", device,
                       NULL);
}
