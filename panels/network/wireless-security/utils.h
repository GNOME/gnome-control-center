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
 * Copyright 2007 - 2015 Red Hat, Inc.
 */

#include <nm-setting-wired.h>
#include <nm-setting-connection.h>

gboolean utils_char_is_ascii_print (char character);

#define NMA_ERROR (g_quark_from_static_string ("nma-error-quark"))

typedef enum  {
	NMA_ERROR_GENERIC
} NMAError;

typedef gboolean (*UtilsFilterGtkEditableFunc) (char character);
gboolean utils_filter_editable_on_insert_text (GtkEditable *editable,
					       const gchar *text,
					       gint length,
					       gint *position,
					       void *user_data,
					       UtilsFilterGtkEditableFunc validate_character,
					       gpointer block_func);

extern void widget_set_error (GtkWidget *widget);
extern void widget_unset_error (GtkWidget *widget);
