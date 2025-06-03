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
                                    g_list_model_get_n_items (G_LIST_MODEL (self->profile_list)));

      g_signal_handlers_unblock_by_func(self, profile_changed_cb, self);
    }
}

gint
cc_profile_combo_row_get_profile_count (CcProfileComboRow *self)
{
  g_return_val_if_fail (CC_IS_PROFILE_COMBO_ROW (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self->profile_list));
}
