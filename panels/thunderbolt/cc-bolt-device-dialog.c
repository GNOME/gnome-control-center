/* Copyright (C) 2018 Red Hat, Inc
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
 * Authors: Christian J. Kellner <ckellner@redhat.com>
 *
 */

#include <config.h>

#include <glib/gi18n.h>

#include "bolt-device.h"
#include "bolt-error.h"
#include "bolt-time.h"

#include "cc-thunderbolt-resources.h"

#include "cc-bolt-device-dialog.h"

struct _CcBoltDeviceDialog
{
  GtkDialog     parent;

  BoltClient   *client;
  BoltDevice   *device;
  GCancellable *cancel;

  /* main ui */
  GtkHeaderBar *header_bar;

  /* notifications */
  GtkLabel    *notify_label;
  GtkRevealer *notify_revealer;

  /* device details */
  GtkLabel *name_label;
  GtkLabel *status_label;
  GtkLabel *uuid_label;

  GtkLabel *time_title;
  GtkLabel *time_label;

  /* actions */
  GtkWidget  *button_box;
  GtkSpinner *spinner;
  GtkButton  *connect_button;
  GtkButton  *forget_button;
};

static void     on_notify_button_clicked_cb (GtkButton          *button,
                                             CcBoltDeviceDialog *panel);

static void     on_forget_button_clicked_cb (CcBoltDeviceDialog *dialog);
static void     on_connect_button_clicked_cb (CcBoltDeviceDialog *dialog);

G_DEFINE_TYPE (CcBoltDeviceDialog, cc_bolt_device_dialog, GTK_TYPE_DIALOG);

#define RESOURCE_UI "/org/gnome/control-center/thunderbolt/cc-bolt-device-dialog.ui"

static const char *
status_to_string_for_ui (BoltDevice *dev)
{
  BoltStatus status;
  BoltAuthFlags aflags;
  gboolean nopcie;

  status = bolt_device_get_status (dev);
  aflags = bolt_device_get_authflags(dev);
  nopcie = bolt_flag_isset (aflags, BOLT_AUTH_NOPCIE);

  switch (status)
    {
    case BOLT_STATUS_DISCONNECTED:
      return C_("Thunderbolt Device Status", "Disconnected");

    case BOLT_STATUS_CONNECTING:
      return C_("Thunderbolt Device Status", "Connecting");

    case BOLT_STATUS_CONNECTED:
      return C_("Thunderbolt Device Status", "Connected");

    case BOLT_STATUS_AUTH_ERROR:
      return C_("Thunderbolt Device Status", "Authorization Error");

    case BOLT_STATUS_AUTHORIZING:
      return C_("Thunderbolt Device Status", "Authorizing");

    case BOLT_STATUS_AUTHORIZED:
    case BOLT_STATUS_AUTHORIZED_NEWKEY:
    case BOLT_STATUS_AUTHORIZED_SECURE:
    case BOLT_STATUS_AUTHORIZED_DPONLY:
      if (nopcie)
        return C_("Thunderbolt Device Status", "Reduced Functionality");
      else
        return C_("Thunderbolt Device Status", "Connected & Authorized");

    case BOLT_STATUS_UNKNOWN:
      break; /* use default return value, i.e. Unknown */
    }

  return C_("Thunderbolt Device Status", "Unknown");
}

