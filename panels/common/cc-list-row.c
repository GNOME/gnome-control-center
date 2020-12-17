/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
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
#define G_LOG_DOMAIN "cc-list-row"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-list-row.h"

struct _CcListRow
{
  GtkListBoxRow parent_instance;

  GtkBox       *box;
  GtkLabel     *title;
  GtkLabel     *subtitle;
  GtkLabel     *secondary_label;
  GtkImage     *icon;

  GtkSwitch    *enable_switch;
  gboolean      show_switch;

  gboolean      switch_active;
};

G_DEFINE_TYPE (CcListRow, cc_list_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_SECONDARY_LABEL,
  PROP_ICON_NAME,
  PROP_SHOW_SWITCH,
  PROP_ACTIVE,
  PROP_BOLD,
  PROP_USE_UNDERLINE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_list_row_activated_cb (CcListRow     *self,
                          GtkListBoxRow *row)
{
  g_assert (CC_IS_LIST_ROW (self));

  if (!self->show_switch || row != GTK_LIST_BOX_ROW (self))
    return;

  cc_list_row_activate (self);
}

static void
cc_list_row_parent_changed_cb (CcListRow *self)
{
  GtkWidget *parent;

  g_assert (CC_IS_LIST_ROW (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (!parent)
    return;

  g_return_if_fail (GTK_IS_LIST_BOX (parent));
  g_signal_connect_object (parent, "row-activated",
                           G_CALLBACK (cc_list_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
cc_list_row_switch_active_cb (CcListRow *self)
{
  gboolean switch_active;

  g_assert (CC_IS_LIST_ROW (self));
  g_assert (self->show_switch);

  switch_active = gtk_switch_get_active (self->enable_switch);

  if (switch_active == self->switch_active)
    return;

  self->switch_active = switch_active;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
}

static void
cc_list_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcListRow *self = (CcListRow *)object;

  switch (prop_id)
    {
    case PROP_SECONDARY_LABEL:
      g_value_set_string (value, gtk_label_get_label (self->secondary_label));
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, self->switch_active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  CcListRow *self = (CcListRow *)object;
  PangoAttrList *attributes;
  PangoAttribute *attribute;
  gint margin;

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_widget_set_visible (GTK_WIDGET (self->subtitle),
                              g_value_get_string (value) != NULL);
      gtk_label_set_label (self->subtitle, g_value_get_string (value));
      if (g_value_get_string (value) != NULL)
        margin = 6;
      else
        margin = 12;
      g_object_set (self->box,
                    "margin-top", margin,
                    "margin-bottom", margin,
                    NULL);
      break;

    case PROP_SECONDARY_LABEL:
      gtk_label_set_label (self->secondary_label, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      cc_list_row_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_SHOW_SWITCH:
      cc_list_row_set_show_switch (self, g_value_get_boolean (value));
      break;

    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (self->title, g_value_get_boolean (value));
      gtk_label_set_use_underline (self->subtitle, g_value_get_boolean (value));
      gtk_label_set_mnemonic_widget (self->title, GTK_WIDGET (self));
      gtk_label_set_mnemonic_widget (self->subtitle, GTK_WIDGET (self));
      break;

    case PROP_ACTIVE:
      g_signal_handlers_block_by_func (self->enable_switch,
                                       cc_list_row_switch_active_cb, self);
      gtk_switch_set_active (self->enable_switch,
                             g_value_get_boolean (value));
      self->switch_active = g_value_get_boolean (value);
      g_signal_handlers_unblock_by_func (self->enable_switch,
                                         cc_list_row_switch_active_cb, self);
      break;

    case PROP_BOLD:
      if (g_value_get_boolean (value))
        attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
      else
        attribute = pango_attr_weight_new (PANGO_WEIGHT_NORMAL);

      attributes = gtk_label_get_attributes (self->title);

      if (!attributes)
        attributes = pango_attr_list_new ();
      else
        pango_attr_list_ref (attributes);

      pango_attr_list_change (attributes, attribute);
      gtk_label_set_attributes (self->title, attributes);
      pango_attr_list_unref (attributes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_list_row_class_init (CcListRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_list_row_get_property;
  object_class->set_property = cc_list_row_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "List row primary title",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "List row primary subtitle",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SECONDARY_LABEL] =
    g_param_spec_string ("secondary-label",
                         "Secondary Label",
                         "Set Secondary Label",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Secondary Icon name",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_SWITCH] =
    g_param_spec_boolean ("show-switch",
                          "Show Switch",
                          "Whether to show a switch at the end of row",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "The active state of the switch",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_BOLD] =
    g_param_spec_boolean ("bold",
                          "Bold",
                          "Whether title is bold or not",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          "Use underline",
                          "If set, text prefixed with underline shall be used as mnemonic",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-list-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcListRow, box);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, title);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, secondary_label);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, icon);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, enable_switch);

  gtk_widget_class_bind_template_callback (widget_class, cc_list_row_switch_active_cb);
}

static void
cc_list_row_init (CcListRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  g_signal_connect_object (self, "notify::parent",
                           G_CALLBACK (cc_list_row_parent_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

void
cc_list_row_set_icon_name (CcListRow   *self,
                           const gchar *icon_name)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));
  g_return_if_fail (!self->show_switch);

  if (icon_name)
    g_object_set (self->icon, "icon-name", icon_name, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->icon), icon_name != NULL);
}

void
cc_list_row_set_show_switch (CcListRow *self,
                             gboolean   show_switch)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  self->show_switch = !!show_switch;

  gtk_widget_set_visible (GTK_WIDGET (self->enable_switch), self->show_switch);
  gtk_widget_set_visible (GTK_WIDGET (self->icon), !self->show_switch);
  gtk_widget_set_visible (GTK_WIDGET (self->secondary_label), !self->show_switch);
}

gboolean
cc_list_row_get_active (CcListRow *self)
{
  g_return_val_if_fail (CC_IS_LIST_ROW (self), FALSE);
  g_return_val_if_fail (self->show_switch, FALSE);

  return self->switch_active;
}

void
cc_list_row_activate (CcListRow *self)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));
  g_return_if_fail (self->show_switch);

  self->switch_active = !self->switch_active;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);

  gtk_widget_activate (GTK_WIDGET (self->enable_switch));
}

void
cc_list_row_set_secondary_label (CcListRow   *self,
                                 const gchar *label)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));
  g_return_if_fail (!self->show_switch);

  if (!label)
    label = "";

  if (g_str_equal (label, gtk_label_get_label (self->secondary_label)))
    return;

  gtk_label_set_text (self->secondary_label, label);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECONDARY_LABEL]);
}

void
cc_list_row_set_secondary_markup (CcListRow   *self,
                                  const gchar *markup)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));
  g_return_if_fail (!self->show_switch);

  if (!markup)
    markup = "";

  if (g_str_equal (markup, gtk_label_get_label (self->secondary_label)))
    return;

  gtk_label_set_markup (self->secondary_label, markup);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECONDARY_LABEL]);
}

void
cc_list_row_set_switch_sensitive (CcListRow *self,
                                  gboolean   sensitive)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->enable_switch), sensitive);
}
