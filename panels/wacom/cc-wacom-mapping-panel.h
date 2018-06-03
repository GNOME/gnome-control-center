/*
 * Copyright © 2012 Wacom.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Jason Gerecke <killertofu@gmail.com>
 *
 */

#pragma once

#include <gtk/gtk.h>
#include "cc-wacom-device.h"

G_BEGIN_DECLS

#define CC_TYPE_WACOM_MAPPING_PANEL (cc_wacom_mapping_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcWacomMappingPanel, cc_wacom_mapping_panel, CC, WACOM_MAPPING_PANEL, GtkBox)

GtkWidget * cc_wacom_mapping_panel_new (void);

void cc_wacom_mapping_panel_set_device (CcWacomMappingPanel *self,
                                        CcWacomDevice       *device);

G_END_DECLS
