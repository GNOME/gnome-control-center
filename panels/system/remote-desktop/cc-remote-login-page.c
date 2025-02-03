/*
 * Copyright 2024 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-remote-login-page"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <polkit/polkit.h>
#include <fcntl.h>
#include <stdio.h>

#include "cc-remote-login-page.h"
#include "cc-encryption-fingerprint-dialog.h"
#include "cc-hostname.h"
#include "cc-password-utils.h"
#include "cc-permission-infobar.h"
#include "cc-tls-certificate.h"
#include "cc-systemd-service.h"

#include "org.gnome.RemoteDesktop.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S 2
#define REMOTE_LOGIN_DBUS_SERVICE "org.gnome.RemoteDesktop.Configuration"
#define REMOTE_LOGIN_OBJECT_PATH "/org/gnome/RemoteDesktop/Configuration"
#define REMOTE_LOGIN_PERMISSION "org.gnome.controlcenter.remote-session-helper"

struct _CcRemoteLoginPage {
  AdwBin parent_instance;

  GsdRemoteDesktopConfigurationRdpServer *configuration_rdp_server;

  AdwSwitchRow *remote_login_row;
  GtkWidget    *toast_overlay;
  CcPermissionInfobar *permission_infobar;
  AdwActionRow *hostname_row;
  AdwActionRow *port_row;
  GtkWidget    *credentials_group;
  GtkWidget    *username_entry;
  GtkWidget    *password_entry;
  AdwButtonRow *generate_password_button_row;
  AdwButtonRow *verify_encryption_button_row;

  GTlsCertificate *certificate;

  GCancellable *cancellable;
  GPermission *permission;

  char *temp_cert_dir;
  char *cert_path;
  char *key_path;
  char *fingerprint;
  guint store_credentials_id;

  gboolean activating_blocked;
  gboolean have_credentials;
};

G_DEFINE_TYPE (CcRemoteLoginPage, cc_remote_login_page, ADW_TYPE_BIN)
static void fetch_credentials (CcRemoteLoginPage *self);

static void
add_toast (CcRemoteLoginPage *self,
           const char          *message)
{
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay),
                               adw_toast_new (message));
}

static void
on_address_copy_clicked (CcRemoteLoginPage *self,
                         GtkButton           *button)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          adw_action_row_get_subtitle (self->hostname_row));
  add_toast (self, _("Device address copied to clipboard"));
}

static void
on_port_copy_clicked (CcRemoteLoginPage *self,
                      GtkButton           *button)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          adw_action_row_get_subtitle (self->port_row));
  add_toast (self, _("Port number copied to clipboard"));
}

static void
on_username_copy_clicked (CcRemoteLoginPage *self,
                          GtkButton           *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->username_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Username copied to clipboard"));
}

static void
on_password_copy_clicked (CcRemoteLoginPage *self,
                          GtkButton           *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->password_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Password copied to clipboard"));
}

static void
on_generate_password_button_row_activated (CcRemoteLoginPage *self)
{
  g_autofree char *new_password = cc_generate_password ();

  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), new_password);
}

static void
on_verify_encryption_button_row_activated (CcRemoteLoginPage *self)
{
  CcEncryptionFingerprintDialog *dialog;

  g_return_if_fail (self->fingerprint);

  dialog = g_object_new (CC_TYPE_ENCRYPTION_FINGERPRINT_DIALOG, NULL);
  cc_encryption_fingerprint_dialog_set_fingerprint (dialog, self->fingerprint, ":");
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
block_remote_login_row_activation (CcRemoteLoginPage *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->remote_login_row), FALSE);
  self->activating_blocked = TRUE;
}

static void
unblock_remote_login_row_activation (CcRemoteLoginPage *self)
{
  if (g_permission_get_allowed (self->permission))
    gtk_widget_set_sensitive (GTK_WIDGET (self->remote_login_row), TRUE);
  self->activating_blocked = FALSE;
}

static void
on_remote_login_enabled (GsdRemoteDesktopConfigurationRdpServer *configuration_rdp_server,
                         GAsyncResult                           *result,
                         gpointer                                user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autoptr(GError) error = NULL;
  gboolean success;

  success = gsd_remote_desktop_configuration_rdp_server_call_enable_finish (configuration_rdp_server,
                                                                            result,
                                                                            &error);
  if (!success)
    {
      g_warning ("Failed to enable RDP server: %s", error->message);
      g_clear_error (&error);
    }

  unblock_remote_login_row_activation (self);
}

static void
enable_remote_login_service (CcRemoteLoginPage *self)
{
  gsd_remote_desktop_configuration_rdp_server_call_enable (self->configuration_rdp_server,
                                                           self->cancellable,
                                                           (GAsyncReadyCallback)
                                                           on_remote_login_enabled,
                                                           self);
}

static void
on_certificate_imported (GsdRemoteDesktopConfigurationRdpServer *configuration_rdp_server,
                         GAsyncResult                           *result,
                         gpointer                                user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autoptr(GError) error = NULL;
  gboolean success;
  g_autofree char *dir = g_steal_pointer (&self->temp_cert_dir);
  g_autofree char *certificate_path = g_build_filename (dir, "rdp-tls.crt", NULL);
  g_autofree char *key_path = g_build_filename (dir, "rdp-tls.key", NULL);

  success = gsd_remote_desktop_configuration_rdp_server_call_import_certificate_finish (configuration_rdp_server,
                                                                                        NULL,
                                                                                        result,
                                                                                        &error);
  if (!success)
    {
      g_warning ("Failed to import newly generated certificates: %s", error->message);
      g_clear_error (&error);
    }

  if (g_remove (certificate_path) != 0)
    g_warning ("Failed to remove generated certificate %s", certificate_path);

  if (g_remove (key_path) != 0)
    g_warning ("Failed to remove generated private key %s", key_path);

  if (g_remove (dir) != 0)
    g_warning ("Failed to remove temporary directory %s", dir);

  enable_remote_login_service (self);
}

static void
on_tls_certificate_generated (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autofree char *certificate_path = g_build_filename (self->temp_cert_dir, "rdp-tls.crt", NULL);
  g_autofree char *key_path = g_build_filename (self->temp_cert_dir, "rdp-tls.key", NULL);
  g_autoptr(GTlsCertificate) tls_certificate = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autofd int certificate_fd = -1;
  g_autofd int key_fd = -1;
  int certificate_fd_index = -1;
  int key_fd_index = -1;

  tls_certificate = bonsai_tls_certificate_new_generate_finish (res, &error);
  if (!tls_certificate)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to generate TLS certificate: %s", error->message);
      goto fail;
    }

  fd_list = g_unix_fd_list_new ();

  certificate_fd = open (certificate_path, O_RDONLY);
  key_fd = open (key_path, O_RDONLY);

  if (certificate_fd != -1 && key_fd != -1)
    {
      certificate_fd_index = g_unix_fd_list_append (fd_list, certificate_fd, &error);
      if (certificate_fd_index == -1)
        {
          g_warning ("Failed to append certificate fd to list: %s", error->message);
          goto fail;
        }

      key_fd_index = g_unix_fd_list_append (fd_list, key_fd, &error);
      if (key_fd_index == -1)
       {
          g_warning ("Failed to append key fd to list: %s", error->message);
          goto fail;
       }

      gsd_remote_desktop_configuration_rdp_server_call_import_certificate (self->configuration_rdp_server,
                                                                           g_variant_new ("(sh)", certificate_path, certificate_fd_index),
                                                                           g_variant_new ("(sh)", key_path, key_fd_index),
                                                                           fd_list,
                                                                           self->cancellable,
                                                                           (GAsyncReadyCallback)
                                                                           on_certificate_imported,
                                                                           self);
      return;
    }

fail:
  unblock_remote_login_row_activation (self);
}

static void
enable_remote_login (CcRemoteLoginPage *self)
{
  g_autoptr (GKeyFile) conf_file = NULL;

  block_remote_login_row_activation (self);

  if ((!self->cert_path) ||
      (!self->key_path) ||
      (strlen (self->cert_path) == 0) ||
      (strlen (self->key_path) == 0))
    {
      g_autofree char *temp_dir = g_dir_make_tmp ("gnome-remote-desktop-XXXXXX", NULL);
      g_autofree char *cert_path_tmp = NULL;
      g_autofree char *key_path_tmp = NULL;

      if (!temp_dir)
        {
          g_warning ("Failed to create temporary directory");
          unblock_remote_login_row_activation (self);
          return;
        }

      cert_path_tmp = g_build_filename (temp_dir, "rdp-tls.crt", NULL);
      key_path_tmp = g_build_filename (temp_dir, "rdp-tls.key", NULL);

      g_set_str (&self->temp_cert_dir, temp_dir);

      bonsai_tls_certificate_new_generate_async (cert_path_tmp,
                                                 key_path_tmp,
                                                 "US", "GNOME",
                                                 self->cancellable,
                                                 on_tls_certificate_generated,
                                                 self);

      return;
    }

  enable_remote_login_service (self);
}

static void
on_remote_login_disabled (GsdRemoteDesktopConfigurationRdpServer *configuration_rdp_server,
                          GAsyncResult                           *result,
                          gpointer                                user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autoptr(GError) error = NULL;
  gboolean success;

  success = gsd_remote_desktop_configuration_rdp_server_call_disable_finish (configuration_rdp_server,
                                                                             result,
                                                                             &error);
  if (!success)
    {
      g_warning ("Failed to disable RDP server: %s", error->message);
      g_clear_error (&error);
    }

  unblock_remote_login_row_activation (self);
}

static void
disable_remote_login_service (CcRemoteLoginPage *self)
{
  block_remote_login_row_activation (self);

  gsd_remote_desktop_configuration_rdp_server_call_disable (self->configuration_rdp_server,
                                                            self->cancellable,
                                                            (GAsyncReadyCallback)
                                                            on_remote_login_disabled,
                                                            self);
}

static void
on_remote_login_active_changed (CcRemoteLoginPage *self)
{
  if (adw_switch_row_get_active (self->remote_login_row))
    enable_remote_login (self);
  else
    disable_remote_login_service (self);
}

static void
on_set_rdp_credentials (GsdRemoteDesktopConfigurationRdpServer *configuration_rdp_server,
                        GAsyncResult                           *result,
                        gpointer                                user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autoptr(GVariant) credentials = NULL;
  g_autoptr(GError) error = NULL;

  gsd_remote_desktop_configuration_rdp_server_call_set_credentials_finish (configuration_rdp_server,
                                                                           result,
                                                                           &error);

  self->store_credentials_id = 0;

  if (error)
    {
      g_debug ("Could not set credentials for remote session access: %s", error->message);
      return;
    }

  /* Do a roundtrip to make sure it stuck and also so we repopulate the tls fingerprint */
  fetch_credentials (self);
}

