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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include "bg-pictures-source.h"

#include "cc-background-grilo-miner.h"
#include "cc-background-item.h"

#include <string.h>
#include <cairo-gobject.h>
#include <gio/gio.h>
#include <grilo.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gdesktop-enums.h>

G_DEFINE_TYPE (BgPicturesSource, bg_pictures_source, BG_TYPE_SOURCE)

#define PICTURES_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_PICTURES_SOURCE, BgPicturesSourcePrivate))

#define ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED

struct _BgPicturesSourcePrivate
{
  GCancellable *cancellable;

  CcBackgroundGriloMiner *grl_miner;

  GnomeDesktopThumbnailFactory *thumb_factory;

  GFileMonitor *picture_dir_monitor;
  GFileMonitor *cache_dir_monitor;

  GHashTable *known_items;
};

const char * const content_types[] = {
	"image/png",
	"image/jp2",
	"image/jpeg",
	"image/bmp",
	"image/svg+xml",
	"image/x-portable-anymap",
	NULL
};

const char * const screenshot_types[] = {
	"image/png",
	NULL
};

static char *bg_pictures_source_get_unique_filename (const char *uri);

static void picture_opened_for_read (GObject *source_object, GAsyncResult *res, gpointer user_data);

static void
bg_pictures_source_dispose (GObject *object)
{
  BgPicturesSourcePrivate *priv = BG_PICTURES_SOURCE (object)->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_object (&priv->grl_miner);
  g_clear_object (&priv->thumb_factory);

  G_OBJECT_CLASS (bg_pictures_source_parent_class)->dispose (object);
}

static void
bg_pictures_source_finalize (GObject *object)
{
  BgPicturesSource *bg_source = BG_PICTURES_SOURCE (object);

  g_clear_object (&bg_source->priv->thumb_factory);

  g_clear_pointer (&bg_source->priv->known_items, g_hash_table_destroy);

  g_clear_object (&bg_source->priv->picture_dir_monitor);
  g_clear_object (&bg_source->priv->cache_dir_monitor);

  G_OBJECT_CLASS (bg_pictures_source_parent_class)->finalize (object);
}

static void
bg_pictures_source_class_init (BgPicturesSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgPicturesSourcePrivate));

  object_class->dispose = bg_pictures_source_dispose;
  object_class->finalize = bg_pictures_source_finalize;
}

static void
remove_placeholder (BgPicturesSource *bg_source, CcBackgroundItem *item)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;

  store = bg_source_get_liststore (BG_SOURCE (bg_source));
  row_ref = g_object_get_data (G_OBJECT (item), "row-ref");
  if (row_ref == NULL)
    return;

  path = gtk_tree_row_reference_get_path (row_ref);
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
    return;

  gtk_list_store_remove (store, &iter);
}

static gboolean
picture_needs_rotation (GdkPixbuf *pixbuf)
{
  const gchar *str;

  str = gdk_pixbuf_get_option (pixbuf, "orientation");
  if (str == NULL)
    return FALSE;

  if (*str == '5' || *str == '6' || *str == '7' || *str == '8')
    return TRUE;

  return FALSE;
}

static GdkPixbuf *
swap_rotated_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *tmp_pixbuf;

  tmp_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
  if (tmp_pixbuf == NULL)
    return pixbuf;

  g_object_unref (pixbuf);
  return tmp_pixbuf;
}

