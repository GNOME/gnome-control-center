/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2022 Red Hat, Inc
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

#include "cc-remote-desktop-panel.h"
#include "cc-remote-desktop-resources.h"
#include "cc-util.h"

#include <glib/gi18n.h>
#include <libsecret/secret.h>

struct _CcRemoteDesktopPanel
{
  CcPanel           parent_instance;

  GSettings        *remote_desktop_settings;
  GCancellable     *cancellable;
  guint             remote_desktop_name_watch;

  GtkStack         *stack;
  GtkWidget        *remote_desktop_page;
  GtkSwitch        *view_only_switch;
  GtkEntry         *username_entry;
  GtkPasswordEntry *password_entry;
  GtkButton        *tls_key_button;
  GtkButton        *tls_cert_button;
};

CC_PANEL_REGISTER (CcRemoteDesktopPanel, cc_remote_desktop_panel)

#define GRD_RDP_SCHEMA_ID "org.gnome.desktop.remote-desktop"
#define GRD_RDP_CREDENTIALS_SCHEMA grd_rdp_credentials_get_schema ()

static const SecretSchema *
grd_rdp_credentials_get_schema (void)
{
  static const SecretSchema grd_rdp_credentials_schema =
    {
      .name = "org.gnome.RemoteDesktop.RdpCredentials",
      .flags = SECRET_SCHEMA_NONE,
      .attributes =
        {
          { "credentials", SECRET_SCHEMA_ATTRIBUTE_STRING },
          { "NULL", 0 },
        },
    };

  return &grd_rdp_credentials_schema;
}

static void
store_rdp_credentials (const char *username,
                       const char *password)
{
  GVariantBuilder builder;
  char *credentials;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}", "username", g_variant_new_string (username));
  g_variant_builder_add (&builder, "{sv}", "password", g_variant_new_string (password));
  credentials = g_variant_print (g_variant_builder_end (&builder), TRUE);

  secret_password_store_sync (GRD_RDP_CREDENTIALS_SCHEMA,
                              SECRET_COLLECTION_DEFAULT,
                              "GNOME Remote Desktop RDP credentials",
                              credentials,
                              NULL, NULL,
                              NULL);

  g_free (credentials);
}

static void
load_rdp_credentials (CcRemoteDesktopPanel *self)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *secret;
  GVariant *variant = NULL;
  const gchar *username = NULL;
  const gchar *password = NULL;

  secret = secret_password_lookup_sync (GRD_RDP_CREDENTIALS_SCHEMA,
                                        self->cancellable,
                                        &error,
                                        NULL);
  if (error) {
    g_warning ("Failed to get password: %s", error->message);
    return;
  }

  variant = g_variant_parse (NULL, secret, NULL, NULL, &error);
  if (variant == NULL)
    g_warning ("Invalid credentials format in the keyring: %s", error->message);

  g_variant_lookup (variant, "username", "&s", &username);
  g_variant_lookup (variant, "password", "&s", &password);

  if (username != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->username_entry), username);
  if (password != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->password_entry), password);
}

static void
on_credential_entries_changed (CcRemoteDesktopPanel *self)
{
  const gchar *username = NULL;
  const gchar *password = NULL;

  username = gtk_editable_get_text (GTK_EDITABLE (self->username_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  if (username != NULL && password != NULL)
    store_rdp_credentials (username, password);
}

static void
on_tls_key_file_selected (GtkDialog  *filechooser,
                           gint        response,
                           CcRemoteDesktopPanel *self)
{
  g_autoptr(GFile) tls_key_file = NULL;
  const gchar *path;

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  tls_key_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (filechooser));
  path = g_file_get_path (tls_key_file);
  g_settings_set_string (self->remote_desktop_settings,
                         "tls-key", path);
  gtk_button_set_label (self->tls_key_button, path);
  gtk_window_destroy (GTK_WINDOW (filechooser));
}

static void
on_tls_key_button_clicked (GtkWidget *widget,
                            CcRemoteDesktopPanel *self)
{
  GtkWindow *toplevel;
  GtkWidget *filechooser;

  toplevel = (GtkWindow *) gtk_widget_get_native (GTK_WIDGET (self));
  filechooser = gtk_file_chooser_dialog_new (_("Select a TLS Key file"),
                                             toplevel,
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
                                             _("_Open"), GTK_RESPONSE_ACCEPT,
                                             NULL);
  gtk_window_set_modal (GTK_WINDOW (filechooser), TRUE);

  g_signal_connect_object (filechooser,
                           "response",
                           G_CALLBACK (on_tls_key_file_selected),
                           self,
                           0);
  gtk_window_present (GTK_WINDOW (filechooser));
}

static void
on_tls_cert_file_selected (GtkDialog  *filechooser,
                           gint        response,
                           CcRemoteDesktopPanel *self)
{
  g_autoptr(GFile) tls_cert_file = NULL;
  const gchar *path;

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  tls_cert_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (filechooser));
  path = g_file_get_path (tls_cert_file);
  g_settings_set_string (self->remote_desktop_settings,
                         "tls-cert", path);
  gtk_button_set_label (self->tls_cert_button, path);
  gtk_window_destroy (GTK_WINDOW (filechooser));
}

