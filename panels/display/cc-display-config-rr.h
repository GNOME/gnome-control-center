/*
 * Copyright (C) 2016  Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _CC_DISPLAY_CONFIG_RR_H
#define _CC_DISPLAY_CONFIG_RR_H

#include <glib-object.h>

#include "cc-display-config.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_MODE_RR (cc_display_mode_rr_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayModeRR, cc_display_mode_rr,
                      CC, DISPLAY_MODE_RR, CcDisplayMode)

#define CC_TYPE_DISPLAY_MONITOR_RR (cc_display_monitor_rr_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayMonitorRR, cc_display_monitor_rr,
                      CC, DISPLAY_MONITOR_RR, CcDisplayMonitor)

#define CC_TYPE_DISPLAY_CONFIG_RR (cc_display_config_rr_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayConfigRR, cc_display_config_rr,
                      CC, DISPLAY_CONFIG_RR, CcDisplayConfig)

G_END_DECLS

#endif /* _CC_DISPLAY_CONFIG_RR_H */
