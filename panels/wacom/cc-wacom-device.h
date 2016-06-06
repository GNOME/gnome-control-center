/*
 * Copyright © 2016 Red Hat, Inc.
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *
 */
#ifndef __CC_WACOM_DEVICE_H__
#define __CC_WACOM_DEVICE_H__

#include "config.h"
#include <glib-object.h>
#include <libwacom/libwacom.h>

#include "gsd-device-manager.h"

#define CC_TYPE_WACOM_DEVICE (cc_wacom_device_get_type ())

G_DECLARE_FINAL_TYPE (CcWacomDevice, cc_wacom_device, CC, WACOM_DEVICE, GObject)

WacomDeviceDatabase *
                cc_wacom_device_database_get    (void);

CcWacomDevice * cc_wacom_device_new             (GsdDevice *device);

const gchar   * cc_wacom_device_get_name        (CcWacomDevice *device);
const gchar   * cc_wacom_device_get_icon_name   (CcWacomDevice *device);

gboolean        cc_wacom_device_is_reversible   (CcWacomDevice *device);

WacomIntegrationFlags
		cc_wacom_device_get_integration_flags (CcWacomDevice *device);

GsdDevice     * cc_wacom_device_get_device      (CcWacomDevice *device);
GSettings     * cc_wacom_device_get_settings    (CcWacomDevice *device);

const gint    * cc_wacom_device_get_supported_tools (CcWacomDevice *device,
						     gint          *n_tools);

#endif /* __CC_WACOM_DEVICE_H__ */
