/* -*- mode: c; style: linux -*- */

/* capplet-dir-window.h
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

#ifndef __CAPPLET_DIR_WINDOW
#define __CAPPLET_DIR_WINDOW

#include <gnome.h>

#include "capplet-dir.h"

#define CAPPLET_DIR_WINDOW(obj) ((CappletDirWindow *) obj)

struct _CappletDirWindow 
{
	CappletDir *capplet_dir;

	GnomeApp *app;
	GnomeIconList *icon_list;

	gboolean destroyed;
};

CappletDirWindow *capplet_dir_window_new (CappletDir *dir);
void capplet_dir_window_destroy (CappletDirWindow *window);

#endif /* __CAPPLET_DIR_WINDOW */
