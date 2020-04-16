/* cc-snap-row.c
 *
 * Copyright 2019 Canonical Ltd.
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

#include <config.h>
#include <glib/gi18n.h>

#include "cc-snap-row.h"
#include "cc-applications-resources.h"

struct _CcSnapRow
{
  GtkListBoxRow parent;

  GtkLabel     *title_label;
  GtkSwitch    *slot_toggle;
  GtkComboBox  *slots_combo;
  GtkListStore *slots_combo_model;

  GCancellable *cancellable;

  SnapdPlug    *plug;
  SnapdSlot    *connected_slot;
  GPtrArray    *slots;
};

G_DEFINE_TYPE (CcSnapRow, cc_snap_row, GTK_TYPE_LIST_BOX_ROW)

typedef struct
{
  CcSnapRow *self;
  SnapdSlot *slot;
} ConnectData;

static void
update_state (CcSnapRow *self)
{
  gboolean have_single_option;
  GtkTreeIter iter;

  have_single_option = self->slots->len == 1;
  gtk_widget_set_visible (GTK_WIDGET (self->slot_toggle), have_single_option);
  gtk_widget_set_visible (GTK_WIDGET (self->slots_combo), !have_single_option);

  gtk_switch_set_active (self->slot_toggle, self->connected_slot != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->slots_combo_model), &iter))
    {
      do
        {
          SnapdSlot *slot;

          gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
          if (slot == self->connected_slot)
            gtk_combo_box_set_active_iter (self->slots_combo, &iter);
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->slots_combo_model), &iter));
    }
}

static void
disable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), FALSE);
}

static void
enable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), TRUE);
}

static ConnectData *
connect_data_new (CcSnapRow *self, SnapdSlot *slot)
{
  ConnectData *data;

  data = g_new0 (ConnectData, 1);
  data->self = self;
  data->slot = g_object_ref (slot);

  return data;
}

static void
connect_data_free (ConnectData *data)
{
  g_clear_object (&data->slot);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ConnectData, connect_data_free)

static void
connect_interface_cb (GObject *client, GAsyncResult *result, gpointer user_data)
{
  g_autoptr(ConnectData) data = user_data;
  CcSnapRow *self = data->self;
  SnapdSlot *slot = data->slot;
  g_autoptr(GError) error = NULL;

  if (snapd_client_connect_interface_finish (SNAPD_CLIENT (client), result, &error))
    {
      g_clear_object (&self->connected_slot);
      self->connected_slot = g_object_ref (slot);
    }
  else
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      g_warning ("Failed to connect interface: %s", error->message);
    }

  update_state (self);
  enable_controls (self);
}

static void
connect_plug (CcSnapRow *self, SnapdSlot *slot)
{
  g_autoptr(SnapdClient) client = NULL;

  /* already connected */
  if (self->connected_slot == slot)
    return;

  disable_controls (self);

  client = snapd_client_new ();
  snapd_client_connect_interface_async (client,
                                        snapd_plug_get_snap (self->plug), snapd_plug_get_name (self->plug),
                                        snapd_slot_get_snap (slot), snapd_slot_get_name (slot),
                                        NULL, NULL,
                                        self->cancellable,
                                        connect_interface_cb, connect_data_new (self, slot));
}

static void
disconnect_interface_cb (GObject *client, GAsyncResult *result, gpointer user_data)
{
  CcSnapRow *self = user_data;
  g_autoptr(GError) error = NULL;

  if (snapd_client_disconnect_interface_finish (SNAPD_CLIENT (client), result, &error))
    {
      g_clear_object (&self->connected_slot);
    }
  else
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      g_warning ("Failed to disconnect interface: %s", error->message);
    }

  update_state (self);
  enable_controls (self);
}

static void
disconnect_plug (CcSnapRow *self)
{
  g_autoptr(SnapdClient) client = NULL;

  /* already disconnected */
  if (self->connected_slot == NULL)
    return;

  disable_controls (self);

  client = snapd_client_new ();
  snapd_client_disconnect_interface_async (client,
                                           snapd_plug_get_snap (self->plug), snapd_plug_get_name (self->plug),
                                           NULL, NULL,
                                           NULL, NULL,
                                           self->cancellable,
                                           disconnect_interface_cb, self);
}

static void
switch_changed_cb (CcSnapRow *self)
{
  if (gtk_switch_get_active (self->slot_toggle))
    {
      if (self->slots->len == 1)
        connect_plug (self, g_ptr_array_index (self->slots, 0));
    }
  else
    {
      disconnect_plug (self);
    }
}

static void
combo_changed_cb (CcSnapRow *self)
{
  GtkTreeIter iter;
  SnapdSlot *slot = NULL;

  if (!gtk_combo_box_get_active_iter (self->slots_combo, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
  if (slot != NULL)
    connect_plug (self, slot);
  else
    disconnect_plug (self);
}

static void
cc_snap_row_finalize (GObject *object)
{
  CcSnapRow *self = CC_SNAP_ROW (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->plug);
  g_clear_pointer (&self->slots, g_ptr_array_unref);

  G_OBJECT_CLASS (cc_snap_row_parent_class)->finalize (object);
}

static void
cc_snap_row_class_init (CcSnapRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_snap_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-snap-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slot_toggle);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo_model);

  gtk_widget_class_bind_template_callback (widget_class, combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_changed_cb);
}

static void
cc_snap_row_init (CcSnapRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcSnapRow *
cc_snap_row_new (GCancellable *cancellable, SnapdInterface *interface, SnapdPlug *plug, GPtrArray *slots)
{
  CcSnapRow *self;
  GPtrArray *connected_slots;
  g_autofree gchar *label = NULL;
  GtkTreeIter iter;

  g_return_val_if_fail (SNAPD_IS_PLUG (plug), NULL);

  self = CC_SNAP_ROW (g_object_new (CC_TYPE_SNAP_ROW, NULL));

  self->cancellable = g_object_ref (cancellable);
  self->plug = g_object_ref (plug);
  self->slots = g_ptr_array_ref (slots);

  connected_slots = snapd_plug_get_connected_slots (plug);
  if (connected_slots->len > 0)
    {
      SnapdSlotRef *connected_slot_ref = g_ptr_array_index (connected_slots, 0);

      for (int i = 0; i < slots->len; i++)
        {
          SnapdSlot *slot = g_ptr_array_index (slots, i);

          if (g_strcmp0 (snapd_slot_get_snap (slot), snapd_slot_ref_get_snap (connected_slot_ref)) == 0 &&
              g_strcmp0 (snapd_slot_get_name (slot), snapd_slot_ref_get_slot (connected_slot_ref)) == 0)
            self->connected_slot = slot;
        }
    }

  if (interface != NULL)
    label = snapd_interface_make_label (interface);
  else
    label = g_strdup (snapd_plug_get_interface (plug));
  gtk_label_set_label (self->title_label, label);

  /* Add option into combo box */
  gtk_list_store_append (self->slots_combo_model, &iter);
  gtk_list_store_set (self->slots_combo_model, &iter, 1, "--", -1);
  for (int i = 0; i < slots->len; i++)
    {
      SnapdSlot *slot = g_ptr_array_index (slots, i);
      g_autofree gchar *label = NULL;

      label = g_strdup_printf ("%s:%s", snapd_slot_get_snap (slot), snapd_slot_get_name (slot));
      gtk_list_store_append (self->slots_combo_model, &iter);
      gtk_list_store_set (self->slots_combo_model, &iter, 0, slot, 1, label, -1);
    }

  update_state (self);

  return self;
}
