/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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

#include "pp-samba.h"

#include "config.h"

#include <glib/gi18n.h>
#include <libsmbclient.h>
#include <errno.h>

#define POLL_DELAY 100000

struct _PpSambaPrivate
{
  /* Auth info */
  gchar    *server;
  gchar    *share;
  gchar    *workgroup;
  gchar    *username;
  gchar    *password;
  gboolean  waiting;
};

G_DEFINE_TYPE (PpSamba, pp_samba, PP_TYPE_HOST);

static void
pp_samba_finalize (GObject *object)
{
  PpSambaPrivate *priv;

  priv = PP_SAMBA (object)->priv;

  g_free (priv->server);
  g_free (priv->share);
  g_free (priv->workgroup);
  g_free (priv->username);
  g_free (priv->password);

  G_OBJECT_CLASS (pp_samba_parent_class)->finalize (object);
}

static void
pp_samba_class_init (PpSambaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (PpSambaPrivate));

  gobject_class->finalize = pp_samba_finalize;
}

static void
pp_samba_init (PpSamba *samba)
{
  samba->priv = G_TYPE_INSTANCE_GET_PRIVATE (samba,
                                             PP_TYPE_SAMBA,
                                             PpSambaPrivate);
}

PpSamba *
pp_samba_new (GtkWindow   *parent,
              const gchar *hostname)
{
  return g_object_new (PP_TYPE_SAMBA,
                       "hostname", hostname,
                       NULL);
}

typedef struct
{
  PpSamba       *samba;
  PpDevicesList *devices;
  GMainContext  *context;
  gboolean       waiting;
  gboolean       auth_if_needed;
  gboolean       hostname_set;
  gboolean       cancelled;
} SMBData;

static void
smb_data_free (SMBData *data)
{
  if (data)
    {
      pp_devices_list_free (data->devices);

      g_free (data);
    }
}

static gboolean
get_auth_info (gpointer user_data)
{
  SMBData *data = (SMBData *) user_data;
  PpSamba *samba = PP_SAMBA (data->samba);

  samba->priv->waiting = TRUE;

  g_signal_emit_by_name (samba, "authentication-required");

  return FALSE;
}

void
pp_samba_set_auth_info (PpSamba     *samba,
                        const gchar *username,
                        const gchar *password)
{
  PpSambaPrivate *priv = samba->priv;

  if ((username != NULL) && (username[0] != '\0'))
    {
      g_free (priv->username);
      priv->username = g_strdup (username);
    }

  if ((password != NULL) && (password[0] != '\0'))
    {
      g_free (priv->password);
      priv->password = g_strdup (password);
    }

  priv->waiting = FALSE;
}

static void
auth_fn (SMBCCTX    *smb_context,
         const char *server,
         const char *share,
         char       *workgroup,
         int         wgmaxlen,
         char       *username,
         int         unmaxlen,
         char       *password,
         int         pwmaxlen)
{
  PpSamba *samba;
  GSource *source;
  SMBData *data;

  data = (SMBData *) smbc_getOptionUserData (smb_context);
  samba = data->samba;

  if (!data->cancelled)
    {
      samba->priv->server = g_strdup (server);
      samba->priv->share = g_strdup (share);
      samba->priv->workgroup = g_strdup (workgroup);
      samba->priv->username = g_strdup (username);
      samba->priv->password = g_strdup (password);

      source = g_idle_source_new ();
      g_source_set_callback (source,
                             get_auth_info,
                             data,
                             NULL);
      g_source_attach (source, data->context);
      g_source_unref (source);

      /*
       * smbclient needs to get authentication data
       * from this synchronous callback so we are blocking
       * until we get them
       */
      while (samba->priv->waiting)
        {
          g_usleep (POLL_DELAY);
        }

      if (g_strcmp0 (username, samba->priv->username) != 0)
        g_strlcpy (username, samba->priv->username, unmaxlen);

      if (g_strcmp0 (password, samba->priv->password) != 0)
        g_strlcpy (password, samba->priv->password, pwmaxlen);
    }
}

static void
anonymous_auth_fn (SMBCCTX    *smb_context,
                   const char *server,
                   const char *share,
                   char       *workgroup,
                   int         wgmaxlen,
                   char       *username,
                   int         unmaxlen,
                   char       *password,
                   int         pwmaxlen)
{
  username[0] = '\0';
  password[0] = '\0';
}

