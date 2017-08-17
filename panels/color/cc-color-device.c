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
#include <gtk/gtk.h>

#include "cc-color-common.h"
#include "cc-color-device.h"

struct _CcColorDevicePrivate
{
  CdDevice    *device;
  gboolean     expanded;
  gchar       *sortable;
  GtkWidget   *widget_description;
  GtkWidget   *widget_button;
  GtkWidget   *widget_switch;
  GtkWidget   *widget_arrow;
  GtkWidget   *widget_nocalib;
  guint        device_changed_id;
};

G_DEFINE_TYPE (CcColorDevice, cc_color_device, GTK_TYPE_LIST_BOX_ROW)

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
cc_color_device_refresh (CcColorDevice *color_device)
{
  CcColorDevicePrivate *priv = color_device->priv;
  gchar *title = NULL;
  GPtrArray *profiles = NULL;
  AtkObject *accessible;
  gchar *name = NULL;

  /* add switch and expander if there are profiles, otherwise use a label */
  profiles = cd_device_get_profiles (priv->device);
  if (profiles == NULL)
    goto out;

  title = cc_color_device_get_title (priv->device);
  gtk_label_set_label (GTK_LABEL (priv->widget_description), title);
  gtk_widget_set_visible (priv->widget_description, TRUE);

  gtk_widget_set_visible (priv->widget_switch, profiles->len > 0);
  gtk_widget_set_visible (priv->widget_button, profiles->len > 0);
  gtk_image_set_from_icon_name (GTK_IMAGE (priv->widget_arrow),
                                priv->expanded ? "pan-down-symbolic" : "pan-end-symbolic",
                                GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_visible (priv->widget_nocalib, profiles->len == 0);
  gtk_widget_set_sensitive (priv->widget_button, cd_device_get_enabled (priv->device));
  gtk_switch_set_active (GTK_SWITCH (priv->widget_switch),
                         cd_device_get_enabled (priv->device));

  accessible = gtk_widget_get_accessible (priv->widget_switch);
  name = g_strdup_printf (_("Enable color management for %s"), title);
  atk_object_set_name (accessible, name);
  g_free (name);

  name = g_strdup_printf (_("Show color profiles for %s"), title);
  accessible = gtk_widget_get_accessible (priv->widget_button);
  atk_object_set_name (accessible, name);
  g_free (name);

out:
  if (profiles != NULL)
    g_ptr_array_unref (profiles);
  g_free (title);
}

CdDevice *
cc_color_device_get_device (CcColorDevice *color_device)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (color_device), NULL);
  return color_device->priv->device;
}

const gchar *
cc_color_device_get_sortable (CcColorDevice *color_device)
{
  g_return_val_if_fail (CC_IS_COLOR_DEVICE (color_device), NULL);
  return color_device->priv->sortable;
}

static void
cc_color_device_get_property (GObject *object, guint param_id,
                              GValue *value, GParamSpec *pspec)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);
  switch (param_id)
    {
      case PROP_DEVICE:
        g_value_set_object (value, color_device->priv->device);
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
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);

  switch (param_id)
    {
      case PROP_DEVICE:
        color_device->priv->device = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_device_finalize (GObject *object)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);
  CcColorDevicePrivate *priv = color_device->priv;

  if (priv->device_changed_id > 0)
    g_signal_handler_disconnect (priv->device, priv->device_changed_id);

  g_free (priv->sortable);
  g_object_unref (priv->device);

  G_OBJECT_CLASS (cc_color_device_parent_class)->finalize (object);
}

void
cc_color_device_set_expanded (CcColorDevice *color_device,
                              gboolean expanded)
{
  CcColorDevicePrivate *priv = color_device->priv;

  /* same as before */
  if (priv->expanded == expanded)
    return;

  /* refresh */
  priv->expanded = expanded;
  g_signal_emit (color_device,
                 signals[SIGNAL_EXPANDED_CHANGED], 0,
                 priv->expanded);
  cc_color_device_refresh (color_device);
}

static void
cc_color_device_notify_enable_device_cb (GtkSwitch *sw,
                                         GParamSpec *pspec,
                                         gpointer user_data)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (user_data);
  CcColorDevicePrivate *priv = color_device->priv;
  gboolean enable;
  gboolean ret;
  GError *error = NULL;

  enable = gtk_switch_get_active (sw);
  g_debug ("Set %s to %i", cd_device_get_id (priv->device), enable);
  ret = cd_device_set_enabled_sync (priv->device,
                                    enable, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to %s to the device: %s",
                 enable ? "enable" : "disable", error->message);
      g_error_free (error);
    }

  /* if expanded, close */
  cc_color_device_set_expanded (color_device, FALSE);
}

static void
cc_color_device_changed_cb (CdDevice *device,
                                   CcColorDevice *color_device)
{
  cc_color_device_refresh (color_device);
}

