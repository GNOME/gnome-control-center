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
#include "shell/cc-application.h"
#include "shell/cc-log.h"
#include "cc-list-row-info-button.h"
#include "cc-hostname.h"

#include "cc-sharing-resources.h"
#include "file-share-properties.h"
#include "cc-media-sharing.h"
#include "cc-sharing-networks.h"
#include "org.gnome.SettingsDaemon.Sharing.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#include <glib/gi18n.h>

#include <config.h>

#include <unistd.h>

static GtkWidget *cc_sharing_panel_new_media_sharing_row (const char     *uri_or_path,
                                                          CcSharingPanel *self);

#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"

struct _CcSharingPanel
{
  CcPanel parent_instance;

  GtkWidget *hostname_entry;
  GtkWidget *main_list_box;
  AdwDialog *media_sharing_dialog;
  AdwPreferencesPage *media_sharing_page;
  AdwActionRow *media_sharing_enable_row;
  GtkWidget *media_sharing_row;
  GtkWidget *media_sharing_switch;
  AdwDialog *personal_file_sharing_dialog;
  AdwToastOverlay *personal_file_sharing_toast_overlay;
  AdwActionRow *personal_file_sharing_enable_row;
  CcListRowInfoButton *personal_file_sharing_info_button;
  AdwActionRow *personal_file_sharing_address_row;
  AdwPreferencesPage *personal_file_sharing_page;
  GtkWidget *personal_file_sharing_password_entry_row;
  GtkWidget *personal_file_sharing_require_password_switch;
  GtkWidget *personal_file_sharing_row;
  GtkWidget *personal_file_sharing_switch;

  GtkWidget *shared_folders_listbox;

  GDBusProxy *sharing_proxy;
};

CC_PANEL_REGISTER (CcSharingPanel, cc_sharing_panel)

static void
cc_sharing_panel_dispose (GObject *object)
{
  CcSharingPanel *self = CC_SHARING_PANEL (object);

  if (self->media_sharing_dialog)
    {
      adw_dialog_force_close (self->media_sharing_dialog);
      self->media_sharing_dialog = NULL;
    }

  if (self->personal_file_sharing_dialog)
    {
      adw_dialog_force_close (self->personal_file_sharing_dialog);
      self->personal_file_sharing_dialog = NULL;
    }

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->dispose (object);
}

static const char *
cc_sharing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-sharing";
}

static void
on_copy_personal_file_sharing_address_clicked (CcSharingPanel *self)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)),
                          adw_action_row_get_subtitle (ADW_ACTION_ROW (self->personal_file_sharing_address_row)));

  adw_toast_overlay_add_toast (self->personal_file_sharing_toast_overlay, adw_toast_new (_("Address copied to clipboard")));
}

static void
on_public_folder_row_clicked (CcSharingPanel *self)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GtkFileLauncher) launcher = NULL;
  const char *public_folder_uri;
  GtkWindow *toplevel;

  public_folder_uri = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
  file = g_file_new_for_path (public_folder_uri);
  launcher = gtk_file_launcher_new (file);

  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_file_launcher_launch (launcher, toplevel, NULL, NULL, NULL);
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
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, main_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_page);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_enable_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_enable_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_info_button);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_address_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_page);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_entry_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_require_password_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_copy_personal_file_sharing_address_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_public_folder_row_clicked);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
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

  return TRUE;
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
on_folder_selected_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  CcSharingPanel *self = CC_SHARING_PANEL (user_data);
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  g_autofree gchar *folder = NULL;
  g_autoptr(GFile) file = NULL;
  GtkWidget *child;
  gboolean matching = FALSE;
  g_autoptr(GError) error = NULL;
  gint n_rows = 0;

  file = gtk_file_dialog_select_folder_finish (dialog, result, &error);
  if (!file)
    {
      g_warning ("Failed to select folder: %s", error->message);
      return;
    }

  folder = g_file_get_uri (file);
  if (!folder || g_str_equal (folder, ""))
    return;

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
}

