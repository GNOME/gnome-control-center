/* ws-file-chooser-button.h
 *
 * Copyright 2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WS_TYPE_FILE_CHOOSER_BUTTON (ws_file_chooser_button_get_type())
G_DECLARE_FINAL_TYPE (WsFileChooserButton, ws_file_chooser_button, WS, FILE_CHOOSER_BUTTON, GtkButton)

GtkWidget      *ws_file_chooser_button_new             (void);

void            ws_file_chooser_button_set_file        (WsFileChooserButton *self,
                                                        GFile               *file);

GFile          *ws_file_chooser_button_get_file        (WsFileChooserButton *self);

void            ws_file_chooser_button_set_title       (WsFileChooserButton *self,
                                                        const char          *title);

const char     *ws_file_chooser_button_get_title       (WsFileChooserButton *self);

GtkFileChooser *ws_file_chooser_button_get_filechooser (WsFileChooserButton *self);

G_END_DECLS
