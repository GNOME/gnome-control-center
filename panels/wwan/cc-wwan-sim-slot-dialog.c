/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-sim-slot-dialog.c
 *
 * Copyright (C) 2024 Josef Vincent Ouano <josef_ouano@yahoo.com.ph>
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
 *   Josef Vincent Ouano <josef_ouano@yahoo.com.ph>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-network-sim-slot-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wwan-sim-slot-dialog.h"
#include "cc-wwan-sim-slot-row.h"
#include "cc-wwan-resources.h"
#include "cc-wwan-device.h"

/**
 * @short_description: WWAN sim slot selection dialog
 */

struct _CcWwanSimSlotDialog
{
  AdwDialog               parent_instance;

  CcWwanDevice           *device;

  AdwPreferencesGroup    *sim_slot_list;
  CcWwanSimSlotRow       *selected_row;

  GCancellable           *cancellable;
};

G_DEFINE_TYPE (CcWwanSimSlotDialog, cc_wwan_sim_slot_dialog, ADW_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_wwan_sim_slot_row_class_init (CcWwanSimSlotRowClass *klass)
{
}

static void
cc_wwan_sim_slot_row_init (CcWwanSimSlotRow *row)
{
}

static void
cc_wwan_sim_slot_changed_cb (CcWwanSimSlotDialog *self,
                         CcWwanSimSlotRow    *row)
{
  g_assert (CC_IS_WWAN_SIM_SLOT_DIALOG (self));

  if (row == self->selected_row)
    return;

  cc_wwan_sim_slot_row_update_icon (row, TRUE);

  if (self->selected_row) {
    cc_wwan_sim_slot_row_update_icon (self->selected_row, FALSE);
  }

  self->selected_row = row;
}

static void
cc_wwan_sim_slot_dialog_ok_clicked_cb (CcWwanSimSlotDialog *self)
{
  g_assert (CC_IS_WWAN_SIM_SLOT_DIALOG (self));

  if (self->selected_row)
    {
      cc_wwan_device_set_primary_sim_slot (self->device, cc_wwan_sim_slot_row_get_slot_num (self->selected_row), self->cancellable);
    }
  else
    {
      g_return_if_reached ();
    }

  adw_dialog_close (ADW_DIALOG (self));
}

static CcWwanSimSlotRow *
cc_wwan_sim_slot_dialog_row_new (CcWwanSimSlotDialog *self, MMSim *sim, guint slot_num)
{
  CcWwanSimSlotRow *row;
  g_autofree gchar *slot_label = NULL;

  slot_label = cc_wwan_device_get_string_from_slots (self->device, sim, slot_num);
  row = cc_wwan_sim_slot_row_new (slot_label, slot_num);

  return row;
}

static void
cc_wwan_sim_slot_dialog_update (CcWwanSimSlotDialog *self)
{
  GPtrArray *sim_slots;
  gboolean simActive = false;

  sim_slots = g_ptr_array_new ();
  sim_slots = cc_wwan_device_get_sim_slots (self->device, self->cancellable);

  for (int i = 0; i < sim_slots->len; i++)
  {
    MMSim *sim;
    CcWwanSimSlotRow *row;

    sim = MM_SIM (g_ptr_array_index (sim_slots, i));
    row = cc_wwan_sim_slot_dialog_row_new (self, sim, i+1);

    simActive = mm_sim_get_active (sim);
    if (simActive == true) {
      cc_wwan_sim_slot_row_update_icon (row, TRUE);
      self->selected_row = row;
    }
    else if (simActive == false) {
      cc_wwan_sim_slot_row_update_icon (row, FALSE);
    }

    g_signal_connect_swapped (G_OBJECT (row), "activated", G_CALLBACK (cc_wwan_sim_slot_changed_cb), self);
    adw_preferences_group_add (self->sim_slot_list, GTK_WIDGET (row));
  }
}

static void
cc_wwan_sim_slot_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcWwanSimSlotDialog *self = (CcWwanSimSlotDialog *)object;

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wwan_sim_slot_dialog_constructed (GObject *object)
{
  CcWwanSimSlotDialog *self = CC_WWAN_SIM_SLOT_DIALOG (object);

  G_OBJECT_CLASS (cc_wwan_sim_slot_dialog_parent_class)->constructed (object);
  cc_wwan_sim_slot_dialog_update (self);
}

static void
cc_wwan_sim_slot_dialog_dispose (GObject *object)
{
  CcWwanSimSlotDialog *self = CC_WWAN_SIM_SLOT_DIALOG (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_wwan_sim_slot_dialog_parent_class)->dispose (object);
}

static void
cc_wwan_sim_slot_dialog_class_init (CcWwanSimSlotDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_sim_slot_dialog_set_property;
  object_class->constructed  = cc_wwan_sim_slot_dialog_constructed;
  object_class->dispose = cc_wwan_sim_slot_dialog_dispose;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The WWAN Device",
                         CC_TYPE_WWAN_DEVICE,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-sim-slot-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanSimSlotDialog, sim_slot_list);

  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_sim_slot_dialog_ok_clicked_cb);
}

static void
cc_wwan_sim_slot_dialog_init (CcWwanSimSlotDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
}

GtkWidget *
cc_wwan_sim_slot_dialog_new (GtkBox    *parent_window,
                         CcWwanDevice *device)
{
  CcWwanSimSlotDialog *self;

  self = g_object_new (CC_TYPE_WWAN_SIM_SLOT_DIALOG,
                       "device", device,
                        NULL);
  return GTK_WIDGET (self);
}
