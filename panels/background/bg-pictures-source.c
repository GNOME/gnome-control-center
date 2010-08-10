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

#include "gnome-wp-item.h"

#include <string.h>
#include <gio/gio.h>
#include <libgnomeui/gnome-desktop-thumbnail.h>


G_DEFINE_TYPE (BgPicturesSource, bg_pictures_source, G_TYPE_OBJECT)

#define PICTURES_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_PICTURES_SOURCE, BgPicturesSourcePrivate))

struct _BgPicturesSourcePrivate
{
  GtkListStore *liststore;

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

  if (priv->liststore)
    {
      g_object_unref (priv->liststore);
      priv->liststore = NULL;
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
file_info_async_ready (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  BgPicturesSourcePrivate *priv = BG_PICTURES_SOURCE (user_data)->priv;
  GList *files, *l;
  GError *err = NULL;
  GFile *parent;
  gchar *path;

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
  path = g_file_get_path (parent);


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
          GdkPixbuf *pixbuf;
          GnomeWPItem *item;
          gchar *filename;
          GtkTreeIter iter;
          GtkTreePath *tree_path;

          filename = g_build_filename (path, g_file_info_get_name (info), NULL);

          /* create a new GnomeWpItem */
          item = gnome_wp_item_new (filename, NULL,
                                    priv->thumb_factory);

          if (!item)
            {
              g_warning ("Could not load picture \"%s\"", filename);
              g_free (filename);
              continue;
            }



          /* insert the item into the liststore */
          pixbuf = gdk_pixbuf_new_from_file_at_scale (filename, 100, 75, TRUE,
                                                      NULL);
          gtk_list_store_insert_with_values (priv->liststore, &iter, 0,
                                             0, pixbuf,
                                             1, item,
                                             -1);
          tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->liststore),
                                               &iter);
          item->rowref =
            gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->liststore),
                                        tree_path);
          gtk_tree_path_free (tree_path);

          g_free (filename);
        }
    }

  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);

  g_free (path);
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


  priv->liststore = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_POINTER);

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

GtkListStore*
bg_pictures_source_get_liststore (BgPicturesSource *source)
{
  return source->priv->liststore;
}
