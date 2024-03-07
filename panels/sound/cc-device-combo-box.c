/*
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

#include "cc-device-combo-box.h"
#include "cc-sound-resources.h"

struct _CcDeviceComboBox
{
  GtkComboBox      parent_instance;

  GtkListStore    *device_model;

  GvcMixerControl *mixer_control;
  gboolean         is_output;
};

G_DEFINE_TYPE (CcDeviceComboBox, cc_device_combo_box, GTK_TYPE_COMBO_BOX)

static gboolean get_iter (CcDeviceComboBox *self, guint id, GtkTreeIter *iter);

void
cc_device_combo_box_device_added (CcDeviceComboBox *self,
                                  guint             id)
{
  GvcMixerUIDevice *device = NULL;
  g_autofree gchar *label = NULL;
  g_autofree gchar *icon_name = NULL;
  const gchar *origin;
  GtkTreeIter iter;

  if (self->is_output)
    device = gvc_mixer_control_lookup_output_id (self->mixer_control, id);
  else
    device = gvc_mixer_control_lookup_input_id (self->mixer_control, id);
  if (device == NULL)
    return;

  origin = gvc_mixer_ui_device_get_origin (device);
  if (origin && origin[0] != '\0')
    {
      label = g_strdup_printf ("%s - %s",
                               gvc_mixer_ui_device_get_description (device),
                               origin);
    }
  else
    {
      label = g_strdup (gvc_mixer_ui_device_get_description (device));
    }

  if (gvc_mixer_ui_device_get_icon_name (device) != NULL)
    icon_name = g_strdup_printf ("%s-symbolic", gvc_mixer_ui_device_get_icon_name (device));

  if (!get_iter (self, id, &iter))
    gtk_list_store_append (self->device_model, &iter);

  gtk_list_store_set (self->device_model, &iter,
                      0, label,
                      1, icon_name,
                      2, id,
                      -1);
}

static gboolean
get_iter (CcDeviceComboBox *self,
          guint             id,
          GtkTreeIter      *iter)
{
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->device_model), iter))
    return FALSE;

  do
    {
      guint i;

      gtk_tree_model_get (GTK_TREE_MODEL (self->device_model), iter, 2, &i, -1);
      if (i == id)
        return TRUE;
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->device_model), iter));

  return FALSE;
}

void
cc_device_combo_box_device_removed (CcDeviceComboBox *self,
                                    guint             id)
{
  GtkTreeIter iter;

  if (get_iter (self, id, &iter))
    gtk_list_store_remove (self->device_model, &iter);
}

void
cc_device_combo_box_active_device_changed (CcDeviceComboBox *self,
                                           guint             id)
{
  GtkTreeIter iter;

  if (get_iter (self, id, &iter))
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
  else
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), NULL);
}

static void
cc_device_combo_box_dispose (GObject *object)
{
  CcDeviceComboBox *self = CC_DEVICE_COMBO_BOX (object);

  g_clear_object (&self->mixer_control);

  G_OBJECT_CLASS (cc_device_combo_box_parent_class)->dispose (object);
}

void
cc_device_combo_box_class_init (CcDeviceComboBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_device_combo_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-device-combo-box.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDeviceComboBox, device_model);
}

void
cc_device_combo_box_init (CcDeviceComboBox *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_device_combo_box_set_mixer_control (CcDeviceComboBox *self,
                                       GvcMixerControl  *mixer_control,
                                       gboolean          is_output)
{
  g_return_if_fail (CC_IS_DEVICE_COMBO_BOX (self));

  g_clear_object (&self->mixer_control);

  self->mixer_control = g_object_ref (mixer_control);
  self->is_output = is_output;
}

GvcMixerUIDevice *
cc_device_combo_box_get_device (CcDeviceComboBox *self)
{
  GtkTreeIter iter;
  guint id;

  g_return_val_if_fail (CC_IS_DEVICE_COMBO_BOX (self), NULL);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (self->device_model), &iter, 2, &id, -1);

  if (self->is_output)
    return gvc_mixer_control_lookup_output_id (self->mixer_control, id);
  else
    return gvc_mixer_control_lookup_input_id (self->mixer_control, id);
}
