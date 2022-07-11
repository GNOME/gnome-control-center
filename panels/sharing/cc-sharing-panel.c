/*
 * Copyright (C) 2013 Intel, Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-sharing-panel.h"
#include "cc-hostname-entry.h"
#include "cc-list-row.h"

#include "cc-sharing-resources.h"
#include "cc-remote-login.h"
#include "file-share-properties.h"
#include "cc-media-sharing.h"
#include "cc-sharing-networks.h"
#include "cc-gnome-remote-desktop.h"
#include "cc-tls-certificate.h"
#include "cc-systemd-service.h"
#include "org.gnome.SettingsDaemon.Sharing.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#include <glib/gi18n.h>

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr-base.h>

#include <pwquality.h>

#include <config.h>

#include <unistd.h>
#include <pwd.h>

static void cc_sharing_panel_setup_label_with_hostname (CcSharingPanel *self, GtkWidget *label);
static GtkWidget *cc_sharing_panel_new_media_sharing_row (const char     *uri_or_path,
                                                          CcSharingPanel *self);

#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"
#define GNOME_REMOTE_DESKTOP_SCHEMA_ID "org.gnome.desktop.remote-desktop"
#define GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID "org.gnome.desktop.remote-desktop.rdp"

#define REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S 1

#define REMOTE_DESKTOP_SERVICE "gnome-remote-desktop.service"

struct _CcSharingPanel
{
  CcPanel parent_instance;

  GtkWidget *hostname_entry;
  GtkWidget *main_list_box;
  GtkWidget *master_switch;
  GtkWidget *media_sharing_dialog;
  GtkWidget *media_sharing_headerbar;
  GtkWidget *media_sharing_row;
  GtkWidget *media_sharing_switch;
  GtkWidget *personal_file_sharing_dialog;
  GtkWidget *personal_file_sharing_grid;
  GtkWidget *personal_file_sharing_headerbar;
  GtkWidget *personal_file_sharing_label;
  GtkWidget *personal_file_sharing_password_entry;
  GtkWidget *personal_file_sharing_password_label;
  GtkWidget *personal_file_sharing_require_password_switch;
  GtkWidget *personal_file_sharing_row;
  GtkWidget *personal_file_sharing_switch;
  GtkWidget *remote_login_dialog;
  GtkWidget *remote_login_label;
  GtkWidget *remote_login_row;
  GtkWidget *remote_login_switch;

  GtkWidget *remote_control_switch;
  GtkWidget *remote_control_checkbutton;
  GtkWidget *remote_desktop_toast_overlay;
  GtkWidget *remote_desktop_password_entry;
  GtkWidget *remote_desktop_password_copy;
  GtkWidget *remote_desktop_username_entry;
  GtkWidget *remote_desktop_username_copy;
  GtkWidget *remote_desktop_dialog;
  GtkWidget *remote_desktop_device_name_label;
  GtkWidget *remote_desktop_device_name_copy;
  GtkWidget *remote_desktop_address_label;
  GtkWidget *remote_desktop_address_copy;
  GtkWidget *remote_desktop_row;
  GtkWidget *remote_desktop_switch;
  GtkWidget *remote_desktop_verify_encryption;
  GtkWidget *remote_desktop_fingerprint_dialog;
  GtkWidget *remote_desktop_fingerprint_left;
  GtkWidget *remote_desktop_fingerprint_right;

  GtkWidget *shared_folders_grid;
  GtkWidget *shared_folders_listbox;

  GDBusProxy *sharing_proxy;

  guint remote_desktop_name_watch;
  guint remote_desktop_store_credentials_id;
  GTlsCertificate *remote_desktop_certificate;
};

CC_PANEL_REGISTER (CcSharingPanel, cc_sharing_panel)

#define OFF_IF_VISIBLE(x, y) { if (gtk_widget_is_visible(x) && (y) != NULL && gtk_widget_is_sensitive(y)) gtk_switch_set_active (GTK_SWITCH(y), FALSE); }

static gboolean store_remote_desktop_credentials_timeout (gpointer user_data);

static void
cc_sharing_panel_master_switch_notify (CcSharingPanel *self)
{
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (self->master_switch));

  if (!active)
    {
      /* disable all services if the master switch is not active */
      OFF_IF_VISIBLE(self->media_sharing_row, self->media_sharing_switch);
      OFF_IF_VISIBLE(self->personal_file_sharing_row, self->personal_file_sharing_switch);
      OFF_IF_VISIBLE(self->remote_desktop_row, self->remote_desktop_switch);

      gtk_switch_set_active (GTK_SWITCH (self->remote_login_switch), FALSE);
    }

  gtk_widget_set_sensitive (self->main_list_box, active);
}