static void
on_tls_cert_button_clicked (GtkWidget *widget,
                            CcRemoteDesktopPanel *self)
{
  GtkWindow *toplevel;
  GtkWidget *filechooser;

  toplevel = (GtkWindow *) gtk_widget_get_native (GTK_WIDGET (self));
  filechooser = gtk_file_chooser_dialog_new (_("Select a TLS Cert"),
                                             toplevel,
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
                                             _("_Open"), GTK_RESPONSE_ACCEPT,
                                             NULL);
  gtk_window_set_modal (GTK_WINDOW (filechooser), TRUE);

  g_signal_connect_object (filechooser,
                           "response",
                           G_CALLBACK (on_tls_cert_file_selected),
                           self,
                           0);
  gtk_window_present (GTK_WINDOW (filechooser));
}

static void
remote_desktop_name_appeared (GDBusConnection *connection,
                              const gchar     *name,
                              const gchar     *name_owner,
                              gpointer         user_data)
{
  CcRemoteDesktopPanel *self = CC_REMOTE_DESKTOP_PANEL (user_data);

  g_bus_unwatch_name (self->remote_desktop_name_watch);
  self->remote_desktop_name_watch = 0;

  gtk_stack_set_visible_child (self->stack, self->remote_desktop_page);
}

static void
check_remote_desktop_available (CcRemoteDesktopPanel *self)
{
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;

  source = g_settings_schema_source_get_default ();
  if (!source)
    return;

  schema = g_settings_schema_source_lookup (source, GRD_RDP_SCHEMA_ID, TRUE);
  if (!schema)
    return;

  self->remote_desktop_name_watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                      "org.gnome.Mutter.RemoteDesktop",
                                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                      remote_desktop_name_appeared,
                                                      NULL,
                                                      self,
                                                      NULL);
}

static void
cc_remote_desktop_panel_finalize (GObject *object)
{
  CcRemoteDesktopPanel *self = CC_REMOTE_DESKTOP_PANEL (object);

  if (self->remote_desktop_name_watch)
    g_bus_unwatch_name (self->remote_desktop_name_watch);
  self->remote_desktop_name_watch = 0;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->remote_desktop_settings);

  G_OBJECT_CLASS (cc_remote_desktop_panel_parent_class)->finalize (object);
}

static void
cc_remote_desktop_panel_class_init (CcRemoteDesktopPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_remote_desktop_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/remote-desktop/cc-remote-desktop-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, remote_desktop_page);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, view_only_switch);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, username_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, tls_cert_button);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPanel, tls_key_button);

  gtk_widget_class_bind_template_callback (widget_class, on_credential_entries_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_tls_cert_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_tls_key_button_clicked);
}

static void
cc_remote_desktop_panel_init (CcRemoteDesktopPanel *self)
{
  g_resources_register (cc_remote_desktop_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  check_remote_desktop_available (self);

  self->cancellable = g_cancellable_new ();

  self->remote_desktop_settings = g_settings_new ("org.gnome.desktop.remote-desktop.rdp");

  g_settings_bind (self->remote_desktop_settings,
                   "view-only",
                   self->view_only_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->remote_desktop_settings,
                   "tls-cert",
                   self->tls_cert_button,
                   "label",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->remote_desktop_settings,
                   "tls-key",
                   self->tls_key_button,
                   "label",
                   G_SETTINGS_BIND_DEFAULT);

  load_rdp_credentials (self);
}
