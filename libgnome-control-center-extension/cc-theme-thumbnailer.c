/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jonathan Blandford
 * Copyright (C) 2010 William Jon McCann
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include "cc-theme-thumbnailer.h"
#include "gtkrc-utils.h"

#define CC_THEME_THUMBNAILER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_THUMBNAILER, CcThemeThumbnailerPrivate))

#define THUMBNAIL_TYPE_META     "meta"
#define THUMBNAIL_TYPE_GTK      "gtk"
#define THUMBNAIL_TYPE_METACITY "metacity"
#define THUMBNAIL_TYPE_ICON     "icon"

#define META_THUMBNAIL_SIZE       128
#define GTK_THUMBNAIL_SIZE         96
#define METACITY_THUMBNAIL_WIDTH  120
#define METACITY_THUMBNAIL_HEIGHT  60

typedef struct
{
        char                *thumbnail_type;
        gpointer             theme_info;
        CcThemeThumbnailFunc func;
        gpointer             user_data;
        GDestroyNotify       destroy;
} ThemeQueueItem;

struct CcThemeThumbnailerPrivate
{
        GPid                 child_pid;
        int                  fd_to_factory;
        int                  fd_from_factory;

        GList               *theme_queue;

        /* async data */
        gboolean             set;
        int                  thumbnail_width;
        int                  thumbnail_height;
        GByteArray          *data;
        char                *theme_name;
        CcThemeThumbnailFunc func;
        gpointer             user_data;
        GDestroyNotify       destroy;
        GIOChannel          *channel;
        guint                watch_id;
};

enum {
        PROP_0,
};

static void     cc_theme_thumbnailer_class_init  (CcThemeThumbnailerClass *klass);
static void     cc_theme_thumbnailer_init        (CcThemeThumbnailer      *theme_thumbnailer);
static void     cc_theme_thumbnailer_finalize    (GObject         *object);

static gpointer theme_thumbnailer_object = NULL;

G_DEFINE_TYPE (CcThemeThumbnailer, cc_theme_thumbnailer, G_TYPE_OBJECT)

/* Protocol */

/* Our protocol is pretty simple.  The parent process will write several strings
 * (separated by a '\000'). They are the widget theme, the wm theme, the icon
 * theme, etc.  Then, it will wait for the child to write back the data.  The
 * parent expects ICON_SIZE_WIDTH * ICON_SIZE_HEIGHT * 4 bytes of information.
 * After that, the child is ready for the next theme to render.
 */

enum {
        READY_FOR_THEME,
        READING_TYPE,
        READING_CONTROL_THEME_NAME,
        READING_GTK_COLOR_SCHEME,
        READING_WM_THEME_NAME,
        READING_ICON_THEME_NAME,
        READING_APPLICATION_FONT,
        WRITING_PIXBUF_DATA
};

GQuark
cc_theme_thumbnailer_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("cc_theme_thumbnailer_error");
        }

        return ret;
}

static void
cc_theme_thumbnailer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue  *value,
                                   GParamSpec    *pspec)
{
        CcThemeThumbnailer *self;

        self = CC_THEME_THUMBNAILER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_theme_thumbnailer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        CcThemeThumbnailer *self;

        self = CC_THEME_THUMBNAILER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
generate_next_in_queue (CcThemeThumbnailer *thumbnailer)
{
        ThemeQueueItem *item;

        if (thumbnailer->priv->theme_queue == NULL)
                return;

        item = thumbnailer->priv->theme_queue->data;
        thumbnailer->priv->theme_queue = g_list_delete_link (thumbnailer->priv->theme_queue,
                                                             g_list_first (thumbnailer->priv->theme_queue));

        if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_META) == 0)
                cc_theme_thumbnailer_create_meta_async (thumbnailer,
                                                        (GnomeThemeMetaInfo *) item->theme_info,
                                                        item->func,
                                                        item->user_data,
                                                        item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_GTK) == 0)
                cc_theme_thumbnailer_create_gtk_async (thumbnailer,
                                                       (GnomeThemeInfo *) item->theme_info,
                                                       item->func,
                                                       item->user_data,
                                                       item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_METACITY) == 0)
                cc_theme_thumbnailer_create_metacity_async (thumbnailer,
                                                            (GnomeThemeInfo *) item->theme_info,
                                                            item->func,
                                                            item->user_data,
                                                            item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_ICON) == 0)
                cc_theme_thumbnailer_create_icon_async (thumbnailer,
                                                        (GnomeThemeIconInfo *) item->theme_info,
                                                        item->func,
                                                        item->user_data,
                                                        item->destroy);

        g_free (item);
}

