/* -*- mode: c; style: linux -*- */

/* gnome-settings-keyboard-xkb.h
 *
 * Copyright © 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
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

#ifndef __GNOME_SETTINGS_KEYBOARD_XKB_H
#define __GNOME_SETTINGS_KEYBOARD_XKB_H

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

void gnome_settings_keyboard_xkb_init (GConfClient * client);
void gnome_settings_keyboard_xkb_load (GConfClient * client);

#endif
