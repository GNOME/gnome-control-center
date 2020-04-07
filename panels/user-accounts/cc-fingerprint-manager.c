/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include "cc-fingerprint-manager.h"

#include "cc-fprintd-generated.h"
#include "cc-user-accounts-enum-types.h"

#define CC_FPRINTD_NAME "net.reactivated.Fprint"
#define CC_FPRINTD_MANAGER_PATH "/net/reactivated/Fprint/Manager"

struct _CcFingerprintManager
{
  GObject parent_instance;
};

typedef struct
{
  ActUser           *user;
  GTask             *current_task;
  CcFingerprintState state;
  GList             *cached_devices;
} CcFingerprintManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CcFingerprintManager, cc_fingerprint_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_USER,
  PROP_STATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

CcFingerprintManager *
cc_fingerprint_manager_new (ActUser *user)
{
  return g_object_new (CC_TYPE_FINGERPRINT_MANAGER, "user", user, NULL);
}

static void
cc_fingerprint_manager_dispose (GObject *object)
{
  CcFingerprintManager *self = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);

  if (priv->current_task)
    {
      g_cancellable_cancel (g_task_get_cancellable (priv->current_task));
      priv->current_task = NULL;
    }

  g_clear_object (&priv->user);
  g_list_free_full (g_steal_pointer (&priv->cached_devices), g_object_unref);

  G_OBJECT_CLASS (cc_fingerprint_manager_parent_class)->dispose (object);
}

static void
cc_fingerprint_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CcFingerprintManager *self = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, priv->state);
      break;

    case PROP_USER:
      g_value_set_object (value, priv->user);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_fingerprint_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcFingerprintManager *self = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_USER:
      g_set_object (&priv->user, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_fingerprint_manager_constructed (GObject *object)
{
  cc_fingerprint_manager_update_state (CC_FINGERPRINT_MANAGER (object), NULL, NULL);
}

static void
cc_fingerprint_manager_class_init (CcFingerprintManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = cc_fingerprint_manager_constructed;
  object_class->dispose = cc_fingerprint_manager_dispose;
  object_class->get_property = cc_fingerprint_manager_get_property;
  object_class->set_property = cc_fingerprint_manager_set_property;

  properties[PROP_USER] =
    g_param_spec_object ("user",
                         "User",
                         "The user account we manage the fingerprint for",
                         ACT_TYPE_USER,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The state of the fingerprint for the user",
                       CC_TYPE_FINGERPRINT_STATE, CC_FINGERPRINT_STATE_NONE,
                       G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_fingerprint_manager_init (CcFingerprintManager *self)
{
}

typedef struct
{
  guint  waiting_devices;
  GList *devices;
} DeviceListData;

static void
object_list_destroy_notify (gpointer data)
{
  GList *list = data;
  g_list_free_full (list, g_object_unref);
}

static void
on_device_proxy (GObject *object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(CcFprintdDevice) fprintd_device = NULL;
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;
  CcFingerprintManager *self = g_task_get_source_object (task);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);
  DeviceListData *list_data = g_task_get_task_data (task);

  fprintd_device = cc_fprintd_device_proxy_new_for_bus_finish (res, &error);
  list_data->waiting_devices--;

  if (error)
    {
      if (list_data->waiting_devices == 0)
        g_task_return_error (task, g_steal_pointer (&error));

      return;
    }

  g_debug ("Got fingerprint device %s", cc_fprintd_device_get_name (fprintd_device));

  list_data->devices = g_list_append (list_data->devices, g_steal_pointer (&fprintd_device));

  if (list_data->waiting_devices == 0)
    {
      g_list_free_full (g_steal_pointer (&priv->cached_devices), g_object_unref);
      priv->cached_devices = g_list_copy_deep (list_data->devices, (GCopyFunc) g_object_ref, NULL);
      g_task_return_pointer (task, g_steal_pointer (&list_data->devices), object_list_destroy_notify);
    }
}

static void
on_devices_list (GObject *object, GAsyncResult *res, gpointer user_data)
{
  CcFprintdManager *fprintd_manager = CC_FPRINTD_MANAGER (object);
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) devices_list = NULL;
  DeviceListData *list_data;
  guint i;

  cc_fprintd_manager_call_get_devices_finish (fprintd_manager, &devices_list, res, &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!devices_list || !devices_list[0])
    {
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  list_data = g_new0 (DeviceListData, 1);
  g_task_set_task_data (task, list_data, g_free);

  g_debug ("Fprintd replied with %u device(s)", g_strv_length (devices_list));

  for (i = 0; devices_list[i] != NULL; ++i)
    {
      const char *device_path = devices_list[i];

      list_data->waiting_devices++;

      cc_fprintd_device_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           CC_FPRINTD_NAME,
                                           device_path,
                                           g_task_get_cancellable (task),
                                           on_device_proxy,
                                           g_object_ref (task));
    }
}

static void
on_manager_proxy (GObject *object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(CcFprintdManager) fprintd_manager = NULL;
  g_autoptr(GError) error = NULL;

  fprintd_manager = cc_fprintd_manager_proxy_new_for_bus_finish (res, &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_debug ("Fprintd manager connected");

  cc_fprintd_manager_call_get_devices (fprintd_manager,
                                       g_task_get_cancellable (task),
                                       on_devices_list,
                                       g_object_ref (task));
}

static void
fprintd_manager_connect (CcFingerprintManager *self,
                         GAsyncReadyCallback   callback,
                         GTask                *task)
{
  g_assert (G_IS_TASK (task));

  cc_fprintd_manager_proxy_new_for_bus (G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                        CC_FPRINTD_NAME, CC_FPRINTD_MANAGER_PATH,
                                        g_task_get_cancellable (task),
                                        callback,
                                        task);
}

void
cc_fingerprint_manager_get_devices (CcFingerprintManager *self,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, cc_fingerprint_manager_get_devices);

  if (priv->cached_devices)
    {
      GList *devices;

      devices = g_list_copy_deep (priv->cached_devices, (GCopyFunc) g_object_ref, NULL);
      g_task_return_pointer (task, devices, object_list_destroy_notify);
      return;
    }

  fprintd_manager_connect (self, on_manager_proxy, g_object_ref (task));
}

/**
 * cc_fingerprint_manager_get_devices_finish:
 * @self: The #CcFingerprintManager
 * @result: A #GAsyncResult
 * @error: Return location for errors, or %NULL to ignore
 *
 * Finish an asynchronous operation to list all devices.
 *
 * Returns: (element-type CcFprintdDevice) (transfer full): List of prints or %NULL on error
 */
GList *
cc_fingerprint_manager_get_devices_finish (CcFingerprintManager *self,
                                           GAsyncResult         *res,
                                           GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
set_state (CcFingerprintManager *self,
           CcFingerprintState    state)
{
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);

  if (priv->state == state)
    return;

  g_debug ("Fingerprint manager state changed to %d", state);

  priv->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

typedef struct
{
  guint                     waiting_devices;
  CcFingerprintStateUpdated callback;
  gpointer                  user_data;
} UpdateStateData;

static void
update_state_callback (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  CcFingerprintManager *self = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);
  g_autoptr(GError) error = NULL;
  CcFingerprintState state;
  UpdateStateData *data;
  GTask *task;

  g_return_if_fail (g_task_is_valid (res, self));

  task = G_TASK (res);
  g_assert (g_steal_pointer (&priv->current_task) == task);

  state = g_task_propagate_int (task, &error);
  data = g_task_get_task_data (task);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Impossible to update fingerprint manager state: %s",
                 error->message);

      state = CC_FINGERPRINT_STATE_NONE;
    }

  set_state (self, state);

  if (data->callback)
    data->callback (self, state, data->user_data, error);
}