static gboolean
message_from_child (GIOChannel         *source,
                    GIOCondition        condition,
                    CcThemeThumbnailer *thumbnailer)
{
        gboolean finished = FALSE;

        if (condition & G_IO_IN) {
                char      buffer[1024];
                GIOStatus status;
                gsize     bytes_read;

                if (thumbnailer->priv->set == FALSE)
                        return TRUE;

                status = g_io_channel_read_chars (source,
                                                  buffer,
                                                  1024,
                                                  &bytes_read,
                                                  NULL);
                switch (status) {
                case G_IO_STATUS_NORMAL:
                        g_byte_array_append (thumbnailer->priv->data, (guchar *) buffer, bytes_read);

                        if (thumbnailer->priv->thumbnail_width == -1
                            && thumbnailer->priv->data->len >= 2 * sizeof (int)) {

                                thumbnailer->priv->thumbnail_width = *((int *) thumbnailer->priv->data->data);
                                thumbnailer->priv->thumbnail_height = *(((int *) thumbnailer->priv->data->data) + 1);
                                g_byte_array_remove_range (thumbnailer->priv->data, 0, 2 * sizeof (int));
                        }

                        if (thumbnailer->priv->thumbnail_width >= 0
                            && thumbnailer->priv->data->len == thumbnailer->priv->thumbnail_width * thumbnailer->priv->thumbnail_height * 4) {
                                GdkPixbuf *pixbuf = NULL;

                                if (thumbnailer->priv->thumbnail_width > 0) {
                                        char *pixels;
                                        int   i, rowstride;

                                        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                                                 TRUE,
                                                                 8,
                                                                 thumbnailer->priv->thumbnail_width,
                                                                 thumbnailer->priv->thumbnail_height);
                                        pixels = (char *) gdk_pixbuf_get_pixels (pixbuf);
                                        rowstride = gdk_pixbuf_get_rowstride (pixbuf);

                                        for (i = 0; i < thumbnailer->priv->thumbnail_height; ++i)
                                                memcpy (pixels + rowstride * i,
                                                        thumbnailer->priv->data->data + 4 * thumbnailer->priv->thumbnail_width * i,
                                                        thumbnailer->priv->thumbnail_width * 4);
                                }

                                /* callback function needs to ref the pixbuf if it wants to keep it */
                                (* thumbnailer->priv->func) (pixbuf,
                                                             thumbnailer->priv->theme_name,
                                                             thumbnailer->priv->user_data);

                                if (thumbnailer->priv->destroy)
                                        (* thumbnailer->priv->destroy) (thumbnailer->priv->user_data);

                                if (pixbuf)
                                        g_object_unref (pixbuf);

                                /* Clean up async_data */
                                g_free (thumbnailer->priv->theme_name);
                                g_source_remove (thumbnailer->priv->watch_id);
                                g_io_channel_unref (thumbnailer->priv->channel);

                                /* reset async_data */
                                thumbnailer->priv->thumbnail_width = -1;
                                thumbnailer->priv->thumbnail_height = -1;
                                thumbnailer->priv->theme_name = NULL;
                                thumbnailer->priv->channel = NULL;
                                thumbnailer->priv->func = NULL;
                                thumbnailer->priv->user_data = NULL;
                                thumbnailer->priv->destroy = NULL;
                                thumbnailer->priv->set = FALSE;
                                g_byte_array_set_size (thumbnailer->priv->data, 0);

                                generate_next_in_queue (thumbnailer);
                        }
                        break;

                case G_IO_STATUS_AGAIN:
                        break;

                case G_IO_STATUS_EOF:
                case G_IO_STATUS_ERROR:
                        finished = TRUE;
                        break;

                default:
                        g_assert_not_reached ();
                }
        } else if (condition & G_IO_HUP) {
                finished = TRUE;
        }

        if (finished) {
                return FALSE;
        }

        return TRUE;
}