static gboolean
store_credentials_timeout (gpointer user_data)
{
  CcRemoteLoginPage *self = (CcRemoteLoginPage *)user_data;
  const char *username, *password;

  if (!g_permission_get_allowed (self->permission))
    return G_SOURCE_REMOVE;

  username = gtk_editable_get_text (GTK_EDITABLE (self->username_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  if (username && password)
    {
      GVariantBuilder credentials;

      g_variant_builder_init (&credentials, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&credentials, "{sv}", "username", g_variant_new_string (username));
      g_variant_builder_add (&credentials, "{sv}", "password", g_variant_new_string (password));

      gsd_remote_desktop_configuration_rdp_server_call_set_credentials (self->configuration_rdp_server,
                                                                        g_variant_builder_end (&credentials),
                                                                        self->cancellable,
                                                                        (GAsyncReadyCallback)
                                                                        on_set_rdp_credentials,
                                                                        self);
    }
  else
    {
      self->store_credentials_id = 0;
    }

  return G_SOURCE_REMOVE;
}

static void
on_credentials_changed (CcRemoteLoginPage *self)
{
  g_clear_handle_id (&self->store_credentials_id, g_source_remove);

  self->store_credentials_id =
    g_timeout_add_seconds (REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S,
                           store_credentials_timeout,
                           self);
}

static void
hide_password (CcRemoteLoginPage *self)
{
  GtkEditable *text = gtk_editable_get_delegate (GTK_EDITABLE (self->password_entry));
  gtk_text_set_visibility (GTK_TEXT (text), FALSE);
}

static void
sync_permissions (CcRemoteLoginPage *self)
{
  if (!g_permission_get_allowed (self->permission))
    {
      hide_password (self);

      g_clear_handle_id (&self->store_credentials_id, g_source_remove);
      gtk_widget_set_sensitive (GTK_WIDGET (self->remote_login_row), FALSE);
      gtk_widget_set_sensitive (self->credentials_group, FALSE);
    }
  else
    {
      if (!self->activating_blocked)
        gtk_widget_set_sensitive (GTK_WIDGET (self->remote_login_row), TRUE);

      if (self->have_credentials)
        gtk_widget_set_sensitive (self->credentials_group, TRUE);
    }
}

static void
cc_remote_login_page_dispose (GObject *object)
{
  CcRemoteLoginPage *self = (CcRemoteLoginPage *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->permission);

  g_clear_pointer (&self->temp_cert_dir, g_free);
  g_clear_pointer (&self->cert_path, g_free);
  g_clear_pointer (&self->key_path, g_free);
  g_clear_pointer (&self->fingerprint, g_free);

  g_clear_object (&self->configuration_rdp_server);

  G_OBJECT_CLASS (cc_remote_login_page_parent_class)->dispose (object);
}

static void
cc_remote_login_page_class_init (CcRemoteLoginPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_remote_login_page_dispose;

  g_type_ensure (CC_TYPE_PERMISSION_INFOBAR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/remote-desktop/cc-remote-login-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, port_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, permission_infobar);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, remote_login_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, credentials_group);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, username_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, generate_password_button_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteLoginPage, verify_encryption_button_row);

  gtk_widget_class_bind_template_callback (widget_class, on_address_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_port_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_username_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_password_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_generate_password_button_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_verify_encryption_button_row_activated);
}

