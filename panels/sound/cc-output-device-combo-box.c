/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-output-device-combo-box.h"
#include "cc-sound-resources.h"

struct _CcOutputDeviceComboBox
{
  GtkComboBoxText  parent_instance;

  GtkListStore    *device_model;

  GvcMixerControl *mixer_control;
};

G_DEFINE_TYPE (CcOutputDeviceComboBox, cc_output_device_combo_box, GTK_TYPE_COMBO_BOX_TEXT)

static void
output_added_cb (CcOutputDeviceComboBox *self,
                 guint                   id)
{
  GvcMixerUIDevice *device = NULL;
  GtkTreeIter iter;

  device = gvc_mixer_control_lookup_output_id (self->mixer_control, id);
  if (device == NULL)
    return;

  gtk_list_store_append (self->device_model, &iter);
  gtk_list_store_set (self->device_model, &iter,
                      0, gvc_mixer_ui_device_get_description (device),
                      1, id,
                      -1);
}

static gboolean
get_iter (CcOutputDeviceComboBox *self,
          guint                   id,
          GtkTreeIter            *iter)
{
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->device_model), iter))
    return FALSE;

  do {
    guint i;

    gtk_tree_model_get (GTK_TREE_MODEL (self->device_model), iter, 1, &i, -1);
    if (i == id)
      return TRUE;
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->device_model), iter));

  return FALSE;
}

static void
output_removed_cb (CcOutputDeviceComboBox *self,
                  guint                   id)
{
  GtkTreeIter iter;

  if (get_iter (self, id, &iter))
    gtk_list_store_remove (self->device_model, &iter);
}

static void
active_output_update_cb (CcOutputDeviceComboBox *self,
                        guint                   id)
{
  GtkTreeIter iter;

  if (get_iter (self, id, &iter))
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
}

static void
cc_output_device_combo_box_dispose (GObject *object)
{
  CcOutputDeviceComboBox *self = CC_OUTPUT_DEVICE_COMBO_BOX (object);

  g_clear_object (&self->mixer_control);

  G_OBJECT_CLASS (cc_output_device_combo_box_parent_class)->dispose (object);
}

void
cc_output_device_combo_box_class_init (CcOutputDeviceComboBoxClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_output_device_combo_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-output-device-combo-box.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOutputDeviceComboBox, device_model);
}

void
cc_output_device_combo_box_init (CcOutputDeviceComboBox *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_output_device_combo_box_set_mixer_control (CcOutputDeviceComboBox *self,
                                             GvcMixerControl         *mixer_control)
{
  g_return_if_fail (CC_IS_OUTPUT_DEVICE_COMBO_BOX (self));

  // FIXME: disconnect signals
  g_clear_object (&self->mixer_control);

  self->mixer_control = g_object_ref (mixer_control);

  g_signal_connect_object (self->mixer_control,
                           "output-added",
                           G_CALLBACK (output_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           "output-removed",
                           G_CALLBACK (output_removed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           "active-output-update",
                           G_CALLBACK (active_output_update_cb),
                           self, G_CONNECT_SWAPPED);
}

GvcMixerUIDevice *
cc_output_device_combo_box_get_device (CcOutputDeviceComboBox *self)
{
  GtkTreeIter iter;
  guint id;

  g_return_val_if_fail (CC_IS_OUTPUT_DEVICE_COMBO_BOX (self), NULL);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (self->device_model), &iter, 1, &id, -1);

  return gvc_mixer_control_lookup_output_id (self->mixer_control, id);
}
