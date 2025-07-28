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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * GNOME Control Center display configuration system:
 *
 * The display configuration system consists of multiple concepts:
 *
 * CcDisplayConfig:
 *
 *   Configuration instance, read from mutter using the
 *   org.gnome.Mutter.DisplayConfig D-Bus API. Contains information about the
 *   current configuration. Can be copied, to create a representation of a
 *   configuration at a given time, and applied, applying any changes that has
 *   been made to the objects associated with the configuration.
 *
 *   CcDisplayConfig provides a list of all known "monitors" known to the
 *   compositor. It does not know about ports without any monitors connected,
 *   nor low level details about monitors, such as tiling etc.
 *
 * CcDisplayMonitor:
 *
 *   A high level representation of a connected monitor. A monitor have details
 *   associated with it, some which can be altered. Each CcDisplayMonitor
 *   instance is associated with a single CcDisplayConfig instance. All
 *   alteration to a monitor is cached and not applied until
 *   cc_display_config_apply() is called on the corresponding CcDisplayConfig
 *   object.
 *
 * CcDisplayMode:
 *
 *   A monitor mode, including resolution, refresh rate, and scale. Each monitor
 *   will have a list of possible modes.
 *
 */

typedef enum _CcDisplayRotation
{
  CC_DISPLAY_ROTATION_NONE,
  CC_DISPLAY_ROTATION_90,
  CC_DISPLAY_ROTATION_180,
  CC_DISPLAY_ROTATION_270,
  CC_DISPLAY_ROTATION_FLIPPED,
  CC_DISPLAY_ROTATION_90_FLIPPED,
  CC_DISPLAY_ROTATION_180_FLIPPED,
  CC_DISPLAY_ROTATION_270_FLIPPED,
} CcDisplayRotation;

typedef enum _CcDisplayMonitorPrivacy
{
  CC_DISPLAY_MONITOR_PRIVACY_UNSUPPORTED = 0,
  CC_DISPLAY_MONITOR_PRIVACY_DISABLED = 1 << 0,
  CC_DISPLAY_MONITOR_PRIVACY_ENABLED = 1 << 1,
  CC_DISPLAY_MONITOR_PRIVACY_LOCKED = 1 << 2,
} CcDisplayMonitorPrivacy;

typedef enum _CcDisplayModeRefreshRateMode
{
  MODE_REFRESH_RATE_MODE_FIXED,
  MODE_REFRESH_RATE_MODE_VARIABLE,
} CcDisplayModeRefreshRateMode;

typedef enum _CcDisplayColorMode
{
  CC_DISPLAY_COLOR_MODE_DEFAULT = 0,
  CC_DISPLAY_COLOR_MODE_BT2100 = 1,
} CcDisplayColorMode;

#define CC_TYPE_DISPLAY_MODE (cc_display_mode_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayMode, cc_display_mode,
                      CC, DISPLAY_MODE, GObject)

#define CC_TYPE_DISPLAY_MONITOR (cc_display_monitor_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayMonitor, cc_display_monitor,
                      CC, DISPLAY_MONITOR, GObject)

#define CC_TYPE_DISPLAY_LOGICAL_MONITOR (cc_display_logical_monitor_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayLogicalMonitor, cc_display_logical_monitor,
                      CC, DISPLAY_LOGICAL_MONITOR, GObject)

#define CC_TYPE_DISPLAY_CONFIG (cc_display_config_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayConfig, cc_display_config,
                      CC, DISPLAY_CONFIG, GObject)

GList*            cc_display_config_get_monitors            (CcDisplayConfig    *config);
GList*            cc_display_config_get_ui_sorted_monitors  (CcDisplayConfig    *config);
int               cc_display_config_count_useful_monitors   (CcDisplayConfig    *config);
gboolean          cc_display_config_is_applicable           (CcDisplayConfig    *config);
gboolean          cc_display_config_equal                   (CcDisplayConfig    *config,
                                                             CcDisplayConfig    *other);
gboolean          cc_display_config_apply                   (CcDisplayConfig    *config,
                                                             GError            **error);
gboolean          cc_display_config_is_cloning              (CcDisplayConfig    *config);
void              cc_display_config_set_cloning             (CcDisplayConfig    *config,
                                                             gboolean            clone);
GList*            cc_display_config_generate_cloning_modes  (CcDisplayConfig    *config);

void              cc_display_config_set_mode_on_all_outputs (CcDisplayConfig *config,
                                                             CcDisplayMode   *mode);

gboolean          cc_display_config_is_layout_logical       (CcDisplayConfig    *self);
gboolean          cc_display_config_is_scaled_mode_valid    (CcDisplayConfig    *self,
                                                             CcDisplayMode      *mode,
                                                             double              scale);
gboolean          cc_display_config_get_panel_orientation_managed
                                                            (CcDisplayConfig    *self);
void              cc_display_config_update_ui_numbers_names (CcDisplayConfig    *self);

const char*       cc_display_monitor_get_display_name       (CcDisplayMonitor   *monitor);
gboolean          cc_display_monitor_is_active              (CcDisplayMonitor   *monitor);
void              cc_display_monitor_set_active             (CcDisplayMonitor   *monitor,
                                                             gboolean            active);
