/* -*- mode: c; style: linux -*- */

/* applier.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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

#ifndef __APPLIER_H
#define __APPLIER_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <X11/Xlib.h>
#include <pthread.h>

#include "preferences.h"

#define APPLIER(obj)          GTK_CHECK_CAST (obj, applier_get_type (), Applier)
#define APPLIER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applier_get_type (), ApplierClass)
#define IS_APPLIER(obj)       GTK_CHECK_TYPE (obj, applier_get_type ())

typedef struct _Applier Applier;
typedef struct _ApplierClass ApplierClass;

typedef struct _ApplierPrivate ApplierPrivate;

typedef enum _ApplierType {
	APPLIER_ROOT, APPLIER_PREVIEW
} ApplierType;

struct _Applier
{
	GtkObject         object;
	ApplierPrivate   *p;
};

struct _ApplierClass
{
	GtkObjectClass klass;
};

guint        applier_get_type             (void);

GtkObject   *applier_new                  (ApplierType        type);

void         applier_apply_prefs          (Applier           *applier,
					   const Preferences *prefs);

gboolean     applier_render_color_p       (const Applier     *applier,
					   const Preferences *prefs);

GtkWidget   *applier_get_preview_widget   (Applier           *applier);
GdkPixbuf   *applier_get_wallpaper_pixbuf (Applier           *applier);

#endif /* __APPLIER_H */
