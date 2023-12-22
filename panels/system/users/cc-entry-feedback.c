/*
 * cc-entry-feedback.c
 *
 * Copyright 2023 Red Hat Inc
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
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-entry-feedback"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cc-entry-feedback.h"

struct _CcEntryFeedback
{
  GtkBox       parent_instance;

  GtkImage    *image;
  GtkLabel    *label;
};

G_DEFINE_TYPE (CcEntryFeedback, cc_entry_feedback, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_ICON_NAME,
  PROP_TEXT,
};

static void
cc_entry_feedback_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcEntryFeedback *self = CC_ENTRY_FEEDBACK (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_image_get_icon_name (self->image));
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_label (self->label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_entry_feedback_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcEntryFeedback *self = CC_ENTRY_FEEDBACK (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (self->image, g_value_get_string (value));
      break;
    case PROP_TEXT:
      gtk_label_set_label (self->label, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_entry_feedback_init (CcEntryFeedback *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_entry_feedback_class_init (CcEntryFeedbackClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_entry_feedback_get_property;
  object_class->set_property = cc_entry_feedback_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-entry-feedback.ui");

  gtk_widget_class_bind_template_child (widget_class, CcEntryFeedback, image);
  gtk_widget_class_bind_template_child (widget_class, CcEntryFeedback, label);

  g_object_class_install_property (object_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "Icon name",
                                                        "The icon theme name for the icon to be shown",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "Text",
                                                        "The text to be displayed.",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

void
cc_entry_feedback_reset (CcEntryFeedback *self)
{
  // FIXME: assert
  gtk_image_set_from_icon_name (self->image, NULL);
  gtk_label_set_label (self->label, NULL);
}

void
cc_entry_feedback_update (CcEntryFeedback *self,
                          const gchar     *icon_name,
                          const gchar     *text)
{
  // FIXME: assert
  gtk_image_set_from_icon_name (self->image, icon_name);
  gtk_label_set_label (self->label, text);
}
