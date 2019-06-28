/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2019  Red Hat, Inc,
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
 * Written by: Kalev Lember <klember@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-subscription-register-dialog.h"

#define DBUS_TIMEOUT 300000 /* 5 minutes */
#define SERVER_URL "subscription.rhsm.redhat.com"

typedef enum {
  DIALOG_STATE_REGISTER,
  DIALOG_STATE_REGISTERING
} DialogState;

static void dialog_validate (CcSubscriptionRegisterDialog *self);

struct _CcSubscriptionRegisterDialog
{
  GtkDialog       parent_instance;

  DialogState     state;
  GCancellable   *cancellable;
  GDBusProxy     *subscription_proxy;
  gboolean        valid;

  /* template widgets */
  GtkSpinner     *spinner;
  GtkButton      *register_button;
  GtkRevealer    *notification_revealer;
  GtkLabel       *error_label;
  GtkRadioButton *default_url_radio;
  GtkRadioButton *custom_url_radio;
  GtkRadioButton *register_radio;
  GtkRadioButton *register_with_activation_keys_radio;
  GtkStack       *stack;
  GtkGrid        *register_grid;
  GtkGrid        *register_with_activation_keys_grid;
  GtkEntry       *url_label;
  GtkEntry       *url_entry;
  GtkEntry       *login_entry;
  GtkEntry       *password_entry;
  GtkEntry       *activation_keys_entry;
  GtkLabel       *organization_label;
  GtkEntry       *organization_entry;
  GtkEntry       *organization_entry_with_activation_keys;
};

G_DEFINE_TYPE (CcSubscriptionRegisterDialog, cc_subscription_register_dialog, GTK_TYPE_DIALOG);

static void
dialog_reload (CcSubscriptionRegisterDialog *self)
{
  gboolean sensitive;
  gboolean url_entry_enabled;

  switch (self->state)
    {
    case DIALOG_STATE_REGISTER:
      gtk_window_set_title (GTK_WINDOW (self), _("Register System"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->register_button), self->valid);

      sensitive = TRUE;
      break;

    case DIALOG_STATE_REGISTERING:
      gtk_window_set_title (GTK_WINDOW (self), _("Registering System…"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->register_button), FALSE);

      sensitive = FALSE;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  url_entry_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->custom_url_radio));
  gtk_widget_set_sensitive (GTK_WIDGET (self->url_entry), sensitive && url_entry_enabled);

  gtk_widget_set_sensitive (GTK_WIDGET (self->default_url_radio), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->custom_url_radio), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->register_radio), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->register_with_activation_keys_radio), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->login_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->password_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->activation_keys_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->password_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->organization_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->organization_entry_with_activation_keys), sensitive);
}

static void
custom_url_radio_toggled_cb (CcSubscriptionRegisterDialog *self)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->custom_url_radio));
  if (active)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->url_entry), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->url_label), TRUE);

      gtk_entry_set_text (self->url_entry, "");
      gtk_widget_grab_focus (GTK_WIDGET (self->url_entry));
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->url_entry), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->url_label), FALSE);

      gtk_entry_set_text (self->url_entry, SERVER_URL);
    }

  dialog_validate (self);
}

static void
register_with_activation_keys_radio_toggled_cb (CcSubscriptionRegisterDialog *self)
{
  gint active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->register_with_activation_keys_radio));
  if (active)
    {
      gtk_stack_set_visible_child_name (self->stack, "register-with-activation-keys");
      gtk_widget_grab_focus (GTK_WIDGET (self->activation_keys_entry));
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "register");
      gtk_widget_grab_focus (GTK_WIDGET (self->login_entry));
    }

  dialog_validate (self);
}

static void
dialog_validate (CcSubscriptionRegisterDialog *self)
{
  gboolean valid_url = TRUE;
  gboolean valid_login = TRUE;
  gboolean valid_password = TRUE;
  gboolean valid_activation_keys = TRUE;
  gboolean valid_organization = TRUE;

  /* require url when custom url radio is selected */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->custom_url_radio)))
    {
      const gchar *url;

      url = gtk_entry_get_text (self->url_entry);
      valid_url = url != NULL && strlen (url) != 0;
    }

  /* activation keys radio selected */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->register_with_activation_keys_radio)))
    {
      const gchar *activation_keys;
      const gchar *organization;

      /* require activation keys */
      activation_keys = gtk_entry_get_text (self->activation_keys_entry);
      valid_activation_keys = activation_keys != NULL && strlen (activation_keys) != 0;

      /* organization is required when using activation keys */
      organization = gtk_entry_get_text (self->organization_entry_with_activation_keys);
      valid_organization = organization != NULL && strlen (organization) != 0;

      /* username/password radio selected */
    }
  else
    {
      const gchar *login;
      const gchar *password;

      /* require login */
      login = gtk_entry_get_text (self->login_entry);
      valid_login = login != NULL && strlen (login) != 0;

      /* require password */
      password = gtk_entry_get_text (self->password_entry);
      valid_password = password != NULL && strlen (password) != 0;
    }

  self->valid = valid_url && valid_login && valid_password && valid_activation_keys && valid_organization;
  gtk_widget_set_sensitive (GTK_WIDGET (self->register_button), self->valid);
}