static void
cc_sharing_panel_dispose (GObject *object)
{
  CcSharingPanel *self = CC_SHARING_PANEL (object);

  if (self->remote_desktop_name_watch)
    g_bus_unwatch_name (self->remote_desktop_name_watch);
  self->remote_desktop_name_watch = 0;

  if (self->media_sharing_dialog)
    {
      gtk_window_destroy (GTK_WINDOW (self->media_sharing_dialog));
      self->media_sharing_dialog = NULL;
    }

  if (self->personal_file_sharing_dialog)
    {
      gtk_window_destroy (GTK_WINDOW (self->personal_file_sharing_dialog));
      self->personal_file_sharing_dialog = NULL;
    }

  if (self->remote_login_dialog)
    {
      gtk_window_destroy (GTK_WINDOW (self->remote_login_dialog));
      self->remote_login_dialog = NULL;
    }

  if (self->remote_desktop_dialog)
    {
      gtk_window_destroy (GTK_WINDOW (self->remote_desktop_dialog));
      self->remote_desktop_dialog = NULL;
    }

  g_clear_object (&self->sharing_proxy);

  if (self->remote_desktop_store_credentials_id)
    {
      g_clear_handle_id (&self->remote_desktop_store_credentials_id,
                         g_source_remove);
      store_remote_desktop_credentials_timeout (self);
    }

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->dispose (object);
}

static const char *
cc_sharing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-sharing";
}

static void
remote_desktop_show_encryption_fingerprint (CcSharingPanel *self)
{
  g_autoptr(GByteArray) der = NULL;
  g_autoptr(GcrCertificate) gcr_cert = NULL;
  g_autofree char *fingerprint = NULL;
  g_auto(GStrv) fingerprintv = NULL;
  g_autofree char *left_string = NULL;
  g_autofree char *right_string = NULL;

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

  gtk_window_set_transient_for (GTK_WINDOW (self->remote_desktop_fingerprint_dialog),
                                GTK_WINDOW (self->remote_desktop_dialog));
  gtk_window_present (GTK_WINDOW (self->remote_desktop_fingerprint_dialog));
}

static void
cc_sharing_panel_class_init (CcSharingPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_sharing_panel_dispose;

  panel_class->get_help_uri = cc_sharing_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sharing/cc-sharing-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, hostname_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, master_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, main_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_require_password_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_username_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_username_copy);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_password_copy);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_device_name_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_device_name_copy);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_address_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_address_copy);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_verify_encryption);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_fingerprint_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_fingerprint_left);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_desktop_fingerprint_right);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_listbox);

  gtk_widget_class_bind_template_callback (widget_class, remote_desktop_show_encryption_fingerprint);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_sharing_panel_run_dialog (CcSharingPanel *self,
                             GtkWidget      *dialog)
{
  GtkWidget *parent;

  /* ensure labels with the hostname are updated if the hostname has changed */
  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->remote_desktop_address_label);
  cc_sharing_panel_setup_label_with_hostname (self, self->remote_login_label);
  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->personal_file_sharing_label);


  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
cc_sharing_panel_main_list_box_row_activated (CcSharingPanel *self,
                                              GtkListBoxRow  *row)
{
  GtkWidget *dialog;

  if (row == GTK_LIST_BOX_ROW (self->media_sharing_row))
    dialog = self->media_sharing_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->personal_file_sharing_row))
    dialog = self->personal_file_sharing_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->remote_login_row))
    dialog = self->remote_login_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->remote_desktop_row))
    dialog = self->remote_desktop_dialog;
  else
    return;

  gtk_list_box_select_row (GTK_LIST_BOX (self->main_list_box), NULL);

  cc_sharing_panel_run_dialog (self, dialog);
}

static gboolean
cc_sharing_panel_switch_to_label_transform_func (GBinding       *binding,
                                                 const GValue   *source_value,
                                                 GValue         *target_value,
                                                 CcSharingPanel *self)
{
  gboolean active;

  if (!G_VALUE_HOLDS_BOOLEAN (source_value))
    return FALSE;

  if (!G_VALUE_HOLDS_STRING (target_value))
    return FALSE;

  active = g_value_get_boolean (source_value);

  if (active)
    g_value_set_string (target_value, C_("service is enabled", "On"));
  else
    g_value_set_string (target_value, C_("service is disabled", "Off"));

  /* ensure the master switch is active if one of the services is active */
  if (active)
    gtk_switch_set_active (GTK_SWITCH (self->master_switch), TRUE);

  return TRUE;
}

static gboolean
cc_sharing_panel_networks_to_label_transform_func (GBinding       *binding,
                                                   const GValue   *source_value,
                                                   GValue         *target_value,
                                                   CcSharingPanel *self)
{
  CcSharingStatus status;

  if (!G_VALUE_HOLDS_UINT (source_value))
    return FALSE;

  if (!G_VALUE_HOLDS_STRING (target_value))
    return FALSE;

  status = g_value_get_uint (source_value);

  switch (status) {
  case CC_SHARING_STATUS_OFF:
    g_value_set_string (target_value, C_("service is disabled", "Off"));
    break;
  case CC_SHARING_STATUS_ENABLED:
    g_value_set_string (target_value, C_("service is enabled", "Enabled"));
    break;
  case CC_SHARING_STATUS_ACTIVE:
    g_value_set_string (target_value, C_("service is active", "Active"));
    break;
  default:
    return FALSE;
  }

  /* ensure the master switch is active if one of the services is active */
  if (status != CC_SHARING_STATUS_OFF)
    gtk_switch_set_active (GTK_SWITCH (self->master_switch), TRUE);

  return TRUE;
}

