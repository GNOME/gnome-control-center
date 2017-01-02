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

#include "cc-display-config.h"

G_DEFINE_TYPE (CcDisplayMode,
               cc_display_mode,
               G_TYPE_OBJECT)

static void
cc_display_mode_init (CcDisplayMode *self)
{
}

static void
cc_display_mode_class_init (CcDisplayModeClass *klass)
{
}

void
cc_display_mode_get_resolution (CcDisplayMode *self, int *w, int *h)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_resolution (self, w, h);
}

gboolean
cc_display_mode_is_interlaced (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->is_interlaced (self);
}

int
cc_display_mode_get_freq (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_freq (self);
}

double
cc_display_mode_get_freq_f (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_freq_f (self);
}


G_DEFINE_TYPE (CcDisplayMonitor,
               cc_display_monitor,
               G_TYPE_OBJECT)

static void
cc_display_monitor_init (CcDisplayMonitor *self)
{
}

static void
cc_display_monitor_class_init (CcDisplayMonitorClass *klass)
{
}

const char *
cc_display_monitor_get_display_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_display_name (self);
}

const char *
cc_display_monitor_get_connector_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_connector_name (self);
}

gboolean
cc_display_monitor_is_builtin (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_builtin (self);
}

gboolean
cc_display_monitor_is_primary (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_primary (self);
}

void
cc_display_monitor_set_primary (CcDisplayMonitor *self, gboolean primary)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_primary (self, primary);
}

gboolean
cc_display_monitor_is_active (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_active (self);
}

void
cc_display_monitor_set_active (CcDisplayMonitor *self, gboolean active)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_active (self, active);
}

CcDisplayRotation
cc_display_monitor_get_rotation (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_rotation (self);
}

void
cc_display_monitor_set_rotation (CcDisplayMonitor *self,
                                 CcDisplayRotation rotation)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_rotation (self, rotation);
}

gboolean
cc_display_monitor_supports_rotation (CcDisplayMonitor *self, CcDisplayRotation r)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->supports_rotation (self, r);
}

void
cc_display_monitor_get_physical_size (CcDisplayMonitor *self, int *w, int *h)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_physical_size (self, w, h);
}

void
cc_display_monitor_get_geometry (CcDisplayMonitor *self, int *x, int *y, int *w, int *h)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_geometry (self, x, y, w, h);
}

CcDisplayMode *
cc_display_monitor_get_mode (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_mode (self);
}

CcDisplayMode *
cc_display_monitor_get_preferred_mode (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_preferred_mode (self);
}

guint32
cc_display_monitor_get_id (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_id (self);
}

GList *
cc_display_monitor_get_modes (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_modes (self);
}

gboolean
cc_display_monitor_supports_underscanning (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->supports_underscanning (self);
}

gboolean
cc_display_monitor_get_underscanning (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_underscanning (self);
}

void
cc_display_monitor_set_underscanning (CcDisplayMonitor *self,
                                      gboolean underscanning)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_underscanning (self, underscanning);
}

void
cc_display_monitor_set_mode (CcDisplayMonitor *self, CcDisplayMode *m)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_mode (self, m);
}

void
cc_display_monitor_set_position (CcDisplayMonitor *self, int x, int y)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_position (self, x, y);
}

double
cc_display_monitor_get_scale (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_scale (self);
}

void
cc_display_monitor_set_scale (CcDisplayMonitor *self, double s)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_scale (self, s);
}


G_DEFINE_TYPE (CcDisplayConfig,
               cc_display_config,
               G_TYPE_OBJECT)

static void
cc_display_config_init (CcDisplayConfig *self)
{
}

static void
cc_display_config_class_init (CcDisplayConfigClass *klass)
{
}

GList *
cc_display_config_get_monitors (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->get_monitors (self);
}

gboolean
cc_display_config_is_applicable (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_applicable (self);
}

gboolean
cc_display_config_equal (CcDisplayConfig *self,
                         CcDisplayConfig *other)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->equal (self, other);
}

gboolean
cc_display_config_apply (CcDisplayConfig *self,
                         GError **error)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->apply (self, error);
}

gboolean
cc_display_config_is_cloning (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_cloning (self);
}

void
cc_display_config_set_cloning (CcDisplayConfig *self,
                               gboolean clone)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->set_cloning (self, clone);
}

GList *
cc_display_config_get_cloning_modes (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->get_cloning_modes (self);
}

const double *
cc_display_config_get_supported_scales (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->get_supported_scales (self);
}

gboolean
cc_display_config_is_layout_logical (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_layout_logical (self);
}
