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

#include "cc-device-combo-row.h"
#include "cc-sound-resources.h"

struct _CcDeviceComboRow
{
  AdwComboRow      parent_instance;

  GListStore      *device_list;

  GvcMixerControl *mixer_control;
  gboolean         is_output;
};

G_DEFINE_TYPE (CcDeviceComboRow, cc_device_combo_row, ADW_TYPE_COMBO_ROW)

static void
selected_item_changed (CcDeviceComboRow *self,
                       GParamSpec       *pspec,
                       GtkListItem      *list_item)
{
  GtkWidget *box;
  GtkWidget *selected_icon;

  box = gtk_list_item_get_child (list_item);
  selected_icon = gtk_widget_get_last_child (box);

  if (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self)) == gtk_list_item_get_item (list_item))
    gtk_widget_set_opacity (selected_icon, 1.0);
  else
    gtk_widget_set_opacity (selected_icon, 0.0);
}

static void
item_root_changed (GtkWidget        *box,
                   GParamSpec       *pspec,
                   CcDeviceComboRow *self)
{
  GtkWidget *selected_icon;
  GtkWidget *box_popover;
  gboolean is_in_combo_popover;

  selected_icon = gtk_widget_get_last_child (box);
  box_popover = gtk_widget_get_ancestor (box, GTK_TYPE_POPOVER);
  is_in_combo_popover = (box_popover != NULL &&
                         gtk_widget_get_ancestor (box_popover, ADW_TYPE_COMBO_ROW) == (GtkWidget *) self);

  /* Selection icon should only be visible when in the popover */
  gtk_widget_set_visible (selected_icon, is_in_combo_popover);
}

static void
factory_setup_cb (CcDeviceComboRow *self,
                 GtkListItem       *list_item)
{
  GtkWidget *box;
  GtkWidget *device_icon;
  GtkWidget *label;
  GtkWidget *selected_icon;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  device_icon = g_object_new (GTK_TYPE_IMAGE,
                              "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                              "use-fallback", TRUE,
                              NULL);
  gtk_widget_set_margin_start (device_icon, 6);
  gtk_widget_set_margin_end (device_icon, 6);
  gtk_box_append (GTK_BOX (box), device_icon);

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 1);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), label);

  selected_icon = g_object_new (GTK_TYPE_IMAGE,
                                "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                "icon-name", "object-select-symbolic",
                                NULL);
  gtk_box_append (GTK_BOX (box), selected_icon);

  gtk_list_item_set_child (list_item, box);
}

static void
factory_bind_cb (CcDeviceComboRow *self,
                 GtkListItem      *list_item)
{
  GvcMixerUIDevice *device;
  GtkWidget *box;
  GtkWidget *device_icon;
  GtkWidget *label;
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *description = NULL;
  const gchar *origin;

  device = gtk_list_item_get_item (list_item);
  box = gtk_list_item_get_child (list_item);
  device_icon = gtk_widget_get_first_child (box);
  label = gtk_widget_get_next_sibling (device_icon);

  if (gvc_mixer_ui_device_get_icon_name (device) != NULL)
    icon_name = g_strdup_printf ("%s-symbolic", gvc_mixer_ui_device_get_icon_name (device));

  gtk_image_set_from_icon_name (GTK_IMAGE (device_icon), icon_name);

  origin = gvc_mixer_ui_device_get_origin (device);
  if (origin && origin[0] != '\0')
    {
      description = g_strdup_printf ("%s - %s",
                                     gvc_mixer_ui_device_get_description (device),
                                     origin);
    }
  else
    {
      description = g_strdup (gvc_mixer_ui_device_get_description (device));
    }

  gtk_label_set_label (GTK_LABEL (label), description);

  g_signal_connect (self, "notify::selected-item",
                    G_CALLBACK (selected_item_changed), list_item);
  selected_item_changed (self, NULL, list_item);

  g_signal_connect (box, "notify::root",
                    G_CALLBACK (item_root_changed), self);
  item_root_changed (box, NULL, self);
}