static void
registration_done_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  CcSubscriptionRegisterDialog *self = (CcSubscriptionRegisterDialog *) user_data;
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_dbus_error_strip_remote_error (error);
      gtk_label_set_text (self->error_label, error->message);
      gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

      gtk_spinner_stop (self->spinner);

      self->state = DIALOG_STATE_REGISTER;
      dialog_reload (self);
      return;
    }

  gtk_spinner_stop (self->spinner);
  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
  return;
}

static void
subscription_register_with_activation_keys (CcSubscriptionRegisterDialog *self)
{
  g_autoptr(GVariantBuilder) options_builder = NULL;
  const gchar *hostname;
  const gchar *organization;
  const gchar *activation_keys;

  hostname = gtk_entry_get_text (self->url_entry);
  organization = gtk_entry_get_text (self->organization_entry_with_activation_keys);
  activation_keys = gtk_entry_get_text (self->activation_keys_entry);

  options_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (options_builder, "{sv}", "kind", g_variant_new_string ("key"));
  g_variant_builder_add (options_builder, "{sv}", "hostname", g_variant_new_string (hostname));
  g_variant_builder_add (options_builder, "{sv}", "organisation", g_variant_new_string (organization));
  g_variant_builder_add (options_builder, "{sv}", "activation-key", g_variant_new_string (activation_keys));

  g_dbus_proxy_call (self->subscription_proxy,
                     "Register",
                     g_variant_new ("(a{sv})",
                                    options_builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     DBUS_TIMEOUT,
                     self->cancellable,
                     registration_done_cb,
                     self);
}

static void
subscription_register_with_username (CcSubscriptionRegisterDialog *self)
{
  g_autoptr(GVariantBuilder) options_builder = NULL;
  const gchar *hostname;
  const gchar *organization;
  const gchar *username;
  const gchar *password;

  hostname = gtk_entry_get_text (self->url_entry);
  organization = gtk_entry_get_text (self->organization_entry);
  username = gtk_entry_get_text (self->login_entry);
  password = gtk_entry_get_text (self->password_entry);

  options_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (options_builder, "{sv}", "kind", g_variant_new_string ("username"));
  g_variant_builder_add (options_builder, "{sv}", "hostname", g_variant_new_string (hostname));
  g_variant_builder_add (options_builder, "{sv}", "organisation", g_variant_new_string (organization));
  g_variant_builder_add (options_builder, "{sv}", "username", g_variant_new_string (username));
  g_variant_builder_add (options_builder, "{sv}", "password", g_variant_new_string (password));

  g_dbus_proxy_call (self->subscription_proxy,
                     "Register",
                     g_variant_new ("(a{sv})", options_builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     DBUS_TIMEOUT,
                     self->cancellable,
                     registration_done_cb,
                     self);
}

static void
register_button_clicked_cb (CcSubscriptionRegisterDialog *self)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->register_with_activation_keys_radio)))
    subscription_register_with_activation_keys (self);
  else
    subscription_register_with_username (self);

  gtk_spinner_start (self->spinner);

  self->state = DIALOG_STATE_REGISTERING;
  dialog_reload (self);
}

static void
dismiss_notification (CcSubscriptionRegisterDialog *self)
{
  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
}

static void
cc_subscription_register_dialog_init (CcSubscriptionRegisterDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  self->state = DIALOG_STATE_REGISTER;

  gtk_entry_set_text (self->url_entry, SERVER_URL);
  gtk_widget_grab_focus (GTK_WIDGET (self->login_entry));
  dialog_validate (self);
  dialog_reload (self);
}

static void
cc_subscription_register_dialog_dispose (GObject *obj)
{
  CcSubscriptionRegisterDialog *self = (CcSubscriptionRegisterDialog *) obj;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->subscription_proxy);

  G_OBJECT_CLASS (cc_subscription_register_dialog_parent_class)->dispose (obj);
}

static void
cc_subscription_register_dialog_finalize (GObject *obj)
{
  G_OBJECT_CLASS (cc_subscription_register_dialog_parent_class)->finalize (obj);
}

static void
cc_subscription_register_dialog_class_init (CcSubscriptionRegisterDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_subscription_register_dialog_dispose;
  object_class->finalize = cc_subscription_register_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info/cc-subscription-register-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, register_button);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, error_label);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, default_url_radio);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, custom_url_radio);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, register_radio);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, register_with_activation_keys_radio);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, register_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, register_with_activation_keys_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, url_label);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, url_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, login_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, activation_keys_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, organization_label);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, organization_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionRegisterDialog, organization_entry_with_activation_keys);

  gtk_widget_class_bind_template_callback (widget_class, dialog_validate);
  gtk_widget_class_bind_template_callback (widget_class, register_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dismiss_notification);
  gtk_widget_class_bind_template_callback (widget_class, custom_url_radio_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, register_with_activation_keys_radio_toggled_cb);
}

CcSubscriptionRegisterDialog *
cc_subscription_register_dialog_new (GDBusProxy *subscription_proxy)
{
  CcSubscriptionRegisterDialog *self;

  self = g_object_new (CC_TYPE_SUBSCRIPTION_REGISTER_DIALOG, "use-header-bar", TRUE, NULL);
  self->subscription_proxy = g_object_ref (subscription_proxy);

  return self;
}
