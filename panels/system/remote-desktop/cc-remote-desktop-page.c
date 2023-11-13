/*
 * Copyright 2023 Gotam Gorabh <gautamy672@gmail.com>
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
#define G_LOG_DOMAIN "cc-remote-desktop-page"

#include "cc-gnome-remote-desktop.h"
#include "cc-hostname.h"
#include "cc-list-row.h"
#include "cc-remote-desktop-page.h"
#include "cc-tls-certificate.h"
#include "cc-systemd-service.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>

#include "org.gnome.SettingsDaemon.Sharing.h"

#include <pwd.h>
#include <pwquality.h>
#include <unistd.h>

#define GNOME_REMOTE_DESKTOP_SCHEMA_ID "org.gnome.desktop.remote-desktop"
#define GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID "org.gnome.desktop.remote-desktop.rdp"
#define REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S 1
#define REMOTE_DESKTOP_SERVICE "gnome-remote-desktop.service"

struct _CcRemoteDesktopPage {
  AdwNavigationPage parent_instance;

  AdwPreferencesPage *remote_desktop_page;
  AdwSwitchRow *remote_control_row;
  GtkWidget *remote_desktop_toast_overlay;
  GtkWidget *remote_desktop_password_entry;
  GtkWidget *remote_desktop_username_entry;
  GtkWidget *remote_desktop_device_name_label;
  GtkWidget *remote_desktop_address_label;
  AdwSwitchRow *remote_desktop_row;
  GtkWidget *remote_desktop_verify_encryption;
  GtkWidget *remote_desktop_fingerprint_dialog;
  GtkWidget *remote_desktop_fingerprint_left;
  GtkWidget *remote_desktop_fingerprint_right;

  guint remote_desktop_name_watch;
  guint remote_desktop_store_credentials_id;
  GTlsCertificate *remote_desktop_certificate;

  GSettings *rdp_settings;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (CcRemoteDesktopPage, cc_remote_desktop_page, ADW_TYPE_NAVIGATION_PAGE)

static void
remote_desktop_show_encryption_fingerprint (CcRemoteDesktopPage *self)
{
  g_autoptr(GByteArray) der = NULL;
  g_autoptr(GcrCertificate) gcr_cert = NULL;
  g_autofree char *fingerprint = NULL;
  g_auto(GStrv) fingerprintv = NULL;
  g_autofree char *left_string = NULL;
  g_autofree char *right_string = NULL;
  GtkNative *native;

  g_return_if_fail (self->remote_desktop_certificate);

  g_object_get (self->remote_desktop_certificate,
                "certificate", &der, NULL);
  gcr_cert = gcr_simple_certificate_new (der->data, der->len);
  if (!gcr_cert)
    {
      g_warning ("Failed to load GCR TLS certificate representation");
      return;
    }

  fingerprint = gcr_certificate_get_fingerprint_hex (gcr_cert, G_CHECKSUM_SHA256);

  fingerprintv = g_strsplit (fingerprint, " ", -1);
  g_return_if_fail (g_strv_length (fingerprintv) == 32);

  left_string = g_strdup_printf (
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n",
    fingerprintv[0], fingerprintv[1], fingerprintv[2], fingerprintv[3],
    fingerprintv[8], fingerprintv[9], fingerprintv[10], fingerprintv[11],
    fingerprintv[16], fingerprintv[17], fingerprintv[18], fingerprintv[19],
    fingerprintv[24], fingerprintv[25], fingerprintv[26], fingerprintv[27]);

 right_string = g_strdup_printf (
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n",
   fingerprintv[4], fingerprintv[5], fingerprintv[6], fingerprintv[7],
   fingerprintv[12], fingerprintv[13], fingerprintv[14], fingerprintv[15],
   fingerprintv[20], fingerprintv[21], fingerprintv[22], fingerprintv[23],
   fingerprintv[28], fingerprintv[29], fingerprintv[30], fingerprintv[31]);

  gtk_label_set_label (GTK_LABEL (self->remote_desktop_fingerprint_left),
                       left_string);
  gtk_label_set_label (GTK_LABEL (self->remote_desktop_fingerprint_right),
                       right_string);

  native = gtk_widget_get_native (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (self->remote_desktop_fingerprint_dialog),
                                GTK_WINDOW (native));

  gtk_window_present (GTK_WINDOW (self->remote_desktop_fingerprint_dialog));
}

static char *
get_hostname (void)
{
  return cc_hostname_get_display_hostname (cc_hostname_get_default ());
}

static void
cc_remote_desktop_page_setup_label_with_hostname (CcRemoteDesktopPage *self,
                                                         GtkWidget      *label)
{
  g_autofree gchar *text = NULL;
  const gchar *hostname;

  hostname = get_hostname ();

  if (label == self->remote_desktop_address_label)
    {
      text = g_strdup_printf ("rdp://%s", hostname);
    }
  else
    g_assert_not_reached ();

  gtk_label_set_label (GTK_LABEL (label), text);
}

static gboolean
cc_remote_desktop_page_check_schema_available (CcRemoteDesktopPage *self,
                                                      const gchar *schema_id)
{
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;

  source = g_settings_schema_source_get_default ();
  if (!source)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
  if (!schema)
    return FALSE;

  return TRUE;
}

static gboolean
store_remote_desktop_credentials_timeout (gpointer user_data)
{
  CcRemoteDesktopPage *self = (CcRemoteDesktopPage *)user_data;
  const char *username, *password;

  username = gtk_editable_get_text (GTK_EDITABLE (self->remote_desktop_username_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->remote_desktop_password_entry));

  if (username && password)
    {
      cc_grd_store_rdp_credentials (username, password,
                                    self->cancellable);
    }

  self->remote_desktop_store_credentials_id = 0;

  return G_SOURCE_REMOVE;
}

static gboolean
is_remote_desktop_enabled (CcRemoteDesktopPage *self)
{
  if (!g_settings_get_boolean (self->rdp_settings, "enable"))
    return FALSE;

  return cc_is_service_active (REMOTE_DESKTOP_SERVICE, G_BUS_TYPE_SESSION);
}

static void
enable_gnome_remote_desktop_service (CcRemoteDesktopPage *self)
{
  g_autoptr(GError) error = NULL;

  if (is_remote_desktop_enabled (self))
    return;

  if (!cc_enable_service (REMOTE_DESKTOP_SERVICE,
                          G_BUS_TYPE_SESSION,
                          &error))
    g_warning ("Failed to enable remote desktop service: %s", error->message);
}

static void
remote_desktop_credentials_changed (CcRemoteDesktopPage *self)
{
  g_clear_handle_id (&self->remote_desktop_store_credentials_id,
                     g_source_remove);

  self->remote_desktop_store_credentials_id =
    g_timeout_add_seconds (REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S,
                           store_remote_desktop_credentials_timeout,
                           self);
}

static void
calc_default_tls_paths (char **out_dir_path,
                        char **out_cert_path,
                        char **out_key_path)
{
  g_autofree char *dir_path = NULL;

  dir_path = g_build_filename(g_get_user_data_dir(), "gnome-remote-desktop", NULL);

  if (out_cert_path)
    *out_cert_path = g_build_filename(dir_path, "rdp-tls.crt", NULL);
  if (out_key_path)
    *out_key_path = g_build_filename(dir_path, "rdp-tls.key", NULL);

  if (out_dir_path)
    *out_dir_path = g_steal_pointer (&dir_path);
}

static void
set_tls_certificate (CcRemoteDesktopPage  *self,
                     GTlsCertificate *tls_certificate)
{
  g_set_object (&self->remote_desktop_certificate,
                tls_certificate);
  gtk_widget_set_sensitive (self->remote_desktop_verify_encryption, TRUE);
}

static void
on_certificate_generated (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcRemoteDesktopPage *self;
  g_autoptr(GTlsCertificate) tls_certificate = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;

  tls_certificate = bonsai_tls_certificate_new_generate_finish (res, &error);
  if (!tls_certificate)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Failed to generate TLS certificate: %s", error->message);
      return;
    }

  self = (CcRemoteDesktopPage *)user_data;

  calc_default_tls_paths (NULL, &cert_path, &key_path);

  g_settings_set_string (self->rdp_settings, "tls-cert", cert_path);
  g_settings_set_string (self->rdp_settings, "tls-key", key_path);

  set_tls_certificate (self, tls_certificate);

  enable_gnome_remote_desktop_service (self);
}

static void
disable_gnome_remote_desktop_service (CcRemoteDesktopPage *self)
{
  g_autoptr(GError) error = NULL;

  g_settings_set_boolean (self->rdp_settings, "enable", FALSE);

  if (!cc_disable_service (REMOTE_DESKTOP_SERVICE,
                           G_BUS_TYPE_SESSION,
                           &error))
    g_warning ("Failed to enable remote desktop service: %s", error->message);
}

static void
enable_gnome_remote_desktop (CcRemoteDesktopPage *self)
{
  g_autofree char *dir_path = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;
  g_autoptr(GFile) dir = NULL;
  g_autoptr(GFile) cert_file = NULL;
  g_autoptr(GFile) key_file = NULL;
  g_autoptr(GError) error = NULL;

  g_settings_set_boolean (self->rdp_settings, "enable", TRUE);

  cert_path = g_settings_get_string (self->rdp_settings, "tls-cert");
  key_path = g_settings_get_string (self->rdp_settings, "tls-key");
  if (strlen (cert_path) > 0 &&
      strlen (key_path) > 0)
    {
      g_autoptr(GTlsCertificate) tls_certificate = NULL;

      tls_certificate = g_tls_certificate_new_from_file (cert_path, &error);
      if (tls_certificate)
        {
          set_tls_certificate (self, tls_certificate);

          enable_gnome_remote_desktop_service (self);
          return;
        }

      g_warning ("Configured TLS certificate invalid: %s", error->message);
      return;
    }

  calc_default_tls_paths (&dir_path, &cert_path, &key_path);

  dir = g_file_new_for_path (dir_path);
  if (!g_file_query_exists (dir, NULL))
    {
      if (!g_file_make_directory_with_parents (dir, NULL, &error))
        {
          g_warning ("Failed to create remote desktop certificate directory: %s",
                     error->message);
          return;
        }
    }

  cert_file = g_file_new_for_path (cert_path);
  key_file = g_file_new_for_path (key_path);

  if (g_file_query_exists (cert_file, NULL) &&
      g_file_query_exists (key_file, NULL))
    {
      g_autoptr(GTlsCertificate) tls_certificate = NULL;

      tls_certificate = g_tls_certificate_new_from_file (cert_path, &error);
      if (tls_certificate)
        {
          g_settings_set_string (self->rdp_settings, "tls-cert", cert_path);
          g_settings_set_string (self->rdp_settings, "tls-key", key_path);

          set_tls_certificate (self, tls_certificate);

          enable_gnome_remote_desktop_service (self);
          return;
        }

      g_warning ("Existing TLS certificate invalid: %s", error->message);
      return;
    }

  bonsai_tls_certificate_new_generate_async (cert_path,
                                             key_path,
                                             "US",
                                             "GNOME",
                                             self->cancellable,
                                             on_certificate_generated,
                                             self);
}

static void
on_remote_desktop_active_changed (CcRemoteDesktopPage *self)
{
  if (adw_switch_row_get_active (self->remote_desktop_row))
    enable_gnome_remote_desktop (self);
  else
    disable_gnome_remote_desktop_service (self);
}

static void
add_toast (CcRemoteDesktopPage *self,
           const char                *message)
{
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->remote_desktop_toast_overlay),
                               adw_toast_new (message));
}

static void
on_device_name_copy_clicked (CcRemoteDesktopPage *self,
                             GtkButton                 *button)
{
  GtkLabel *label = GTK_LABEL (self->remote_desktop_device_name_label);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_label_get_text (label));
  add_toast (self, _("Device name copied"));
}

static void
on_device_address_copy_clicked (CcRemoteDesktopPage *self,
                                GtkButton                 *button)
{
  GtkLabel *label = GTK_LABEL (self->remote_desktop_address_label);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_label_get_text (label));
  add_toast (self, _("Device address copied"));
}

static void
on_username_copy_clicked (CcRemoteDesktopPage *self,
                          GtkButton                 *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->remote_desktop_username_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Username copied"));
}

static void
on_password_copy_clicked (CcRemoteDesktopPage *self,
                          GtkButton                 *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->remote_desktop_password_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Password copied"));
}

static pwquality_settings_t *
get_pwq (void)
{
  static pwquality_settings_t *settings;

  if (settings == NULL)
    {
      gchar *err = NULL;
      gint rv = 0;

      settings = pwquality_default_settings ();
      pwquality_set_int_value (settings, PWQ_SETTING_MAX_SEQUENCE, 4);

      rv = pwquality_read_config (settings, NULL, (gpointer)&err);
      if (rv < 0)
        {
          g_warning ("Failed to read pwquality configuration: %s\n",
                     pwquality_strerror (NULL, 0, rv, err));
          pwquality_free_settings (settings);

          /* Load just default settings in case of failure. */
          settings = pwquality_default_settings ();
          pwquality_set_int_value (settings, PWQ_SETTING_MAX_SEQUENCE, 4);
        }
    }

  return settings;
}