static void
picture_scaled (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  GError *error = NULL;
  GdkPixbuf *pixbuf = NULL;
  const char *software;
  const char *uri;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;
  GtkListStore *store;
  cairo_surface_t *surface = NULL;
  int scale_factor;
  gboolean rotation_applied;

  item = g_object_get_data (source_object, "item");
  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (pixbuf == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to load image: %s", error->message);
          remove_placeholder (BG_PICTURES_SOURCE (user_data), item);
        }

      g_error_free (error);
      goto out;
    }

  /* since we were not cancelled, we can now cast user_data
   * back to BgPicturesSource.
   */
  bg_source = BG_PICTURES_SOURCE (user_data);
  store = bg_source_get_liststore (BG_SOURCE (bg_source));
  uri = cc_background_item_get_uri (item);
  if (uri == NULL)
    uri = cc_background_item_get_source_url (item);

  /* Ignore screenshots */
  software = gdk_pixbuf_get_option (pixbuf, "tEXt::Software");
  if (software != NULL &&
      g_str_equal (software, "gnome-screenshot"))
    {
      g_debug ("Ignored URL '%s' as it's a screenshot from gnome-screenshot", uri);
      remove_placeholder (BG_PICTURES_SOURCE (user_data), item);
      goto out;
    }

  /* Process embedded orientation */
  rotation_applied = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "rotation-applied"));

  if (!rotation_applied && picture_needs_rotation (pixbuf))
    {
      /* the width and height of pixbuf we requested are wrong for EXIF
       * orientations 5, 6, 7 and 8. the file has to be reloaded. */
      GFile *file;

      file = g_file_new_for_uri (uri);
      g_object_set_data (G_OBJECT (item), "needs-rotation", GINT_TO_POINTER (TRUE));
      g_object_set_data_full (G_OBJECT (file), "item", g_object_ref (item), g_object_unref);
      g_file_read_async (G_FILE (file), G_PRIORITY_DEFAULT,
                         bg_source->priv->cancellable,
                         picture_opened_for_read, bg_source);
      g_object_unref (file);
      goto out;
    }

  pixbuf = swap_rotated_pixbuf (pixbuf);

  scale_factor = bg_source_get_scale_factor (BG_SOURCE (bg_source));
  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
  cc_background_item_load (item, NULL);

  row_ref = g_object_get_data (G_OBJECT (item), "row-ref");
  if (row_ref == NULL)
    {
      /* insert the item into the liststore if it did not exist */
      gtk_list_store_insert_with_values (store, NULL, -1,
                                         0, surface,
                                         1, item,
                                         -1);
    }
  else
    {
      path = gtk_tree_row_reference_get_path (row_ref);
      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
        {
          /* otherwise update the thumbnail */
          gtk_list_store_set (store, &iter,
                              0, surface,
                              -1);
        }
    }

  g_hash_table_insert (bg_source->priv->known_items,
                       bg_pictures_source_get_unique_filename (uri),
                       GINT_TO_POINTER (TRUE));


 out:
  g_clear_pointer (&surface, (GDestroyNotify) cairo_surface_destroy);
  g_clear_object (&pixbuf);
}

static void
picture_opened_for_read (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  GFileInputStream *stream;
  GError *error = NULL;
  gint thumbnail_height;
  gint thumbnail_width;
  gboolean needs_rotation;

  item = g_object_get_data (source_object, "item");
  stream = g_file_read_finish (G_FILE (source_object), res, &error);
  if (stream == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          char *filename = g_file_get_path (G_FILE (source_object));
          g_warning ("Failed to load picture '%s': %s", filename, error->message);
          remove_placeholder (BG_PICTURES_SOURCE (user_data), item);
          g_free (filename);
        }

      g_error_free (error);
      return;
    }

  /* since we were not cancelled, we can now cast user_data
   * back to BgPicturesSource.
   */
  bg_source = BG_PICTURES_SOURCE (user_data);

  needs_rotation = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "needs-rotation"));
  if (needs_rotation)
    {
      /* swap width and height for EXIF orientations that need it */
      thumbnail_width = bg_source_get_thumbnail_height (BG_SOURCE (bg_source));
      thumbnail_height = bg_source_get_thumbnail_width (BG_SOURCE (bg_source));
      g_object_set_data (G_OBJECT (item), "rotation-applied", GINT_TO_POINTER (TRUE));
    }
  else
    {
      thumbnail_width = bg_source_get_thumbnail_width (BG_SOURCE (bg_source));
      thumbnail_height = bg_source_get_thumbnail_height (BG_SOURCE (bg_source));
    }

  g_object_set_data_full (G_OBJECT (stream), "item", g_object_ref (item), g_object_unref);
  gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
                                             thumbnail_width, thumbnail_height,
                                             TRUE,
                                             bg_source->priv->cancellable,
                                             picture_scaled, bg_source);
  g_object_unref (stream);
}

