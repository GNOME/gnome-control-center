/* -*- mode: c; style: linux -*- */

/* resources.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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

#ifndef __RESOURCES_H
#define __RESOURCES_H

#include "preferences.h"

void init_resource_database (int argc, char **argv);

void preferences_load_from_xrdb (Preferences *prefs);
void screensaver_get_desc_from_xrdb (Screensaver *saver);

gchar *screensaver_get_label_from_xrdb (gchar *name);

#endif /* __RESOURCES_H */
