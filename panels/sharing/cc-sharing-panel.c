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
#include "org.gnome.SettingsDaemon.Sharing.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#include <glib/gi18n.h>
#include <config.h>

static void cc_sharing_panel_setup_group_with_hostname (CcSharingPanel *self, GtkWidget *group);
static GtkWidget *cc_sharing_panel_new_media_sharing_row (const char     *uri_or_path,
                                                          CcSharingPanel *self);

#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"
#define GNOME_REMOTE_DESKTOP_SCHEMA_ID "org.gnome.desktop.remote-desktop"
#define GNOME_REMOTE_DESKTOP_VNC_SCHEMA_ID "org.gnome.desktop.remote-desktop.vnc"

typedef enum
{
  GRD_VNC_AUTH_METHOD_PROMPT,
  GRD_VNC_AUTH_METHOD_PASSWORD
} GrdVncAuthMethod;

struct _CcSharingPanel
{
  CcPanel parent_instance;

  GtkWidget *approve_connections_radiobutton;
  GtkWidget *hostname_entry;
  GtkWidget *main_group;
  GtkWidget *master_switch;
  GtkWidget *media_sharing_dialog;
  GtkWidget *media_sharing_headerbar;
  GtkWidget *media_sharing_row;
  GtkWidget *media_sharing_switch;
  GtkWidget *personal_file_sharing_dialog;
  GtkWidget *personal_file_sharing_page;
  GtkWidget *personal_file_sharing_headerbar;
  GtkWidget *personal_file_sharing_group;
  GtkWidget *personal_file_sharing_password_entry;
  GtkWidget *personal_file_sharing_require_password_switch;
  GtkWidget *personal_file_sharing_row;
  GtkWidget *personal_file_sharing_switch;
  GtkWidget *screen_sharing_page;
  GtkWidget *remote_control_switch;
  GtkWidget *remote_control_password_entry;
  GtkWidget *remote_login_dialog;
  GtkWidget *remote_login_group;
  GtkWidget *remote_login_row;
  GtkWidget *remote_login_switch;
  GtkWidget *require_password_radiobutton;
  GtkWidget *screen_sharing_dialog;
  GtkWidget *screen_sharing_headerbar;
  GtkWidget *screen_sharing_group;
  GtkWidget *screen_sharing_row;
  GtkWidget *screen_sharing_switch;
  GtkWidget *shared_folders_page;
  GtkWidget *shared_folders_listbox;

  GDBusProxy *sharing_proxy;

  guint remote_desktop_name_watch;
};

CC_PANEL_REGISTER (CcSharingPanel, cc_sharing_panel)

#define OFF_IF_VISIBLE(x, y) { if (gtk_widget_is_visible(x) && (y) != NULL && gtk_widget_is_sensitive(y)) gtk_switch_set_active (GTK_SWITCH(y), FALSE); }

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
      OFF_IF_VISIBLE(self->screen_sharing_row, self->screen_sharing_switch);

      gtk_switch_set_active (GTK_SWITCH (self->remote_login_switch), FALSE);
    }

  gtk_widget_set_sensitive (self->main_group, active);
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

  if (self->screen_sharing_dialog)
    {
      gtk_window_destroy (GTK_WINDOW (self->screen_sharing_dialog));
      self->screen_sharing_dialog = NULL;
    }

  g_clear_object (&self->sharing_proxy);

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->dispose (object);
}

static const char *
cc_sharing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-sharing";
}

