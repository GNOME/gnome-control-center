/* bg-wallpapers-source.c */
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


#include "bg-wallpapers-source.h"

#include "gnome-wp-item.h"
#include "gnome-wp-xml.h"

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gio/gio.h>

G_DEFINE_TYPE (BgWallpapersSource, bg_wallpapers_source, BG_TYPE_SOURCE)

#define WALLPAPERS_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_WALLPAPERS_SOURCE, BgWallpapersSourcePrivate))

struct _BgWallpapersSourcePrivate
{
  GtkListStore *store;
  GnomeDesktopThumbnailFactory *thumb_factory;
  guint reload_id;
};


static void
bg_wallpapers_source_get_property (GObject    *object,
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
bg_wallpapers_source_set_property (GObject      *object,
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
bg_wallpapers_source_dispose (GObject *object)
{
  BgWallpapersSourcePrivate *priv = BG_WALLPAPERS_SOURCE (object)->priv;

  if (priv->thumb_factory)
    {
      g_object_unref (priv->thumb_factory);
      priv->thumb_factory = NULL;
    }

  if (priv->reload_id != 0)
    {
      g_source_remove (priv->reload_id);
      priv->reload_id = 0;
    }

  G_OBJECT_CLASS (bg_wallpapers_source_parent_class)->dispose (object);
}

static void
bg_wallpapers_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (bg_wallpapers_source_parent_class)->finalize (object);
}

static void
bg_wallpapers_source_class_init (BgWallpapersSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgWallpapersSourcePrivate));

  object_class->get_property = bg_wallpapers_source_get_property;
  object_class->set_property = bg_wallpapers_source_set_property;
  object_class->dispose = bg_wallpapers_source_dispose;
  object_class->finalize = bg_wallpapers_source_finalize;
}

static gboolean
find_wallpaper (gpointer key,
                gpointer value,
                gpointer data)
{
  GnomeBG *bg = data;
  GnomeWPItem *item = value;

  return item->bg == bg;
}

/* FIXME: Is this used for anything? */
static void
item_changed_cb (GnomeBG    *bg,
                 GnomeWpXml *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GnomeWPItem *item;

  item = g_hash_table_find (data->wp_hash, find_wallpaper, bg);

  if (!item)
    return;

  model = gtk_tree_row_reference_get_model (item->rowref);
  path = gtk_tree_row_reference_get_path (item->rowref);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      GdkPixbuf *pixbuf;

      g_signal_handlers_block_by_func (bg, G_CALLBACK (item_changed_cb), data);

      pixbuf = gnome_wp_item_get_thumbnail (item,
                                            data->thumb_factory,
                                            data->thumb_width,
                                            data->thumb_height);
      if (pixbuf)
        {
          gtk_list_store_set (GTK_LIST_STORE (data->wp_model), &iter,
                              0, pixbuf, -1);
          g_object_unref (pixbuf);
        }

      g_signal_handlers_unblock_by_func (bg, G_CALLBACK (item_changed_cb),
                                         data);
    }
}




static void
load_wallpapers (gchar              *key,
                 GnomeWPItem        *item,
                 BgWallpapersSource *source)
{
  BgWallpapersSourcePrivate *priv = source->priv;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixbuf *pixbuf;
  GtkListStore *store = bg_source_get_liststore (BG_SOURCE (source));

  if (item->deleted == TRUE)
    return;

  gtk_list_store_append (store, &iter);

  pixbuf = gnome_wp_item_get_thumbnail (item, priv->thumb_factory,
                                        THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);
  gnome_wp_item_update_size (item);

  gtk_list_store_set (store, &iter,
                      0, pixbuf,
                      1, item,
                      -1);

  if (pixbuf)
    g_object_unref (pixbuf);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  item->rowref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
  gtk_tree_path_free (path);
}

static gboolean
reload_wallpapers (BgWallpapersSource *self)
{
  GnomeWpXml *wp_xml;

  /* set up wallpaper source */
  wp_xml = g_new0 (GnomeWpXml, 1);
  wp_xml->wp_hash = g_hash_table_new (g_str_hash, g_str_equal);
  wp_xml->settings = g_settings_new (WP_PATH_ID);
  wp_xml->wp_model = bg_source_get_liststore (BG_SOURCE (self));
  wp_xml->thumb_width = THUMBNAIL_WIDTH;
  wp_xml->thumb_height = THUMBNAIL_HEIGHT;
  wp_xml->thumb_factory = self->priv->thumb_factory;

  gnome_wp_xml_load_list (wp_xml);
  g_hash_table_foreach (wp_xml->wp_hash,
                        (GHFunc) load_wallpapers,
                        self);

  g_hash_table_destroy (wp_xml->wp_hash);
  g_object_unref (wp_xml->settings);
  g_free (wp_xml);

  self->priv->reload_id = 0;

  return FALSE;
}

static void
bg_wallpapers_source_init (BgWallpapersSource *self)
{
  BgWallpapersSourcePrivate *priv;

  priv = self->priv = WALLPAPERS_SOURCE_PRIVATE (self);

  priv->thumb_factory =
    gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  priv->reload_id = g_idle_add ((GSourceFunc)reload_wallpapers, self);
}

BgWallpapersSource *
bg_wallpapers_source_new (void)
{
  return g_object_new (BG_TYPE_WALLPAPERS_SOURCE, NULL);
}

