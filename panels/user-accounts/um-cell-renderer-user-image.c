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

#include "um-cell-renderer-user-image.h"

#include <gtk/gtk.h>
#include <act/act.h>

#include "um-utils.h"

struct _UmCellRendererUserImagePrivate {
        GtkWidget *parent;
        ActUser *user;
};

#define UM_CELL_RENDERER_USER_IMAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_CELL_RENDERER_USER_IMAGE, UmCellRendererUserImagePrivate))

enum {
        PROP_0,
        PROP_PARENT,
        PROP_USER
};

G_DEFINE_TYPE_WITH_CODE (UmCellRendererUserImage, um_cell_renderer_user_image, GTK_TYPE_CELL_RENDERER_PIXBUF, G_ADD_PRIVATE (UmCellRendererUserImage));

static void
render_user_image (UmCellRendererUserImage *cell_renderer)
{
        cairo_surface_t *surface;
        gint scale;

        if (cell_renderer->priv->user != NULL) {
                scale = gtk_widget_get_scale_factor (cell_renderer->priv->parent);
                surface = render_user_icon (cell_renderer->priv->user, UM_ICON_STYLE_FRAME | UM_ICON_STYLE_STATUS, 48, scale);
                g_object_set (GTK_CELL_RENDERER_PIXBUF (cell_renderer), "surface", surface, NULL);
                cairo_surface_destroy (surface);
        } else {
                g_object_set (GTK_CELL_RENDERER_PIXBUF (cell_renderer), "surface", NULL, NULL);
        }
}

static void
on_scale_factor_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    data)
{
        UmCellRendererUserImage *cell_renderer = UM_CELL_RENDERER_USER_IMAGE (data);

        render_user_image (cell_renderer);
}

static void
um_cell_renderer_user_image_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
        UmCellRendererUserImage *cell_renderer = UM_CELL_RENDERER_USER_IMAGE (object);

        switch (prop_id) {
        case PROP_PARENT:
                cell_renderer->priv->parent = g_value_dup_object (value);
                g_signal_connect (cell_renderer->priv->parent, "notify::scale-factor", G_CALLBACK (on_scale_factor_changed), cell_renderer);
                break;
        case PROP_USER:
                g_clear_object (&cell_renderer->priv->user);
                cell_renderer->priv->user = g_value_dup_object (value);
                render_user_image (cell_renderer);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_cell_renderer_user_image_finalize (GObject *object)
{
        UmCellRendererUserImage *cell_renderer = UM_CELL_RENDERER_USER_IMAGE (object);

        g_clear_object (&cell_renderer->priv->parent);
        g_clear_object (&cell_renderer->priv->user);

        G_OBJECT_CLASS (um_cell_renderer_user_image_parent_class)->finalize (object);
}

static void
um_cell_renderer_user_image_class_init (UmCellRendererUserImageClass *class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (class);

        object_class->set_property = um_cell_renderer_user_image_set_property;
        object_class->finalize = um_cell_renderer_user_image_finalize;

        g_object_class_install_property (object_class, PROP_PARENT,
                                         g_param_spec_object ("parent",
                                                              "Parent",
                                                              "Tree view aprent widget",
                                                              GTK_TYPE_WIDGET,
                                                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class, PROP_USER,
                                         g_param_spec_object ("user",
                                                              "User",
                                                              "Accountsservice user used to generate image",
                                                              ACT_TYPE_USER,
                                                              G_PARAM_WRITABLE));
}

static void
um_cell_renderer_user_image_init (UmCellRendererUserImage *cell_renderer)
{
        cell_renderer->priv = UM_CELL_RENDERER_USER_IMAGE_GET_PRIVATE (cell_renderer);
}

GtkCellRenderer *
um_cell_renderer_user_image_new (GtkWidget *parent)
{
        return g_object_new (UM_TYPE_CELL_RENDERER_USER_IMAGE, "parent", parent, NULL);
}