static void
cc_sharing_panel_bind_switch_to_label (CcSharingPanel *self,
                                       GtkWidget      *gtkswitch,
                                       GtkWidget      *row)
{
  g_object_bind_property_full (gtkswitch, "active", row, "secondary-label",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) cc_sharing_panel_switch_to_label_transform_func,
                               NULL, self, NULL);
}

static void
cc_sharing_panel_bind_networks_to_label (CcSharingPanel *self,
					 GtkWidget      *networks,
					 GtkWidget      *list_row)
{
  g_object_bind_property_full (networks, "status", list_row, "secondary-label",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) cc_sharing_panel_networks_to_label_transform_func,
                               NULL, self, NULL);
}

static void
cc_sharing_panel_bind_switch_to_widgets (GtkWidget *gtkswitch,
                                         GtkWidget *first_widget,
                                         ...)
{
  va_list w;
  GtkWidget *widget;

  va_start (w, first_widget);

  g_object_bind_property (gtkswitch, "active", first_widget,
                          "sensitive", G_BINDING_SYNC_CREATE);

  while ((widget = va_arg (w, GtkWidget*)))
    {
      g_object_bind_property (gtkswitch, "active", widget,
                              "sensitive", G_BINDING_SYNC_CREATE);
    }

  va_end (w);
}

static void
on_add_folder_dialog_response_cb (GtkDialog      *dialog,
                                  gint            response,
                                  CcSharingPanel *self)
{
  g_autofree gchar *folder = NULL;
  g_autoptr(GFile) file = NULL;
  GtkWidget *child;
  gboolean matching = FALSE;
  gint n_rows = 0;

  if (response != GTK_RESPONSE_ACCEPT)
    goto bail;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
  folder = g_file_get_uri (file);
  if (!folder || g_str_equal (folder, ""))
    goto bail;

  g_debug ("Trying to add %s", folder);

  for (child = gtk_widget_get_first_child (self->shared_folders_listbox);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      const char *string;

      string = g_object_get_data (G_OBJECT (child), "path");
      matching = (g_strcmp0 (string, folder) == 0);

      if (matching)
        {
          g_debug ("Found a duplicate for %s", folder);
          break;
        }

      n_rows++;
    }

  if (!matching)
    {
      GtkWidget *row = cc_sharing_panel_new_media_sharing_row (folder, self);

      gtk_list_box_insert (GTK_LIST_BOX (self->shared_folders_listbox),
                           row,
                           n_rows - 1);
    }

bail:
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
cc_sharing_panel_add_folder (CcSharingPanel *self,
                             GtkListBoxRow  *row)
{
  GtkWidget *dialog;

  if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-add")))
    return;

  dialog = gtk_file_chooser_dialog_new (_("Choose a Folder"),
                                        GTK_WINDOW (self->media_sharing_dialog),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_add_folder_dialog_response_cb),
                           self,
                           0);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
cc_sharing_panel_remove_folder (CcSharingPanel *self,
                                GtkButton      *button)
{
  GtkWidget *row;

  row = g_object_get_data (G_OBJECT (button), "row");
  gtk_list_box_remove (GTK_LIST_BOX (self->shared_folders_listbox), row);
}

static void
cc_sharing_panel_media_sharing_dialog_response (CcSharingPanel *self,
                                                gint            reponse_id)
{
  g_autoptr(GPtrArray) folders = NULL;
  GtkWidget *child;

  folders = g_ptr_array_new_with_free_func (g_free);

  for (child = gtk_widget_get_first_child (self->shared_folders_listbox);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      const char *folder;

      folder = g_object_get_data (G_OBJECT (child), "path");
      if (folder == NULL)
        continue;
      g_ptr_array_add (folders, g_strdup (folder));
    }

  g_ptr_array_add (folders, NULL);

  cc_media_sharing_set_preferences ((gchar **) folders->pdata);
}

#define ICON_NAME_FOLDER                "folder-symbolic"
#define ICON_NAME_FOLDER_DESKTOP        "user-desktop-symbolic"
#define ICON_NAME_FOLDER_DOCUMENTS      "folder-documents-symbolic"
#define ICON_NAME_FOLDER_DOWNLOAD       "folder-download-symbolic"
#define ICON_NAME_FOLDER_MUSIC          "folder-music-symbolic"
#define ICON_NAME_FOLDER_PICTURES       "folder-pictures-symbolic"
#define ICON_NAME_FOLDER_PUBLIC_SHARE   "folder-publicshare-symbolic"
#define ICON_NAME_FOLDER_TEMPLATES      "folder-templates-symbolic"
#define ICON_NAME_FOLDER_VIDEOS         "folder-videos-symbolic"
#define ICON_NAME_FOLDER_SAVED_SEARCH   "folder-saved-search-symbolic"

static GIcon *
special_directory_get_gicon (GUserDirectory directory)
{
#define ICON_CASE(x)                      \
  case G_USER_DIRECTORY_ ## x:            \
          return g_themed_icon_new_with_default_fallbacks (ICON_NAME_FOLDER_ ## x);

  switch (directory)
    {
      ICON_CASE (DESKTOP);
      ICON_CASE (DOCUMENTS);
      ICON_CASE (DOWNLOAD);
      ICON_CASE (MUSIC);
      ICON_CASE (PICTURES);
      ICON_CASE (PUBLIC_SHARE);
      ICON_CASE (TEMPLATES);
      ICON_CASE (VIDEOS);

    default:
      return g_themed_icon_new_with_default_fallbacks (ICON_NAME_FOLDER);
    }

#undef ICON_CASE
}

static GtkWidget *
cc_sharing_panel_new_media_sharing_row (const char     *uri_or_path,
                                        CcSharingPanel *self)
{
  GtkWidget *row, *w;
  GUserDirectory dir = G_USER_N_DIRECTORIES;
  g_autoptr(GIcon) icon = NULL;
  guint i;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;

  file = g_file_new_for_commandline_arg (uri_or_path);
  path = g_file_get_path (file);

  row = adw_action_row_new ();

  /* Find the icon and create it */
  for (i = 0; i < G_USER_N_DIRECTORIES; i++)
    {
      if (g_strcmp0 (path, g_get_user_special_dir (i)) == 0)
        {
          dir = i;
          break;
        }
    }

  icon = special_directory_get_gicon (dir);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row),
                             gtk_image_new_from_gicon (icon));

  /* Label */
  basename = g_filename_display_basename (path);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), basename);

  /* Remove button */
  w = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_add_css_class (w, "flat");
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_accessible_update_property (GTK_ACCESSIBLE (w),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Remove"),
                                -1);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);
  g_signal_connect_object (G_OBJECT (w), "clicked",
                           G_CALLBACK (cc_sharing_panel_remove_folder), self, G_CONNECT_SWAPPED);
  g_object_set_data (G_OBJECT (w), "row", row);

  g_object_set_data_full (G_OBJECT (row), "path", g_steal_pointer (&path), g_free);

  return row;
}

