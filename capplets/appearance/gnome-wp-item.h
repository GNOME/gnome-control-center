/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
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
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

#ifndef _GNOME_WP_ITEM_H_
#define _GNOME_WP_ITEM_H_

#define WP_PATH_KEY "/desktop/gnome/background"
#define WP_FILE_KEY WP_PATH_KEY "/picture_filename"
#define WP_OPTIONS_KEY WP_PATH_KEY "/picture_options"
#define WP_SHADING_KEY WP_PATH_KEY "/color_shading_type"
#define WP_PCOLOR_KEY WP_PATH_KEY "/primary_color"
#define WP_SCOLOR_KEY WP_PATH_KEY "/secondary_color"
#define WP_KEYBOARD_PATH "/desktop/gnome/peripherals/keyboard"
#define WP_DELAY_KEY WP_KEYBOARD_PATH "/delay"

typedef struct _GnomeWPItem GnomeWPItem;

struct _GnomeWPItem {
  GnomeBG *bg;
    
  gchar * name;
  gchar * filename;
  gchar * description;
  gchar * options;
  gchar * shade_type;

  /* Where the Item is in the List */
  GtkTreeRowReference * rowref;

  /* Real colors */
  GdkColor * pcolor;
  GdkColor * scolor;

  GnomeWPInfo * fileinfo;

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
void gnome_wp_item_ensure_gnome_bg (GnomeWPItem *item);

#endif