static void
dialog_update_from_device (CcBoltDeviceDialog *dialog)
{
  g_autofree char *generated = NULL;
  g_autofree char *timestr = NULL;
  const char *label;
  const char *uuid;
  const char *status_brief;
  BoltStatus status;
  gboolean stored;
  BoltDevice *dev;
  guint timestamp;

  if (gtk_widget_in_destruction (GTK_WIDGET (dialog)))
    return;

  dev = dialog->device;

  uuid = bolt_device_get_uid (dev);
  label = bolt_device_get_label (dev);

  stored = bolt_device_is_stored (dev);
  status = bolt_device_get_status (dev);

  if (label == NULL)
    {
      const char *name = bolt_device_get_name (dev);
      const char *vendor = bolt_device_get_vendor (dev);

      generated = g_strdup_printf ("%s %s", name, vendor);
      label = generated;
    }

  gtk_label_set_label (dialog->name_label, label);
  gtk_header_bar_set_title (dialog->header_bar, label);

  status_brief = status_to_string_for_ui (dev);
  gtk_label_set_label (dialog->status_label, status_brief);
  gtk_widget_set_visible (GTK_WIDGET (dialog->forget_button), stored);

  /* while we are having an ongoing operation we are setting the buttons
   * to be in-sensitive. In that case, if the button was visible
   * before it will be hidden when the operation is finished by the
   * dialog_operation_done() function */
  if (gtk_widget_is_sensitive (GTK_WIDGET (dialog->connect_button)))
    gtk_widget_set_visible (GTK_WIDGET (dialog->connect_button),
                            status == BOLT_STATUS_CONNECTED);

  gtk_label_set_label (dialog->uuid_label, uuid);

  if (bolt_status_is_authorized (status))
    {
      /* Translators: The time point the device was authorized. */
      gtk_label_set_label (dialog->time_title, _("Authorized at:"));
      timestamp = bolt_device_get_authtime (dev);
    }
  else if (bolt_status_is_connected (status))
    {
      /* Translators: The time point the device was connected. */
      gtk_label_set_label (dialog->time_title, _("Connected at:"));
      timestamp = bolt_device_get_conntime (dev);
    }
  else
    {
      /* Translators: The time point the device was enrolled,
       * i.e. authorized and stored in the device database. */
      gtk_label_set_label (dialog->time_title, _("Enrolled at:"));
      timestamp = bolt_device_get_storetime (dev);
    }

  timestr = bolt_epoch_format (timestamp, "%c");
  gtk_label_set_label (dialog->time_label, timestr);

}

static void
on_device_notify_cb (GObject    *gobject,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);

  dialog_update_from_device (dialog);
}

static void
dialog_operation_start (CcBoltDeviceDialog *dialog)
{
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->connect_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->forget_button), FALSE);
  gtk_spinner_start (dialog->spinner);
}

static void
dialog_operation_done (CcBoltDeviceDialog *dialog,
                       GtkWidget          *sender,
                       GError             *error)
{
  GtkWidget *cb = GTK_WIDGET (dialog->connect_button);
  GtkWidget *fb = GTK_WIDGET (dialog->forget_button);

  /* don' do anything if we are being destroyed */
  if (gtk_widget_in_destruction (GTK_WIDGET (dialog)))
    return;

  /* also don't do anything if the op was canceled */
  if (error != NULL && bolt_err_cancelled (error))
    return;

  gtk_spinner_stop (dialog->spinner);

  if (error != NULL)
    {
      gtk_label_set_label (dialog->notify_label, error->message);
      gtk_revealer_set_reveal_child (dialog->notify_revealer, TRUE);

      /* set the *other* button to sensitive */
      gtk_widget_set_sensitive (cb, cb != sender);
      gtk_widget_set_sensitive (fb, fb != sender);
    }
  else
    {
      gtk_widget_set_visible (sender, FALSE);
      gtk_widget_set_sensitive (cb, TRUE);
      gtk_widget_set_sensitive (fb, TRUE);
    }
}

static void
dialog_authorize_done (CcBoltDeviceDialog *dialog,
                       gboolean            ok,
                       GError             *error)
{
  if (!ok)
    g_prefix_error (&error, _("Failed to authorize device: "));

  dialog_operation_done (dialog, GTK_WIDGET (dialog->connect_button), error);
}

static void
on_device_authorized (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);
  gboolean ok;

  ok = bolt_device_authorize_finish (BOLT_DEVICE (source), res, &err);
  dialog_authorize_done (dialog, ok, err);
}

static void
on_device_enrolled (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);
  gboolean ok;

  ok = bolt_client_enroll_device_finish (dialog->client, res, NULL, &err);
  dialog_authorize_done (dialog, ok, err);
}

static void
on_connect_button_clicked_cb (CcBoltDeviceDialog *dialog)
{
  BoltDevice *device = dialog->device;
  gboolean stored;

  g_return_if_fail (device != NULL);

  dialog_operation_start (dialog);

  stored = bolt_device_is_stored (device);
  if (stored)
    {
      bolt_device_authorize_async (device,
                                   BOLT_AUTHCTRL_NONE,
                                   dialog->cancel,
                                   on_device_authorized,
                                   dialog);
    }
  else
    {
      const char *uid = bolt_device_get_uid (device);

      bolt_client_enroll_device_async (dialog->client,
                                       uid,
                                       BOLT_POLICY_DEFAULT,
                                       BOLT_AUTHCTRL_NONE,
                                       dialog->cancel,
                                       on_device_enrolled,
                                       dialog);
    }

}