static GtkWidget *
cc_sharing_panel_new_add_media_sharing_row (CcSharingPanel *self)
{
  GtkWidget *row, *box, *w;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  w = gtk_image_new_from_icon_name ("list-add-symbolic");
  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_margin_top (w, 12);
  gtk_widget_set_margin_bottom (w, 12);
  gtk_accessible_update_property (GTK_ACCESSIBLE (w),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Add"),
                                -1);
  gtk_box_append (GTK_BOX (box), w);

  g_object_set_data (G_OBJECT (w), "row", row);

  g_object_set_data (G_OBJECT (row), "is-add", GINT_TO_POINTER (1));

  return row;
}

static GtkWidget *
create_switch_with_bindings (GtkSwitch *from)
{
  GtkWidget *new_switch = gtk_switch_new ();

  g_object_bind_property (from, "visible", new_switch, "visible", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (from, "state", new_switch, "state", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (from, "active", new_switch, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (from, "sensitive", new_switch, "sensitive", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  return new_switch;
}

static void
cc_sharing_panel_setup_media_sharing_dialog (CcSharingPanel *self)
{
  g_auto(GStrv) folders = NULL;
  GStrv list;
  GtkWidget *row, *networks, *w;
  g_autofree gchar *path = NULL;

  path = g_find_program_in_path ("rygel");
  if (path == NULL)
    {
      gtk_widget_hide (self->media_sharing_row);
      return;
    }

  g_signal_connect_object (self->media_sharing_dialog, "response",
                           G_CALLBACK (cc_sharing_panel_media_sharing_dialog_response),
                           self, G_CONNECT_SWAPPED);

  cc_media_sharing_get_preferences (&folders);

  list = folders;
  while (list && *list)
    {
      row = cc_sharing_panel_new_media_sharing_row (*list, self);
      gtk_list_box_insert (GTK_LIST_BOX (self->shared_folders_listbox), row, -1);
      list++;
    }

  row = cc_sharing_panel_new_add_media_sharing_row (self);
  gtk_list_box_append (GTK_LIST_BOX (self->shared_folders_listbox), row);

  g_signal_connect_object (self->shared_folders_listbox, "row-activated",
                           G_CALLBACK (cc_sharing_panel_add_folder), self, G_CONNECT_SWAPPED);

  networks = cc_sharing_networks_new (self->sharing_proxy, "rygel");
  gtk_grid_attach (GTK_GRID (self->shared_folders_grid), networks, 0, 4, 2, 1);

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_accessible_update_property (GTK_ACCESSIBLE (w),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Enable media sharing"),
                                -1);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->media_sharing_headerbar), w);
  self->media_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->media_sharing_row);
}

static void
cc_sharing_panel_setup_label (CcSharingPanel *self,
                              GtkWidget      *label,
                              const gchar    *hostname)
{
  g_autofree gchar *text = NULL;

  if (label == self->personal_file_sharing_label)
    {
      g_autofree gchar *url = g_strdup_printf ("<a href=\"dav://%s\">dav://%s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a dav://<hostname> URL */
      text = g_strdup_printf (_("File Sharing allows you to share your Public folder with others on your current network using: %s"), url);
    }
  else if (label == self->remote_login_label)
    {
      g_autofree gchar *command = g_strdup_printf ("<a href=\"ssh %s\">ssh %s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a "ssh <hostname>" command to run */
      text = g_strdup_printf (_("When remote login is enabled, remote users can connect using the Secure Shell command:\n%s"), command);
    }
  else if (label == self->remote_desktop_address_label)
    {
      text = g_strdup_printf ("ms-rd://%s", hostname);
    }
  else
    g_assert_not_reached ();

  gtk_label_set_label (GTK_LABEL (label), text);
}

typedef struct
{
  CcSharingPanel *panel;
  GtkWidget *label;
} GetHostNameData;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GetHostNameData, g_free);

static void
cc_sharing_panel_get_host_name_fqdn_done (GObject         *object,
                                          GAsyncResult    *res,
                                          gpointer         user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (object);
  g_autoptr(GetHostNameData) data = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) variant = NULL;
  const gchar *fqdn;

  variant = g_dbus_connection_call_finish (connection, res, &error);

  if (variant == NULL)
    {
      /* Avahi service may not be available */
      g_debug ("Error calling GetHostNameFqdn: %s", error->message);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_autofree gchar *hostname = NULL;

          hostname = cc_hostname_entry_get_hostname (CC_HOSTNAME_ENTRY (data->panel->hostname_entry));

          cc_sharing_panel_setup_label (data->panel, data->label, hostname);
        }

      return;
    }

  g_variant_get (variant, "(&s)", &fqdn);

  cc_sharing_panel_setup_label (data->panel, data->label, fqdn);
}

