/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2016  Red Hat, Inc,
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
 * Author: Felipe Borges <feborges@redhat.com>
 */

#ifndef __PP_DETAILS_DIALOG_H__
#define __PP_DETAILS_DIALOG_H__

#include <gtk/gtk.h>
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_DETAILS_DIALOG_TYPE (pp_details_dialog_get_type ())
#define PP_DETAILS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PP_DETAILS_DIALOG_TYPE, PpDetailsDialog))

typedef struct _PpDetailsDialog PpDetailsDialog;
typedef struct _PpDetailsDialogClass PpDetailsDialogClass;

GType            pp_details_dialog_get_type (void);

PpDetailsDialog *pp_details_dialog_new      (GtkWindow            *parent,
                                             gchar                *printer_name,
                                             gchar                *printer_location,
                                             gchar                *printer_address,
                                             gchar                *printer_make_and_model,
                                             gboolean              sensitive);
void             pp_details_dialog_free     (PpDetailsDialog      *dialog);

G_END_DECLS

#endif
