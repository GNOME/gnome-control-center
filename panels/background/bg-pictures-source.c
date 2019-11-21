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

#define ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED

struct _BgPicturesSource
{
  BgSource parent_instance;

  GCancellable *cancellable;

  CcBackgroundGriloMiner *grl_miner;

  GFileMonitor *picture_dir_monitor;
  GFileMonitor *cache_dir_monitor;

  GHashTable *known_items;
};

G_DEFINE_TYPE (BgPicturesSource, bg_pictures_source, BG_TYPE_SOURCE)

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
  BgPicturesSource *source = BG_PICTURES_SOURCE (object);

  if (source->cancellable)
    {
      g_cancellable_cancel (source->cancellable);
      g_clear_object (&source->cancellable);
    }

  g_clear_object (&source->grl_miner);

  G_OBJECT_CLASS (bg_pictures_source_parent_class)->dispose (object);
}

static void
bg_pictures_source_finalize (GObject *object)
{
  BgPicturesSource *bg_source = BG_PICTURES_SOURCE (object);

  g_clear_pointer (&bg_source->known_items, g_hash_table_destroy);

  g_clear_object (&bg_source->picture_dir_monitor);
  g_clear_object (&bg_source->cache_dir_monitor);

  G_OBJECT_CLASS (bg_pictures_source_parent_class)->finalize (object);
}

static void
bg_pictures_source_class_init (BgPicturesSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bg_pictures_source_dispose;
  object_class->finalize = bg_pictures_source_finalize;
}

static void
remove_placeholder (BgPicturesSource *bg_source,
                    CcBackgroundItem *item)
{
  GListStore *store;
  guint i;

  store = bg_source_get_liststore (BG_SOURCE (bg_source));

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      g_autoptr(CcBackgroundItem) item_n = NULL;

      item_n = g_list_model_get_item (G_LIST_MODEL (store), i);

      if (item_n == item)
        {
          g_list_store_remove (store, i);
          break;
        }
    }
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

static int
sort_func (gconstpointer a,
           gconstpointer b,
           gpointer      user_data)
{
  CcBackgroundItem *item_a;
  CcBackgroundItem *item_b;
  guint64 modified_a;
  guint64 modified_b;
  int retval;

  item_a = (CcBackgroundItem *) a;
  item_b = (CcBackgroundItem *) b;
  modified_a = cc_background_item_get_modified (item_a);
  modified_b = cc_background_item_get_modified (item_b);

  retval = modified_b - modified_a;

  return retval;
}

static void
picture_scaled (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  g_autoptr(GError) error = NULL;
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  const char *software;
  const char *uri;
  GListStore *store;
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

      return;
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
      return;
    }

  /* Process embedded orientation */
  rotation_applied = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "rotation-applied"));

  if (!rotation_applied && picture_needs_rotation (pixbuf))
    {
      /* the width and height of pixbuf we requested are wrong for EXIF
       * orientations 5, 6, 7 and 8. the file has to be reloaded. */
      g_autoptr(GFile) file = NULL;

      file = g_file_new_for_uri (uri);
      g_object_set_data (G_OBJECT (item), "needs-rotation", GINT_TO_POINTER (TRUE));
      g_object_set_data_full (G_OBJECT (file), "item", g_object_ref (item), g_object_unref);
      g_file_read_async (G_FILE (file), G_PRIORITY_DEFAULT,
                         bg_source->cancellable,
                         picture_opened_for_read, bg_source);
      return;
    }

  pixbuf = swap_rotated_pixbuf (pixbuf);

  scale_factor = bg_source_get_scale_factor (BG_SOURCE (bg_source));
  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
  cc_background_item_load (item, NULL);

  /* insert the item into the liststore */
  g_list_store_insert_sorted (store, item, sort_func, bg_source);

  g_hash_table_insert (bg_source->known_items,
                       bg_pictures_source_get_unique_filename (uri),
                       GINT_TO_POINTER (TRUE));

  g_clear_pointer (&surface, cairo_surface_destroy);
}

static void
picture_opened_for_read (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  g_autoptr(GFileInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  gint thumbnail_height;
  gint thumbnail_width;
  gboolean needs_rotation;

  item = g_object_get_data (source_object, "item");
  stream = g_file_read_finish (G_FILE (source_object), res, &error);
  if (stream == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_autofree gchar *filename = g_file_get_path (G_FILE (source_object));
          g_warning ("Failed to load picture '%s': %s", filename, error->message);
          remove_placeholder (BG_PICTURES_SOURCE (user_data), item);
        }

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
                                             bg_source->cancellable,
                                             picture_scaled, bg_source);
}