static void
cc_sharing_panel_bus_ready (GObject         *object,
                            GAsyncResult    *res,
                            gpointer         user_data)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GetHostNameData) data = user_data;
  g_autoptr(GError) error = NULL;

  connection = g_bus_get_finish (res, &error);

  if (connection == NULL)
    {
      g_warning ("Could not connect to system bus: %s", error->message);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_autofree gchar *hostname = NULL;

          hostname = cc_hostname_entry_get_hostname (CC_HOSTNAME_ENTRY (data->panel->hostname_entry));

          cc_sharing_panel_setup_label (data->panel, data->label, hostname);
        }

      return;
    }

  g_dbus_connection_call (connection,
                          "org.freedesktop.Avahi",
                          "/",
                          "org.freedesktop.Avahi.Server",
                          "GetHostNameFqdn",
                          NULL,
                          (GVariantType*)"(s)",
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cc_panel_get_cancellable (CC_PANEL (data->panel)),
                          cc_sharing_panel_get_host_name_fqdn_done,
                          data);
  g_steal_pointer (&data);
}


static void
cc_sharing_panel_setup_label_with_hostname (CcSharingPanel *self,
                                            GtkWidget      *label)

{
  GetHostNameData *get_hostname_data;

  /* set the hostname */
  get_hostname_data = g_new (GetHostNameData, 1);
  get_hostname_data->panel = self;
  get_hostname_data->label = label;
  g_bus_get (G_BUS_TYPE_SYSTEM,
             cc_panel_get_cancellable (CC_PANEL (self)),
             cc_sharing_panel_bus_ready,
             get_hostname_data);
}

static gboolean
file_sharing_get_require_password (GValue   *value,
                                   GVariant *variant,
                                   gpointer  user_data)
{
  if (g_str_equal (g_variant_get_string (variant, NULL), "always"))
    g_value_set_boolean (value, TRUE);
  else
    g_value_set_boolean (value, FALSE);

  return TRUE;
}

static GVariant *
file_sharing_set_require_password (const GValue       *value,
                                   const GVariantType *type,
                                   gpointer            user_data)
{
  if (g_value_get_boolean (value))
    return g_variant_new_string ("always");
  else
    return g_variant_new_string ("never");
}