static char *
pw_generate (void)
{
  g_autofree gchar *res = NULL;
  int rv;

  rv = pwquality_generate (get_pwq (), 0, &res);

  if (rv < 0)
    {
      g_warning ("Password generation failed: %s\n",
                 pwquality_strerror (NULL, 0, rv, NULL));
      return NULL;
    }

  return g_steal_pointer (&res);
}

static void
cc_remote_desktop_page_setup_remote_desktop_dialog (CcRemoteDesktopPage *self)
{
  const gchar *username = NULL;
  const gchar *password = NULL;
  g_autofree char *hostname = NULL;

  self->rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  adw_switch_row_set_active (self->remote_desktop_row, is_remote_desktop_enabled (self));
  g_settings_bind (self->rdp_settings,
                   "enable",
                   self->remote_desktop_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->rdp_settings,
                   "view-only",
                   self->remote_control_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_object_bind_property (self->remote_desktop_row, "active",
                          self->remote_control_row, "sensitive",
                          G_BINDING_SYNC_CREATE);

  hostname = get_hostname ();
  gtk_label_set_label (GTK_LABEL (self->remote_desktop_device_name_label),
                       hostname);

  username = cc_grd_lookup_rdp_username (self->cancellable);
  password = cc_grd_lookup_rdp_password (self->cancellable);
  if (username != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_username_entry), username);
  if (password != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_password_entry), password);

  g_signal_connect_swapped (self->remote_desktop_username_entry,
                            "notify::text",
                            G_CALLBACK (remote_desktop_credentials_changed),
                            self);
  g_signal_connect_swapped (self->remote_desktop_password_entry,
                            "notify::text",
                            G_CALLBACK (remote_desktop_credentials_changed),
                            self);
  if (username == NULL)
    {
      struct passwd *pw = getpwuid (getuid ());
      if (pw != NULL)
        username = g_strdup (pw->pw_name);
      else
        g_warning ("Failed to get username: %s", g_strerror (errno));
    }
  gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_username_entry), username);

  if (password == NULL)
    {
      g_autofree gchar *pw = pw_generate ();
      if (pw != NULL)
        gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_password_entry),
                               pw );
    }

  if (is_remote_desktop_enabled (self))
    {
      adw_switch_row_set_active (self->remote_desktop_row,
                             TRUE);
    }
  g_signal_connect_object (self->remote_desktop_row, "notify::active",
                           G_CALLBACK (on_remote_desktop_active_changed), self,
                           G_CONNECT_SWAPPED);
  on_remote_desktop_active_changed (self);
}