static void
on_forget_device_done (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (user_data);
  gboolean ok;

  ok = bolt_client_forget_device_finish (dialog->client, res, &err);

  if (!ok)
    g_prefix_error (&err, _("Failed to forget device: "));

  dialog_operation_done (dialog, GTK_WIDGET (dialog->forget_button), err);
}

static void
on_forget_button_clicked_cb (CcBoltDeviceDialog *dialog)
{
  const char *uid = NULL;

  g_return_if_fail (dialog->device != NULL);

  uid = bolt_device_get_uid (dialog->device);
  dialog_operation_start (dialog);

  bolt_client_forget_device_async (dialog->client,
                                   uid,
                                   dialog->cancel,
                                   on_forget_device_done,
                                   dialog);
}

static void
on_notify_button_clicked_cb (GtkButton          *button,
                             CcBoltDeviceDialog *dialog)
{
  gtk_revealer_set_reveal_child (dialog->notify_revealer, FALSE);
}


static void
cc_bolt_device_dialog_finalize (GObject *object)
{
  CcBoltDeviceDialog *dialog = CC_BOLT_DEVICE_DIALOG (object);

  g_clear_object (&dialog->device);
  g_cancellable_cancel (dialog->cancel);
  g_clear_object (&dialog->cancel);
  g_clear_object (&dialog->client);

  G_OBJECT_CLASS (cc_bolt_device_dialog_parent_class)->finalize (object);
}

static void
cc_bolt_device_dialog_class_init (CcBoltDeviceDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_bolt_device_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_UI);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, header_bar);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, notify_label);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, notify_revealer);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, status_label);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, uuid_label);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, time_title);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, time_label);

  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, button_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, connect_button);
  gtk_widget_class_bind_template_child (widget_class, CcBoltDeviceDialog, forget_button);

  gtk_widget_class_bind_template_callback (widget_class, on_notify_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_connect_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_forget_button_clicked_cb);
}

static void
cc_bolt_device_dialog_init (CcBoltDeviceDialog *dialog)
{
  g_resources_register (cc_thunderbolt_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

/* public functions */
CcBoltDeviceDialog *
cc_bolt_device_dialog_new (void)
{
  CcBoltDeviceDialog *dialog;

  dialog = g_object_new (CC_TYPE_BOLT_DEVICE_DIALOG,
                         "use-header-bar", TRUE,
                         NULL);
  return dialog;
}

void
cc_bolt_device_dialog_set_client (CcBoltDeviceDialog *dialog,
                                  BoltClient         *client)
{
  g_clear_object (&dialog->client);
  dialog->client = g_object_ref (client);
}

void
cc_bolt_device_dialog_set_device (CcBoltDeviceDialog *dialog,
                                  BoltDevice         *device)
{
  if (device == dialog->device)
    return;

  if (dialog->device)
    {
      g_cancellable_cancel (dialog->cancel);
      g_clear_object (&dialog->cancel);
      dialog->cancel = g_cancellable_new ();

      g_signal_handlers_disconnect_by_func (dialog->device,
                                            G_CALLBACK (on_device_notify_cb),
                                            dialog);
      g_clear_object (&dialog->device);
    }

  if (device == NULL)
    return;

  dialog->device = g_object_ref (device);
  g_signal_connect_object (dialog->device,
                           "notify",
                           G_CALLBACK (on_device_notify_cb),
                           dialog,
                           0);

  /* reset the sensitivity of the buttons, because
   * dialog_update_from_device, because it can't know */
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->connect_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->forget_button), TRUE);

  dialog_update_from_device (dialog);
}

BoltDevice *
cc_bolt_device_dialog_peek_device (CcBoltDeviceDialog *dialog)
{
  return dialog->device;
}

gboolean
cc_bolt_device_dialog_device_equal (CcBoltDeviceDialog *dialog,
                                    BoltDevice         *device)
{
  return dialog->device != NULL && device == dialog->device;
}