static void
file_sharing_password_changed (GtkEntry *entry)
{
  file_share_write_out_password (gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
cc_sharing_panel_setup_personal_file_sharing_dialog (CcSharingPanel *self)
{
  GSettings *settings;
  GtkWidget *networks, *w;

  cc_sharing_panel_bind_switch_to_widgets (self->personal_file_sharing_require_password_switch,
                                           self->personal_file_sharing_password_entry,
                                           self->personal_file_sharing_password_label,
                                           NULL);

  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->personal_file_sharing_label);

  /* the password cannot be read, so just make sure the entry is not empty */
  gtk_editable_set_text (GTK_EDITABLE (self->personal_file_sharing_password_entry),
                         "password");

  settings = g_settings_new (FILE_SHARING_SCHEMA_ID);
  g_settings_bind_with_mapping (settings, "require-password",
                                self->personal_file_sharing_require_password_switch,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                file_sharing_get_require_password,
                                file_sharing_set_require_password, NULL, NULL);

  g_signal_connect (self->personal_file_sharing_password_entry,
                    "notify::text", G_CALLBACK (file_sharing_password_changed),
                    NULL);

  networks = cc_sharing_networks_new (self->sharing_proxy, "gnome-user-share-webdav");
  gtk_grid_attach (GTK_GRID (self->personal_file_sharing_grid), networks, 0, 3, 2, 1);

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_accessible_update_property (GTK_ACCESSIBLE (w),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Enable personal media sharing"),
                                -1);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->personal_file_sharing_headerbar), w);
  self->personal_file_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self,
                                           networks,
                                           self->personal_file_sharing_row);
}

static void
remote_login_switch_activate (CcSharingPanel *self)
{
  cc_remote_login_set_enabled (cc_panel_get_cancellable (CC_PANEL (self)), GTK_SWITCH (self->remote_login_switch));
}

static void
cc_sharing_panel_setup_remote_login_dialog (CcSharingPanel *self)
{
  cc_sharing_panel_bind_switch_to_label (self, self->remote_login_switch,
                                         self->remote_login_row);

  cc_sharing_panel_setup_label_with_hostname (self, self->remote_login_label);

  g_signal_connect_object (self->remote_login_switch, "notify::active",
                           G_CALLBACK (remote_login_switch_activate), self, G_CONNECT_SWAPPED);
  gtk_widget_set_sensitive (self->remote_login_switch, FALSE);

  cc_remote_login_get_enabled (cc_panel_get_cancellable (CC_PANEL (self)),
                               GTK_SWITCH (self->remote_login_switch),
                               self->remote_login_row);
}

static gboolean
cc_sharing_panel_check_schema_available (CcSharingPanel *self,
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
  CcSharingPanel *self = CC_SHARING_PANEL (user_data);
  const char *username, *password;

  username = gtk_editable_get_text (GTK_EDITABLE (self->remote_desktop_username_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->remote_desktop_password_entry));

  if (username && password)
    {
      cc_grd_store_rdp_credentials (username, password,
                                    cc_panel_get_cancellable (CC_PANEL (self)));
    }

  self->remote_desktop_store_credentials_id = 0;

  return G_SOURCE_REMOVE;
}

static void
remote_desktop_credentials_changed (CcSharingPanel *self)
{
  g_clear_handle_id (&self->remote_desktop_store_credentials_id,
                     g_source_remove);

  self->remote_desktop_store_credentials_id =
    g_timeout_add_seconds (REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S,
                           store_remote_desktop_credentials_timeout,
                           self);
}

static gboolean
is_remote_desktop_enabled (CcSharingPanel *self)
{
  g_autoptr(GSettings) rdp_settings = NULL;

  rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  if (!g_settings_get_boolean (rdp_settings, "enable"))
    return FALSE;

  return cc_is_service_active (REMOTE_DESKTOP_SERVICE, G_BUS_TYPE_SESSION);
}

static void
enable_gnome_remote_desktop_service (CcSharingPanel *self)
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
disable_gnome_remote_desktop_service (CcSharingPanel *self)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GSettings) rdp_settings = NULL;

  rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  g_settings_set_boolean (rdp_settings, "enable", FALSE);

  if (!cc_disable_service (REMOTE_DESKTOP_SERVICE,
                           G_BUS_TYPE_SESSION,
                           &error))
    g_warning ("Failed to enable remote desktop service: %s", error->message);
}

static void
calc_default_tls_paths (char **out_dir_path,
                        char **out_cert_path,
                        char **out_key_path)
{
  g_autofree char *dir_path = NULL;

  dir_path = g_strdup_printf ("%s/gnome-remote-desktop",
                              g_get_user_data_dir ());

  if (out_cert_path)
    *out_cert_path = g_strdup_printf ("%s/rdp-tls.crt", dir_path);
  if (out_key_path)
    *out_key_path = g_strdup_printf ("%s/rdp-tls.key", dir_path);

  if (out_dir_path)
    *out_dir_path = g_steal_pointer (&dir_path);
}

static void
set_tls_certificate (CcSharingPanel  *self,
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
  CcSharingPanel *self;
  g_autoptr(GTlsCertificate) tls_certificate = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;
  g_autoptr(GSettings) rdp_settings = NULL;

  tls_certificate = bonsai_tls_certificate_new_generate_finish (res, &error);
  if (!tls_certificate)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Failed to generate TLS certificate: %s", error->message);
      return;
    }

  self = CC_SHARING_PANEL (user_data);

  calc_default_tls_paths (NULL, &cert_path, &key_path);

  rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  g_settings_set_string (rdp_settings, "tls-cert", cert_path);
  g_settings_set_string (rdp_settings, "tls-key", key_path);

  set_tls_certificate (self, tls_certificate);

  enable_gnome_remote_desktop_service (self);
}

