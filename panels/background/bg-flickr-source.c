/* bg-flickr-source.c */
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

#include "bg-flickr-source.h"

#include <libsocialweb-client/sw-client.h>
#include <libsocialweb-client/sw-item.h>
#include <libsocialweb-client/sw-client-service.h>

#include "cc-background-item.h"
#include <gdesktop-enums.h>

G_DEFINE_TYPE (BgFlickrSource, bg_flickr_source, BG_TYPE_SOURCE)

#define FLICKR_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_FLICKR_SOURCE, BgFlickrSourcePrivate))

struct _BgFlickrSourcePrivate
{
  SwClient *client;
  SwClientService *service;
};


static void
bg_flickr_source_dispose (GObject *object)
{
  BgFlickrSourcePrivate *priv = BG_FLICKR_SOURCE (object)->priv;

  if (priv->client)
    {
      g_object_unref (priv->client);
      priv->client = NULL;
    }

  if (priv->service)
    {
      g_object_unref (priv->service);
      priv->service = NULL;
    }

  G_OBJECT_CLASS (bg_flickr_source_parent_class)->dispose (object);
}

static void
bg_flickr_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (bg_flickr_source_parent_class)->finalize (object);
}

static void
bg_flickr_source_class_init (BgFlickrSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgFlickrSourcePrivate));

  object_class->dispose = bg_flickr_source_dispose;
  object_class->finalize = bg_flickr_source_finalize;
}

static void
_view_items_added_cb (SwClientItemView *item_view,
                      GList            *items,
                      gpointer          userdata)
{
  GList *l;
  BgFlickrSource *source = (BgFlickrSource *) userdata;
  GtkListStore *store = bg_source_get_liststore (BG_SOURCE (source));

  for (l = items; l; l = l->next)
    {
      CcBackgroundItem *item;
      GdkPixbuf *pixbuf;
      SwItem *sw_item = (SwItem *) l->data;
      const gchar *thumb_url;

      item = cc_background_item_new (NULL);

      g_object_set (G_OBJECT (item),
		    "placement", G_DESKTOP_BACKGROUND_STYLE_ZOOM,
		    "name", sw_item_get_value (sw_item, "title"),
		    "primary-color", "#000000000000",
		    "seconday-color", "#000000000000",
		    "shading", G_DESKTOP_BACKGROUND_SHADING_SOLID,
		    "source-url", sw_item_get_value (sw_item, "x-flickr-photo-url"),
		    NULL);

      //FIXME
//      cc_background_item_ensure_gnome_bg (item);

      /* insert the item into the liststore */
      thumb_url = sw_item_get_value (sw_item, "thumbnail");
      pixbuf = gdk_pixbuf_new_from_file_at_scale (thumb_url, THUMBNAIL_WIDTH,
                                                  THUMBNAIL_HEIGHT, TRUE,
                                                  NULL);
      gtk_list_store_insert_with_values (store, NULL, 0,
                                         0, pixbuf,
                                         1, item,
                                         -1);
      g_object_unref (pixbuf);
    }
}

static void
_query_open_view_cb (SwClientService  *service,
                     SwClientItemView *item_view,
                     gpointer          userdata)
{

  if (!item_view)
  {
    g_warning ("Could not connect to Flickr service");
    return;
  }

  g_signal_connect (item_view,
                    "items-added",
                    (GCallback)_view_items_added_cb,
                    userdata);
  sw_client_item_view_start (item_view);
}

static void
bg_flickr_source_init (BgFlickrSource *self)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  BgFlickrSourcePrivate *priv;

  priv = self->priv = FLICKR_SOURCE_PRIVATE (self);

  priv->client = sw_client_new ();
  priv->service = sw_client_get_service (priv->client, "flickr");
  sw_client_service_query_open_view (priv->service,
                                     "feed",
                                     NULL,
                                     _query_open_view_cb,
                                     self);

  thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  g_object_unref (thumb_factory);
}

BgFlickrSource *
bg_flickr_source_new (void)
{
  return g_object_new (BG_TYPE_FLICKR_SOURCE, NULL);
}

