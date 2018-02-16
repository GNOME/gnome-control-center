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

#ifndef __PP_JOBS_DIALOG_H__
#define __PP_JOBS_DIALOG_H__

#include <gtk/gtk.h>
#include "pp-utils.h"

G_BEGIN_DECLS

typedef struct _PpJobsDialog PpJobsDialog;

PpJobsDialog *pp_jobs_dialog_new               (GtkWindow            *parent,
                                                UserResponseCallback  user_callback,
                                                gpointer              user_data,
                                                gchar                *printer_name);
void          pp_jobs_dialog_update            (PpJobsDialog         *dialog);
void          pp_jobs_dialog_set_callback      (PpJobsDialog         *dialog,
                                                UserResponseCallback  user_callback,
                                                gpointer              user_data);
void          pp_jobs_dialog_free              (PpJobsDialog         *dialog);

void          pp_jobs_dialog_authenticate_jobs (PpJobsDialog         *dialog);

G_END_DECLS

#endif
