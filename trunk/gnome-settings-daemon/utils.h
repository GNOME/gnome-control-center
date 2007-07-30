/*
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Authors:  Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

void         gnome_settings_spawn_with_input (gchar **argv, const gchar *input);

GConfClient *gnome_settings_get_config_client (void);

typedef void (* GnomeSettingsConfigCallback) (GConfEntry *entry);

void         gnome_settings_register_config_callback (const gchar *dir, GnomeSettingsConfigCallback func);

void         gnome_settings_delayed_show_dialog (GtkWidget *dialog);

G_END_DECLS

#endif
