/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include "pp-utils.h"

G_BEGIN_DECLS

typedef struct _PpOptionsDialog PpOptionsDialog;

PpOptionsDialog *pp_options_dialog_new          (GtkWindow            *parent,
                                                 UserResponseCallback  user_callback,
                                                 gpointer              user_data,
                                                 gchar                *printer_name,
                                                 gboolean              sensitive);
void             pp_options_dialog_set_callback (PpOptionsDialog      *dialog,
                                                 UserResponseCallback  user_callback,
                                                 gpointer              user_data);
void             pp_options_dialog_free         (PpOptionsDialog      *dialog);

G_END_DECLS