static void
picture_copied_for_read (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  BgPicturesSource *bg_source;
  CcBackgroundItem *item;
  g_autoptr(GError) error = NULL;
  GFile *thumbnail_file = G_FILE (source_object);
  GFile *native_file;

  if (!g_file_copy_finish (thumbnail_file, res, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_autofree gchar *uri = NULL;

          uri = g_file_get_uri (thumbnail_file);
          g_warning ("Failed to download '%s': %s", uri, error->message);
          return;
        }
    }

  bg_source = BG_PICTURES_SOURCE (user_data);

  native_file = g_object_get_data (G_OBJECT (thumbnail_file), "native-file");
  item = g_object_get_data (G_OBJECT (thumbnail_file), "item");
  g_object_set_data_full (G_OBJECT (native_file), "item", g_object_ref (item), g_object_unref);
  g_file_read_async (native_file,
                     G_PRIORITY_DEFAULT,
                     bg_source->cancellable,
                     picture_opened_for_read,
                     bg_source);
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

static GFile *
bg_pictures_source_get_cache_file (void)
{
  g_autofree gchar *path = NULL;
  GFile *file;

  path = bg_pictures_source_get_cache_path ();
  file = g_file_new_for_path (path);

  return file;
}

static gboolean
add_single_file (BgPicturesSource     *bg_source,
                 GFile                *file,
                 const gchar          *content_type,
                 guint64               mtime)
{
  g_autoptr(CcBackgroundItem) item = NULL;
  CcBackgroundItemFlags flags = 0;
  g_autofree gchar *source_uri = NULL;
  g_autofree gchar *uri = NULL;
  gboolean needs_download;
  gboolean retval = FALSE;
  const gchar *pictures_path;
  g_autoptr(GFile) pictures_dir = NULL;
  g_autoptr(GFile) cache_dir = NULL;
  GrlMedia *media;

  /* find png and jpeg files */
  if (!content_type)
    goto out;
  if (!in_content_types (content_type))
    goto out;

  /* create a new CcBackgroundItem */
  uri = g_file_get_uri (file);

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures_path == NULL)
    pictures_path = g_get_home_dir ();
  pictures_dir = g_file_new_for_path (pictures_path);
  cache_dir = bg_pictures_source_get_cache_file ();
  needs_download = !g_file_has_parent (file, pictures_dir) &&
          !g_file_has_parent (file, cache_dir);

  if (!needs_download)
    {
      source_uri = g_strdup (uri);
      flags |= CC_BACKGROUND_ITEM_HAS_URI;
    }
  else
    {
      source_uri = g_steal_pointer (&uri);
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

  media = g_object_get_data (G_OBJECT (file), "grl-media");
  if (media == NULL)
    {
      g_object_set_data_full (G_OBJECT (file), "item", g_object_ref (item), g_object_unref);
      g_file_read_async (file, G_PRIORITY_DEFAULT,
                         bg_source->cancellable,
                         picture_opened_for_read, bg_source);
    }
  else
    {
      g_autoptr(GFile) native_file = NULL;
      g_autoptr(GFile) thumbnail_file = NULL;
      g_autofree gchar *native_dir = NULL;
      g_autofree gchar *native_path = NULL;
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
                         bg_source->cancellable,
                         NULL,
                         NULL,
                         picture_copied_for_read,
                         bg_source);
    }

  retval = TRUE;

 out:
  return retval;
}

static gboolean
add_single_file_from_info (BgPicturesSource *bg_source,
                           GFile            *file,
                           GFileInfo        *info)
{
  const gchar *content_type;
  guint64 mtime;

  content_type = g_file_info_get_content_type (info);
  mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  return add_single_file (bg_source, file, content_type, mtime);
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

  return add_single_file (bg_source, file, content_type, (guint64) mtime_unix);
}

gboolean
bg_pictures_source_add (BgPicturesSource     *bg_source,
                        const char           *uri,
                        GtkTreeRowReference **ret_row_ref)
{
  g_autoptr(GFile) file = NULL;
  GFileInfo *info;
  gboolean retval;

  file = g_file_new_for_uri (uri);
  info = g_file_query_info (file, ATTRIBUTES, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info == NULL)
    return FALSE;

  retval = add_single_file_from_info (bg_source, file, info);

  return retval;
}

