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

#include <adwaita.h>
#include <gvc-mixer-control.h>

G_BEGIN_DECLS

#define CC_TYPE_DEVICE_COMBO_ROW (cc_device_combo_row_get_type ())
G_DECLARE_FINAL_TYPE (CcDeviceComboRow, cc_device_combo_row, CC, DEVICE_COMBO_ROW, AdwComboRow)

void                   cc_device_combo_row_set_mixer_control (CcDeviceComboRow *self,
                                                              GvcMixerControl  *mixer_control,
                                                              gboolean          is_output);

GvcMixerUIDevice      *cc_device_combo_row_get_device        (CcDeviceComboRow *self);

G_END_DECLS
