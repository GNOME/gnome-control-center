/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __PP_UTILS_H__
#define __PP_UTILS_H__

#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

DBusGProxy *get_dbus_proxy (const gchar *name,
                            const gchar *path,
                            const gchar *iface,
                            const gboolean system_bus);

gchar      *get_tag_value (const gchar *tag_string,
                           const gchar *tag_name);

gchar      *get_ppd_name (gchar *device_class,
                          gchar *device_id,
                          gchar *device_info,
                          gchar *device_make_and_model,
                          gchar *device_uri,
                          gchar *device_location);

char       *get_dest_attr (const char *dest_name,
                           const char *attr);

ipp_t      *execute_maintenance_command (const char *printer_name,
                                         const char *command,
                                         const char *title);

int         ccGetAllowedUsers (gchar      ***allowed_users,
                               const char   *printer_name);

gchar      *get_ppd_attribute (const gchar *printer_name,
                               const gchar *attribute_name);

G_END_DECLS

#endif /* __PP_UTILS_H */