static void
on_got_rdp_credentials (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  CcRemoteLoginPage *self = user_data;
  gboolean got_credentials;
  g_autoptr(GVariant) credentials = NULL;
  g_autoptr(GError) error = NULL;

  got_credentials = gsd_remote_desktop_configuration_rdp_server_call_get_credentials_finish (self->configuration_rdp_server,
                                                                                             &credentials,
                                                                                             result,
                                                                                             &error);

  if (error)
    {
      g_debug ("Could not get credentials for remote session access: %s", error->message);
      return;
    }

  if (got_credentials)
    {
      const char *username = NULL;
      const char *password = NULL;

      self->have_credentials = TRUE;

      sync_permissions (self);

      g_variant_lookup (credentials, "username", "&s", &username);
      if (username && g_strcmp0 (username, gtk_editable_get_text (GTK_EDITABLE (self->username_entry))))
        {
          g_signal_handlers_block_by_func (self->username_entry, on_credentials_changed, self);
          gtk_editable_set_text (GTK_EDITABLE (self->username_entry), username);
          g_signal_handlers_unblock_by_func (self->username_entry, on_credentials_changed, self);
        }

      g_variant_lookup (credentials, "password", "&s", &password);
      if (password && g_strcmp0 (password, gtk_editable_get_text (GTK_EDITABLE (self->password_entry))))
        {
          g_signal_handlers_block_by_func (self->password_entry, on_credentials_changed, self);
          gtk_editable_set_text (GTK_EDITABLE (self->password_entry), password);
          g_signal_handlers_unblock_by_func (self->password_entry, on_credentials_changed, self);
        }
    }
}

