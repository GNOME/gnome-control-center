/* -*- mode: c; style: linux -*- */

/* capplet-dir-view.h
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

#ifndef __CAPPLET_DIR_VIEW
#define __CAPPLET_DIR_VIEW

#include <gnome.h>

#include "capplet-dir.h"
#include "preferences.h"

#define CAPPLET_DIR_VIEW(obj)          GTK_CHECK_CAST (obj, capplet_dir_view_get_type (), CappletDirView)
#define CAPPLET_DIR_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, capplet_dir_view_get_type (), CappletDirViewClass)
#define IS_CAPPLET_DIR_VIEW(obj)       GTK_CHECK_TYPE (obj, capplet_dir_view_get_type ())

typedef struct _CappletDirViewClass CappletDirViewClass;

struct _CappletDirView 
{
	GnomeApp app;

	CappletDir *capplet_dir;
	CappletDirEntry *selected;

	union {
		GnomeIconList *icon_list;
		GtkCTree *tree;
	} u;

	gboolean destroyed;
	CappletDirViewLayout layout;

	GtkScrolledWindow *scrolled_win;

	GtkWidget *up_button;
};

struct _CappletDirViewClass 
{
	GnomeAppClass parent;
};

guint capplet_dir_view_get_type (void);

GtkWidget *capplet_dir_view_new (void);
void capplet_dir_view_destroy   (CappletDirView *view);

void capplet_dir_view_load_dir  (CappletDirView *view, CappletDir *dir);

void gnomecc_init (void);

#endif /* __CAPPLET_DIR_VIEW */
