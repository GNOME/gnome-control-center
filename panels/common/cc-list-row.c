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

#include "cc-common-resources.h"
#include "cc-list-row.h"

struct _CcListRow
{
  AdwActionRow  parent_instance;

  GtkLabel     *secondary_label;

  GtkImage     *arrow;
  gboolean      show_arrow;

  GtkSwitch    *enable_switch;
  gboolean      show_switch;

  gboolean      switch_active;
};

G_DEFINE_TYPE (CcListRow, cc_list_row, ADW_TYPE_ACTION_ROW)


enum {
  PROP_0,
  PROP_SECONDARY_LABEL,
  PROP_SHOW_ARROW,
  PROP_SHOW_SWITCH,
  PROP_ACTIVE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
update_checked_state (CcListRow *self)
{
  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_CHECKED, self->switch_active,
                               -1);
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
  update_checked_state (self);
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

    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, self->show_arrow);
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

  switch (prop_id)
    {
    case PROP_SECONDARY_LABEL:
      gtk_label_set_label (self->secondary_label, g_value_get_string (value));
      break;

    case PROP_SHOW_ARROW:
      cc_list_row_set_show_arrow (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SWITCH:
      cc_list_row_set_show_switch (self, g_value_get_boolean (value));
      break;

    case PROP_ACTIVE:
      g_signal_handlers_block_by_func (self->enable_switch,
                                       cc_list_row_switch_active_cb, self);
      gtk_switch_set_active (self->enable_switch,
                             g_value_get_boolean (value));
      self->switch_active = g_value_get_boolean (value);
      update_checked_state (self);
      g_signal_handlers_unblock_by_func (self->enable_switch,
                                         cc_list_row_switch_active_cb, self);
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

  properties[PROP_SECONDARY_LABEL] =
    g_param_spec_string ("secondary-label",
                         "Secondary Label",
                         "Set Secondary Label",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_ARROW] =
    g_param_spec_boolean ("show-arrow",
                          "Show Arrow",
                          "Whether to show an arrow at the end of the row",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-list-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcListRow, secondary_label);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, arrow);
  gtk_widget_class_bind_template_child (widget_class, CcListRow, enable_switch);

  gtk_widget_class_bind_template_callback (widget_class, cc_list_row_switch_active_cb);
}

static void
cc_list_row_init (CcListRow *self)
{
  g_resources_register (cc_common_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_list_row_set_show_arrow (CcListRow *self,
                            gboolean   show_arrow)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));
  g_return_if_fail (!self->show_switch);

  if (self->show_arrow == show_arrow)
    return;

  self->show_arrow = show_arrow;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ARROW]);
}

void
cc_list_row_set_show_switch (CcListRow *self,
                             gboolean   show_switch)
{
  g_return_if_fail (CC_IS_LIST_ROW (self));

  self->show_switch = !!show_switch;

  gtk_widget_set_visible (GTK_WIDGET (self->enable_switch), self->show_switch);
  gtk_widget_set_visible (GTK_WIDGET (self->arrow), !self->show_switch);
  gtk_widget_set_visible (GTK_WIDGET (self->secondary_label), !self->show_switch);

  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (self),
                                         self->show_switch ? GTK_WIDGET (self->enable_switch) : NULL);

  update_checked_state (self);
}

gboolean
cc_list_row_get_active (CcListRow *self)
{
  g_return_val_if_fail (CC_IS_LIST_ROW (self), FALSE);
  g_return_val_if_fail (self->show_switch, FALSE);

  return self->switch_active;
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