static void
fetch_credentials (CcRemoteLoginPage *self)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *username = NULL;
  g_autofree gchar *password = NULL;

  if (!g_permission_get_allowed (self->permission))
    return;

  gsd_remote_desktop_configuration_rdp_server_call_get_credentials (self->configuration_rdp_server,
                                                                    self->cancellable,
                                                                    (GAsyncReadyCallback)
                                                                    on_got_rdp_credentials,
                                                                    self);
}

static void
fetch_remote_desktop_rdp_server_configuration (CcRemoteLoginPage *self)
{
  g_autofree char *name = NULL;
  gboolean enabled;
  int port;

  g_object_get (G_OBJECT (self->configuration_rdp_server),
                "g-name-owner", &name,
                NULL);
  if (!name)
    return;

  enabled = gsd_remote_desktop_configuration_rdp_server_get_enabled (self->configuration_rdp_server);
  if (!self->activating_blocked)
  {
    g_signal_handlers_block_by_func (self->remote_login_row, on_remote_login_active_changed, self);
    adw_switch_row_set_active (self->remote_login_row, enabled);
    g_signal_handlers_unblock_by_func (self->remote_login_row, on_remote_login_active_changed, self);
  }

  port = gsd_remote_desktop_configuration_rdp_server_get_port (self->configuration_rdp_server);
  if (enabled)
    {
      g_autofree char *str_port = (port <= 0) ? g_strdup ("—") : g_strdup_printf ("%u", port);
      adw_action_row_set_subtitle (self->port_row, str_port);
      gtk_widget_set_sensitive (GTK_WIDGET (self->port_row), port > 0);
    }
  else
    {
      adw_action_row_set_subtitle (self->port_row, "—");
      gtk_widget_set_sensitive (GTK_WIDGET (self->port_row), FALSE);
    }

  if (g_permission_get_allowed (self->permission))
    fetch_credentials (self);

  g_signal_connect_object (self->permission, "notify::allowed",
                           G_CALLBACK (fetch_credentials),
                           self, G_CONNECT_SWAPPED);

  self->cert_path = gsd_remote_desktop_configuration_rdp_server_dup_tls_cert (self->configuration_rdp_server);
  self->key_path = gsd_remote_desktop_configuration_rdp_server_dup_tls_key (self->configuration_rdp_server);
  self->fingerprint = gsd_remote_desktop_configuration_rdp_server_dup_tls_fingerprint (self->configuration_rdp_server);
  gtk_widget_set_sensitive (GTK_WIDGET (self->verify_encryption_button_row), self->fingerprint && strlen (self->fingerprint) > 0);
}