static void
cc_sharing_panel_run_dialog (CcSharingPanel *self,
                             GtkWidget      *dialog)
{
  GtkWidget *parent;

  /* ensure labels with the hostname are updated if the hostname has changed */
  cc_sharing_panel_setup_group_with_hostname (self, self->screen_sharing_group);
  cc_sharing_panel_setup_group_with_hostname (self, self->remote_login_group);
  cc_sharing_panel_setup_group_with_hostname (self,
                                              self->personal_file_sharing_group);


  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
cc_sharing_panel_main_list_box_row_activated (CcSharingPanel *self,
                                              AdwActionRow   *row)
{
  GtkWidget *dialog;

  if (row == ADW_ACTION_ROW (self->media_sharing_row))
    dialog = self->media_sharing_dialog;
  else if (row == ADW_ACTION_ROW (self->personal_file_sharing_row))
    dialog = self->personal_file_sharing_dialog;
  else if (row == ADW_ACTION_ROW (self->remote_login_row))
    dialog = self->remote_login_dialog;
  else if (row == ADW_ACTION_ROW (self->screen_sharing_row))
    dialog = self->screen_sharing_dialog;
  else
    return;

  cc_sharing_panel_run_dialog (self, dialog);
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

  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, approve_connections_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, hostname_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_page);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, master_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, main_group);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_page);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_group);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_require_password_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_page);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_group);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, require_password_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_group);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_listbox);

  gtk_widget_class_bind_template_callback (widget_class, cc_sharing_panel_main_list_box_row_activated);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
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
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->shared_folders_page),
                            ADW_PREFERENCES_GROUP (networks));

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->media_sharing_headerbar), w);
  self->media_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->media_sharing_row);
}

static void
cc_sharing_panel_setup_group (CcSharingPanel *self,
                              GtkWidget      *group,
                              const gchar    *hostname)
{
  g_autofree gchar *text = NULL;

  if (group == self->personal_file_sharing_group)
    {
      g_autofree gchar *url = g_strdup_printf ("<a href=\"dav://%s\">dav://%s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a dav://<hostname> URL */
      text = g_strdup_printf (_("File Sharing allows you to share your Public folder with others on your current network using: %s"), url);
    }
  else if (group == self->remote_login_group)
    {
      g_autofree gchar *command = g_strdup_printf ("<a href=\"ssh %s\">ssh %s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a "ssh <hostname>" command to run */
      text = g_strdup_printf (_("When remote login is enabled, remote users can connect using the Secure Shell command:\n%s"), command);
    }
  else if (group == self->screen_sharing_group)
    {
      g_autofree gchar *url = g_strdup_printf ("<a href=\"vnc://%s\">vnc://%s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a vnc://<hostname> URL */
      text = g_strdup_printf (_("Screen sharing allows remote users to view or control your screen by connecting to %s"), url);
    }
  else
    g_assert_not_reached ();

  adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (group), text);
}

typedef struct
{
  CcSharingPanel *panel;
  GtkWidget *group;
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

          cc_sharing_panel_setup_group (data->panel, data->group, hostname);
        }

      return;
    }

  g_variant_get (variant, "(&s)", &fqdn);

  cc_sharing_panel_setup_group (data->panel, data->group, fqdn);
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

          cc_sharing_panel_setup_group (data->panel, data->group, hostname);
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
cc_sharing_panel_setup_group_with_hostname (CcSharingPanel *self,
                                            GtkWidget      *group)

{
  GetHostNameData *get_hostname_data;

  /* set the hostname */
  get_hostname_data = g_new (GetHostNameData, 1);
  get_hostname_data->panel = self;
  get_hostname_data->group = group;
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

  cc_sharing_panel_setup_group_with_hostname (self,
                                              self->personal_file_sharing_group);

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
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->personal_file_sharing_page),
                            ADW_PREFERENCES_GROUP (networks));

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
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

  cc_sharing_panel_setup_group_with_hostname (self, self->remote_login_group);

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

static void
screen_sharing_hide_cb (CcSharingPanel *self)
{
  GtkCheckButton *ac_radio;
  const gchar *password;

  ac_radio = GTK_CHECK_BUTTON (self->approve_connections_radiobutton);
  password = gtk_editable_get_text (GTK_EDITABLE (self->remote_control_password_entry));

  if (password == NULL || *password == '\0')
    gtk_check_button_set_active (ac_radio, TRUE);
}

#define MAX_PASSWORD_SIZE 8
static void
screen_sharing_password_insert_text_cb (CcSharingPanel *self,
                                        gchar          *new_text,
                                        gint            new_text_length,
                                        gpointer        position)
{
  GtkText *text;
  int l, available_size;

  text = GTK_TEXT (gtk_editable_get_delegate (GTK_EDITABLE (self->remote_control_password_entry)));
  l = gtk_entry_buffer_get_bytes (gtk_text_get_buffer (text));

  if (l + new_text_length <= MAX_PASSWORD_SIZE)
    return;

  g_signal_stop_emission_by_name (self->remote_control_password_entry, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (self->remote_control_password_entry));

  available_size = g_utf8_strlen (new_text, MAX_PASSWORD_SIZE - l);
  if (available_size == 0)
    return;

  g_signal_handlers_block_by_func (self->remote_control_password_entry,
                                   (gpointer) screen_sharing_password_insert_text_cb,
                                   self);
  gtk_editable_insert_text (GTK_EDITABLE (self->remote_control_password_entry), new_text, available_size, position);
  g_signal_handlers_unblock_by_func (self->remote_control_password_entry,
                                     (gpointer) screen_sharing_password_insert_text_cb,
                                     self);
}
#undef MAX_PASSWORD_SIZE

