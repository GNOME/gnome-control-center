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

#ifndef _GNOME_WP_XML_H_
#define _GNOME_WP_XML_H_

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gio/gio.h>

typedef struct _GnomeWpXml GnomeWpXml;

struct _GnomeWpXml
{
  GHashTable *wp_hash;
  GnomeDesktopThumbnailFactory *thumb_factory;
  GSettings *settings;
  gint thumb_height;
  gint thumb_width;
  GtkListStore *wp_model;
};

void gnome_wp_xml_load_list (GnomeWpXml *data);
void gnome_wp_xml_save_list (GnomeWpXml *data);
/* FIXME this should be an iterator instead, so the bg
 * pops up as soon as a new one is available */
void gnome_wp_xml_load_list_async (GnomeWpXml *data,
				   GCancellable *cancellable,
				   GAsyncReadyCallback callback,
				   gpointer user_data);
/* FIXME, this is ugly API, which wouldn't be
 * needed if this was an object */
GnomeWpXml *gnome_wp_xml_load_list_finish (GAsyncResult  *async_result);

#endif