static void
on_connected_to_remote_desktop_configuration_rdp_server (GObject      *source_object,
                                                         GAsyncResult *result,
                                                         gpointer      user_data)
{
  CcRemoteLoginPage *self = user_data;
  g_autoptr (GError) error = NULL;

  g_clear_object (&self->configuration_rdp_server);
  self->configuration_rdp_server = gsd_remote_desktop_configuration_rdp_server_proxy_new_finish (result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to create remote desktop proxy: %s", error->message);
      return;
    }

  fetch_remote_desktop_rdp_server_configuration (self);

  g_signal_connect_object (self->configuration_rdp_server, "g-properties-changed",
                           G_CALLBACK (fetch_remote_desktop_rdp_server_configuration),
                           self, G_CONNECT_SWAPPED);
}

static void
connect_to_remote_desktop_configuration_rdp_server (CcRemoteLoginPage *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDBusConnection) connection = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, self->cancellable, &error);

  if (error)
    g_warning ("Could not connect to system message bus: %s", error->message);

  if (!connection)
    return;

  gsd_remote_desktop_configuration_rdp_server_proxy_new (connection,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         REMOTE_LOGIN_DBUS_SERVICE,
                                                         REMOTE_LOGIN_OBJECT_PATH,
                                                         self->cancellable,
                                                         (GAsyncReadyCallback)
                                                         on_connected_to_remote_desktop_configuration_rdp_server,
                                                         self);
}

static void
cc_remote_login_page_init (CcRemoteLoginPage *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;
  g_autoptr(GVariant) credentials = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *hostname = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  hostname = cc_hostname_get_static_hostname (cc_hostname_get_default ());
  adw_action_row_set_subtitle (self->hostname_row, hostname);

  g_signal_connect_swapped (self->username_entry, "notify::text",
                            G_CALLBACK (on_credentials_changed),
                            self);
  g_signal_connect_swapped (self->password_entry, "notify::text",
                            G_CALLBACK (on_credentials_changed),
                            self);

  g_signal_connect_object (self->remote_login_row, "notify::active",
                           G_CALLBACK (on_remote_login_active_changed), self,
                           G_CONNECT_SWAPPED);

  self->permission = (GPermission*) polkit_permission_new_sync (REMOTE_LOGIN_PERMISSION, NULL, self->cancellable, &error);

  if (error != NULL)
    {
      g_warning ("Cannot create '%s' permission: %s", REMOTE_LOGIN_PERMISSION, error->message);
      g_clear_error (&error);
    }

  sync_permissions (self);
  g_signal_connect_swapped (self->permission, "notify::allowed",
                            G_CALLBACK (sync_permissions),
                            self);

  g_object_bind_property (self->password_entry, "sensitive",
                          self->generate_password_button_row, "sensitive",
                          G_BINDING_SYNC_CREATE);
  cc_permission_infobar_set_permission (self->permission_infobar, self->permission);

  connect_to_remote_desktop_configuration_rdp_server (self);
}