static void
picture_copied_for_read (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  GError *error = NULL;
  GFile *thumbnail_file = G_FILE (source_object);
  GFile *native_file;

  if (!g_file_copy_finish (thumbnail_file, res, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        goto out;
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          gchar *uri;

          uri = g_file_get_uri (thumbnail_file);
          g_warning ("Failed to download '%s': %s", uri, error->message);
          g_free (uri);
          goto out;
        }
    }

  bg_source = BG_PICTURES_SOURCE (user_data);

  native_file = g_object_get_data (G_OBJECT (thumbnail_file), "native-file");
  item = g_object_get_data (G_OBJECT (thumbnail_file), "item");
  g_object_set_data_full (G_OBJECT (native_file), "item", g_object_ref (item), g_object_unref);
  g_file_read_async (native_file,
                     G_PRIORITY_DEFAULT,
                     bg_source->priv->cancellable,
                     picture_opened_for_read,
                     bg_source);

 out:
  g_clear_error (&error);
}

static gboolean
in_content_types (const char *content_type)
{
	guint i;
	for (i = 0; content_types[i]; i++)
		if (g_str_equal (content_types[i], content_type))
			return TRUE;
	return FALSE;
}

static gboolean
in_screenshot_types (const char *content_type)
{
	guint i;
	for (i = 0; screenshot_types[i]; i++)
		if (g_str_equal (screenshot_types[i], content_type))
			return TRUE;
	return FALSE;
}

static cairo_surface_t *
get_content_loading_icon (BgSource *source)
{
  GtkIconTheme *theme;
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf, *ret;
  GError *error = NULL;
  int scale_factor;
  cairo_surface_t *surface;
  int thumbnail_height;
  int thumbnail_width;

  theme = gtk_icon_theme_get_default ();
  icon_info = gtk_icon_theme_lookup_icon (theme,
                                          "content-loading-symbolic",
                                          16,
                                          GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_GENERIC_FALLBACK);
  if (icon_info == NULL)
    {
      g_warning ("Failed to find placeholder icon");
      return NULL;
    }

  pixbuf = gtk_icon_info_load_icon (icon_info, &error);
  if (pixbuf == NULL)
    {
      g_warning ("Failed to load placeholder icon: %s", error->message);
      g_clear_error (&error);
      g_clear_object (&icon_info);
      return NULL;
    }

  thumbnail_height = bg_source_get_thumbnail_height (source);
  thumbnail_width = bg_source_get_thumbnail_width (source);
  ret = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                        TRUE,
                        8, thumbnail_width, thumbnail_height);
  gdk_pixbuf_fill (ret, 0x00000000);

  /* Put the icon in the middle */
  gdk_pixbuf_copy_area (pixbuf, 0, 0,
			gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
			ret,
			(thumbnail_width - gdk_pixbuf_get_width (pixbuf)) / 2,
			(thumbnail_height - gdk_pixbuf_get_height (pixbuf)) / 2);
  g_object_unref (pixbuf);

  scale_factor = bg_source_get_scale_factor (source);
  surface = gdk_cairo_surface_create_from_pixbuf (ret, scale_factor, NULL);
  g_object_unref (ret);
  g_clear_object (&icon_info);

  return surface;
}

