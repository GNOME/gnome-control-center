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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "pp-cups.h"

G_DEFINE_TYPE (PpCups, pp_cups, G_TYPE_OBJECT);

static void
pp_cups_finalize (GObject *object)
{
  G_OBJECT_CLASS (pp_cups_parent_class)->finalize (object);
}

static void
pp_cups_class_init (PpCupsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = pp_cups_finalize;
}

static void
pp_cups_init (PpCups *cups)
{
}

PpCups *
pp_cups_new ()
{
  return g_object_new (PP_TYPE_CUPS, NULL);
}

typedef struct
{
  PpCupsDests *dests;
} CGDData;

static void
_pp_cups_get_dests_thread (GSimpleAsyncResult *res,
                           GObject            *object,
                           GCancellable       *cancellable)
{
  CGDData *data;

  data = g_simple_async_result_get_op_res_gpointer (res);

  data->dests = g_new0 (PpCupsDests, 1);
  data->dests->num_of_dests = cupsGetDests (&data->dests->dests);
}

static void
cgd_data_free (CGDData *data)
{
  if (data)
    {
      if (data->dests)
        {
          cupsFreeDests (data->dests->num_of_dests, data->dests->dests);
          g_free (data->dests);
        }

      g_free (data);
    }
}

void
pp_cups_get_dests_async (PpCups              *cups,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GSimpleAsyncResult *res;
  CGDData            *data;

  res = g_simple_async_result_new (G_OBJECT (cups), callback, user_data, pp_cups_get_dests_async);
  data = g_new0 (CGDData, 1);
  data->dests = NULL;

  g_simple_async_result_set_check_cancellable (res, cancellable);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify) cgd_data_free);
  g_simple_async_result_run_in_thread (res, _pp_cups_get_dests_thread, 0, cancellable);

  g_object_unref (res);
}

PpCupsDests *
pp_cups_get_dests_finish (PpCups        *cups,
                          GAsyncResult  *res,
                          GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  PpCupsDests        *result = NULL;
  CGDData            *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_cups_get_dests_async);

  if (g_simple_async_result_propagate_error (simple, error))
    {
      return NULL;
    }

  data = g_simple_async_result_get_op_res_gpointer (simple);
  result = data->dests;
  data->dests = NULL;

  return result;
}
