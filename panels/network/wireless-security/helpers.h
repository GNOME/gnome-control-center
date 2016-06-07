/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2009 - 2014 Red Hat, Inc.
 */

#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <glib.h>
#include <gtk/gtk.h>

#if defined (LIBNM_BUILD)
#include <NetworkManager.h>
#elif defined (LIBNM_GLIB_BUILD)
#include <nm-connection.h>
#include <nm-setting.h>
#else
#error neither LIBNM_BUILD nor LIBNM_GLIB_BUILD defined
#endif

typedef const char * (*HelperSecretFunc)(NMSetting *);

void helper_fill_secret_entry (NMConnection *connection,
                               GtkBuilder *builder,
                               const char *entry_name,
                               GType setting_type,
                               HelperSecretFunc func);

#endif  /* _HELPERS_H_ */

