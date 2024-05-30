/* cc-wwan-sim-slot-row.h
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

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_WWAN_SIM_SLOT_ROW (cc_wwan_sim_slot_row_get_type())
G_DECLARE_FINAL_TYPE (CcWwanSimSlotRow, cc_wwan_sim_slot_row, CC, WWAN_SIM_SLOT_ROW, AdwActionRow)

CcWwanSimSlotRow*  cc_wwan_sim_slot_row_new            (gchar *slot_label, guint slot_num);

void               cc_wwan_sim_slot_row_update_icon    (CcWwanSimSlotRow *self, gboolean isEnabled);

guint              cc_wwan_sim_slot_row_get_slot_num   (CcWwanSimSlotRow *self);

G_END_DECLS