static void
list_dir (SMBCCTX      *smb_context,
          const gchar  *dirname,
          const gchar  *path,
          GCancellable *cancellable,
          SMBData      *data)
{
  struct smbc_dirent *dirent;
  smbc_closedir_fn    smbclient_closedir;
  smbc_readdir_fn     smbclient_readdir;
  smbc_opendir_fn     smbclient_opendir;
  PpPrintDevice      *device;
  const gchar        *host_name;
  SMBCFILE           *dir;

  if (!g_cancellable_is_cancelled (cancellable))
    {
      smbclient_closedir = smbc_getFunctionClosedir (smb_context);
      smbclient_readdir = smbc_getFunctionReaddir (smb_context);
      smbclient_opendir = smbc_getFunctionOpendir (smb_context);

      dir = smbclient_opendir (smb_context, dirname);
      if (!dir && errno == EACCES)
        {
          if (g_str_has_prefix (dirname, "smb://"))
            host_name = dirname + 6;
          else
            host_name = dirname;

          if (data->auth_if_needed)
            {
              data->cancelled = FALSE;
              smbc_setFunctionAuthDataWithContext (smb_context, auth_fn);
              dir = smbclient_opendir (smb_context, dirname);
              smbc_setFunctionAuthDataWithContext (smb_context, anonymous_auth_fn);

              if (data->cancelled)
                {
                  device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                         "host-name", host_name,
                                         "is-authenticated-server", TRUE,
                                         NULL);

                  data->devices->devices = g_list_append (data->devices->devices, device);

                  if (dir)
                    smbclient_closedir (smb_context, dir);
                  return;
                }
            }
          else
            {
              device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                     "host-name", host_name,
                                     "is-authenticated-server", TRUE,
                                     NULL);

              data->devices->devices = g_list_append (data->devices->devices, device);
            }
        }

      while (dir && (dirent = smbclient_readdir (smb_context, dir)))
        {
          gchar *device_name;
          gchar *device_uri;
          gchar *subdirname = NULL;
          gchar *subpath = NULL;
          gchar *uri;

          if (dirent->smbc_type == SMBC_WORKGROUP)
            {
              subdirname = g_strdup_printf ("%s%s", dirname, dirent->name);
              subpath = g_strdup_printf ("%s%s", path, dirent->name);
            }

          if (dirent->smbc_type == SMBC_SERVER)
            {
              subdirname = g_strdup_printf ("smb://%s", dirent->name);
              subpath = g_strdup_printf ("%s//%s", path, dirent->name);
            }

          if (dirent->smbc_type == SMBC_PRINTER_SHARE)
            {
              uri = g_strdup_printf ("%s/%s", dirname, dirent->name);
              device_uri = g_uri_escape_string (uri,
                                                G_URI_RESERVED_CHARS_GENERIC_DELIMITERS
                                                G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS,
                                                FALSE);

              device_name = g_strdup (dirent->name);
              device_name = g_strcanon (device_name, ALLOWED_CHARACTERS, '-');

              device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                     "device-uri", device_uri,
                                     "is-network-device", TRUE,
                                     "device-info", dirent->comment,
                                     "device-name", device_name,
                                     "acquisition-method", data->hostname_set ? ACQUISITION_METHOD_SAMBA_HOST : ACQUISITION_METHOD_SAMBA,
                                     "device-location", path,
                                     "host-name", dirname,
                                     NULL);

              g_free (device_name);
              g_free (device_uri);
              g_free (uri);

              data->devices->devices = g_list_append (data->devices->devices, device);
            }

          if (subdirname)
            {
              list_dir (smb_context,
                        subdirname,
                        subpath,
                        cancellable,
                        data);
              g_free (subdirname);
              g_free (subpath);
            }
        }

      if (dir)
        smbclient_closedir (smb_context, dir);
    }
}

static void
_pp_samba_get_devices_thread (GSimpleAsyncResult *res,
                              GObject            *object,
                              GCancellable       *cancellable)
{
  static GMutex   mutex;
  SMBData        *data;
  SMBCCTX        *smb_context;
  gchar          *dirname;
  gchar          *path;
  gchar          *hostname = NULL;

  data = g_simple_async_result_get_op_res_gpointer (res);
  data->devices = g_new0 (PpDevicesList, 1);
  data->devices->devices = NULL;
  data->samba = PP_SAMBA (object);

  g_mutex_lock (&mutex);

  smb_context = smbc_new_context ();
  if (smb_context)
    {
      if (smbc_init_context (smb_context))
        {
          smbc_setOptionUserData (smb_context, data);

          g_object_get (object, "hostname", &hostname, NULL);
          if (hostname != NULL)
            {
              dirname = g_strdup_printf ("smb://%s", hostname);
              path = g_strdup_printf ("//%s", hostname);

              g_free (hostname);
            }
          else
            {
              dirname = g_strdup_printf ("smb://");
              path = g_strdup_printf ("//");
            }

          smbc_setFunctionAuthDataWithContext (smb_context, anonymous_auth_fn);
          list_dir (smb_context, dirname, path, cancellable, data);

          g_free (dirname);
          g_free (path);
        }

      smbc_free_context (smb_context, 1);
    }

  g_mutex_unlock (&mutex);
}

void
pp_samba_get_devices_async (PpSamba             *samba,
                            gboolean             auth_if_needed,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GSimpleAsyncResult *res;
  SMBData            *data;
  gchar              *hostname = NULL;

  g_object_get (G_OBJECT (samba), "hostname", &hostname, NULL);

  res = g_simple_async_result_new (G_OBJECT (samba), callback, user_data, pp_samba_get_devices_async);
  data = g_new0 (SMBData, 1);
  data->devices = NULL;
  data->context = g_main_context_default ();
  data->hostname_set = hostname != NULL;
  data->auth_if_needed = auth_if_needed;

  g_simple_async_result_set_check_cancellable (res, cancellable);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify) smb_data_free);
  g_simple_async_result_run_in_thread (res, _pp_samba_get_devices_thread, 0, cancellable);

  g_free (hostname);
  g_object_unref (res);
}

PpDevicesList *
pp_samba_get_devices_finish (PpSamba       *samba,
                             GAsyncResult  *res,
                             GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  SMBData            *data;
  PpDevicesList      *result;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_samba_get_devices_async);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);
  result = data->devices;
  data->devices = NULL;

  return result;
}