static void
enable_gnome_remote_desktop (CcSharingPanel *self)
{
  g_autofree char *dir_path = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;
  g_autoptr(GFile) dir = NULL;
  g_autoptr(GFile) cert_file = NULL;
  g_autoptr(GFile) key_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GSettings) rdp_settings = NULL;

  rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  g_settings_set_boolean (rdp_settings, "enable", TRUE);

  cert_path = g_settings_get_string (rdp_settings, "tls-cert");
  key_path = g_settings_get_string (rdp_settings, "tls-key");
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
          g_settings_set_string (rdp_settings, "tls-cert", cert_path);
          g_settings_set_string (rdp_settings, "tls-key", key_path);

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
                                             cc_panel_get_cancellable (CC_PANEL (self)),
                                             on_certificate_generated,
                                             self);
}

static void
on_remote_desktop_state_changed (GtkWidget      *widget,
                                 GParamSpec     *pspec,
                                 CcSharingPanel *self)
{
  if (gtk_switch_get_active (GTK_SWITCH (widget)))
    enable_gnome_remote_desktop (self);
  else
    disable_gnome_remote_desktop_service (self);
}

static char *
get_hostname (void)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) res = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GError) error = NULL;
  const char *hostname;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (bus == NULL)
    {
      g_warning ("Failed to get system bus connection: %s", error->message);
      return NULL;
    }
  res = g_dbus_connection_call_sync (bus,
                                     "org.freedesktop.hostname1",
                                     "/org/freedesktop/hostname1",
                                     "org.freedesktop.DBus.Properties",
                                     "Get",
                                     g_variant_new ("(ss)",
                                                    "org.freedesktop.hostname1",
                                                    "PrettyHostname"),
                                     (GVariantType*)"(v)",
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);

  if (res == NULL)
    {
      g_warning ("Getting pretty hostname failed: %s", error->message);
      return NULL;
    }

  g_variant_get (res, "(v)", &inner);
  hostname = g_variant_get_string (inner, NULL);
  if (g_strcmp0 (hostname, "") != 0)
    return g_strdup (hostname);

  g_clear_pointer (&inner, g_variant_unref);
  g_clear_pointer (&res, g_variant_unref);

  res = g_dbus_connection_call_sync (bus,
                                     "org.freedesktop.hostname1",
                                     "/org/freedesktop/hostname1",
                                     "org.freedesktop.DBus.Properties",
                                     "Get",
                                     g_variant_new ("(ss)",
                                                    "org.freedesktop.hostname1",
                                                    "Hostname"),
                                     (GVariantType*)"(v)",
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);

  if (res == NULL)
    {
      g_warning ("Getting hostname failed: %s", error->message);
      return NULL;
    }

  g_variant_get (res, "(v)", &inner);
  return g_variant_dup_string (inner, NULL);
}

static void
add_toast (CcSharingPanel *self,
           const char     *message)
{
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->remote_desktop_toast_overlay),
                               adw_toast_new (message));
}

static void
on_device_name_copy_clicked (GtkButton      *button,
                             CcSharingPanel *self)
{
  GtkLabel *label = GTK_LABEL (self->remote_desktop_device_name_label);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_label_get_text (label));
  add_toast (self, _("Device name copied"));
}

static void
on_device_address_copy_clicked (GtkButton      *button,
                                CcSharingPanel *self)
{
  GtkLabel *label = GTK_LABEL (self->remote_desktop_address_label);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_label_get_text (label));
  add_toast (self, _("Device address copied"));
}

static void
on_username_copy_clicked (GtkButton      *button,
                          CcSharingPanel *self)
{
  GtkEditable *editable = GTK_EDITABLE (self->remote_desktop_username_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Username copied"));
}

static void
on_password_copy_clicked (GtkButton      *button,
                          CcSharingPanel *self)
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
  char *res;
  int rv;

  rv = pwquality_generate (get_pwq (), 0, &res);

  if (rv < 0) {
      g_warning ("Password generation failed: %s\n",
                 pwquality_strerror (NULL, 0, rv, NULL));
      return NULL;
  }

  return res;
}

