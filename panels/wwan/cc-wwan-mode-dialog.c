/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-mode-dialog.c
 *
 * Copyright 2019 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-network-mode-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "list-box-helper.h"
#include "cc-wwan-mode-dialog.h"
#include "cc-wwan-resources.h"

/**
 * @short_description: WWAN network type selection dialog
 */

#define CC_TYPE_WWAN_MODE_ROW (cc_wwan_mode_row_get_type())
G_DECLARE_FINAL_TYPE (CcWwanModeRow, cc_wwan_mode_row, CC, WWAN_MODE_ROW, GtkListBoxRow)

struct _CcWwanModeDialog
{
  HdyDialog      parent_instance;

  CcWwanDevice  *device;
  GtkListBox    *network_mode_list;
  CcWwanModeRow *selected_row;

  MMModemMode preferred;
  MMModemMode allowed;
  MMModemMode new_allowed;
  MMModemMode new_preferred;
};

G_DEFINE_TYPE (CcWwanModeDialog, cc_wwan_mode_dialog, HDY_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

struct _CcWwanModeRow
{
  GtkListBoxRow  parent_instance;
  GtkImage      *ok_emblem;
  MMModemMode    allowed;
  MMModemMode    preferred;
};

G_DEFINE_TYPE (CcWwanModeRow, cc_wwan_mode_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_wwan_mode_row_class_init (CcWwanModeRowClass *klass)
{
}

static void
cc_wwan_mode_row_init (CcWwanModeRow *row)
{
}

static void
cc_wwan_mode_changed_cb (CcWwanModeDialog *self,
                         CcWwanModeRow    *row)
{
  g_assert (CC_IS_WWAN_MODE_DIALOG (self));
  g_assert (CC_IS_WWAN_MODE_ROW (row));

  if (row == self->selected_row)
    return;

  gtk_widget_show (GTK_WIDGET (row->ok_emblem));

  if (self->selected_row)
    gtk_widget_hide (GTK_WIDGET (self->selected_row->ok_emblem));

  self->selected_row = row;
}

static GtkWidget *
cc_wwan_mode_dialog_row_new (CcWwanModeDialog *self,
                             MMModemMode       allowed,
                             MMModemMode       preferred)
{
  CcWwanModeRow *row;
  GtkWidget *box, *label, *image;
  g_autofree gchar *mode = NULL;

  g_assert (CC_WWAN_MODE_DIALOG (self));

  row = g_object_new (CC_TYPE_WWAN_MODE_ROW, NULL);
  row->allowed = allowed;
  row->preferred = preferred;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box);
  g_object_set (box, "margin", 18, NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  mode = cc_wwan_device_get_string_from_mode (self->device, allowed, preferred);
  label = gtk_label_new (mode);
  gtk_widget_show (label);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (box), label);

  /* image should be hidden by default */
  image = gtk_image_new_from_icon_name ("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (box), image);
  row->ok_emblem = GTK_IMAGE (image);

  return GTK_WIDGET (row);
}

static void
cc_wwan_mode_dialog_update (CcWwanModeDialog *self)
{
  MMModemMode allowed;
  MMModemMode modes[][2] = {
    {MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G},
    {MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, 0},
    {MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G},
    {MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, 0},
    {MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_3G},
    {MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, 0},
    {MM_MODEM_MODE_4G, 0},
    {MM_MODEM_MODE_3G, 0},
    {MM_MODEM_MODE_2G, 0},
  };
  size_t i;

  g_assert (CC_IS_WWAN_MODE_DIALOG (self));

  if (!cc_wwan_device_get_supported_modes (self->device, &allowed, NULL))
    {
      g_warning ("No modes supported by modem");
      return;
    }

  for (i = 0; i < G_N_ELEMENTS (modes); i++)
    {
      GtkWidget *row;

      if ((modes[i][0] & allowed) != modes[i][0])
        continue;

      if (modes[i][1] && !(modes[i][1] & allowed))
        continue;

      row = cc_wwan_mode_dialog_row_new (self, modes[i][0], modes[i][1]);
      gtk_widget_show (row);
      gtk_container_add (GTK_CONTAINER (self->network_mode_list), row);
    }
}

static void
cc_wwan_mode_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcWwanModeDialog *self = CC_WWAN_MODE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wwan_mode_dialog_constructed (GObject *object)
{
  CcWwanModeDialog *self = CC_WWAN_MODE_DIALOG (object);

  G_OBJECT_CLASS (cc_wwan_mode_dialog_parent_class)->constructed (object);

  if(!cc_wwan_device_get_current_mode (self->device, &self->allowed, &self->preferred))
    g_warning ("Can't get allowed and preferred wwan modes");

  cc_wwan_mode_dialog_update (self);
}

static void
cc_wwan_mode_dialog_dispose (GObject *object)
{
  CcWwanModeDialog *self = CC_WWAN_MODE_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_mode_dialog_parent_class)->dispose (object);
}

static void
cc_wwan_mode_dialog_update_mode (CcWwanModeRow    *row,
                                 CcWwanModeDialog *self)
{
  if (self->allowed == row->allowed && self->preferred == row->preferred)
    {
      self->selected_row = row;
      gtk_widget_show (GTK_WIDGET (row->ok_emblem));
    }
  else
    gtk_widget_hide (GTK_WIDGET (row->ok_emblem));
}

static void
cc_wwan_mode_dialog_show (GtkWidget *widget)
{
  CcWwanModeDialog *self = CC_WWAN_MODE_DIALOG (widget);

  if(!cc_wwan_device_get_current_mode (self->device, &self->allowed, &self->preferred))
    {
      g_warning ("Can't get allowed and preferred wwan modes");
      goto end;
    }

  gtk_container_foreach (GTK_CONTAINER (self->network_mode_list),
                         (GtkCallback)cc_wwan_mode_dialog_update_mode,
                         self);
 end:
  GTK_WIDGET_CLASS (cc_wwan_mode_dialog_parent_class)->show (widget);
}

static void
cc_wwan_mode_dialog_response (GtkDialog *dialog,
                              gint       response_id)
{
  CcWwanModeDialog *self = CC_WWAN_MODE_DIALOG (dialog);

  if (response_id != GTK_RESPONSE_APPLY)
    return;

  if (self->selected_row)
    cc_wwan_device_set_current_mode (self->device,
                                     self->selected_row->allowed,
                                     self->selected_row->preferred,
                                     NULL, NULL, NULL);
  else
    g_return_if_reached ();
}

static void
cc_wwan_mode_dialog_class_init (CcWwanModeDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_mode_dialog_set_property;
  object_class->constructed  = cc_wwan_mode_dialog_constructed;
  object_class->dispose = cc_wwan_mode_dialog_dispose;

  widget_class->show = cc_wwan_mode_dialog_show;

  dialog_class->response = cc_wwan_mode_dialog_response;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-mode-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanModeDialog, network_mode_list);

  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_mode_changed_cb);
}

static void
cc_wwan_mode_dialog_init (CcWwanModeDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->network_mode_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);
}

CcWwanModeDialog *
cc_wwan_mode_dialog_new (GtkWindow    *parent_window,
                         CcWwanDevice *device)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  return g_object_new (CC_TYPE_WWAN_MODE_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       "device", device,
                       NULL);
}
