/* -*- mode: c; style: linux -*- */

/* capplet-dir.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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

#define CAPPLET_DIR_ENTRY(obj) ((CappletDirEntry *) obj)
#define CAPPLET_DIR(obj) ((CappletDir *) obj)
#define CAPPLET(obj) ((Capplet *) obj)

typedef struct _CappletDirEntry CappletDirEntry;
typedef struct _CappletDir CappletDir;
typedef struct _Capplet Capplet;

typedef struct _CappletDirWindow CappletDirWindow;

typedef enum {
	TYPE_CAPPLET, TYPE_CAPPLET_DIR
} CappletEntryType;

struct _CappletDirEntry 
{
	CappletEntryType type;
	GnomeDesktopEntry *entry;
	gchar *label;
	gchar *icon;
};

struct _CappletDir
{
	CappletDirEntry entry;
	gchar *path;
	CappletDirEntry **entries;
	CappletDirWindow *window;
};

struct _Capplet
{
	CappletDirEntry entry;
};

CappletDirEntry *capplet_new                (gchar *desktop_path);
CappletDirEntry *capplet_dir_new            (gchar *dir_path);

void             capplet_dir_entry_destroy  (CappletDirEntry *entry);

void             capplet_dir_entry_activate (CappletDirEntry *entry);
void             capplet_dir_entry_shutdown (CappletDirEntry *entry);

void             control_center_init        (int *argc, char **argv);

#endif /* __CAPPLET_DIR_H */
