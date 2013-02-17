/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef CC_APPLICATION_H
#define CC_APPLICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_APPLICATION (cc_application_get_type ())

#define CC_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   CC_TYPE_APPLICATION, CcApplication))

#define CC_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   CC_TYPE_APPLICATION, CcApplicationClass))

#define CC_IS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   CC_TYPE_APPLICATION))

#define CC_IS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   CC_TYPE_APPLICATION))

#define CC_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   CC_TYPE_APPLICATION, CcApplicationClass))

typedef struct _CcApplication        CcApplication;
typedef struct _CcApplicationClass   CcApplicationClass;
typedef struct _CcApplicationPrivate CcApplicationPrivate;

struct _CcApplication
{
  GtkApplication parent_instance;
  CcApplicationPrivate *priv;
};

struct _CcApplicationClass
{
  GtkApplicationClass parent_class;
};

GType                  cc_application_get_type               (void) G_GNUC_CONST;

GtkApplication        *cc_application_new                    (void);

G_END_DECLS

#endif /* CC_APPLICATION_H */
