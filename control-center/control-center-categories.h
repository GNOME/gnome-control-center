/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Ximian, Inc.
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Written by Mark McLoughlin <mark@skynet.ie>
 *            Bradford Hovinen <hovinen@ximian.com>,
 *            Jonathan Blandford <jrb@redhat.com>
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

#ifndef __CONTROL_CENTER_CATEGORIES_H__
#define __CONTROL_CENTER_CATEGORIES_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct ControlCenterEntry       ControlCenterEntry;
typedef struct ControlCenterCategory    ControlCenterCategory;
typedef struct ControlCenterInformation ControlCenterInformation;

struct ControlCenterEntry {
	ControlCenterCategory *category;

	char *title;
	char *comment;
	char *desktop_entry;

	GdkPixbuf *icon_pixbuf;

	gpointer user_data;
};

struct ControlCenterCategory {
	ControlCenterEntry **entries;
	int n_entries;

	char *title;

	gpointer user_data;

	guint real_category : 1;
};

struct ControlCenterInformation {
	ControlCenterCategory **categories;
	int n_categories;
};

ControlCenterInformation *control_center_get_information  (void);
void                      control_center_information_free (ControlCenterInformation *information);

G_END_DECLS

#endif /* __CONTROL_CENTER_CATEGORIES_H__ */
