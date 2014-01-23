/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#ifndef _CC_MOUSE_PROPERTIES_H
#define _CC_MOUSE_PROPERTIES_H

G_BEGIN_DECLS

#define CC_TYPE_MOUSE_PROPERTIES cc_mouse_properties_get_type ()

#define CC_MOUSE_PROPERTIES(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_MOUSE_PROPERTIES, CcMouseProperties))
#define CC_MOUSE_PROPERTIES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_MOUSE_PROPERTIES, CcMousePropertiesClass))
#define CC_IS_MOUSE_PROPERTIES(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_MOUSE_PROPERTIES))
#define CC_IS_MOUSE_PROPERTIES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_MOUSE_PROPERTIES))
#define CC_MOUSE_PROPERTIES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_MOUSE_PROPERTIES, CcMousePropertiesClass))

typedef struct _CcMouseProperties CcMouseProperties;
typedef struct _CcMousePropertiesClass CcMousePropertiesClass;
typedef struct _CcMousePropertiesPrivate CcMousePropertiesPrivate;

struct _CcMouseProperties
{
  GtkAlignment parent;

  CcMousePropertiesPrivate *priv;
};

struct _CcMousePropertiesClass
{
  GtkAlignmentClass parent_class;
};

GType cc_mouse_properties_get_type (void) G_GNUC_CONST;
GtkWidget *cc_mouse_properties_new (void);

G_END_DECLS

#endif /* _CC_MOUSE_PROPERTIES_H */
