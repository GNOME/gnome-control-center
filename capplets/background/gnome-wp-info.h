/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2004 Novell, Inc. (www.novell.com)
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

#ifndef _GNOME_WP_INFO_H_
#define _GNOME_WP_INFO_H_

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-thumbnail.h>

typedef struct _GnomeWPInfo GnomeWPInfo;

struct _GnomeWPInfo {
  gchar * uri;
  gchar * thumburi;
  gchar * name;
  gchar * mime_type;

  GnomeVFSFileSize size;

  time_t mtime;
};

GnomeWPInfo * gnome_wp_info_new (const gchar * uri,
				 GnomeThumbnailFactory * thumbs);
GnomeWPInfo * gnome_wp_info_dup (const GnomeWPInfo * info);
void gnome_wp_info_free (GnomeWPInfo * info);

#endif