static void
cc_sharing_panel_add_folder (CcSharingPanel *self,
                             GtkListBoxRow  *row)
{
  g_autoptr(GtkFileDialog) dialog = NULL;
  GtkWindow *toplevel;

  if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-add")))
    return;

  dialog = gtk_file_dialog_new ();
  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_file_dialog_set_title (dialog, _("Choose a Folder"));
  gtk_file_dialog_set_modal (dialog, TRUE);

  gtk_file_dialog_select_folder (dialog,
                                 toplevel,
                                 cc_panel_get_cancellable (CC_PANEL (self)),
                                 on_folder_selected_cb,
                                 self);
}

static void
cc_sharing_panel_open_folder (CcSharingPanel *self,
                              GtkButton      *button)
{
  GtkWidget *row;
  GtkWindow *toplevel;
  const gchar *path;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GtkFileLauncher) launcher = NULL;

  row = g_object_get_data (G_OBJECT (button), "row");
  path = g_object_get_data (G_OBJECT (row), "path");
  file = g_file_new_for_path (path);
  launcher = gtk_file_launcher_new (file);

  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_file_launcher_launch (launcher, toplevel, NULL, NULL, NULL);
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
cc_sharing_panel_media_sharing_dialog_close_attempt (CcSharingPanel *self)
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

  adw_dialog_set_can_close (self->media_sharing_dialog, TRUE);
  adw_dialog_close (self->media_sharing_dialog);
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

  /* Open folder button */
  w = gtk_button_new_from_icon_name ("folder-open-symbolic");
  gtk_widget_set_tooltip_text (w, _("Open Folder"));
  gtk_widget_add_css_class (w, "flat");
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);
  g_signal_connect_object (G_OBJECT (w), "clicked",
                           G_CALLBACK (cc_sharing_panel_open_folder), self, G_CONNECT_SWAPPED);
  g_object_set_data (G_OBJECT (w), "row", row);

  /* Remove button */
  w = gtk_button_new_from_icon_name ("edit-delete-symbolic");
  gtk_widget_set_tooltip_text (GTK_WIDGET (w), _("Remove Folder"));
  gtk_widget_add_css_class (w, "flat");
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
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
  GtkWidget *row;

  row = adw_button_row_new ();
  adw_button_row_set_start_icon_name (ADW_BUTTON_ROW (row), "list-add-symbolic");
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("_Add Folder"));
  adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (row), TRUE);

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

static gboolean
cc_sharing_panel_check_media_sharing_available (void)
{
  g_autofree gchar *path = NULL;

  path = g_find_program_in_path ("rygel");
  return (path != NULL);
}

static void
cc_sharing_panel_setup_media_sharing_dialog (CcSharingPanel *self)
{
  g_auto(GStrv) folders = NULL;
  GStrv list;
  GtkWidget *row, *networks, *w;

  g_signal_connect_object (self->media_sharing_dialog, "close-attempt",
                           G_CALLBACK (cc_sharing_panel_media_sharing_dialog_close_attempt),
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
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->media_sharing_page),
                            ADW_PREFERENCES_GROUP (networks));

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (self->media_sharing_enable_row, w);
  adw_action_row_set_activatable_widget (self->media_sharing_enable_row, w);
  self->media_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->media_sharing_row);
}

