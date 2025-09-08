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

#include "cc-profile-combo-row.h"
#include "cc-sound-resources.h"

#define PROFILE_KEY "profile-string"

struct _CcProfileComboRow
{
  AdwComboRow       parent_instance;

  GListStore       *profile_list;

  GvcMixerControl  *mixer_control;
  GvcMixerUIDevice *device;
};

G_DEFINE_TYPE (CcProfileComboRow, cc_profile_combo_row, ADW_TYPE_COMBO_ROW)

static void
selected_item_changed (CcProfileComboRow *self,
                       GParamSpec        *pspec,
                       GtkListItem       *list_item)
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
item_root_changed (GtkWidget         *box,
                   GParamSpec        *pspec,
                   CcProfileComboRow *self)
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
factory_setup_cb (CcProfileComboRow *self,
                  GtkListItem       *list_item)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *selected_icon;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

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
factory_bind_cb (CcProfileComboRow *self,
                 GtkListItem       *list_item)
{
  GtkStringObject *selected_profile;
  const gchar *profile;
  GtkWidget *box;
  GtkWidget *label;

  selected_profile = GTK_STRING_OBJECT (gtk_list_item_get_item (list_item));
  profile = gtk_string_object_get_string (selected_profile);

  box = gtk_list_item_get_child (list_item);
  label = gtk_widget_get_first_child (box);

  gtk_label_set_label (GTK_LABEL (label), profile);

  g_signal_connect (self, "notify::selected-item",
                    G_CALLBACK (selected_item_changed), list_item);
  selected_item_changed (self, NULL, list_item);

  g_signal_connect (box, "notify::root",
                    G_CALLBACK (item_root_changed), self);
  item_root_changed (box, NULL, self);
}

static void
factory_unbind_cb (CcProfileComboRow *self,
                   GtkListItem       *list_item)
{
  GtkWidget *box;

  box = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (self, selected_item_changed, list_item);
  g_signal_handlers_disconnect_by_func (box, item_root_changed, self);
}

static void
profile_changed_cb (CcProfileComboRow *self)
{
  GtkStringObject *selected_profile;
  char *profile;

  selected_profile = GTK_STRING_OBJECT (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self)));

  if (!selected_profile)
    return;

  profile = g_object_get_data (G_OBJECT (selected_profile), PROFILE_KEY);

  if (!gvc_mixer_control_change_profile_on_selected_device (self->mixer_control,
                                                            self->device,
                                                            profile))
    g_warning ("Failed to change profile on %s", gvc_mixer_ui_device_get_description (self->device));
}

static void
cc_profile_combo_row_dispose (GObject *object)
{
  CcProfileComboRow *self = CC_PROFILE_COMBO_ROW (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_profile_combo_row_parent_class)->dispose (object);
}

void
cc_profile_combo_row_class_init (CcProfileComboRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_profile_combo_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-profile-combo-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcProfileComboRow, profile_list);

  gtk_widget_class_bind_template_callback (widget_class, factory_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_unbind_cb);
  gtk_widget_class_bind_template_callback (widget_class, profile_changed_cb);
}

void
cc_profile_combo_row_init (CcProfileComboRow *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_profile_combo_row_set_device (CcProfileComboRow *self,
                                 GvcMixerControl   *mixer_control,
                                 GvcMixerUIDevice  *device)
{
  GList *profiles, *link;

  g_return_if_fail (CC_IS_PROFILE_COMBO_ROW (self));

  if (device == self->device)
    return;

  g_clear_object (&self->mixer_control);
  self->mixer_control = g_object_ref (mixer_control);
  g_clear_object (&self->device);
  g_list_store_remove_all (self->profile_list);

  if (device == NULL)
    return;

  self->device = g_object_ref (device);
  profiles = gvc_mixer_ui_device_get_profiles (device);
  for (link = profiles; link; link = link->next)
    {
      GvcMixerCardProfile *profile = link->data;
      g_autoptr(GtkStringObject) profile_object = NULL;

      profile_object = gtk_string_object_new (profile->human_profile);
      g_object_set_data_full (G_OBJECT (profile_object), PROFILE_KEY,
                              g_strdup (profile->profile), g_free);

      g_signal_handlers_block_by_func(self, profile_changed_cb, self);

      g_list_store_append (self->profile_list, profile_object);

      if (g_strcmp0 (gvc_mixer_ui_device_get_active_profile (device), profile->profile) == 0)
        adw_combo_row_set_selected (ADW_COMBO_ROW (self),
                                    g_list_model_get_n_items (G_LIST_MODEL (self->profile_list)) - 1);
      g_signal_handlers_unblock_by_func(self, profile_changed_cb, self);
    }
}

gint
cc_profile_combo_row_get_profile_count (CcProfileComboRow *self)
{
  g_return_val_if_fail (CC_IS_PROFILE_COMBO_ROW (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self->profile_list));
}