static void
send_thumbnail_request (CcThemeThumbnailer *thumbnailer,
                        char               *thumbnail_type,
                        char               *gtk_theme_name,
                        char               *gtk_color_scheme,
                        char               *metacity_theme_name,
                        char               *icon_theme_name,
                        char               *application_font)
{
        ssize_t res;

        res = write (thumbnailer->priv->fd_to_factory,
                     thumbnail_type,
                     strlen (thumbnail_type) + 1);

        if (gtk_theme_name != NULL)
                res = write (thumbnailer->priv->fd_to_factory,
                             gtk_theme_name,
                             strlen (gtk_theme_name) + 1);
        else
                res = write (thumbnailer->priv->fd_to_factory,
                             "",
                             1);

        if (gtk_color_scheme != NULL)
                res = write (thumbnailer->priv->fd_to_factory,
                             gtk_color_scheme,
                             strlen (gtk_color_scheme) + 1);
        else
                res = write (thumbnailer->priv->fd_to_factory,
                             "",
                             1);

        if (metacity_theme_name != NULL)
                res = write (thumbnailer->priv->fd_to_factory,
                             metacity_theme_name,
                             strlen (metacity_theme_name) + 1);
        else
                res = write (thumbnailer->priv->fd_to_factory,
                             "",
                             1);

        if (icon_theme_name != NULL)
                res = write (thumbnailer->priv->fd_to_factory,
                             icon_theme_name,
                             strlen (icon_theme_name) + 1);
        else
                res = write (thumbnailer->priv->fd_to_factory,
                             "",
                             1);

        if (application_font != NULL)
                res = write (thumbnailer->priv->fd_to_factory,
                             application_font,
                             strlen (application_font) + 1);
        else
                res = write (thumbnailer->priv->fd_to_factory,
                             "Sans 10",
                             strlen ("Sans 10") + 1);

        /* FIXME: check the return values */
}

static void
generate_thumbnail_async (CcThemeThumbnailer  *thumbnailer,
                          gpointer             theme_info,
                          char                *theme_name,
                          char                *thumbnail_type,
                          char                *gtk_theme_name,
                          char                *gtk_color_scheme,
                          char                *metacity_theme_name,
                          char                *icon_theme_name,
                          char                *application_font,
                          CcThemeThumbnailFunc func,
                          gpointer             user_data,
                          GDestroyNotify       destroy)
{
        if (thumbnailer->priv->set) {
                ThemeQueueItem *item;

                item = g_new0 (ThemeQueueItem, 1);
                item->thumbnail_type = thumbnail_type;
                item->theme_info = theme_info;
                item->func = func;
                item->user_data = user_data;
                item->destroy = destroy;

                thumbnailer->priv->theme_queue = g_list_append (thumbnailer->priv->theme_queue, item);

                return;
        }

        if (!thumbnailer->priv->fd_to_factory
            || !thumbnailer->priv->fd_from_factory) {

                (* func) (NULL, theme_name, user_data);

                if (destroy)
                        (* destroy) (user_data);

                return;
        }

        if (thumbnailer->priv->channel == NULL) {
                thumbnailer->priv->channel = g_io_channel_unix_new (thumbnailer->priv->fd_from_factory);
                g_io_channel_set_flags (thumbnailer->priv->channel,
                                        g_io_channel_get_flags (thumbnailer->priv->channel)
                                        | G_IO_FLAG_NONBLOCK,
                                        NULL);
                g_io_channel_set_encoding (thumbnailer->priv->channel, NULL, NULL);
                thumbnailer->priv->watch_id = g_io_add_watch (thumbnailer->priv->channel,
                                                              G_IO_IN | G_IO_HUP,
                                                              (GIOFunc) message_from_child,
                                                              thumbnailer);
        }

        thumbnailer->priv->set = TRUE;
        thumbnailer->priv->thumbnail_width = -1;
        thumbnailer->priv->thumbnail_height = -1;
        thumbnailer->priv->theme_name = g_strdup (theme_name);
        thumbnailer->priv->func = func;
        thumbnailer->priv->user_data = user_data;
        thumbnailer->priv->destroy = destroy;

        send_thumbnail_request (thumbnailer,
                                thumbnail_type,
                                gtk_theme_name,
                                gtk_color_scheme,
                                metacity_theme_name,
                                icon_theme_name,
                                application_font);
}

void
cc_theme_thumbnailer_create_meta_async (CcThemeThumbnailer  *thumbnailer,
                                        GnomeThemeMetaInfo  *theme_info,
                                        CcThemeThumbnailFunc func,
                                        gpointer             user_data,
                                        GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_META,
                                  theme_info->gtk_theme_name,
                                  theme_info->gtk_color_scheme,
                                  theme_info->metacity_theme_name,
                                  theme_info->icon_theme_name,
                                  theme_info->application_font,
                                  func,
                                  user_data,
                                  destroy);
}