static GFile *
bg_pictures_source_get_cache_file (void)
{
  char *path;
  GFile *file;

  path = bg_pictures_source_get_cache_path ();
  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

static gboolean
add_single_file (BgPicturesSource     *bg_source,
                 GFile                *file,
                 const gchar          *content_type,
                 guint64               mtime,
                 GtkTreeRowReference **ret_row_ref)
{
  CcBackgroundItem *item = NULL;
  CcBackgroundItemFlags flags = 0;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkTreePath *path = NULL;
  GtkTreeRowReference *row_ref = NULL;
  cairo_surface_t *surface = NULL;
  char *source_uri = NULL;
  char *uri = NULL;
  gboolean needs_download;
  gboolean retval = FALSE;
  GFile *pictures_dir, *cache_dir;
  GrlMedia *media;

  /* find png and jpeg files */
  if (!content_type)
    goto out;
  if (!in_content_types (content_type))
    goto out;

  /* create a new CcBackgroundItem */
  uri = g_file_get_uri (file);

  pictures_dir = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
  cache_dir = bg_pictures_source_get_cache_file ();
  needs_download = !g_file_has_parent (file, pictures_dir) &&
          !g_file_has_parent (file, cache_dir);
  g_object_unref (pictures_dir);
  g_object_unref (cache_dir);

  if (!needs_download)
    {
      source_uri = g_strdup (uri);
      flags |= CC_BACKGROUND_ITEM_HAS_URI;
    }
  else
    {
      source_uri = uri;
      uri = NULL;
    }

  item = cc_background_item_new (uri);
  flags |= CC_BACKGROUND_ITEM_HAS_SHADING | CC_BACKGROUND_ITEM_HAS_PLACEMENT;
  g_object_set (G_OBJECT (item),
		"flags", flags,
		"shading", G_DESKTOP_BACKGROUND_SHADING_SOLID,
		"placement", G_DESKTOP_BACKGROUND_STYLE_ZOOM,
                "modified", mtime,
                "needs-download", needs_download,
                "source-url", source_uri,
		NULL);

  if (!ret_row_ref && in_screenshot_types (content_type))
    goto read_file;

  surface = get_content_loading_icon (BG_SOURCE (bg_source));
  store = bg_source_get_liststore (BG_SOURCE (bg_source));

  /* insert the item into the liststore */
  gtk_list_store_insert_with_values (store, &iter, -1,
                                     0, surface,
                                     1, item,
                                     -1);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
  g_object_set_data_full (G_OBJECT (item), "row-ref", row_ref, (GDestroyNotify) gtk_tree_row_reference_free);


 read_file:

  media = g_object_get_data (G_OBJECT (file), "grl-media");
  if (media == NULL)
    {
      g_object_set_data_full (G_OBJECT (file), "item", g_object_ref (item), g_object_unref);
      g_file_read_async (file, G_PRIORITY_DEFAULT,
                         bg_source->priv->cancellable,
                         picture_opened_for_read, bg_source);
    }
  else
    {
      GFile *native_file;
      GFile *thumbnail_file = NULL;
      gchar *native_dir;
      gchar *native_path;
      const gchar *title;
      const gchar *thumbnail_uri;

      title = grl_media_get_title (media);
      g_object_set (G_OBJECT (item), "name", title, NULL);

      thumbnail_uri = grl_media_get_thumbnail (media);
      thumbnail_file = g_file_new_for_uri (thumbnail_uri);

      native_path = gnome_desktop_thumbnail_path_for_uri (source_uri, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
      native_file = g_file_new_for_path (native_path);

      native_dir = g_path_get_dirname (native_path);
      g_mkdir_with_parents (native_dir, USER_DIR_MODE);

      g_object_set_data_full (G_OBJECT (thumbnail_file), "item", g_object_ref (item), g_object_unref);
      g_object_set_data_full (G_OBJECT (thumbnail_file),
                              "native-file",
                              g_object_ref (native_file),
                              g_object_unref);
      g_file_copy_async (thumbnail_file,
                         native_file,
                         G_FILE_COPY_ALL_METADATA,
                         G_PRIORITY_DEFAULT,
                         bg_source->priv->cancellable,
                         NULL,
                         NULL,
                         picture_copied_for_read,
                         bg_source);

      g_clear_object (&thumbnail_file);
      g_object_unref (native_file);
      g_free (native_dir);
      g_free (native_path);
    }

  retval = TRUE;

 out:
  if (ret_row_ref)
    {
      if (row_ref && retval != FALSE)
        *ret_row_ref = gtk_tree_row_reference_copy (row_ref);
      else
        *ret_row_ref = NULL;
    }
  gtk_tree_path_free (path);
  g_clear_pointer (&surface, (GDestroyNotify) cairo_surface_destroy);
  g_clear_object (&item);
  g_object_unref (file);
  g_free (source_uri);
  g_free (uri);
  return retval;
}

static gboolean
add_single_file_from_info (BgPicturesSource     *bg_source,
                           GFile                *file,
                           GFileInfo            *info,
                           GtkTreeRowReference **ret_row_ref)
{
  const gchar *content_type;
  guint64 mtime;

  content_type = g_file_info_get_content_type (info);
  mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  return add_single_file (bg_source, file, content_type, mtime, ret_row_ref);
}

static gboolean
add_single_file_from_media (BgPicturesSource *bg_source,
                            GFile            *file,
                            GrlMedia         *media)
{
  GDateTime *mtime;
  const gchar *content_type;
  gint64 mtime_unix;

  content_type = grl_media_get_mime (media);

  /* only GRL_METADATA_KEY_CREATION_DATE is implemented in the Flickr
   * plugin, GRL_METADATA_KEY_MODIFICATION_DATE is not
   */
  mtime = grl_media_get_creation_date (media);
  if (!mtime)
    mtime = grl_media_get_modification_date (media);
  if (mtime)
    mtime_unix = g_date_time_to_unix (mtime);
  else
    mtime_unix = g_get_real_time () / G_USEC_PER_SEC;

  return add_single_file (bg_source, file, content_type, (guint64) mtime_unix, NULL);
}

gboolean
bg_pictures_source_add (BgPicturesSource     *bg_source,
                        const char           *uri,
                        GtkTreeRowReference **ret_row_ref)
{
  GFile *file;
  GFileInfo *info;
  gboolean retval;

  file = g_file_new_for_uri (uri);
  info = g_file_query_info (file, ATTRIBUTES, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info == NULL)
    return FALSE;

  retval = add_single_file_from_info (bg_source, file, info, ret_row_ref);

  return retval;
}

gboolean
bg_pictures_source_remove (BgPicturesSource *bg_source,
                           const char       *uri)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean cont;
  gboolean retval;

  retval = FALSE;
  model = GTK_TREE_MODEL (bg_source_get_liststore (BG_SOURCE (bg_source)));

  cont = gtk_tree_model_get_iter_first (model, &iter);
  while (cont)
    {
      CcBackgroundItem *tmp_item;
      const char *tmp_uri;

      gtk_tree_model_get (model, &iter, 1, &tmp_item, -1);
      tmp_uri = cc_background_item_get_uri (tmp_item);
      if (g_str_equal (tmp_uri, uri))
        {
          char *uuid;
          uuid = bg_pictures_source_get_unique_filename (uri);
          g_hash_table_insert (bg_source->priv->known_items,
			       uuid, NULL);

          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
          retval = TRUE;
          break;
        }
      g_object_unref (tmp_item);
      cont = gtk_tree_model_iter_next (model, &iter);
    }
  return retval;
}

static int
file_sort_func (gconstpointer a,
                gconstpointer b)
{
  GFileInfo *file_a = G_FILE_INFO (a);
  GFileInfo *file_b = G_FILE_INFO (b);
  guint64 modified_a, modified_b;

  modified_a = g_file_info_get_attribute_uint64 (file_a, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  modified_b = g_file_info_get_attribute_uint64 (file_b, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  return modified_b - modified_a;
}

static void
file_info_async_ready (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  BgPicturesSource *bg_source;
  GList *files, *l;
  GError *err = NULL;
  GFile *parent;

  files = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source),
                                               res,
                                               &err);
  if (err)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get pictures file information: %s", err->message);
      g_error_free (err);

      g_list_foreach (files, (GFunc) g_object_unref, NULL);
      g_list_free (files);
      return;
    }

  bg_source = BG_PICTURES_SOURCE (user_data);

  parent = g_file_enumerator_get_container (G_FILE_ENUMERATOR (source));

  files = g_list_sort (files, file_sort_func);

  /* iterate over the available files */
  for (l = files; l; l = g_list_next (l))
    {
      GFileInfo *info = l->data;
      GFile *file;

      file = g_file_get_child (parent, g_file_info_get_name (info));

      add_single_file_from_info (bg_source, file, info, NULL);
    }

  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);
}

