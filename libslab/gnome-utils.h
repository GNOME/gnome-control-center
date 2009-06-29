/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GNOME_UTILS_H__
#define __GNOME_UTILS_H__

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-desktop-item.h>

G_BEGIN_DECLS

gboolean load_image_by_id (GtkImage * image, GtkIconSize size,
	const gchar * image_id);
GnomeDesktopItem *load_desktop_item_by_unknown_id (const gchar * id);
gpointer get_gconf_value (const gchar * key);
void set_gconf_value (const gchar * key, gconstpointer data);
guint connect_gconf_notify (const gchar * key, GConfClientNotifyFunc cb, gpointer user_data);
void handle_g_error (GError ** error, const gchar * user_format, ...);
GtkWidget *get_main_menu_section_header (const gchar * markup);

G_END_DECLS
#endif /* __GNOME_UTILS_H__ */
