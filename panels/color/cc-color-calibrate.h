/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 */

#ifndef __CC_COLOR_CALIBRATE_H
#define __CC_COLOR_CALIBRATE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

G_BEGIN_DECLS

#define CC_TYPE_COLOR_CALIBRATE         (cc_color_calibrate_get_type ())
#define CC_COLOR_CALIBRATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_COLOR_CALIBRATE, CcColorCalibrate))
#define CC_COLOR_CALIBRATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_COLOR_CALIBRATE, CcColorCalibrateClass))
#define CC_IS_COLOR_CALIB(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_COLOR_CALIBRATE))
#define CC_IS_COLOR_CALIB_CLASS(k)      (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_COLOR_CALIBRATE))
#define CC_COLOR_CALIBRATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_COLOR_CALIBRATE, CcColorCalibrateClass))

typedef struct _CcColorCalibratePrivate         CcColorCalibratePrivate;
typedef struct _CcColorCalibrate                CcColorCalibrate;
typedef struct _CcColorCalibrateClass           CcColorCalibrateClass;

struct _CcColorCalibrate
{
  GObject                  parent;
  CcColorCalibratePrivate *priv;
};

struct _CcColorCalibrateClass
{
  GObjectClass             parent_class;
};

CcColorCalibrate *cc_color_calibrate_new    (void);
GType     cc_color_calibrate_get_type       (void);
void      cc_color_calibrate_set_kind       (CcColorCalibrate *calibrate,
                                             CdSensorCap       kind);
void      cc_color_calibrate_set_temperature (CcColorCalibrate *calibrate,
                                             guint             temperature);
void      cc_color_calibrate_set_quality    (CcColorCalibrate *calibrate,
                                             CdProfileQuality  quality);
CdProfileQuality cc_color_calibrate_get_quality (CcColorCalibrate *calibrate);
void      cc_color_calibrate_set_device     (CcColorCalibrate *calibrate,
                                             CdDevice         *device);
void      cc_color_calibrate_set_sensor     (CcColorCalibrate *calibrate,
                                             CdSensor         *sensor);
void      cc_color_calibrate_set_title      (CcColorCalibrate *calibrate,
                                             const gchar      *title);
gboolean  cc_color_calibrate_start          (CcColorCalibrate *calibrate,
                                             GtkWindow        *parent,
                                             GError          **error);
gboolean  cc_color_calibrate_setup          (CcColorCalibrate *calibrate,
                                             GError          **error);
CdProfile *cc_color_calibrate_get_profile   (CcColorCalibrate *calibrate);

G_END_DECLS

#endif /* __CC_COLOR_CALIBRATE_H */

