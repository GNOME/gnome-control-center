/* -*- mode: c; style: linux -*- */

/* gnome-settings-keyboard.h
 *
 * Copyright © 2002 Red Hat, Inc.
 *
 * Written by Owen Taylor <otaylor@redhat.com>
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

#ifndef __GNOME_SETTINGS_GTK1THEME_H
#define __GNOME_SETTINGS_GTK1THEME_H

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

void gnome_settings_gtk1_theme_init (GConfClient *client);
void gnome_settings_gtk1_theme_load (GConfClient *client);

#endif
