/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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

#pragma once

#include <gtk/gtk.h>
#include <glib-object.h>

#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_JOB (pp_job_get_type ())
G_DECLARE_FINAL_TYPE (PpJob, pp_job, PP, JOB, GObject)

void           pp_job_set_hold_until_async       (PpJob                *job,
                                                  const gchar          *job_hold_until);

void           pp_job_cancel_purge_async         (PpJob                *job,
                                                  gboolean              job_purge);

void           pp_job_get_attributes_async       (PpJob                *job,
                                                  gchar               **attributes_names,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);

GVariant      *pp_job_get_attributes_finish      (PpJob                *job,
                                                  GAsyncResult         *result,
                                                  GError              **error);

void           pp_job_authenticate_async         (PpJob                *job,
                                                  gchar               **auth_info,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);

gboolean       pp_job_authenticate_finish        (PpJob                *job,
                                                  GAsyncResult         *result,
                                                  GError              **error);

G_END_DECLS
