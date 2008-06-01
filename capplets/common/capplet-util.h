/* -*- mode: c; style: linux -*- */

/* capplet-util.h
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#ifndef __CAPPLET_UTIL_H
#define __CAPPLET_UTIL_H

#include <gnome.h>
#include <gconf/gconf.h>
#include <gconf/gconf-changeset.h>

/* Macros to make certain repetitive tasks a bit easier */

/* Retrieve a widget from the Glade object */

#define WID(s) glade_xml_get_widget (dialog, s)

/* Copy a setting from the legacy gnome-config settings to the ConfigDatabase */

#define COPY_FROM_LEGACY(type, key, legacy_key)                                 \
	val_##type = gnome_config_get_##type##_with_default (legacy_key, &def); \
                                                                                \
	if (!def)                                                               \
		gconf_client_set_##type (client, key, val_##type, NULL);

/* Some miscellaneous functions useful to all capplets */

void capplet_help (GtkWindow *parent, char const *helpfile, char const *section);
void capplet_set_icon (GtkWidget *window, char const *icon_file_name);

#endif /* __CAPPLET_UTIL_H */
