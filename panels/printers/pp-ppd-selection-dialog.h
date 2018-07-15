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

#ifndef __PP_PPD_SELECTION_DIALOG_H__
#define __PP_PPD_SELECTION_DIALOG_H__

#include <gtk/gtk.h>
#include "pp-utils.h"

G_BEGIN_DECLS

typedef struct _PpPPDSelectionDialog PpPPDSelectionDialog;

PpPPDSelectionDialog *pp_ppd_selection_dialog_new                  (GtkWindow                 *parent,
                                                                    PPDList                   *ppd_list,
                                                                    const gchar               *manufacturer,
                                                                    UserResponseCallback       user_callback,
                                                                    gpointer                   user_data);
gchar                *pp_ppd_selection_dialog_get_ppd_name         (PpPPDSelectionDialog      *dialog);
gchar                *pp_ppd_selection_dialog_get_ppd_display_name (PpPPDSelectionDialog      *dialog);
void                  pp_ppd_selection_dialog_set_ppd_list         (PpPPDSelectionDialog      *dialog,
                                                                    PPDList                   *list);
void                  pp_ppd_selection_dialog_free                 (PpPPDSelectionDialog      *dialog);

G_END_DECLS

#endif
