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

#ifndef __PP_NEW_PRINTER_DIALOG_H__
#define __PP_NEW_PRINTER_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PpNewPrinterDialog PpNewPrinterDialog;

typedef void (*UserResponseCallback) (GtkDialog *dialog, gint response_id, gpointer user_data);

PpNewPrinterDialog *pp_new_printer_dialog_new  (GtkWindow            *parent,
                                                UserResponseCallback  user_callback,
                                                gpointer              user_data);
void                pp_new_printer_dialog_free (PpNewPrinterDialog   *dialog);

G_END_DECLS

#endif
