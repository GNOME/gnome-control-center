/* -*- mode: c; style: linux -*- */

/* gnome-settings-sound.h
 *
 * Copyright (C) 2002 Anders Carlsson
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
 *
 * Author: Anders Carlsson <andersca@gnu.org>
 */

#ifndef __GNOME_SETTINGS_WM_H__
#define __GNOME_SETTINGS_WM_H__

#include <gconf/gconf.h>

void gnome_settings_wm_init (GConfClient *client);
void gnome_settings_wm_load (GConfClient *client);

#endif /* __GNOME_SETTINGS_WM_H__ */
