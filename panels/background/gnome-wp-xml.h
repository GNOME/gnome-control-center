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

#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-desktop-thumbnail.h>

typedef struct _GnomeWpXml GnomeWpXml;

struct _GnomeWpXml
{
  GHashTable *wp_hash;
  GnomeDesktopThumbnailFactory *thumb_factory;
  GConfClient *client;
  gint thumb_height;
  gint thumb_width;
  GtkListStore *wp_model;
};

void gnome_wp_xml_load_list (GnomeWpXml *data);
void gnome_wp_xml_save_list (GnomeWpXml *data);

#endif

