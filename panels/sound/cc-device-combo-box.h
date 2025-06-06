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

#pragma once

#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

G_BEGIN_DECLS

#define CC_TYPE_DEVICE_COMBO_BOX (cc_device_combo_box_get_type ())
G_DECLARE_FINAL_TYPE (CcDeviceComboBox, cc_device_combo_box, CC, DEVICE_COMBO_BOX, GtkComboBox)

void                   cc_device_combo_box_set_mixer_control (CcDeviceComboBox *self,
                                                              GvcMixerControl  *mixer_control,
                                                              gboolean          is_output);

GvcMixerUIDevice      *cc_device_combo_box_get_device        (CcDeviceComboBox *self);

void cc_device_combo_box_device_added (CcDeviceComboBox *self,
                                       guint             id);

void cc_device_combo_box_device_removed (CcDeviceComboBox *self,
                                         guint             id);

void cc_device_combo_box_active_device_changed (CcDeviceComboBox *self,
                                                guint             id);

G_END_DECLS
