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

#include "cc-profile-combo-box.h"
#include "cc-sound-resources.h"

struct _CcProfileComboBox
{
  GtkComboBox       parent_instance;

  GtkListStore     *profile_model;

  GvcMixerUIDevice *device;
};

G_DEFINE_TYPE (CcProfileComboBox, cc_profile_combo_box, GTK_TYPE_COMBO_BOX)

static void
cc_profile_combo_box_dispose (GObject *object)
{
  CcProfileComboBox *self = CC_PROFILE_COMBO_BOX (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_profile_combo_box_parent_class)->dispose (object);
}

void
cc_profile_combo_box_class_init (CcProfileComboBoxClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_profile_combo_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-profile-combo-box.ui");

  gtk_widget_class_bind_template_child (widget_class, CcProfileComboBox, profile_model);
}

void
cc_profile_combo_box_init (CcProfileComboBox *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_profile_combo_box_set_device (CcProfileComboBox *self,
                                 GvcMixerUIDevice  *device)
{
  g_return_if_fail (CC_IS_PROFILE_COMBO_BOX (self));

  // FIXME: disconnect signals
  g_clear_object (&self->device);

  self->device = g_object_ref (device);
}

GvcMixerCardProfile *
cc_profile_combo_box_get_profile (CcProfileComboBox *self)
{
  GtkTreeIter iter;

  g_return_val_if_fail (CC_IS_PROFILE_COMBO_BOX (self), NULL);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
    return NULL;

  return NULL; //FIXME
}
