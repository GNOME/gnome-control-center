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

#include "cc-sound-button.h"
#include "cc-sound-resources.h"

struct _CcSoundButton
{
  GtkToggleButton  parent_instance;

  GtkImage        *image;
  GtkLabel        *label;
};

G_DEFINE_TYPE (CcSoundButton, cc_sound_button, GTK_TYPE_TOGGLE_BUTTON)

enum
{
  PROP_0,
  PROP_LABEL
};

static void
cc_sound_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcSoundButton *self = CC_SOUND_BUTTON (object);

  switch (property_id) {
  case PROP_LABEL:
    gtk_label_set_label (self->label, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
cc_sound_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcSoundButton *self = CC_SOUND_BUTTON (object);

  switch (property_id) {
  case PROP_LABEL:
    g_value_set_string (value, gtk_label_get_label (self->label));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

void
cc_sound_button_class_init (CcSoundButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = cc_sound_button_set_property;
  object_class->get_property = cc_sound_button_get_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-sound-button.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSoundButton, image);
  gtk_widget_class_bind_template_child (widget_class, CcSoundButton, label);

  g_object_class_install_property (object_class, PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

void
cc_sound_button_init (CcSoundButton *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}
