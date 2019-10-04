/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-network-dialog.c
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
#define G_LOG_DOMAIN "cc-wwan-network-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "list-box-helper.h"
#include "cc-list-row.h"
#include "cc-wwan-errors-private.h"
#include "cc-wwan-network-dialog.h"
#include "cc-wwan-resources.h"

/**
 * @short_description: WWAN network operator selection dialog
 */

#define CC_TYPE_WWAN_NETWORK_ROW (cc_wwan_network_row_get_type())
G_DECLARE_FINAL_TYPE (CcWwanNetworkRow, cc_wwan_network_row, CC, WWAN_NETWORK_ROW, GtkListBoxRow)

struct _CcWwanNetworkDialog
{
  GtkDialog     parent_instance;

  CcListRow    *automatic_row;
  GtkButton    *button_apply;
  GtkSpinner   *loading_spinner;
  GtkBox       *network_search_title;
  GtkLabel     *notification_label;
  GtkRevealer  *notification_revealer;
  GtkListBox   *operator_list_box;
  GtkButton    *refresh_button;

  CcWwanDevice *device;
  GList        *operator_list;

  CcWwanNetworkRow *selected_row;

  GCancellable *search_cancellable;

  guint         revealer_timeout_id;
  gboolean      no_update_network;
};

G_DEFINE_TYPE (CcWwanNetworkDialog, cc_wwan_network_dialog, GTK_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

struct _CcWwanNetworkRow
{
  GtkListBoxRow  parent_instance;
  GtkImage      *ok_emblem;
  gchar         *operator_code;
};

G_DEFINE_TYPE (CcWwanNetworkRow, cc_wwan_network_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_wwan_network_row_finalize (GObject *object)
{
  CcWwanNetworkRow *row = (CcWwanNetworkRow *)object;

  g_free (row->operator_code);

  G_OBJECT_CLASS (cc_wwan_network_row_parent_class)->finalize (object);
}

static void
cc_wwan_network_row_class_init (CcWwanNetworkRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_wwan_network_row_finalize;
}

static void
cc_wwan_network_row_init (CcWwanNetworkRow *row)
{
}

static void
cc_wwan_on_notification_closed (CcWwanNetworkDialog *self,
                                GtkWidget           *button)
{
  g_assert (CC_IS_WWAN_NETWORK_DIALOG (self));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);

  if (self->revealer_timeout_id != 0)
    g_source_remove (self->revealer_timeout_id);

  self->revealer_timeout_id = 0;
}

static gboolean
cc_wwan_on_notification_timeout (gpointer user_data)
{
  cc_wwan_on_notification_closed (user_data, NULL);

  return G_SOURCE_REMOVE;
}

static void
cc_wwan_network_changed_cb (CcWwanNetworkDialog *self,
                            CcWwanNetworkRow    *row)
{
  if (row == self->selected_row)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (self->button_apply), TRUE);
  gtk_widget_show (GTK_WIDGET (row->ok_emblem));

  if (self->selected_row)
    gtk_widget_hide (GTK_WIDGET (self->selected_row->ok_emblem));

  self->selected_row = row;
}

/*
 * cc_wwan_network_dialog_row_new:
 * @self: a #CcWwanNetworkDialog
 * @operator_name: (transfer full): The long operator name
 * @operator_id: (transfer full): operator id
 */
static CcWwanNetworkRow *
cc_wwan_network_dialog_row_new (CcWwanNetworkDialog *self,
                                const gchar         *operator_name,
                                const gchar         *operator_code)
{
  CcWwanNetworkRow *row;
  GtkWidget *box, *label, *image;

  row = g_object_new (CC_TYPE_WWAN_NETWORK_ROW, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box);
  g_object_set (box, "margin", 18, NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = gtk_label_new (operator_name);
  gtk_widget_show (label);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = gtk_image_new_from_icon_name ("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
  row->ok_emblem = GTK_IMAGE (image);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (row->ok_emblem));

  row->operator_code = g_strdup (operator_code);

  return row;
}

static void
cc_wwan_network_dialog_update_current_network (CcWwanNetworkDialog *self)
{
  CcWwanNetworkRow *row;
  const gchar *operator_name;

  operator_name = cc_wwan_device_get_operator_name (self->device);

  if (!operator_name || operator_name[0] == '\0')
    return;

  gtk_container_foreach (GTK_CONTAINER (self->operator_list_box),
                         (GtkCallback)gtk_widget_destroy, NULL);

  row = cc_wwan_network_dialog_row_new (self, operator_name, "");
  self->selected_row = row;
  gtk_container_add (GTK_CONTAINER (self->operator_list_box), GTK_WIDGET (row));
  gtk_widget_show_all (GTK_WIDGET (self->operator_list_box));
}

static void
cc_wwan_network_dialog_update (CcWwanNetworkDialog *self)
{
  CcWwanNetworkRow *row;
  GList *item;
  const gchar *operator_code, *operator_name;

  gtk_container_foreach (GTK_CONTAINER (self->operator_list_box),
                         (GtkCallback)gtk_widget_destroy, NULL);

  for (item = self->operator_list; item; item = item->next)
    {
      operator_code = mm_modem_3gpp_network_get_operator_code (item->data);
      operator_name = mm_modem_3gpp_network_get_operator_long (item->data);

      row = cc_wwan_network_dialog_row_new (self, operator_name, operator_code);
      gtk_widget_show (GTK_WIDGET (row));
      gtk_container_add (GTK_CONTAINER (self->operator_list_box), GTK_WIDGET (row));
    }
}

static void
cc_wwan_network_scan_complete_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(CcWwanNetworkDialog) self = user_data;
  g_autoptr(GError) error = NULL;

  if (self->operator_list)
    g_list_free_full (self->operator_list, (GDestroyNotify)mm_modem_3gpp_network_free);

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh_button), TRUE);
  gtk_spinner_stop (self->loading_spinner);
  self->operator_list = cc_wwan_device_scan_networks_finish (self->device, result, &error);
  gtk_widget_set_sensitive (GTK_WIDGET (self->operator_list_box), !error);

  if (!error)
    {
      cc_wwan_network_dialog_update (self);
      gtk_widget_show (GTK_WIDGET (self->operator_list_box));
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      self->no_update_network = TRUE;
      gtk_widget_activate (GTK_WIDGET (self->automatic_row));
      gtk_widget_set_sensitive (GTK_WIDGET (self->operator_list_box), FALSE);

      gtk_label_set_label (self->notification_label,
                           cc_wwan_error_get_message (error));
      gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);
      self->revealer_timeout_id = g_timeout_add_seconds (5, cc_wwan_on_notification_timeout, self);

      gtk_widget_show (GTK_WIDGET (self->operator_list_box));
      g_warning ("Error: scanning networks failed: %s", error->message);
    }
}

