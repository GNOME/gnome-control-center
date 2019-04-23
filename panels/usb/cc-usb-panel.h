/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2019 GNOME
 * Copyright (C) 2019 Collabora Ltd
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
 * Authors: Ludovico de Nittis <denittis@gnome.org>
 *
 */

#pragma once

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_USB_PANEL cc_usb_panel_get_type ()

G_DECLARE_FINAL_TYPE (CcUsbPanel, cc_usb_panel, CC, USB_PANEL, CcPanel)

void cc_usb_panel_static_init_func (void);

G_END_DECLS
