/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2015 Red Hat, Inc.
 */

#ifndef _UM_CELL_RENDERER_USER_IMAGE_H
#define _UM_CELL_RENDERER_USER_IMAGE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_CELL_RENDERER_USER_IMAGE um_cell_renderer_user_image_get_type()

#define UM_CELL_RENDERER_USER_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_CELL_RENDERER_USER_IMAGE, UmCellRendererUserImage))
#define UM_CELL_RENDERER_USER_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_CELL_RENDERER_USER_IMAGE, UmCellRendererUserImageClass))
#define UM_IS_CELL_RENDERER_USER_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_CELL_RENDERER_USER_IMAGE))
#define UM_IS_CELL_RENDERER_USER_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_CELL_RENDERER_USER_IMAGE))
#define UM_CELL_RENDERER_USER_IMAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_CELL_RENDERER_USER_IMAGE, UmCellRendererUserImageClass))

typedef struct _UmCellRendererUserImage UmCellRendererUserImage;
typedef struct _UmCellRendererUserImageClass UmCellRendererUserImageClass;
typedef struct _UmCellRendererUserImagePrivate UmCellRendererUserImagePrivate;

struct _UmCellRendererUserImage {
        GtkCellRendererPixbuf parent;

        UmCellRendererUserImagePrivate *priv;
};

struct _UmCellRendererUserImageClass {
        GtkCellRendererPixbufClass parent_class;
};

GType            um_cell_renderer_user_image_get_type (void) G_GNUC_CONST;
GtkCellRenderer *um_cell_renderer_user_image_new      (GtkWidget *parent);

G_END_DECLS

#endif /* _UM_CELL_RENDERER_USER_IMAGE_H */
