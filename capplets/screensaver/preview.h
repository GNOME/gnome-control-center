/* -*- mode: c; style: linux -*- */

/* preview.h
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

#ifndef __PREVIEW_H
#define __PREVIEW_H

#include <gtk/gtk.h>

#include "preferences.h"

void set_preview_window (GtkWidget *widget);

void show_preview (Screensaver *saver);
void close_preview (void);

void show_demo (Screensaver *saver);
void close_demo (void);

#endif /* __PREVIEW_H */
