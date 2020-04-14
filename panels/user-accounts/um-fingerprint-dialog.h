/* gnome-about-me-fingerprint.h
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <act/act.h>

void set_fingerprint_row (GtkWidget    *fingerprint_row,
                          GtkLabel     *state_label,
                          GCancellable *cancellable);
void fingerprint_button_clicked (GtkWindow    *parent,
                                 GtkWidget    *fingerprint_row,
                                 GtkLabel     *state_label,
                                 ActUser      *user,
                                 GCancellable *cancellable);