static void
cc_color_device_constructed (GObject *object)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (object);
  CcColorDevicePrivate *priv = color_device->priv;
  gchar *sortable_tmp;

  /* watch the device for changes */
  priv->device_changed_id =
    g_signal_connect (priv->device, "changed",
                      G_CALLBACK (cc_color_device_changed_cb), color_device);

  /* calculate sortable -- FIXME: we have to hack this as EggListBox
   * does not let us specify a GtkSortType:
   * https://bugzilla.gnome.org/show_bug.cgi?id=691341 */
  sortable_tmp = cc_color_device_get_sortable_base (priv->device);
  priv->sortable = g_strdup_printf ("%sXX", sortable_tmp);
  g_free (sortable_tmp);

  cc_color_device_refresh (color_device);

  /* watch to see if the user flicked the switch */
  g_signal_connect (priv->widget_switch, "notify::active",
                    G_CALLBACK (cc_color_device_notify_enable_device_cb),
                    color_device);
}

static void
cc_color_device_class_init (CcColorDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = cc_color_device_get_property;
  object_class->set_property = cc_color_device_set_property;
  object_class->constructed = cc_color_device_constructed;
  object_class->finalize = cc_color_device_finalize;

  g_object_class_install_property (object_class, PROP_DEVICE,
                                   g_param_spec_object ("device", NULL,
                                                        NULL,
                                                        CD_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  signals [SIGNAL_EXPANDED_CHANGED] =
    g_signal_new ("expanded-changed",
            G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (CcColorDeviceClass, expanded_changed),
            NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  g_type_class_add_private (klass, sizeof (CcColorDevicePrivate));
}

static void
cc_color_device_clicked_expander_cb (GtkButton *button,
                                     gpointer user_data)
{
  CcColorDevice *color_device = CC_COLOR_DEVICE (user_data);
  color_device->priv->expanded = !color_device->priv->expanded;
  cc_color_device_refresh (color_device);
  g_signal_emit (color_device, signals[SIGNAL_EXPANDED_CHANGED], 0,
                 color_device->priv->expanded);
}

static void
cc_color_device_init (CcColorDevice *color_device)
{
  CcColorDevicePrivate *priv;
  GtkStyleContext *context;
  GtkWidget *box;

  color_device->priv = G_TYPE_INSTANCE_GET_PRIVATE (color_device,
                                                    CC_TYPE_COLOR_DEVICE,
                                                    CcColorDevicePrivate);
  priv = color_device->priv;

  /* description */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 9);
  priv->widget_description = gtk_label_new ("");
  gtk_widget_set_margin_start (priv->widget_description, 20);
  gtk_widget_set_margin_top (priv->widget_description, 12);
  gtk_widget_set_margin_bottom (priv->widget_description, 12);
  gtk_widget_set_halign (priv->widget_description, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), priv->widget_description, TRUE, TRUE, 0);

  /* switch */
  priv->widget_switch = gtk_switch_new ();
  gtk_widget_set_valign (priv->widget_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), priv->widget_switch, FALSE, FALSE, 0);

  /* arrow button */
  priv->widget_arrow = gtk_image_new_from_icon_name ("pan-end-symbolic",
                                                     GTK_ICON_SIZE_BUTTON);
  priv->widget_button = gtk_button_new ();
  g_signal_connect (priv->widget_button, "clicked",
                    G_CALLBACK (cc_color_device_clicked_expander_cb),
                    color_device);
  gtk_widget_set_valign (priv->widget_button, GTK_ALIGN_CENTER);
  gtk_button_set_relief (GTK_BUTTON (priv->widget_button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (priv->widget_button), priv->widget_arrow);
  gtk_widget_set_visible (priv->widget_arrow, TRUE);
  gtk_widget_set_margin_top (priv->widget_button, 9);
  gtk_widget_set_margin_bottom (priv->widget_button, 9);
  gtk_widget_set_margin_end (priv->widget_button, 12);
  gtk_box_pack_start (GTK_BOX (box), priv->widget_button, FALSE, FALSE, 0);

  /* not calibrated */
  priv->widget_nocalib = gtk_label_new (_("Not calibrated"));
  context = gtk_widget_get_style_context (priv->widget_nocalib);
  gtk_style_context_add_class (context, "dim-label");
  gtk_widget_set_margin_end (priv->widget_nocalib, 18);
  gtk_box_pack_start (GTK_BOX (box), priv->widget_nocalib, FALSE, FALSE, 0);

  /* refresh */
  gtk_container_add (GTK_CONTAINER (color_device), box);
  gtk_widget_set_visible (box, TRUE);
}

GtkWidget *
cc_color_device_new (CdDevice *device)
{
  return g_object_new (CC_TYPE_COLOR_DEVICE,
                       "device", device,
                       NULL);
}