gboolean
bg_pictures_source_remove (BgPicturesSource *bg_source,
                           const char       *uri)
{
  GListStore *store;
  gboolean retval;
  guint i;

  retval = FALSE;
  store = bg_source_get_liststore (BG_SOURCE (bg_source));

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      g_autoptr(CcBackgroundItem) tmp_item = NULL;
      const char *tmp_uri;

      tmp_item = g_list_model_get_item (G_LIST_MODEL (store), i);
      tmp_uri = cc_background_item_get_uri (tmp_item);
      if (g_str_equal (tmp_uri, uri))
        {
          char *uuid;
          uuid = bg_pictures_source_get_unique_filename (uri);
          g_hash_table_insert (bg_source->known_items,
			       uuid, NULL);

          g_list_store_remove (store, i);
          retval = TRUE;
          break;
        }
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
  g_autoptr(GError) err = NULL;
  GFile *parent;

  files = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source),
                                               res,
                                               &err);
  if (err)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get pictures file information: %s", err->message);

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
      g_autoptr(GFile) file = NULL;

      file = g_file_get_child (parent, g_file_info_get_name (info));

      add_single_file_from_info (bg_source, file, info);
    }

  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);
}

static void
dir_enum_async_ready (GObject      *s,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  BgPicturesSource *source = (BgPicturesSource *) user_data;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) err = NULL;

  enumerator = g_file_enumerate_children_finish (G_FILE (s), res, &err);

  if (err)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not fill pictures source: %s", err->message);
      return;
    }

  /* get the files */
  g_file_enumerator_next_files_async (enumerator,
                                      G_MAXINT,
                                      G_PRIORITY_LOW,
                                      source->cancellable,
                                      file_info_async_ready,
                                      user_data);
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
  g_autoptr(GChecksum) csum = NULL;
  char *ret;

  csum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (csum, (guchar *) uri, -1);
  ret = g_strdup (g_checksum_get_string (csum));

  return ret;
}

char *
bg_pictures_source_get_unique_path (const char *uri)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *cache_path = NULL;
  g_autofree gchar *filename = NULL;

  cache_path = bg_pictures_source_get_cache_path ();
  parent = g_file_new_for_path (cache_path);

  filename = bg_pictures_source_get_unique_filename (uri);
  file = g_file_get_child (parent, filename);

  return g_file_get_path (file);
}

gboolean
bg_pictures_source_is_known (BgPicturesSource *bg_source,
			     const char       *uri)
{
  g_autofree gchar *uuid = NULL;

  uuid = bg_pictures_source_get_unique_filename (uri);

  return GPOINTER_TO_INT (g_hash_table_lookup (bg_source->known_items, uuid));
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

  add_single_file_from_info (BG_PICTURES_SOURCE (user_data), file, info);
}

static void
file_added (GFile            *file,
            BgPicturesSource *self)
{
  g_autofree gchar *uri = NULL;
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
}

static void
files_changed_cb (BgPicturesSource  *self,
                  GFile             *file,
                  GFile             *other_file,
                  GFileMonitorEvent  event_type)
{
  g_autofree gchar *uri = NULL;

  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        file_added (file, self);
        break;

      case G_FILE_MONITOR_EVENT_DELETED:
        uri = g_file_get_uri (file);
        bg_pictures_source_remove (self, uri);
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
  g_autoptr(GFile) dir = NULL;

  g_mkdir_with_parents (path, USER_DIR_MODE);

  dir = g_file_new_for_path (path);
  g_file_enumerate_children_async (dir,
                                   ATTRIBUTES,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW, self->cancellable,
                                   dir_enum_async_ready, self);

  monitor = g_file_monitor_directory (dir,
                                      G_FILE_MONITOR_NONE,
                                      self->cancellable,
                                      NULL);

  if (monitor)
    g_signal_connect_object (monitor,
                             "changed",
                             G_CALLBACK (files_changed_cb),
                             self, G_CONNECT_SWAPPED);

  return monitor;
}

static void
media_found_cb (BgPicturesSource *self, GrlMedia *media)
{
  g_autoptr(GFile) file = NULL;
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
  g_autofree gchar *cache_path = NULL;

  self->cancellable = g_cancellable_new ();
  self->known_items = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify) g_free,
					     NULL);

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures_path == NULL)
    pictures_path = g_get_home_dir ();

  self->picture_dir_monitor = monitor_path (self, pictures_path);

  cache_path = bg_pictures_source_get_cache_path ();
  self->cache_dir_monitor = monitor_path (self, cache_path);

  self->grl_miner = cc_background_grilo_miner_new ();
  g_signal_connect_object (self->grl_miner, "media-found", G_CALLBACK (media_found_cb), self, G_CONNECT_SWAPPED);
  cc_background_grilo_miner_start (self->grl_miner);
}

BgPicturesSource *
bg_pictures_source_new (GtkWidget *widget)
{
  return g_object_new (BG_TYPE_PICTURES_SOURCE, "widget", widget, NULL);
}

const char * const *
bg_pictures_get_support_content_types (void)
{
	return content_types;
}