static void
on_vnc_password_entry_notify_text (CcSharingPanel *self)
{
  const char *password = gtk_editable_get_text (GTK_EDITABLE (self->remote_control_password_entry));
  cc_grd_store_vnc_password (password, cc_panel_get_cancellable (CC_PANEL (self)));
}

static void
cc_sharing_panel_setup_screen_sharing_dialog_gnome_remote_desktop (CcSharingPanel *self)
{
  g_autofree gchar *password = NULL;
  g_autoptr(GSettings) vnc_settings = NULL;
  GtkWidget *networks, *w;

  cc_sharing_panel_setup_group_with_hostname (self, self->screen_sharing_group);

  g_signal_connect_object (self->screen_sharing_dialog,
                           "hide",
                           G_CALLBACK (screen_sharing_hide_cb),
                           self,
                           G_CONNECT_SWAPPED);

  password = cc_grd_lookup_vnc_password (cc_panel_get_cancellable (CC_PANEL (self)));
  if (password != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->remote_control_password_entry), password);

  /* accept at most 8 bytes in password entry */
  g_signal_connect_object (self->remote_control_password_entry,
                           "insert-text",
                           G_CALLBACK (screen_sharing_password_insert_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Bind settings to widgets */
  vnc_settings = g_settings_new (GNOME_REMOTE_DESKTOP_VNC_SCHEMA_ID);

  g_settings_bind (vnc_settings,
                   "view-only",
                   self->remote_control_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind_with_mapping (vnc_settings,
                                "auth-method",
                                self->approve_connections_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cc_grd_get_is_auth_method_prompt,
                                cc_grd_set_is_auth_method_prompt,
                                NULL,
                                NULL);

  g_settings_bind_with_mapping (vnc_settings,
                                "auth-method",
                                self->require_password_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cc_grd_get_is_auth_method_password,
                                cc_grd_set_is_auth_method_password,
                                NULL,
                                NULL);

  g_signal_connect_object (self->remote_control_password_entry,
                           "notify::text",
                           G_CALLBACK (on_vnc_password_entry_notify_text),
                           self,
                           G_CONNECT_SWAPPED);

  networks = cc_sharing_networks_new (self->sharing_proxy, "gnome-remote-desktop");
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->screen_sharing_page),
                            ADW_PREFERENCES_GROUP (networks));

  w = create_switch_with_bindings (GTK_SWITCH (g_object_get_data (G_OBJECT (networks), "switch")));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->screen_sharing_headerbar), w);
  self->screen_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->screen_sharing_row);
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

  cc_sharing_panel_setup_screen_sharing_dialog_gnome_remote_desktop (self);
  gtk_widget_show (self->screen_sharing_row);
}

static void
check_remote_desktop_available (CcSharingPanel *self)
{
  if (!cc_sharing_panel_check_schema_available (self, GNOME_REMOTE_DESKTOP_SCHEMA_ID))
    return;

  if (!cc_sharing_panel_check_schema_available (self, GNOME_REMOTE_DESKTOP_VNC_SCHEMA_ID))
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
  gtk_widget_hide (self->screen_sharing_row);
}

static void
cc_sharing_panel_init (CcSharingPanel *self)
{
  g_resources_register (cc_sharing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->media_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->personal_file_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->remote_login_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->screen_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);

  /* start the panel in the disabled state */
  gtk_switch_set_active (GTK_SWITCH (self->master_switch), FALSE);
  gtk_widget_set_sensitive (self->main_group, FALSE);
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
                            self->main_group);
}

CcSharingPanel *
cc_sharing_panel_new (void)
{
  return g_object_new (CC_TYPE_SHARING_PANEL, NULL);
}
