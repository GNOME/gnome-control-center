/* -*- mode: c; style: linux -*- */

/* capplet-dir.h
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __CAPPLET_DIR_H
#define __CAPPLET_DIR_H

#include <gnome.h>
#include <libgnome/gnome-desktop-item.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define CAPPLET_DIR_ENTRY(obj) ((CappletDirEntry *) obj)
#define CAPPLET_DIR(obj) ((CappletDir *) obj)
#define CAPPLET(obj) ((Capplet *) obj)

#define IS_CAPPLET(obj) (((CappletDirEntry *) obj)->type == TYPE_CAPPLET)
#define IS_CAPPLET_DIR(obj) (((CappletDirEntry *) obj)->type == TYPE_CAPPLET_DIR)

typedef struct _CappletDirEntry CappletDirEntry;
typedef struct _CappletDir CappletDir;
typedef struct _Capplet Capplet;

typedef struct _CappletDirView CappletDirView;

typedef enum {
	TYPE_CAPPLET,
	TYPE_CAPPLET_DIR
} CappletEntryType;

struct _CappletDirEntry 
{
	CappletEntryType type;
	GnomeDesktopItem *entry;
	gchar **exec;
	gchar *label;
	gchar *icon;
	gchar *path;
	GdkPixbuf *pb;
	CappletDir *dir;	
};

struct _CappletDir
{
	CappletDirEntry entry;
	GSList *entries;
	CappletDirView *view;
};

struct _Capplet
{
	CappletDirEntry entry;
	gboolean launching;
};

CappletDirEntry *capplet_new                (CappletDir *dir,
					     gchar *desktop_path);
CappletDirEntry *capplet_dir_new            (CappletDir *dir, gchar *dir_path);

CappletDirEntry *capplet_lookup             (const char *path);

char            *capplet_dir_entry_get_html (CappletDirEntry *entry);

void             capplet_dir_entry_destroy  (CappletDirEntry *entry);

void             capplet_dir_entry_activate (CappletDirEntry *entry,
					     CappletDirView *launcher);
void             capplet_dir_entry_shutdown (CappletDirEntry *entry);

void             capplet_dir_load           (CappletDir *dir);

void             capplet_dir_init           (CappletDirView *(*cb) 
					     (CappletDir *,
					      CappletDirView *));

CappletDir      *get_root_capplet_dir       (void);

GtkWidget       *capplet_control_launch     (const gchar *capplet_name,
					     gchar *window_title);

#endif /* __CAPPLET_DIR_H */
