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

static void
pp_cups_dests_free (PpCupsDests *dests)
{
  cupsFreeDests (dests->num_of_dests, dests->dests);
}

static void
_pp_cups_get_dests_thread (GTask        *task,
                           gpointer     *object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  PpCupsDests *dests;

  dests = g_new0 (PpCupsDests, 1);
  dests->num_of_dests = cupsGetDests (&dests->dests);

  g_task_return_pointer (task, dests, (GDestroyNotify) pp_cups_dests_free);
}

void
pp_cups_get_dests_async (PpCups              *cups,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask       *task;

  task = g_task_new (cups, cancellable, callback, user_data);
  g_task_run_in_thread (task, (GTaskThreadFunc) _pp_cups_get_dests_thread);
  g_object_unref (task);
}

PpCupsDests *
pp_cups_get_dests_finish (PpCups        *cups,
                          GAsyncResult  *res,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, cups), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
connection_test_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  http_t *http;

  http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
  g_task_return_boolean (task, http != NULL);

  httpClose (http);
}

void
pp_cups_connection_test_async (PpCups              *cups,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  task = g_task_new (cups, NULL, callback, user_data);
  g_task_run_in_thread (task, connection_test_thread);

  g_object_unref (task);
}

gboolean
pp_cups_connection_test_finish (PpCups         *cups,
                                GAsyncResult   *result)
{
  g_return_val_if_fail (g_task_is_valid (result, cups), FALSE);

  return g_task_propagate_boolean (G_TASK (result), NULL);
}

/* Cancels subscription of given id */
static void
cancel_subscription_thread (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  ipp_t *request;
  ipp_t *response = NULL;
  gint   id = GPOINTER_TO_INT (task_data);

  if (id >= 0)
    {
      request = ippNewRequest (IPP_CANCEL_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddInteger (request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                     "notify-subscription-id", id);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
    }

  g_task_return_boolean (task, response != NULL && ippGetStatusCode (response) <= IPP_OK);

  ippDelete (response);
}

void
pp_cups_cancel_subscription_async (PpCups              *cups,
                                   gint                 subscription_id,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task;

  task = g_task_new (cups, NULL, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (subscription_id), NULL);
  g_task_run_in_thread (task, cancel_subscription_thread);

  g_object_unref (task);
}

gboolean
pp_cups_cancel_subscription_finish (PpCups       *cups,
                                    GAsyncResult *result)
{
  g_return_val_if_fail (g_task_is_valid (result, cups), FALSE);

  return g_task_propagate_boolean (G_TASK (result), NULL);
}
