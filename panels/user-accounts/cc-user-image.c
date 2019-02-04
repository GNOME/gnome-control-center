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

#include "cc-user-image.h"

#include <gtk/gtk.h>
#include <act/act.h>
#include <sys/stat.h>

#include "user-utils.h"

struct _CcUserImage {
        GtkImage parent_instance;

        ActUser *user;
};

G_DEFINE_TYPE (CcUserImage, cc_user_image, GTK_TYPE_IMAGE)

#define MAX_FILE_SIZE     65536

static gboolean
check_user_file (const char *filename,
                 gssize      max_file_size)
{
        struct stat fileinfo;

        if (max_file_size < 0) {
                max_file_size = G_MAXSIZE;
        }

        /* Exists/Readable? */
        if (stat (filename, &fileinfo) < 0) {
                g_debug ("File does not exist");
                return FALSE;
        }

        /* Is a regular file */
        if (G_UNLIKELY (!S_ISREG (fileinfo.st_mode))) {
                g_debug ("File is not a regular file");
                return FALSE;
        }

        /* Size is sane? */
        if (G_UNLIKELY (fileinfo.st_size > max_file_size)) {
                g_debug ("File is too large");
                return FALSE;
        }

        return TRUE;
}

static cairo_surface_t *
render_user_icon (ActUser *user,
                  gint     icon_size,
                  gint     scale)
{
        g_autoptr(GdkPixbuf) source_pixbuf = NULL;
        GdkPixbuf    *pixbuf = NULL;
        gboolean      res;
        GError       *error;
        const gchar  *icon_file;
        cairo_surface_t *surface = NULL;

        g_return_val_if_fail (ACT_IS_USER (user), NULL);
        g_return_val_if_fail (icon_size > 12, NULL);

        icon_file = act_user_get_icon_file (user);
        pixbuf = NULL;
        if (icon_file) {
                res = check_user_file (icon_file, MAX_FILE_SIZE);
                if (res) {
                        source_pixbuf = gdk_pixbuf_new_from_file_at_size (icon_file,
                                                                          icon_size * scale,
                                                                          icon_size * scale,
                                                                          NULL);
                        pixbuf = round_image (source_pixbuf, icon_size * scale);
                }
                else {
                        pixbuf = NULL;
                }
        }

        if (pixbuf != NULL) {
                goto out;
        }

        error = NULL;
        pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                           "avatar-default",
                                           icon_size * scale,
                                           GTK_ICON_LOOKUP_FORCE_SIZE,
                                           &error);
        if (error) {
                g_warning ("%s", error->message);
                g_error_free (error);
        }

 out:

        if (pixbuf != NULL) {
                surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
                g_object_unref (pixbuf);
        }

        return surface;
}

static void
render_image (CcUserImage *image)
{
        cairo_surface_t *surface;
        gint scale, pixel_size;

        if (image->user == NULL)
                return;

        pixel_size = gtk_image_get_pixel_size (GTK_IMAGE (image));
        scale = gtk_widget_get_scale_factor (GTK_WIDGET (image));
        surface = render_user_icon (image->user,
                                    pixel_size > 0 ? pixel_size : 48,
                                    scale);
        gtk_image_set_from_surface (GTK_IMAGE (image), surface);
        cairo_surface_destroy (surface);
}

void
cc_user_image_set_user (CcUserImage *image,
                        ActUser     *user)
{
        g_clear_object (&image->user);
        image->user = g_object_ref (user);

        render_image (image);
}

static void
cc_user_image_finalize (GObject *object)
{
        CcUserImage *image = CC_USER_IMAGE (object);

        g_clear_object (&image->user);

        G_OBJECT_CLASS (cc_user_image_parent_class)->finalize (object);
}

static void
cc_user_image_class_init (CcUserImageClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->finalize = cc_user_image_finalize;
}

static void
cc_user_image_init (CcUserImage *image)
{
        g_signal_connect_swapped (image, "notify::scale-factor", G_CALLBACK (render_image), image);
        g_signal_connect_swapped (image, "notify::pixel-size", G_CALLBACK (render_image), image);
}

GtkWidget *
cc_user_image_new (void)
{
        return g_object_new (CC_TYPE_USER_IMAGE, NULL);
}
