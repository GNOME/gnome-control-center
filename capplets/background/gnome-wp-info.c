/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
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

#include <config.h>
#include <gnome.h>
#include "gnome-wp-info.h"

GnomeWPInfo * gnome_wp_info_new (const gchar * uri,
				 GnomeThumbnailFactory * thumbs) {
  GnomeWPInfo * new;
  GnomeVFSFileInfo * info;
  GnomeVFSResult result;

  info = gnome_vfs_file_info_new ();
  result = gnome_vfs_get_file_info (gnome_vfs_escape_path_string (uri), info,
				    GNOME_VFS_FILE_INFO_DEFAULT |
				    GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
				    GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (info == NULL || info->mime_type == NULL || result != GNOME_VFS_OK) {
    if (!strcmp (uri, "(none)")) {
      new = g_new0 (GnomeWPInfo, 1);

      new->mime_type = g_strdup ("image/x-no-data");
      new->uri = g_strdup (uri);

      new->thumburi = g_strconcat (g_get_home_dir (),
				   "/.thumbnails/normal/",
				   gnome_thumbnail_md5 (gnome_vfs_escape_path_string (uri)),
				   ".png",
				   NULL);

      new->name = g_strdup (_("No Wallpaper"));

      new->size = 0;
    } else {
      new = NULL;
    }
  } else {
    new = g_new0 (GnomeWPInfo, 1);

    new->uri = g_strdup (uri);

    new->thumburi = gnome_thumbnail_factory_lookup (thumbs,
						    gnome_vfs_escape_path_string (new->uri),
						    info->mtime);
    if (new->thumburi == NULL) {
      new->thumburi = g_strconcat (g_get_home_dir (),
				   "/.thumbnails/normal/",
				   gnome_thumbnail_md5 (gnome_vfs_escape_path_string (uri)),
				   ".png",
				   NULL);
    }
    new->name = g_strdup (info->name);
    new->mime_type = g_strdup (info->mime_type);

    new->size = info->size;
    new->mtime = info->mtime;
  }
  gnome_vfs_file_info_unref (info);

  return new;
}

GnomeWPInfo * gnome_wp_info_dup (const GnomeWPInfo * info) {
  GnomeWPInfo * new;

  new = g_new0 (GnomeWPInfo, 1);

  new->uri = g_strdup (info->uri);
  new->thumburi = g_strdup (info->uri);

  new->name = g_strdup (info->name);
  new->mime_type = g_strdup (info->mime_type);

  new->size = info->size;
  new->mtime = info->mtime;

  return new;
}

void gnome_wp_info_free (GnomeWPInfo * info) {
  if (info == NULL) {
    return;
  }

  g_free (info->uri);
  g_free (info->thumburi);
  g_free (info->name);
  g_free (info->mime_type);

  info = NULL;
}

