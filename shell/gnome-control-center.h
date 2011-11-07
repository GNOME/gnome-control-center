/*
 * Copyright (c) 2010 Intel, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#ifndef _GNOME_CONTROL_CENTER_H
#define _GNOME_CONTROL_CENTER_H

#include <glib-object.h>
#include "cc-shell.h"

G_BEGIN_DECLS

#define GNOME_TYPE_CONTROL_CENTER gnome_control_center_get_type()

#define GNOME_CONTROL_CENTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  GNOME_TYPE_CONTROL_CENTER, GnomeControlCenter))

#define GNOME_CONTROL_CENTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  GNOME_TYPE_CONTROL_CENTER, GnomeControlCenterClass))

#define GNOME_IS_CONTROL_CENTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  GNOME_TYPE_CONTROL_CENTER))

#define GNOME_IS_CONTROL_CENTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  GNOME_TYPE_CONTROL_CENTER))

#define GNOME_CONTROL_CENTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GNOME_TYPE_CONTROL_CENTER, GnomeControlCenterClass))

typedef struct _GnomeControlCenter GnomeControlCenter;
typedef struct _GnomeControlCenterClass GnomeControlCenterClass;
typedef struct _GnomeControlCenterPrivate GnomeControlCenterPrivate;

struct _GnomeControlCenter
{
  CcShell parent;

  GnomeControlCenterPrivate *priv;
};

struct _GnomeControlCenterClass
{
  CcShellClass parent_class;
};

GType gnome_control_center_get_type (void) G_GNUC_CONST;

GnomeControlCenter *gnome_control_center_new (void);

void gnome_control_center_present (GnomeControlCenter *center);

void gnome_control_center_show (GnomeControlCenter *center, GtkApplication *app);

void gnome_control_center_set_overview_page (GnomeControlCenter *center);

G_END_DECLS

#endif /* _GNOME_CONTROL_CENTER_H */