void
cc_theme_thumbnailer_create_gtk_async (CcThemeThumbnailer   *thumbnailer,
                                       GnomeThemeInfo       *theme_info,
                                       CcThemeThumbnailFunc  func,
                                       gpointer              user_data,
                                       GDestroyNotify        destroy)
{
        char *scheme;

        scheme = gtkrc_get_color_scheme_for_theme (theme_info->name);

        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_GTK,
                                  theme_info->name,
                                  scheme,
                                  NULL,
                                  NULL,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
        g_free (scheme);
}

void
cc_theme_thumbnailer_create_metacity_async (CcThemeThumbnailer  *thumbnailer,
                                            GnomeThemeInfo      *theme_info,
                                            CcThemeThumbnailFunc func,
                                            gpointer             user_data,
                                            GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_METACITY,
                                  NULL,
                                  NULL,
                                  theme_info->name,
                                  NULL,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
}

void
cc_theme_thumbnailer_create_icon_async (CcThemeThumbnailer  *thumbnailer,
                                        GnomeThemeIconInfo  *theme_info,
                                        CcThemeThumbnailFunc func,
                                        gpointer             user_data,
                                        GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_ICON,
                                  NULL,
                                  NULL,
                                  NULL,
                                  theme_info->name,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
}

static void
create_server (CcThemeThumbnailer *thumbnailer)
{
        gboolean res;
        int      argc;
        char   **argv;
        GError  *error;

        g_shell_parse_argv (LIBEXECDIR "/cc-theme-thumbnailer-helper", &argc, &argv, NULL);

        error = NULL;
        res = g_spawn_async_with_pipes (NULL,
                                        argv,
                                        NULL,
                                        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                        NULL,
                                        thumbnailer,
                                        &thumbnailer->priv->child_pid,
                                        &thumbnailer->priv->fd_to_factory,
                                        &thumbnailer->priv->fd_from_factory,
                                        NULL,
                                        &error);
        if (! res) {
                g_debug ("Could not start command '%s': %s", argv[0], error->message);
                g_error_free (error);
                g_strfreev (argv);
                return;
        }
}

void
cc_theme_thumbnailer_start (CcThemeThumbnailer  *thumbnailer)
{
        if (thumbnailer->priv->child_pid > 0)
                return;

        create_server (thumbnailer);
}

void
cc_theme_thumbnailer_stop (CcThemeThumbnailer  *thumbnailer)
{
        if (thumbnailer->priv->child_pid <= 0)
                return;

        /* FIXME: */
}

static GObject *
cc_theme_thumbnailer_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
        CcThemeThumbnailer *thumbnailer;

        thumbnailer = CC_THEME_THUMBNAILER (G_OBJECT_CLASS (cc_theme_thumbnailer_parent_class)->constructor (type,
                                                                                                                   n_construct_properties,
                                                                                                                   construct_properties));

        /* FIXME: should probably be async */
        cc_theme_thumbnailer_start (thumbnailer);

        return G_OBJECT (thumbnailer);
}

static void
cc_theme_thumbnailer_class_init (CcThemeThumbnailerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_theme_thumbnailer_get_property;
        object_class->set_property = cc_theme_thumbnailer_set_property;
        object_class->constructor = cc_theme_thumbnailer_constructor;
        object_class->finalize = cc_theme_thumbnailer_finalize;

        g_type_class_add_private (klass, sizeof (CcThemeThumbnailerPrivate));
}

static void
cc_theme_thumbnailer_init (CcThemeThumbnailer *thumbnailer)
{

        thumbnailer->priv = CC_THEME_THUMBNAILER_GET_PRIVATE (thumbnailer);

        thumbnailer->priv->set = FALSE;
        thumbnailer->priv->data = g_byte_array_new ();
}

static void
cc_theme_thumbnailer_finalize (GObject *object)
{
        CcThemeThumbnailer *theme_thumbnailer;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_THUMBNAILER (object));

        theme_thumbnailer = CC_THEME_THUMBNAILER (object);

        g_return_if_fail (theme_thumbnailer->priv != NULL);

        G_OBJECT_CLASS (cc_theme_thumbnailer_parent_class)->finalize (object);
}

CcThemeThumbnailer *
cc_theme_thumbnailer_new (void)
{
        if (theme_thumbnailer_object != NULL) {
                g_object_ref (theme_thumbnailer_object);
        } else {
                theme_thumbnailer_object = g_object_new (CC_TYPE_THEME_THUMBNAILER, NULL);
                g_object_add_weak_pointer (theme_thumbnailer_object,
                                           (gpointer *) &theme_thumbnailer_object);
        }

        return CC_THEME_THUMBNAILER (theme_thumbnailer_object);
}
