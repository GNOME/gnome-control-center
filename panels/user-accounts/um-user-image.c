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

#include "um-user-image.h"

#include <gtk/gtk.h>
#include <act/act.h>

#include "um-utils.h"

struct _UmUserImage {
        GtkImage parent_instance;

        ActUser *user;
};

G_DEFINE_TYPE (UmUserImage, um_user_image, GTK_TYPE_IMAGE)

static void
render_image (UmUserImage *image)
{
        cairo_surface_t *surface;
        gint scale, pixel_size;

        if (image->user == NULL)
                return;

        pixel_size = gtk_image_get_pixel_size (GTK_IMAGE (image));
        scale = gtk_widget_get_scale_factor (GTK_WIDGET (image));
        surface = render_user_icon (image->user,
                                    UM_ICON_STYLE_NONE,
                                    pixel_size > 0 ? pixel_size : 48,
                                    scale);
        gtk_image_set_from_surface (GTK_IMAGE (image), surface);
        cairo_surface_destroy (surface);
}

void
um_user_image_set_user (UmUserImage *image,
                        ActUser     *user)
{
        g_clear_object (&image->user);
        image->user = g_object_ref (user);

        render_image (image);
}

static void
um_user_image_finalize (GObject *object)
{
        UmUserImage *image = UM_USER_IMAGE (object);

        g_clear_object (&image->user);

        G_OBJECT_CLASS (um_user_image_parent_class)->finalize (object);
}

static void
um_user_image_class_init (UmUserImageClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->finalize = um_user_image_finalize;
}

static void
um_user_image_init (UmUserImage *image)
{
        g_signal_connect_swapped (image, "notify::scale-factor", G_CALLBACK (render_image), image);
        g_signal_connect_swapped (image, "notify::pixel-size", G_CALLBACK (render_image), image);
}

GtkWidget *
um_user_image_new (void)
{
        return g_object_new (UM_TYPE_USER_IMAGE, NULL);
}
