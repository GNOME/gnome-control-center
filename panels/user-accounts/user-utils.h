/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <act/act.h>

G_BEGIN_DECLS

void     setup_tooltip_with_embedded_icon (GtkWidget   *widget,
                                           const gchar *text,
                                           const gchar *placeholder,
                                           GIcon       *icon);
gboolean show_tooltip_now                 (GtkWidget   *widget,
                                           GdkEvent    *event);

void     set_entry_generation_icon        (GtkEntry    *entry);
void     set_entry_validation_checkmark   (GtkEntry    *entry);
void     set_entry_validation_error       (GtkEntry    *entry,
                                           const gchar *text);
void     clear_entry_validation_error     (GtkEntry    *entry);

gsize    get_username_max_length          (void);
gboolean is_username_used                 (const gchar *username);
gboolean is_valid_name                    (const gchar *name);
void     is_valid_username_async          (const gchar *username,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer callback_data);
gboolean is_valid_username_finish         (GAsyncResult *result,
                                           gchar **tip,
                                           gchar **username,
                                           GError **error);
GdkPixbuf *round_image                    (GdkPixbuf   *pixbuf);
GdkPixbuf *generate_default_avatar        (ActUser     *user,
                                           gint         size);
void       set_default_avatar             (ActUser     *user);
void       set_user_icon_data             (ActUser     *user,
                                           GdkPixbuf   *pixbuf);

G_END_DECLS
