/* bg-pictures-source.c */
/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "bg-pictures-source.h"

#include "cc-background-item.h"

#include <string.h>
#include <gio/gio.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gsettings-desktop-schemas/gdesktop-enums.h>

G_DEFINE_TYPE (BgPicturesSource, bg_pictures_source, BG_TYPE_SOURCE)

#define PICTURES_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_PICTURES_SOURCE, BgPicturesSourcePrivate))

struct _BgPicturesSourcePrivate
{
  GFile *dir;

  GCancellable *cancellable;

  GnomeDesktopThumbnailFactory *thumb_factory;
};


static void
bg_pictures_source_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bg_pictures_source_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bg_pictures_source_dispose (GObject *object)
{
  BgPicturesSourcePrivate *priv = BG_PICTURES_SOURCE (object)->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  if (priv->thumb_factory)
    {
      g_object_unref (priv->thumb_factory);
      priv->thumb_factory = NULL;
    }

  if (priv->dir)
    {
      g_object_unref (priv->dir);
      priv->dir = NULL;
    }

  G_OBJECT_CLASS (bg_pictures_source_parent_class)->dispose (object);
}

static void
bg_pictures_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (bg_pictures_source_parent_class)->finalize (object);
}

static void
bg_pictures_source_class_init (BgPicturesSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgPicturesSourcePrivate));

  object_class->get_property = bg_pictures_source_get_property;
  object_class->set_property = bg_pictures_source_set_property;
  object_class->dispose = bg_pictures_source_dispose;
  object_class->finalize = bg_pictures_source_finalize;
}

static void
picture_scaled (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
  BgPicturesSource *bg_source = BG_PICTURES_SOURCE (user_data);
  CcBackgroundItem *item;
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  GtkTreeIter iter;
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (bg_source));
  item = g_object_get_data (source_object, "item");

  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (pixbuf == NULL)
    {
      g_warning ("Failed to load image: %s", error->message);
      g_error_free (error);
      g_object_unref (item);
      return;
    }

  /* insert the item into the liststore */
  gtk_list_store_insert_with_values (store, &iter, 0,
                                     0, pixbuf,
                                     1, item,
                                     -1);
  g_object_unref (pixbuf);
}

static void
picture_opened_for_read (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  BgPicturesSource *bg_source = BG_PICTURES_SOURCE (user_data);
  CcBackgroundItem *item;
  GFileInputStream *stream;
  GError *error = NULL;

  item = g_object_get_data (source_object, "item");
  stream = g_file_read_finish (G_FILE (source_object), res, &error);
  if (stream == NULL)
    {
      char *filename;

      filename = g_file_get_path (G_FILE (source_object));
      g_warning ("Failed to load picture '%s': %s", filename, error->message);
      g_free (filename);
      g_error_free (error);
      g_object_unref (item);
      return;
    }

  g_object_set_data (G_OBJECT (stream), "item", item);

  gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
                                             THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT,
                                             TRUE,
                                             NULL,
                                             picture_scaled, bg_source);
  g_object_unref (stream);
}

static void
file_info_async_ready (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  BgPicturesSource *bg_source = BG_PICTURES_SOURCE (user_data);
  GList *files, *l;
  GError *err = NULL;
  GFile *parent;
  files = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source),
                                               res,
                                               &err);

  if (err)
    {
      g_warning ("Could not get pictures file information: %s", err->message);
      g_error_free (err);

      g_list_foreach (files, (GFunc) g_object_unref, NULL);
      g_list_free (files);
      return;
    }

  parent = g_file_enumerator_get_container (G_FILE_ENUMERATOR (source));

  /* iterate over the available files */
  for (l = files; l; l = g_list_next (l))
    {
      GFileInfo *info = l->data;
      const gchar *content_type;

      /* find png and jpeg files */
      content_type = g_file_info_get_content_type (info);

      if (!content_type)
        continue;

      if (!strcmp ("image/png", content_type)
          || !strcmp ("image/jpeg", content_type))
        {
          CcBackgroundItem *item;
          GFile *file;
          char *uri;

          file = g_file_get_child (parent, g_file_info_get_name (info));

          /* create a new CcBackgroundItem */
          uri = g_file_get_uri (file);
          item = cc_background_item_new (uri);
          g_free (uri);
          g_object_set (G_OBJECT (item), "flags", CC_BACKGROUND_ITEM_HAS_URI, NULL);

          if (cc_background_item_get_placement (item) == G_DESKTOP_BACKGROUND_STYLE_NONE)
            g_object_set (G_OBJECT (item), "placement", G_DESKTOP_BACKGROUND_STYLE_ZOOM, NULL);
          g_object_set_data (G_OBJECT (file), "item", item);
          g_file_read_async (file, 0, NULL, picture_opened_for_read, bg_source);
          g_object_unref (file);
        }
    }

  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);
}

static void
dir_enum_async_ready (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  BgPicturesSourcePrivate *priv = BG_PICTURES_SOURCE (user_data)->priv;
  GFileEnumerator *enumerator;
  GError *err = NULL;

  enumerator = g_file_enumerate_children_finish (G_FILE (source), res, &err);

  if (err)
    {
      g_warning ("Could not fill pictures source: %s", err->message);
      g_error_free (err);
      return;
    }

  /* get the files */
  g_file_enumerator_next_files_async (enumerator,
                                      G_MAXINT,
                                      G_PRIORITY_LOW,
                                      priv->cancellable,
                                      file_info_async_ready,
                                      user_data);
}

static void
bg_pictures_source_init (BgPicturesSource *self)
{
  const gchar *pictures_path;
  BgPicturesSourcePrivate *priv;
  priv = self->priv = PICTURES_SOURCE_PRIVATE (self);

  priv->cancellable = g_cancellable_new ();

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  priv->dir = g_file_new_for_path (pictures_path);

  g_file_enumerate_children_async (priv->dir,
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW, priv->cancellable,
                                   dir_enum_async_ready, self);

  priv->thumb_factory =
    gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

}

BgPicturesSource *
bg_pictures_source_new (void)
{
  return g_object_new (BG_TYPE_PICTURES_SOURCE, NULL);
}

