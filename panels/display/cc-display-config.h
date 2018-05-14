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

#ifndef _CC_DISPLAY_CONFIG_H
#define _CC_DISPLAY_CONFIG_H

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


#define CC_TYPE_DISPLAY_MODE (cc_display_mode_get_type ())
G_DECLARE_DERIVABLE_TYPE (CcDisplayMode, cc_display_mode,
                          CC, DISPLAY_MODE, GObject)

struct _CcDisplayModeClass
{
  GObjectClass parent_class;

  void          (*get_resolution)       (CcDisplayMode *self, int *w, int *h);
  const double* (*get_supported_scales) (CcDisplayMode *self);
  double        (*get_preferred_scale)  (CcDisplayMode *self);
  gboolean      (*is_interlaced)        (CcDisplayMode *self);
  int           (*get_freq)             (CcDisplayMode *self);
  double        (*get_freq_f)           (CcDisplayMode *self);
};


#define CC_TYPE_DISPLAY_MONITOR (cc_display_monitor_get_type ())
G_DECLARE_DERIVABLE_TYPE (CcDisplayMonitor, cc_display_monitor,
                          CC, DISPLAY_MONITOR, GObject)

struct _CcDisplayMonitorClass
{
  GObjectClass parent_class;

  guint32           (*get_id)                 (CcDisplayMonitor  *self);
  const char*       (*get_display_name)       (CcDisplayMonitor  *self);
  const char*       (*get_connector_name)     (CcDisplayMonitor  *self);
  gboolean          (*is_builtin)             (CcDisplayMonitor  *self);
  gboolean          (*is_primary)             (CcDisplayMonitor  *self);
  void              (*set_primary)            (CcDisplayMonitor  *self,
                                               gboolean          primary);
  gboolean          (*is_active)              (CcDisplayMonitor *self);
  void              (*set_active)             (CcDisplayMonitor *self,
                                               gboolean          a);
  CcDisplayRotation (*get_rotation)           (CcDisplayMonitor *self);
  void              (*set_rotation)           (CcDisplayMonitor  *self,
                                               CcDisplayRotation  r);
  gboolean          (*supports_rotation)      (CcDisplayMonitor  *self,
                                               CcDisplayRotation  r);
  void              (*get_physical_size)      (CcDisplayMonitor  *self,
                                               int               *w,
                                               int               *h);
  void              (*get_geometry)           (CcDisplayMonitor  *self,
                                               int               *x,
                                               int               *y,
                                               int               *w,
                                               int               *h);
  gboolean          (*supports_underscanning) (CcDisplayMonitor  *self);
  gboolean          (*get_underscanning)      (CcDisplayMonitor  *self);
  void              (*set_underscanning)      (CcDisplayMonitor  *self,
                                               gboolean           u);
  CcDisplayMode*    (*get_mode)               (CcDisplayMonitor  *self);
  CcDisplayMode*    (*get_preferred_mode)     (CcDisplayMonitor  *self);
  GList*            (*get_modes)              (CcDisplayMonitor  *self);
  void              (*set_mode)               (CcDisplayMonitor  *self,
                                               CcDisplayMode     *m);
  void              (*set_position)           (CcDisplayMonitor  *self,
                                               int                x,
                                               int                y);
  double            (*get_scale)              (CcDisplayMonitor  *self);
  void              (*set_scale)              (CcDisplayMonitor  *self,
                                               double             s);
};


#define CC_TYPE_DISPLAY_CONFIG (cc_display_config_get_type ())
G_DECLARE_DERIVABLE_TYPE (CcDisplayConfig, cc_display_config,
                          CC, DISPLAY_CONFIG, GObject)

struct _CcDisplayConfigClass
{
  GObjectClass parent_class;

  GList*   (*get_monitors)      (CcDisplayConfig  *self);
  gboolean (*is_applicable)     (CcDisplayConfig  *self);
  gboolean (*equal)             (CcDisplayConfig  *self,
                                 CcDisplayConfig  *other);
  gboolean (*apply)             (CcDisplayConfig  *self,
                                GError           **error);
  gboolean (*is_cloning)        (CcDisplayConfig  *self);
  void     (*set_cloning)       (CcDisplayConfig  *self,
                                 gboolean          clone);
  GList*   (*get_cloning_modes) (CcDisplayConfig  *self);
  gboolean (*is_layout_logical) (CcDisplayConfig  *self);
};


GList*            cc_display_config_get_monitors            (CcDisplayConfig    *config);
gboolean          cc_display_config_is_applicable           (CcDisplayConfig    *config);
gboolean          cc_display_config_equal                   (CcDisplayConfig    *config,
                                                             CcDisplayConfig    *other);
gboolean          cc_display_config_apply                   (CcDisplayConfig    *config,
                                                             GError            **error);
gboolean          cc_display_config_is_cloning              (CcDisplayConfig    *config);
void              cc_display_config_set_cloning             (CcDisplayConfig    *config,
                                                             gboolean            clone);
GList*            cc_display_config_get_cloning_modes       (CcDisplayConfig    *config);
gboolean          cc_display_config_is_layout_logical       (CcDisplayConfig    *self);

const char*       cc_display_monitor_get_display_name       (CcDisplayMonitor   *monitor);
gboolean          cc_display_monitor_is_active              (CcDisplayMonitor   *monitor);
void              cc_display_monitor_set_active             (CcDisplayMonitor   *monitor,
                                                             gboolean            active);
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

gboolean          cc_display_monitor_supports_underscanning (CcDisplayMonitor  *monitor);
gboolean          cc_display_monitor_get_underscanning      (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_underscanning      (CcDisplayMonitor  *monitor,
                                                             gboolean           underscanning);

CcDisplayMode*    cc_display_monitor_get_mode               (CcDisplayMonitor  *monitor);
void              cc_display_monitor_get_geometry           (CcDisplayMonitor  *monitor,
                                                             int               *x,
                                                             int               *y,
                                                             int               *width,
                                                             int               *height);
GList*            cc_display_monitor_get_modes              (CcDisplayMonitor  *monitor);
CcDisplayMode*    cc_display_monitor_get_preferred_mode     (CcDisplayMonitor  *monitor);
double            cc_display_monitor_get_scale              (CcDisplayMonitor  *monitor);
void              cc_display_monitor_set_scale              (CcDisplayMonitor  *monitor,
                                                             double s);

void              cc_display_monitor_set_mode               (CcDisplayMonitor  *monitor,
                                                             CcDisplayMode     *mode);
void              cc_display_monitor_set_position           (CcDisplayMonitor  *monitor,
                                                             int                x,
                                                             int                y);

void              cc_display_mode_get_resolution            (CcDisplayMode     *mode,
                                                             int               *width,
                                                             int               *height);
const double*     cc_display_mode_get_supported_scales      (CcDisplayMode     *self);
double            cc_display_mode_get_preferred_scale       (CcDisplayMode     *self);
gboolean          cc_display_mode_is_interlaced             (CcDisplayMode     *mode);
int               cc_display_mode_get_freq                  (CcDisplayMode     *mode);
double            cc_display_mode_get_freq_f                (CcDisplayMode     *mode);

G_END_DECLS

#endif /* _CC_DISPLAY_CONFIG_H */