static void
remote_desktop_name_appeared (GDBusConnection *connection,
                              const gchar     *name,
                              const gchar     *name_owner,
                              gpointer         user_data)
{
  CcRemoteDesktopPage *self = (CcRemoteDesktopPage *)user_data;

  g_bus_unwatch_name (self->remote_desktop_name_watch);
  self->remote_desktop_name_watch = 0;

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  cc_remote_desktop_page_setup_remote_desktop_dialog (self);
}

static void
check_remote_desktop_available (CcRemoteDesktopPage *self)
{
  if (!cc_remote_desktop_page_check_schema_available (self, GNOME_REMOTE_DESKTOP_SCHEMA_ID) ||
      !cc_remote_desktop_page_check_schema_available (self, GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID))
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      return;
    }

  self->remote_desktop_name_watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                      "org.gnome.Mutter.RemoteDesktop",
                                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                      remote_desktop_name_appeared,
                                                      NULL,
                                                      self,
                                                      NULL);
}

static void
cc_remote_desktop_page_dispose (GObject *object)
{
  CcRemoteDesktopPage *self = (CcRemoteDesktopPage *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_handle_id (&self->remote_desktop_store_credentials_id, g_source_remove);
  self->remote_desktop_store_credentials_id = 0;

  g_clear_object (&self->rdp_settings);

  G_OBJECT_CLASS (cc_remote_desktop_page_parent_class)->dispose (object);
}

static void
cc_remote_desktop_page_class_init (CcRemoteDesktopPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_remote_desktop_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/remote-desktop/cc-remote-desktop-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_page);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_control_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_username_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_device_name_label);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_address_label);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_verify_encryption);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_fingerprint_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_fingerprint_left);
  gtk_widget_class_bind_template_child (widget_class, CcRemoteDesktopPage, remote_desktop_fingerprint_right);

  gtk_widget_class_bind_template_callback (widget_class, on_device_name_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_device_address_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_username_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_password_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, remote_desktop_show_encryption_fingerprint);
}

static void
cc_remote_desktop_page_init (CcRemoteDesktopPage *self)
{
  g_autofree gchar *learn_more_link = NULL;
  g_autofree gchar *page_description = NULL;

  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  check_remote_desktop_available (self);

  cc_remote_desktop_page_setup_label_with_hostname (self, self->remote_desktop_address_label);

  /* Translators: This will be presented as the text of a link to the documentation */
  learn_more_link = g_strdup_printf ("<a href='help:gnome-help/sharing-desktop'>%s</a>", _("learn how to use it"));
  /* Translators: %s is a link to the documentation with the label "learn how to use it" */
  page_description = g_strdup_printf (_("Remote desktop allows viewing and controlling your desktop from another computer – %s."), learn_more_link);
  adw_preferences_page_set_description (self->remote_desktop_page, page_description);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/system/remote-desktop/remote-desktop.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
