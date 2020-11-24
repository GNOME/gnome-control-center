/* cc-brightness-scale.h
 *
 * Copyright (C) 2020 System76, Inc.
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "cc-brightness-scale-types.h"

G_BEGIN_DECLS

typedef enum {
    BRIGHTNESS_DEVICE_SCREEN,
    BRIGHTNESS_DEVICE_KBD,
} BrightnessDevice;

#define CC_TYPE_BRIGHTNESS_SCALE (cc_brightness_scale_get_type())
G_DECLARE_FINAL_TYPE (CcBrightnessScale, cc_brightness_scale, CC, BRIGHTNESS_SCALE, GtkBox)

gboolean cc_brightness_scale_get_has_brightness (CcBrightnessScale *scale);

G_END_DECLS
