/*
 * Copyright 2011 Inclusive Design Research Centre, OCAD University.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Joseph Scheuhammer <clown@alum.mit.edu>
 */

#ifndef _ZOOM_OPTIONS_H
#define _ZOOM_OPTIONS_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ZoomOptions			ZoomOptions;
typedef struct _ZoomOptionsClass	ZoomOptionsClass;
typedef struct _ZoomOptionsPrivate	ZoomOptionsPrivate;

#define ZOOM_TYPE_OPTIONS (zoom_options_get_type ())

#define ZOOM_OPTIONS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  ZOOM_TYPE_OPTIONS, ZoomOptions))

#define ZOOM_OPTIONS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  ZOOM_TYPE_OPTIONS, ZoomOptionsClass))

#define ZOOM_IS_OPTIONS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  ZOOM_TYPE_OPTIONS))

#define ZOOM_IS_OPTIONS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  ZOOM_TYPE_OPTIONS))

#define ZOOM_OPTIONS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  ZOOM_TYPE_OPTIONS, ZoomOptionsClass))

struct _ZoomOptionsClass
{
  GObjectClass parent_class;
};

struct _ZoomOptions
{
	GObject parent;

	ZoomOptionsPrivate *priv;
};

GType zoom_options_get_type (void) G_GNUC_CONST;

ZoomOptions *zoom_options_new    (void);
void zoom_options_set_parent (ZoomOptions *self,
			      GtkWindow   *parent);

G_END_DECLS

#endif /* _ZOOM_OPTIONS_H */