static void
dir_enum_async_ready (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  BgPicturesSourcePrivate *priv;
  GFileEnumerator *enumerator;
  GError *err = NULL;

  enumerator = g_file_enumerate_children_finish (G_FILE (source), res, &err);

  if (err)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not fill pictures source: %s", err->message);
      g_error_free (err);
      return;
    }

  priv = BG_PICTURES_SOURCE (user_data)->priv;

  /* get the files */
  g_file_enumerator_next_files_async (enumerator,
                                      G_MAXINT,
                                      G_PRIORITY_LOW,
                                      priv->cancellable,
                                      file_info_async_ready,
                                      user_data);
  g_object_unref (enumerator);
}

char *
bg_pictures_source_get_cache_path (void)
{
  return g_build_filename (g_get_user_cache_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   NULL);
}

static char *
bg_pictures_source_get_unique_filename (const char *uri)
{
  GChecksum *csum;
  char *ret;

  csum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (csum, (guchar *) uri, -1);
  ret = g_strdup (g_checksum_get_string (csum));
  g_checksum_free (csum);

  return ret;
}

char *
bg_pictures_source_get_unique_path (const char *uri)
{
  GFile *parent, *file;
  char *cache_path;
  char *filename;
  char *ret;

  cache_path = bg_pictures_source_get_cache_path ();
  parent = g_file_new_for_path (cache_path);
  g_free (cache_path);

  filename = bg_pictures_source_get_unique_filename (uri);
  file = g_file_get_child (parent, filename);
  g_free (filename);
  ret = g_file_get_path (file);
  g_object_unref (file);
  g_object_unref (parent);

  return ret;
}

