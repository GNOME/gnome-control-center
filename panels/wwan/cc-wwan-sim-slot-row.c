/* cc-applications-row.c
 *
 * Copyright 2024 Josef Vincent Ouano <josef_ouano@yahoo.com.ph>
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
 */

#include <config.h>
#include <glib/gi18n.h>

#include "cc-wwan-sim-slot-row.h"

struct _CcWwanSimSlotRow
{
  AdwActionRow       parent_instance;
  GtkImage          *ok_emblem;
  guint             slot_num;
};

G_DEFINE_TYPE (CcWwanSimSlotRow, cc_wwan_sim_slot_row, ADW_TYPE_ACTION_ROW)

static void
cc_wwan_sim_slot_row_finalize (GObject *object)
{
  CcWwanSimSlotRow *self = CC_WWAN_SIM_SLOT_ROW (object);
  G_OBJECT_CLASS (cc_wwan_sim_slot_row_parent_class)->finalize (object);
}

static void
cc_wwan_sim_slot_row_class_init (CcWwanSimSlotRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_wwan_sim_slot_row_finalize;
}

static void
cc_wwan_sim_slot_row_init (CcWwanSimSlotRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_wwan_sim_slot_row_update_icon (CcWwanSimSlotRow *self, gboolean isEnabled)
{
    g_return_if_fail (CC_IS_WWAN_SIM_SLOT_ROW (self));

    gtk_widget_set_visible (GTK_WIDGET (self->ok_emblem), isEnabled);
}

guint
cc_wwan_sim_slot_row_get_slot_num (CcWwanSimSlotRow *self)
{
    return self->slot_num;
}

CcWwanSimSlotRow *
cc_wwan_sim_slot_row_new (gchar *slot_label, guint slot_num)
{
  CcWwanSimSlotRow *self;
  GtkWidget *box, *image;

  self = g_object_new (CC_TYPE_WWAN_SIM_SLOT_ROW, NULL);
  self->slot_num = slot_num;

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), slot_label);

  image = gtk_image_new_from_icon_name ("emblem-ok-symbolic");
  self->ok_emblem = GTK_IMAGE (image);
  adw_action_row_add_suffix (ADW_ACTION_ROW (self), GTK_WIDGET (self->ok_emblem));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (self), box);

  return self;
}
