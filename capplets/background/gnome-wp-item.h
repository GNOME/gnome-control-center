/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2005 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtktreeview.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <gnome-wp-info.h>
#include <libgnomevfs/gnome-vfs.h>

#ifndef _GNOME_WP_ITEM_H_
#define _GNOME_WP_ITEM_H_

typedef struct _GnomeWPItem GnomeWPItem;

struct _GnomeWPItem {
  gchar * name;
  gchar * filename;
  gchar * description;
  gchar * imguri;
  gchar * options;
  gchar * shade_type;
  gchar * pri_color;
  gchar * sec_color;

  /* Where the Item is in the List */
  GtkTreeRowReference * rowref;

  /* Real colors */
  GdkColor * pcolor;
  GdkColor * scolor;

  GnomeWPInfo * fileinfo;
  GnomeWPInfo * uriinfo;

  /* Did the user remove us? */
  gboolean deleted;

  /* Width and Height of the original image */
  gint width;
  gint height;
};

GnomeWPItem * gnome_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 GnomeThumbnailFactory * thumbnails);
void gnome_wp_item_free (GnomeWPItem * item);
GdkPixbuf * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeThumbnailFactory * thumbs);
void gnome_wp_item_update_description (GnomeWPItem * item);

#endif