gboolean
bg_pictures_source_is_known (BgPicturesSource *bg_source,
			     const char       *uri)
{
  gboolean retval;
  char *uuid;

  uuid = bg_pictures_source_get_unique_filename (uri);
  retval = (GPOINTER_TO_INT (g_hash_table_lookup (bg_source->priv->known_items, uuid)));
  g_free (uuid);

  return retval;
}

static int
sort_func (GtkTreeModel *model,
           GtkTreeIter *a,
           GtkTreeIter *b,
           BgPicturesSource *bg_source)
{
  CcBackgroundItem *item_a;
  CcBackgroundItem *item_b;
  guint64 modified_a;
  guint64 modified_b;
  int retval;

  gtk_tree_model_get (model, a,
                      1, &item_a,
                      -1);
  gtk_tree_model_get (model, b,
                      1, &item_b,
                      -1);

  modified_a = cc_background_item_get_modified (item_a);
  modified_b = cc_background_item_get_modified (item_b);

  retval = modified_b - modified_a;

  g_object_unref (item_a);
  g_object_unref (item_b);

  return retval;
}

static void
file_info_ready (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GFileInfo *info;
  GError *error = NULL;
  GFile *file = G_FILE (object);

  info = g_file_query_info_finish (file, res, &error);

  if (!info)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Problem looking up file info: %s", error->message);
      g_clear_error (&error);
      return;
    }

  /* Up the ref count so we can re-use the add_single_item code path which
   * reduces the ref count.
   */
  g_object_ref (file);
  add_single_file_from_info (BG_PICTURES_SOURCE (user_data), file, info, NULL);
}