static void
factory_unbind_cb (CcDeviceComboRow *self,
                   GtkListItem      *list_item)
{
  GtkWidget *box;

  box = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (self, selected_item_changed, list_item);
  g_signal_handlers_disconnect_by_func (box, item_root_changed, self);
}

static GvcMixerUIDevice *
lookup_device_id (CcDeviceComboRow *self,
                  guint             id)
{
  if (self->is_output)
    return gvc_mixer_control_lookup_output_id (self->mixer_control, id);

  return gvc_mixer_control_lookup_input_id (self->mixer_control, id);
}

static void
selection_changed_cb (CcDeviceComboRow *self)
{
  GvcMixerUIDevice *device;

  device = cc_device_combo_row_get_device (self);

  if (device == NULL)
    return;

  if (self->is_output)
    gvc_mixer_control_change_output (self->mixer_control, device);
  else
    gvc_mixer_control_change_input (self->mixer_control, device);
}

static void
device_added_cb (CcDeviceComboRow *self,
                 guint             id)
{
  GvcMixerUIDevice *device = lookup_device_id (self, id);

  if (device == NULL)
    return;

  g_signal_handlers_block_by_func (self, selection_changed_cb, self);

  g_list_store_append (self->device_list, device);

  g_signal_handlers_unblock_by_func (self, selection_changed_cb, self);
}

static void
device_removed_cb (CcDeviceComboRow *self,
                   guint             id)
{
  GvcMixerUIDevice *device = lookup_device_id (self, id);
  guint position;

  if (g_list_store_find (self->device_list, device, &position))
    g_list_store_remove (self->device_list, position);
}

static void
device_update_cb (CcDeviceComboRow *self,
                  guint             id)
{
  GvcMixerUIDevice *device = lookup_device_id (self, id);
  guint position;

  g_signal_handlers_block_by_func (self, selection_changed_cb, self);

  if (g_list_store_find (self->device_list, device, &position))
    adw_combo_row_set_selected (ADW_COMBO_ROW (self), position);
  else
    adw_combo_row_set_selected (ADW_COMBO_ROW (self), GTK_INVALID_LIST_POSITION);

  g_signal_handlers_unblock_by_func (self, selection_changed_cb, self);
}

static void
cc_device_combo_row_dispose (GObject *object)
{
  CcDeviceComboRow *self = CC_DEVICE_COMBO_ROW (object);

  g_clear_object (&self->mixer_control);

  G_OBJECT_CLASS (cc_device_combo_row_parent_class)->dispose (object);
}

void
cc_device_combo_row_class_init (CcDeviceComboRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_device_combo_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-device-combo-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDeviceComboRow, device_list);

  gtk_widget_class_bind_template_callback (widget_class, factory_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_unbind_cb);
  gtk_widget_class_bind_template_callback (widget_class, selection_changed_cb);
}

void
cc_device_combo_row_init (CcDeviceComboRow *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_device_combo_row_set_mixer_control (CcDeviceComboRow *self,
                                       GvcMixerControl  *mixer_control,
                                       gboolean          is_output)
{
  g_return_if_fail (CC_IS_DEVICE_COMBO_ROW (self));
  const char *added_signal_name = is_output ? "output-added" : "input-added";
  const char *removed_signal_name = is_output ? "output-removed" : "input-removed";
  const char *update_signal_name = is_output ? "active-output-update" : "active-input-update";

  g_clear_object (&self->mixer_control);

  self->mixer_control = g_object_ref (mixer_control);
  self->is_output = is_output;

  g_signal_connect_object (self->mixer_control,
                           added_signal_name,
                           G_CALLBACK (device_added_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           removed_signal_name,
                           G_CALLBACK (device_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           update_signal_name,
                           G_CALLBACK (device_update_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GvcMixerUIDevice *
cc_device_combo_row_get_device (CcDeviceComboRow *self)
{
  g_return_val_if_fail (CC_IS_DEVICE_COMBO_ROW (self), NULL);

  return adw_combo_row_get_selected_item (ADW_COMBO_ROW (self));
}
