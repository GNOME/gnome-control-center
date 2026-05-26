/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2026 Red Hat
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
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 */

/* The purpose of this stub is to allow builds without the samba dependency.
 * The pp-new-printer-dialog.c file is currently too entangled with samba
 * specific logic.
 */

#include "config.h"
#include "pp-samba.h"

struct _PpSamba {
    PpHost parent_instance;
};

G_DEFINE_FINAL_TYPE (PpSamba, pp_samba, PP_TYPE_HOST);

static void
pp_samba_class_init (PpSambaClass *klass)
{
}

static void
pp_samba_init (PpSamba *samba)
{
}

PpSamba *
pp_samba_new (const gchar *hostname)
{
    return g_object_new (PP_TYPE_SAMBA, "hostname", hostname, NULL);
}

void
pp_samba_set_auth_info (PpSamba *samba, const gchar *username, const gchar *password)
{
}

void
pp_samba_get_devices_async (PpSamba *samba, gboolean auth_if_needed, GCancellable *cancellable,
                            GAsyncReadyCallback callback, gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    g_autoptr(GPtrArray) empty = NULL;

    task = g_task_new (samba, cancellable, callback, user_data);
    empty = g_ptr_array_new_with_free_func (g_object_unref);

    g_task_return_pointer (task, g_steal_pointer (&empty), (GDestroyNotify) g_ptr_array_unref);
}

GPtrArray *
pp_samba_get_devices_finish (PpSamba *samba, GAsyncResult *res, GError **error)
{
    g_return_val_if_fail (g_task_is_valid (res, samba), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return g_task_propagate_pointer (G_TASK (res), error);
}