static void
cc_sharing_panel_setup_label_with_hostname (CcSharingPanel *self,
                                            AdwPreferencesPage *page)
{
  g_autofree gchar *hostname;

  hostname = cc_hostname_get_static_hostname (cc_hostname_get_default ());

  if (page == self->personal_file_sharing_page)
    {
      g_autofree gchar *dav_address = g_strdup_printf ("dav://%s", hostname);
      adw_action_row_set_subtitle (self->personal_file_sharing_address_row, dav_address);
    }
  else
    g_assert_not_reached ();
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
file_sharing_password_changed (CcSharingPanel *self)
{
  file_share_write_out_password (gtk_editable_get_text (GTK_EDITABLE (self->personal_file_sharing_password_entry_row)));
}

static void
cc_sharing_panel_setup_personal_file_sharing_dialog (CcSharingPanel *self)
{
  GSettings *settings;
  GtkWidget *networks, *w;

  g_autofree gchar *hostname = NULL;
  g_autofree gchar *personal_file_sharing_info_text = NULL;

  g_object_bind_property (self->personal_file_sharing_require_password_switch, "active",
                          self->personal_file_sharing_password_entry_row, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* the password cannot be read, so just make sure the entry is not empty */
  gtk_editable_set_text (GTK_EDITABLE (self->personal_file_sharing_password_entry_row),
                         "password");

  settings = g_settings_new (FILE_SHARING_SCHEMA_ID);
  g_settings_bind_with_mapping (settings, "require-password",
                                self->personal_file_sharing_require_password_switch,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                file_sharing_get_require_password,
                                file_sharing_set_require_password, NULL, NULL);

  g_signal_connect_swapped (self->personal_file_sharing_password_entry_row,
                            "notify::text", G_CALLBACK (file_sharing_password_changed),
                            self);

  networks = cc_sharing_networks_new (self->sharing_proxy, "gnome-user-share-webdav");
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->personal_file_sharing_page),
                            ADW_PREFERENCES_GROUP (networks));

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (self->personal_file_sharing_enable_row, w);
  adw_action_row_set_activatable_widget (self->personal_file_sharing_enable_row, w);
  self->personal_file_sharing_switch = w;

  hostname = cc_hostname_get_display_hostname (cc_hostname_get_default ());
  /* Translators: %s is the hostname of the user's device */
  personal_file_sharing_info_text = g_strdup_printf (_("File sharing allows sharing the Public folder with other devices "
                                                       "on the current network. This device will be visible as “%s”."),
                                                     hostname);
  cc_list_row_info_button_set_text (self->personal_file_sharing_info_button, personal_file_sharing_info_text);

  cc_sharing_panel_bind_networks_to_label (self,
                                           networks,
                                           self->personal_file_sharing_row);
}

static gboolean
cc_sharing_panel_check_schema_available (const gchar *schema_id)
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
  if (cc_sharing_panel_check_media_sharing_available ())
    cc_sharing_panel_setup_media_sharing_dialog (self);
  else
    gtk_widget_set_visible (self->media_sharing_row, FALSE);

  /* personal file sharing */
  if (cc_sharing_panel_check_schema_available (FILE_SHARING_SCHEMA_ID))
    cc_sharing_panel_setup_personal_file_sharing_dialog (self);
  else
    gtk_widget_set_visible (self->personal_file_sharing_row, FALSE);

  cc_sharing_panel_setup_label_with_hostname (self, self->personal_file_sharing_page);
}

static void
cc_sharing_panel_init (CcSharingPanel *self)
{
  g_resources_register (cc_sharing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gsd_sharing_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 "org.gnome.SettingsDaemon.Sharing",
                                 "/org/gnome/SettingsDaemon/Sharing",
                                 cc_panel_get_cancellable (CC_PANEL (self)),
                                 sharing_proxy_ready,
                                 self);
}

CcSharingPanel *
cc_sharing_panel_new (void)
{
  return g_object_new (CC_TYPE_SHARING_PANEL, NULL);
}

void
cc_sharing_panel_static_init_func (void)
{
  CcApplication *application;
  gboolean visible;

  CC_TRACE_MSG ("Updating Sharing panel visibility");

  visible = cc_sharing_panel_check_schema_available (FILE_SHARING_SCHEMA_ID) ||
            cc_sharing_panel_check_media_sharing_available ();

  application = CC_APPLICATION (g_application_get_default ());
  cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                       "sharing",
                                       visible ? CC_PANEL_VISIBLE : CC_PANEL_HIDDEN);
  g_debug ("Sharing panel visible: %s", visible ? "yes" : "no");
}
