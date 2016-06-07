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

#include "nm-default.h"

#include <string.h>
#include <netinet/ether.h>

#include "utils.h"

/**
 * Filters the characters from a text that was just input into GtkEditable.
 * Returns FALSE, if after filtering no characters were left. TRUE means,
 * that valid characters were added and the content of the GtkEditable changed.
 **/
gboolean
utils_filter_editable_on_insert_text (GtkEditable *editable,
                                      const gchar *text,
                                      gint length,
                                      gint *position,
                                      void *user_data,
                                      UtilsFilterGtkEditableFunc validate_character,
                                      gpointer block_func)
{
	int i, count = 0;
	gchar *result = g_new (gchar, length+1);

	for (i = 0; i < length; i++) {
		if (validate_character (text[i]))
			result[count++] = text[i];
	}
	result[count] = 0;

	if (count > 0) {
		if (block_func) {
			g_signal_handlers_block_by_func (G_OBJECT (editable),
			                                 G_CALLBACK (block_func),
			                                 user_data);
		}
		gtk_editable_insert_text (editable, result, count, position);
		if (block_func) {
			g_signal_handlers_unblock_by_func (G_OBJECT (editable),
			                                   G_CALLBACK (block_func),
			                                   user_data);
		}
	}
	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");

	g_free (result);

	return count > 0;
}

gboolean
utils_char_is_ascii_print (char character)
{
	return g_ascii_isprint (character);
}