const char *      cc_display_monitor_get_vendor_name        (CcDisplayMonitor   *monitor);
const char *      cc_display_monitor_get_product_name       (CcDisplayMonitor   *monitor);
const char *      cc_display_monitor_get_product_serial     (CcDisplayMonitor   *monitor);
const char*       cc_display_monitor_get_connector_name     (CcDisplayMonitor   *monitor);
CcDisplayRotation cc_display_monitor_get_rotation           (CcDisplayMonitor   *monitor);
void              cc_display_monitor_set_rotation           (CcDisplayMonitor   *monitor,
                                                             CcDisplayRotation  r);
gboolean          cc_display_monitor_supports_rotation      (CcDisplayMonitor  *monitor,
                                                             CcDisplayRotation  rotation);
void              cc_display_monitor_get_physical_size      (CcDisplayMonitor  *monitor,
                                                             int               *w,
                                                             int               *h);
gboolean          cc_display_monitor_is_builtin             (CcDisplayMonitor  *monitor);
gboolean          cc_display_monitor_is_primary             (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_primary            (CcDisplayMonitor  *monitor,
                                                             gboolean           primary);
guint32           cc_display_monitor_get_id                 (CcDisplayMonitor  *monitor);

gboolean          cc_display_monitor_supports_variable_refresh_rate (CcDisplayMonitor *self);
GList *           cc_display_monitor_get_supported_color_modes (CcDisplayMonitor *monitor);
gboolean          cc_display_monitor_supports_color_mode    (CcDisplayMonitor   *monitor,
                                                             CcDisplayColorMode  color_mode);
CcDisplayColorMode cc_display_monitor_get_color_mode        (CcDisplayMonitor   *monitor);
void              cc_display_monitor_set_color_mode         (CcDisplayMonitor   *monitor,
                                                             CcDisplayColorMode  color_mode);
gboolean          cc_display_monitor_supports_underscanning (CcDisplayMonitor  *monitor);
gboolean          cc_display_monitor_get_underscanning      (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_underscanning      (CcDisplayMonitor  *monitor,
                                                             gboolean           underscanning);

CcDisplayMonitorPrivacy cc_display_monitor_get_privacy      (CcDisplayMonitor *self);

CcDisplayMode*    cc_display_monitor_get_mode               (CcDisplayMonitor  *monitor);
void              cc_display_monitor_get_geometry           (CcDisplayMonitor  *monitor,
                                                             int               *x,
                                                             int               *y,
                                                             int               *width,
                                                             int               *height);
int               cc_display_monitor_get_min_freq           (CcDisplayMonitor  *monitor);
GList*            cc_display_monitor_get_modes              (CcDisplayMonitor  *monitor);
CcDisplayMode*    cc_display_monitor_get_preferred_mode     (CcDisplayMonitor  *monitor);
double            cc_display_monitor_get_scale              (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_scale              (CcDisplayMonitor  *monitor,
                                                             double s);

void              cc_display_monitor_set_compatible_clone_mode (CcDisplayMonitor  *monitor,
                                                                CcDisplayMode     *mode);
void              cc_display_monitor_set_mode               (CcDisplayMonitor  *monitor,
                                                             CcDisplayMode     *mode);
void              cc_display_monitor_set_refresh_rate_mode  (CcDisplayMonitor             *self,
                                                             CcDisplayModeRefreshRateMode  refresh_rate_mode);
void              cc_display_monitor_set_position           (CcDisplayMonitor  *monitor,
                                                             int                x,
                                                             int                y);

gboolean          cc_display_monitor_is_useful              (CcDisplayMonitor  *monitor);
gboolean          cc_display_monitor_is_usable              (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_usable             (CcDisplayMonitor  *monitor,
                                                             gboolean           is_usable);
int               cc_display_monitor_get_ui_number          (CcDisplayMonitor  *monitor);
const char*       cc_display_monitor_get_ui_name            (CcDisplayMonitor  *monitor);
const char*       cc_display_monitor_get_ui_number_name     (CcDisplayMonitor  *monitor);
char*             cc_display_monitor_dup_ui_number_name     (CcDisplayMonitor  *monitor);

gboolean          cc_display_mode_is_clone_mode             (CcDisplayMode     *mode);
void              cc_display_mode_get_resolution            (CcDisplayMode     *mode,
                                                             int               *width,
                                                             int               *height);
GArray*           cc_display_mode_get_supported_scales      (CcDisplayMode     *self);
double            cc_display_mode_get_preferred_scale       (CcDisplayMode     *self);
CcDisplayModeRefreshRateMode cc_display_mode_get_refresh_rate_mode (CcDisplayMode *mode);
gboolean          cc_display_mode_is_interlaced             (CcDisplayMode     *mode);
gboolean          cc_display_mode_is_preferred              (CcDisplayMode     *mode);
int               cc_display_mode_get_freq                  (CcDisplayMode     *mode);
double            cc_display_mode_get_freq_f                (CcDisplayMode     *mode);

G_END_DECLS