static void
cc_wwan_network_dialog_refresh_networks (CcWwanNetworkDialog *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh_button), FALSE);
  gtk_spinner_start (self->loading_spinner);
  cc_wwan_device_scan_networks (self->device, self->search_cancellable,
                                (GAsyncReadyCallback)cc_wwan_network_scan_complete_cb,
                                g_object_ref (self));
}

static void
cc_wwan_network_dialog_apply_clicked_cb (CcWwanNetworkDialog *self)
{
  gboolean is_auto;

  g_assert (CC_IS_WWAN_NETWORK_DIALOG (self));

  is_auto = cc_list_row_get_active (self->automatic_row);

  if (is_auto)
    cc_wwan_device_register_network (self->device, "", NULL, NULL, NULL);
  else if (self->selected_row)
    cc_wwan_device_register_network (self->device, self->selected_row->operator_code, NULL, NULL, self);
  else
    g_warn_if_reached ();

  gtk_widget_hide (GTK_WIDGET (self));
}

static void
cc_wwan_auto_network_changed_cb (CcWwanNetworkDialog *self,
                                 GParamSpec          *pspec,
                                 CcListRow           *auto_network_row)
{
  gboolean is_auto;

  g_assert (CC_IS_WWAN_NETWORK_DIALOG (self));
  g_assert (CC_IS_LIST_ROW (auto_network_row));

  is_auto = cc_list_row_get_active (auto_network_row);
  gtk_widget_set_sensitive (GTK_WIDGET (self->button_apply), is_auto);

  if (self->no_update_network)
    {
      self->no_update_network = FALSE;
      return;
    }

  self->selected_row = NULL;
  gtk_widget_set_visible (GTK_WIDGET (self->network_search_title), !is_auto);
  gtk_widget_set_sensitive (GTK_WIDGET (self->operator_list_box), !is_auto);
  gtk_widget_hide (GTK_WIDGET (self->operator_list_box));

  if (is_auto)
    {
      g_cancellable_cancel (self->search_cancellable);
      g_cancellable_reset (self->search_cancellable);
    }
  else
    {
      cc_wwan_network_dialog_refresh_networks (self);
    }
}

static void
cc_wwan_network_dialog_show (GtkWidget *widget)
{
  CcWwanNetworkDialog *self = (CcWwanNetworkDialog *)widget;
  gboolean is_auto;

  is_auto = cc_wwan_device_is_auto_network (self->device);

  g_object_set (self->automatic_row, "active", is_auto, NULL);

  cc_wwan_network_dialog_update_current_network (self);

  GTK_WIDGET_CLASS (cc_wwan_network_dialog_parent_class)->show (widget);
}

static void
cc_wwan_network_dialog_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcWwanNetworkDialog *self = (CcWwanNetworkDialog *)object;

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
cc_wwan_network_dialog_dispose (GObject *object)
{
  CcWwanNetworkDialog *self = (CcWwanNetworkDialog *)object;

  if (self->revealer_timeout_id != 0)
    g_source_remove (self->revealer_timeout_id);

  self->revealer_timeout_id = 0;

  g_cancellable_cancel (self->search_cancellable);

  g_clear_object (&self->search_cancellable);
  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_network_dialog_parent_class)->dispose (object);
}

static void
cc_wwan_network_dialog_class_init (CcWwanNetworkDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_network_dialog_set_property;
  object_class->dispose = cc_wwan_network_dialog_dispose;

  widget_class->show = cc_wwan_network_dialog_show;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-network-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, automatic_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, button_apply);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, loading_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, network_search_title);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, operator_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcWwanNetworkDialog, refresh_button);

  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_network_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_on_notification_closed);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_auto_network_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_network_dialog_refresh_networks);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_network_dialog_apply_clicked_cb);
}

static void
cc_wwan_network_dialog_init (CcWwanNetworkDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->search_cancellable = g_cancellable_new ();

  gtk_list_box_set_header_func (self->operator_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
}

CcWwanNetworkDialog *
cc_wwan_network_dialog_new (GtkWindow    *parent_window,
                            CcWwanDevice *device)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (CC_IS_WWAN_DEVICE (device), NULL);

  return g_object_new (CC_TYPE_WWAN_NETWORK_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       "device", device,
                       NULL);
}