static void
cc_sharing_panel_setup_remote_desktop_dialog (CcSharingPanel *self)
{
  const gchar *username = NULL;
  const gchar *password = NULL;
  g_autoptr(GSettings) rdp_settings = NULL;
  g_autofree char *hostname = NULL;

  cc_sharing_panel_bind_switch_to_label (self, self->remote_desktop_switch,
                                         self->remote_desktop_row);

  cc_sharing_panel_setup_label_with_hostname (self, self->remote_desktop_address_label);

  rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  g_settings_bind (rdp_settings,
                   "view-only",
                   self->remote_control_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_object_bind_property (self->remote_desktop_switch, "state",
                          self->remote_control_switch, "sensitive",
                          G_BINDING_SYNC_CREATE);

  hostname = get_hostname ();
  gtk_label_set_label (GTK_LABEL (self->remote_desktop_device_name_label),
                       hostname);

  username = cc_grd_lookup_rdp_username (cc_panel_get_cancellable (CC_PANEL (self)));
  password = cc_grd_lookup_rdp_password (cc_panel_get_cancellable (CC_PANEL (self)));
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
        {
          gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_username_entry),
                                 pw->pw_name);
        }
      else
        {
          g_warning ("Failed to get username: %s", g_strerror (errno));
        }
    }

  if (password == NULL) 
    {
      char * pw = pw_generate ();
      if (pw)
        gtk_editable_set_text (GTK_EDITABLE (self->remote_desktop_password_entry),
                               pw );
    }
  g_signal_connect (self->remote_desktop_device_name_copy,
                    "clicked", G_CALLBACK (on_device_name_copy_clicked),
                    self);
  g_signal_connect (self->remote_desktop_address_copy,
                    "clicked", G_CALLBACK (on_device_address_copy_clicked),
                    self);
  g_signal_connect (self->remote_desktop_username_copy,
                    "clicked", G_CALLBACK (on_username_copy_clicked),
                    self);
  g_signal_connect (self->remote_desktop_password_copy,
                    "clicked", G_CALLBACK (on_password_copy_clicked),
                    self);

  g_signal_connect (self->remote_desktop_switch, "notify::state",
                    G_CALLBACK (on_remote_desktop_state_changed), self);

  if (is_remote_desktop_enabled (self))
    {
      gtk_switch_set_active (GTK_SWITCH (self->remote_desktop_switch),
                             TRUE);
    }
}

static void
remote_desktop_name_appeared (GDBusConnection *connection,
                              const gchar     *name,
                              const gchar     *name_owner,
                              gpointer         user_data)
{
  CcSharingPanel *self = CC_SHARING_PANEL (user_data);

  g_bus_unwatch_name (self->remote_desktop_name_watch);
  self->remote_desktop_name_watch = 0;

  cc_sharing_panel_setup_remote_desktop_dialog (self);
  gtk_widget_show (self->remote_desktop_row);
}

static void
check_remote_desktop_available (CcSharingPanel *self)
{
  if (!cc_sharing_panel_check_schema_available (self, GNOME_REMOTE_DESKTOP_SCHEMA_ID))
    return;

  if (!cc_sharing_panel_check_schema_available (self, GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID))
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
sharing_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcSharingPanel *self;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = G_DBUS_PROXY (gsd_sharing_proxy_new_for_bus_finish (res, &error));
  if (!proxy) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to get sharing proxy: %s", error->message);
    return;
  }

  self = CC_SHARING_PANEL (user_data);
  self->sharing_proxy = proxy;

  /* media sharing */
  cc_sharing_panel_setup_media_sharing_dialog (self);

  /* personal file sharing */
  if (cc_sharing_panel_check_schema_available (self, FILE_SHARING_SCHEMA_ID))
    cc_sharing_panel_setup_personal_file_sharing_dialog (self);
  else
    gtk_widget_hide (self->personal_file_sharing_row);

  /* remote login */
  cc_sharing_panel_setup_remote_login_dialog (self);

  /* screen sharing */
  check_remote_desktop_available (self);
  gtk_widget_hide (self->remote_desktop_row);
}

static void
cc_sharing_panel_init (CcSharingPanel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  g_resources_register (cc_sharing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->main_list_box, "row-activated",
                           G_CALLBACK (cc_sharing_panel_main_list_box_row_activated), self, G_CONNECT_SWAPPED);

  g_signal_connect (self->media_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->personal_file_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->remote_login_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->remote_desktop_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (self->main_list_box),
                                             TRUE);

  /* start the panel in the disabled state */
  gtk_switch_set_active (GTK_SWITCH (self->master_switch), FALSE);
  gtk_widget_set_sensitive (self->main_list_box, FALSE);
  g_signal_connect_object (self->master_switch, "notify::active",
                           G_CALLBACK (cc_sharing_panel_master_switch_notify), self, G_CONNECT_SWAPPED);

  gsd_sharing_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 "org.gnome.SettingsDaemon.Sharing",
                                 "/org/gnome/SettingsDaemon/Sharing",
                                 cc_panel_get_cancellable (CC_PANEL (self)),
                                 sharing_proxy_ready,
                                 self);

  /* make sure the hostname entry isn't focused by default */
  g_signal_connect_swapped (self, "map", G_CALLBACK (gtk_widget_grab_focus),
                            self->main_list_box);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/sharing/sharing.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

CcSharingPanel *
cc_sharing_panel_new (void)
{
  return g_object_new (CC_TYPE_SHARING_PANEL, NULL);
}