static void
file_added (GFile            *file,
            BgPicturesSource *self)
{
  char *uri;
  uri = g_file_get_uri (file);

  if (!bg_pictures_source_is_known (self, uri))
    {
      g_file_query_info_async (file,
                               ATTRIBUTES,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_LOW,
                               NULL,
                               file_info_ready,
                               self);
    }

  g_free (uri);
}

static void
files_changed_cb (GFileMonitor      *monitor,
                  GFile             *file,
                  GFile             *other_file,
                  GFileMonitorEvent  event_type,
                  gpointer           user_data)
{
  BgPicturesSource *self = BG_PICTURES_SOURCE (user_data);
  char *uri;

  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        file_added (file, self);
        break;

      case G_FILE_MONITOR_EVENT_DELETED:
        uri = g_file_get_uri (file);
        bg_pictures_source_remove (self, uri);
        g_free (uri);
        break;

      default:
        return;
    }
}

static GFileMonitor *
monitor_path (BgPicturesSource *self,
              const char       *path)
{
  GFileMonitor *monitor;
  GFile *dir;

  g_mkdir_with_parents (path, USER_DIR_MODE);

  dir = g_file_new_for_path (path);
  g_file_enumerate_children_async (dir,
                                   ATTRIBUTES,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW, self->priv->cancellable,
                                   dir_enum_async_ready, self);

  monitor = g_file_monitor_directory (dir,
                                      G_FILE_MONITOR_NONE,
                                      self->priv->cancellable,
                                      NULL);

  if (monitor)
    g_signal_connect (monitor,
                      "changed",
                      G_CALLBACK (files_changed_cb),
                      self);

  g_object_unref (dir);

  return monitor;
}

static void
media_found_cb (BgPicturesSource *self, GrlMedia *media)
{
  GFile *file = NULL;
  const gchar *uri;

  uri = grl_media_get_url (media);
  file = g_file_new_for_uri (uri);
  g_object_set_data_full (G_OBJECT (file), "grl-media", g_object_ref (media), g_object_unref);
  add_single_file_from_media (self, file, media);
}

static void
bg_pictures_source_init (BgPicturesSource *self)
{
  const gchar *pictures_path;
  BgPicturesSourcePrivate *priv;
  char *cache_path;
  GtkListStore *store;

  priv = self->priv = PICTURES_SOURCE_PRIVATE (self);

  priv->cancellable = g_cancellable_new ();
  priv->known_items = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify) g_free,
					     NULL);

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures_path == NULL)
    pictures_path = g_get_home_dir ();

  priv->picture_dir_monitor = monitor_path (self, pictures_path);

  cache_path = bg_pictures_source_get_cache_path ();
  priv->cache_dir_monitor = monitor_path (self, cache_path);
  g_free (cache_path);

  priv->grl_miner = cc_background_grilo_miner_new ();
  g_signal_connect_swapped (priv->grl_miner, "media-found", G_CALLBACK (media_found_cb), self);
  cc_background_grilo_miner_start (priv->grl_miner);

  priv->thumb_factory =
    gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  store = bg_source_get_liststore (BG_SOURCE (self));

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                   1,
                                   (GtkTreeIterCompareFunc)sort_func,
                                   self,
                                   NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        1,
                                        GTK_SORT_ASCENDING);
}

BgPicturesSource *
bg_pictures_source_new (GtkWindow *window)
{
  return g_object_new (BG_TYPE_PICTURES_SOURCE, "window", window, NULL);
}

const char * const *
bg_pictures_get_support_content_types (void)
{
	return content_types;
}