static void
on_device_list_enrolled (GObject      *object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) enrolled_fingers = NULL;
  UpdateStateData *data = g_task_get_task_data (task);
  guint num_enrolled_fingers;

  cc_fprintd_device_call_list_enrolled_fingers_finish (fprintd_device,
                                                       &enrolled_fingers,
                                                       res, &error);
  data->waiting_devices--;

  if (g_task_get_completed (task))
    return;

  if (error)
    {
      g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

      if (!g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoEnrolledPrints"))
        {
          if (data->waiting_devices == 0)
            g_task_return_error (task, g_steal_pointer (&error));

          return;
        }
    }

  num_enrolled_fingers = enrolled_fingers ? g_strv_length (enrolled_fingers) : 0;

  g_debug ("Device %s has %u enrolled fingers",
           cc_fprintd_device_get_name (fprintd_device),
           num_enrolled_fingers);

  if (num_enrolled_fingers > 0)
    g_task_return_int (task, CC_FINGERPRINT_STATE_ENABLED);
  else if (data->waiting_devices == 0)
    g_task_return_int (task, CC_FINGERPRINT_STATE_DISABLED);
}

static void
on_manager_devices_list (GObject      *object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  CcFingerprintManager *self = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);
  g_autolist(CcFprintdDevice) fprintd_devices = NULL;
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;
  UpdateStateData *data = g_task_get_task_data (task);
  const char *user_name;
  GList *l;

  fprintd_devices = cc_fingerprint_manager_get_devices_finish (self, res, &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (fprintd_devices == NULL)
    {
      g_debug ("No fingerprint devices found");
      g_task_return_int (task, CC_FINGERPRINT_STATE_NONE);
      return;
    }

  user_name = act_user_get_user_name (priv->user);

  for (l = fprintd_devices; l; l = l->next)
    {
      CcFprintdDevice *device = l->data;

      g_debug ("Connected to device %s, looking for enrolled fingers",
               cc_fprintd_device_get_name (device));

      data->waiting_devices++;
      cc_fprintd_device_call_list_enrolled_fingers (device, user_name,
                                                    g_task_get_cancellable (task),
                                                    on_device_list_enrolled,
                                                    g_object_ref (task));
    }
}

void
cc_fingerprint_manager_update_state (CcFingerprintManager     *self,
                                     CcFingerprintStateUpdated callback,
                                     gpointer                  user_data)
{
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);
  g_autoptr(GCancellable) cancellable = NULL;
  UpdateStateData *data;

  g_return_if_fail (priv->current_task == NULL);

  if (act_user_get_uid (priv->user) != getuid () ||
      !act_user_is_local_account (priv->user))
    {
      set_state (self, CC_FINGERPRINT_STATE_NONE);
      return;
    }

  cancellable = g_cancellable_new ();
  data = g_new0 (UpdateStateData, 1);
  data->callback = callback;
  data->user_data = user_data;

  priv->current_task = g_task_new (self, cancellable, update_state_callback, NULL);
  g_task_set_source_tag (priv->current_task, cc_fingerprint_manager_update_state);
  g_task_set_task_data (priv->current_task, data, g_free);

  cc_fingerprint_manager_get_devices (self, cancellable, on_manager_devices_list,
                                      priv->current_task);
}

CcFingerprintState
cc_fingerprint_manager_get_state (CcFingerprintManager *self)
{
  CcFingerprintManagerPrivate *priv = cc_fingerprint_manager_get_instance_private (self);

  g_return_val_if_fail (CC_IS_FINGERPRINT_MANAGER (self), CC_FINGERPRINT_STATE_NONE);

  return priv->state;
}
