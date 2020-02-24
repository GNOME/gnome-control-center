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

#include "list-box-helper.h"
#include "cc-sharing-resources.h"
#include "vino-preferences.h"
#include "cc-remote-login.h"
#include "file-share-properties.h"
#include "cc-media-sharing.h"
#include "cc-sharing-networks.h"
#include "cc-sharing-switch.h"
#include "cc-gnome-remote-desktop.h"
#include "org.gnome.SettingsDaemon.Sharing.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#include <glib/gi18n.h>
#include <config.h>

static void cc_sharing_panel_setup_label_with_hostname (CcSharingPanel *self, GtkWidget *label);
static GtkWidget *cc_sharing_panel_new_media_sharing_row (const char     *uri_or_path,
                                                          CcSharingPanel *self);

#define VINO_SCHEMA_ID "org.gnome.Vino"
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
  GtkWidget *main_list_box;
  GtkWidget *master_switch;
  GtkWidget *media_sharing_button;
  GtkWidget *media_sharing_dialog;
  GtkWidget *media_sharing_headerbar;
  GtkWidget *media_sharing_status_label;
  GtkWidget *media_sharing_switch;
  GtkWidget *personal_file_sharing_button;
  GtkWidget *personal_file_sharing_dialog;
  GtkWidget *personal_file_sharing_grid;
  GtkWidget *personal_file_sharing_headerbar;
  GtkWidget *personal_file_sharing_label;
  GtkWidget *personal_file_sharing_password_entry;
  GtkWidget *personal_file_sharing_password_label;
  GtkWidget *personal_file_sharing_require_password_switch;
  GtkWidget *personal_file_sharing_status_label;
  GtkWidget *personal_file_sharing_switch;
  GtkWidget *password_grid;
  GtkWidget *remote_control_box;
  GtkWidget *remote_control_checkbutton;
  GtkWidget *remote_control_password_entry;
  GtkWidget *remote_login_button;
  GtkWidget *remote_login_dialog;
  GtkWidget *remote_login_label;
  GtkWidget *remote_login_status_label;
  GtkWidget *remote_login_switch;
  GtkWidget *require_password_radiobutton;
  GtkWidget *screen_sharing_button;
  GtkWidget *screen_sharing_dialog;
  GtkWidget *screen_sharing_grid;
  GtkWidget *screen_sharing_headerbar;
  GtkWidget *screen_sharing_label;
  GtkWidget *screen_sharing_status_label;
  GtkWidget *screen_sharing_switch;
  GtkWidget *shared_folders_grid;
  GtkWidget *shared_folders_listbox;
  GtkWidget *show_password_checkbutton;

  GDBusProxy *sharing_proxy;

  guint remote_desktop_name_watch;
};

CC_PANEL_REGISTER (CcSharingPanel, cc_sharing_panel)

#define OFF_IF_VISIBLE(x, y) { if (gtk_widget_is_visible(x) && (y) != NULL && gtk_widget_is_sensitive(y)) gtk_switch_set_active (GTK_SWITCH(y), FALSE); }

static void
cc_sharing_panel_master_switch_notify (GtkSwitch      *gtkswitch,
                                       GParamSpec     *pspec,
                                       CcSharingPanel *self)
{
  gboolean active;

  active = gtk_switch_get_active (gtkswitch);

  if (!active)
    {
      /* disable all services if the master switch is not active */
      OFF_IF_VISIBLE(self->media_sharing_button, self->media_sharing_switch);
      OFF_IF_VISIBLE(self->personal_file_sharing_button, self->personal_file_sharing_switch);
      OFF_IF_VISIBLE(self->screen_sharing_button, self->screen_sharing_switch);

      gtk_switch_set_active (GTK_SWITCH (self->remote_login_switch), FALSE);
    }

  gtk_widget_set_sensitive (self->main_list_box, active);
}

static void
cc_sharing_panel_constructed (GObject *object)
{
  CcSharingPanel *self = CC_SHARING_PANEL (object);

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->constructed (object);

  /* add the master switch */
  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                   gtk_widget_get_parent (self->master_switch),
                                   GTK_POS_RIGHT);
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
      gtk_widget_destroy (self->media_sharing_dialog);
      self->media_sharing_dialog = NULL;
    }

  if (self->personal_file_sharing_dialog)
    {
      gtk_widget_destroy (self->personal_file_sharing_dialog);
      self->personal_file_sharing_dialog = NULL;
    }

  if (self->remote_login_dialog)
    {
      gtk_widget_destroy (self->remote_login_dialog);
      self->remote_login_dialog = NULL;
    }

  if (self->screen_sharing_dialog)
    {
      gtk_widget_destroy (self->screen_sharing_dialog);
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
cc_sharing_panel_class_init (CcSharingPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->constructed = cc_sharing_panel_constructed;
  object_class->dispose = cc_sharing_panel_dispose;

  panel_class->get_help_uri = cc_sharing_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sharing/cc-sharing-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, approve_connections_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, hostname_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, main_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_button);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, media_sharing_status_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_button);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_password_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_require_password_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, personal_file_sharing_status_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, password_grid);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_box);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_control_password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_button);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_status_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, remote_login_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, require_password_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_button);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, screen_sharing_status_label);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, shared_folders_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcSharingPanel, show_password_checkbutton);

  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_sharing_panel_run_dialog (CcSharingPanel *self,
                             GtkWidget      *dialog)
{
  GtkWidget *parent;

  /* ensure labels with the hostname are updated if the hostname has changed */
  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->screen_sharing_label);
  cc_sharing_panel_setup_label_with_hostname (self, self->remote_login_label);
  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->personal_file_sharing_label);


  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static void
cc_sharing_panel_main_list_box_row_activated (GtkListBox     *listbox,
                                              GtkListBoxRow  *row,
                                              CcSharingPanel *self)
{
  GtkWidget *dialog;

  if (row == GTK_LIST_BOX_ROW (self->media_sharing_button))
    dialog = self->media_sharing_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->personal_file_sharing_button))
    dialog = self->personal_file_sharing_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->remote_login_button))
    dialog = self->remote_login_dialog;
  else if (row == GTK_LIST_BOX_ROW (self->screen_sharing_button))
    dialog = self->screen_sharing_dialog;
  else
    return;

  gtk_list_box_select_row (listbox, NULL);

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
                                       GtkWidget      *label)
{
  g_object_bind_property_full (gtkswitch, "active", label, "label",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) cc_sharing_panel_switch_to_label_transform_func,
                               NULL, self, NULL);
}

static void
cc_sharing_panel_bind_networks_to_label (CcSharingPanel *self,
					 GtkWidget      *networks,
					 GtkWidget      *label)
{
  g_object_bind_property_full (networks, "status", label, "label",
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
cc_sharing_panel_add_folder (GtkListBox     *box,
                             GtkListBoxRow  *row,
                             CcSharingPanel *self)
{
  GtkWidget *dialog;
  g_autofree gchar *folder = NULL;
  gboolean matching = FALSE;
  GList *rows, *l;

  if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-add")))
    return;

  dialog = gtk_file_chooser_dialog_new (_("Choose a Folder"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (box))),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
    goto bail;

  gtk_widget_hide (dialog);

  rows = gtk_container_get_children (GTK_CONTAINER (self->shared_folders_listbox));

  folder = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
  if (!folder || g_str_equal (folder, ""))
    goto bail;

  g_debug ("Trying to add %s", folder);

  for (l = rows; l != NULL; l = l->next)
    {
      const char *string;

      string = g_object_get_data (G_OBJECT (l->data), "path");
      matching = (g_strcmp0 (string, folder) == 0);

      if (matching)
        {
          g_debug ("Found a duplicate for %s", folder);
          break;
        }
    }

  if (!matching)
    {
      GtkWidget *row;
      int i;

      row = cc_sharing_panel_new_media_sharing_row (folder, self);
      i = g_list_length (rows);
      gtk_list_box_insert (GTK_LIST_BOX (self->shared_folders_listbox), row, i - 1);
      gtk_widget_set_visible (row, TRUE);
    }
  cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->shared_folders_listbox));

bail:
  gtk_widget_destroy (dialog);
}

static void
cc_sharing_panel_remove_folder (GtkButton      *button,
                                CcSharingPanel *self)
{
  GtkWidget *row;

  row = g_object_get_data (G_OBJECT (button), "row");
  gtk_widget_destroy (row);
  cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->shared_folders_listbox));
}

static void
cc_sharing_panel_media_sharing_dialog_response (GtkDialog      *dialog,
                                                gint            reponse_id,
                                                CcSharingPanel *self)
{
  g_autoptr(GPtrArray) folders = NULL;
  GtkWidget *box;
  GList *rows, *l;

  box = self->shared_folders_listbox;
  rows = gtk_container_get_children (GTK_CONTAINER (box));
  folders = g_ptr_array_new_with_free_func (g_free);

  for (l = rows; l != NULL; l = l->next)
    {
      const char *folder;

      folder = g_object_get_data (G_OBJECT (l->data), "path");
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
  GtkWidget *row, *box, *w;
  GUserDirectory dir = G_USER_N_DIRECTORIES;
  g_autoptr(GIcon) icon = NULL;
  guint i;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;

  file = g_file_new_for_commandline_arg (uri_or_path);
  path = g_file_get_path (file);

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (box);
  gtk_container_set_border_width (GTK_CONTAINER (box), 3);
  gtk_widget_set_margin_start (box, 6);
  gtk_container_add (GTK_CONTAINER (row), box);

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
  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  gtk_widget_show (w);
  gtk_widget_set_margin_end (w, 12);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* Label */
  basename = g_filename_display_basename (path);
  w = gtk_label_new (basename);
  gtk_widget_show (w);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* Remove button */
  w = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (w);
  gtk_button_set_relief (GTK_BUTTON (w), GTK_RELIEF_NONE);
  gtk_widget_set_margin_top (w, 3);
  gtk_widget_set_margin_bottom (w, 3);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (w), "clicked",
                    G_CALLBACK (cc_sharing_panel_remove_folder), self);
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
  gtk_widget_show (box);
  gtk_container_set_border_width (GTK_CONTAINER (box), 3);
  gtk_container_add (GTK_CONTAINER (row), box);

  w = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (w);
  gtk_container_add (GTK_CONTAINER (box), w);
  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_margin_top (w, 6);
  gtk_widget_set_margin_bottom (w, 6);

  g_object_set_data (G_OBJECT (w), "row", row);

  g_object_set_data (G_OBJECT (row), "is-add", GINT_TO_POINTER (1));

  return row;
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
      gtk_widget_hide (self->media_sharing_button);
      return;
    }

  g_signal_connect (self->media_sharing_dialog, "response",
                    G_CALLBACK (cc_sharing_panel_media_sharing_dialog_response),
                    self);

  cc_media_sharing_get_preferences (&folders);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->shared_folders_listbox),
                                cc_list_box_update_header_func, NULL,
                                NULL);
  cc_list_box_setup_scrolling (GTK_LIST_BOX (self->shared_folders_listbox), 3);

  list = folders;
  while (list && *list)
    {
      row = cc_sharing_panel_new_media_sharing_row (*list, self);
      gtk_widget_show (row);
      gtk_list_box_insert (GTK_LIST_BOX (self->shared_folders_listbox), row, -1);
      list++;
    }

  row = cc_sharing_panel_new_add_media_sharing_row (self);
  gtk_widget_show (row);
  gtk_list_box_insert (GTK_LIST_BOX (self->shared_folders_listbox), row, -1);

  cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->shared_folders_listbox));

  g_signal_connect (G_OBJECT (self->shared_folders_listbox), "row-activated",
                    G_CALLBACK (cc_sharing_panel_add_folder), self);

  networks = cc_sharing_networks_new (self->sharing_proxy, "rygel");
  gtk_grid_attach (GTK_GRID (self->shared_folders_grid), networks, 0, 4, 2, 1);
  gtk_widget_show (networks);

  w = cc_sharing_switch_new (networks);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->media_sharing_headerbar), w);
  self->media_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->media_sharing_status_label);
}

static gboolean
cc_sharing_panel_label_activate_link (GtkLabel *label,
                                      gchar    *uri,
                                      GtkMenu  *menu)
{
  gtk_menu_popup_at_pointer (menu, NULL);

  g_object_set_data_full (G_OBJECT (menu), "uri-text", g_strdup (uri), g_free);

  return TRUE;
}

static void
copy_uri_to_clipboard (GtkMenuItem *item,
                       GtkMenu     *menu)
{
  GtkClipboard *clipboard;
  const gchar *text;

  text = g_object_get_data (G_OBJECT (menu), "uri-text");

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);
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
  else if (label == self->screen_sharing_label)
    {
      g_autofree gchar *url = g_strdup_printf ("<a href=\"vnc://%s\">vnc://%s</a>", hostname, hostname);
      /* TRANSLATORS: %s is replaced with a link to a vnc://<hostname> URL */
      text = g_strdup_printf (_("Screen sharing allows remote users to view or control your screen by connecting to %s"), url);
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
  GtkWidget *menu;
  GtkWidget *menu_item;
  GetHostNameData *get_hostname_data;

  /* create the menu */
  menu = gtk_menu_new ();

  menu_item = gtk_menu_item_new_with_label (_("Copy"));
  gtk_widget_show (menu_item);

  g_signal_connect (menu_item, "activate", G_CALLBACK (copy_uri_to_clipboard),
                    menu);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  /* show the menu when the link is activated */
  g_signal_connect (label, "activate-link",
                    G_CALLBACK (cc_sharing_panel_label_activate_link), menu);

  /* destroy the menu when the label is destroyed */
  g_signal_connect_swapped (label, "destroy", G_CALLBACK (gtk_widget_destroy),
                            menu);


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
  file_share_write_out_password (gtk_entry_get_text (entry));
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
  gtk_entry_set_text (GTK_ENTRY (self->personal_file_sharing_password_entry),
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
  gtk_widget_show (networks);

  w = cc_sharing_switch_new (networks);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->personal_file_sharing_headerbar), w);
  self->personal_file_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self,
                                           networks,
                                           self->personal_file_sharing_status_label);
}

static void
remote_login_switch_activate (GtkSwitch      *remote_login_switch,
                              GParamSpec     *pspec,
                              CcSharingPanel *self)
{
  cc_remote_login_set_enabled (cc_panel_get_cancellable (CC_PANEL (self)), remote_login_switch);
}

static void
cc_sharing_panel_setup_remote_login_dialog (CcSharingPanel *self)
{
  cc_sharing_panel_bind_switch_to_label (self, self->remote_login_switch,
                                         self->remote_login_status_label);

  cc_sharing_panel_setup_label_with_hostname (self, self->remote_login_label);

  g_signal_connect (self->remote_login_switch, "notify::active",
                    G_CALLBACK (remote_login_switch_activate), self);
  gtk_widget_set_sensitive (self->remote_login_switch, FALSE);

  cc_remote_login_get_enabled (cc_panel_get_cancellable (CC_PANEL (self)),
                               GTK_SWITCH (self->remote_login_switch),
                               self->remote_login_button);
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
screen_sharing_show_cb (GtkWidget *widget, CcSharingPanel *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_checkbutton),
                                FALSE);
}

static void
screen_sharing_hide_cb (GtkWidget *widget, CcSharingPanel *self)
{
  GtkToggleButton *ac_radio;
  GtkEntry    *pw_entry;
  const gchar *password;

  ac_radio = GTK_TOGGLE_BUTTON (self->approve_connections_radiobutton);
  pw_entry = GTK_ENTRY (self->remote_control_password_entry);
  password = gtk_entry_get_text (pw_entry);

  if (password == NULL || *password == '\0')
    gtk_toggle_button_set_active (ac_radio, TRUE);
}

#define MAX_PASSWORD_SIZE 8
static void
screen_sharing_password_insert_text_cb (GtkEditable *editable,
                                        gchar       *new_text,
                                        gint         new_text_length,
                                        gpointer     position,
                                        gpointer     user_data)
{
  int l, available_size;

  l = gtk_entry_buffer_get_bytes (gtk_entry_get_buffer (GTK_ENTRY (editable)));

  if (l + new_text_length <= MAX_PASSWORD_SIZE)
    return;

  g_signal_stop_emission_by_name (editable, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (editable));

  available_size = g_utf8_strlen (new_text, MAX_PASSWORD_SIZE - l);
  if (available_size == 0)
    return;

  g_signal_handlers_block_by_func (editable,
                                   (gpointer) screen_sharing_password_insert_text_cb,
                                   user_data);
  gtk_editable_insert_text (editable, new_text, available_size, position);
  g_signal_handlers_unblock_by_func (editable,
                                     (gpointer) screen_sharing_password_insert_text_cb,
                                     user_data);
}
#undef MAX_PASSWORD_SIZE

static void
cc_sharing_panel_setup_screen_sharing_dialog_vino (CcSharingPanel *self)
{
  GSettings *settings;
  GtkWidget *networks, *box, *w;

  cc_sharing_panel_bind_switch_to_widgets (self->require_password_radiobutton,
                                           self->password_grid,
                                           NULL);

  cc_sharing_panel_setup_label_with_hostname (self,
                                              self->screen_sharing_label);

  /* settings bindings */
  settings = g_settings_new (VINO_SCHEMA_ID);
  g_settings_bind (settings, "view-only", self->remote_control_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (settings, "prompt-enabled",
                   self->approve_connections_radiobutton, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (settings, "authentication-methods",
                                self->require_password_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                vino_get_authtype, vino_set_authtype, NULL, NULL);

  g_settings_bind_with_mapping (settings, "vnc-password",
                                self->remote_control_password_entry,
                                "text",
                                G_SETTINGS_BIND_DEFAULT,
                                vino_get_password, vino_set_password, NULL, NULL);

  g_object_bind_property (self->show_password_checkbutton, "active",
                          self->remote_control_password_entry, "visibility",
                          G_BINDING_SYNC_CREATE);

  /* make sure the password entry is hidden by default */
  g_signal_connect (self->screen_sharing_dialog, "show",
                    G_CALLBACK (screen_sharing_show_cb), self);

  g_signal_connect (self->screen_sharing_dialog, "hide",
                    G_CALLBACK (screen_sharing_hide_cb), self);

  /* accept at most 8 bytes in password entry */
  g_signal_connect (self->remote_control_password_entry, "insert-text",
                    G_CALLBACK (screen_sharing_password_insert_text_cb), self);

  networks = cc_sharing_networks_new (self->sharing_proxy, "vino-server");
  box = self->remote_control_box;
  gtk_box_pack_end (GTK_BOX (box), networks, TRUE, TRUE, 0);
  gtk_widget_show (networks);

  w = cc_sharing_switch_new (networks);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->screen_sharing_headerbar), w);
  self->screen_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->screen_sharing_status_label);
}

static void
cc_sharing_panel_setup_screen_sharing_dialog_gnome_remote_desktop (CcSharingPanel *self)
{
  g_autoptr(GSettings) vnc_settings = NULL;
  GtkWidget *networks, *w;

  cc_sharing_panel_bind_switch_to_widgets (self->require_password_radiobutton, self->password_grid, NULL);

  cc_sharing_panel_setup_label_with_hostname (self, self->screen_sharing_label);

  g_object_bind_property (self->show_password_checkbutton,
                          "active",
                          self->remote_control_password_entry,
                          "visibility",
                          G_BINDING_SYNC_CREATE);

  /* make sure the password entry is hidden by default */
  g_signal_connect (self->screen_sharing_dialog,
                    "show",
                    G_CALLBACK (screen_sharing_show_cb),
                    self);

  g_signal_connect (self->screen_sharing_dialog,
                    "hide",
                    G_CALLBACK (screen_sharing_hide_cb),
                    self);

  cc_grd_update_password_entry (GTK_ENTRY (self->remote_control_password_entry));

  /* accept at most 8 bytes in password entry */
  g_signal_connect (self->remote_control_password_entry,
                    "insert-text",
                    G_CALLBACK (screen_sharing_password_insert_text_cb),
                    self);

  /* Bind settings to widgets */
  vnc_settings = g_settings_new (GNOME_REMOTE_DESKTOP_VNC_SCHEMA_ID);

  g_settings_bind (vnc_settings,
                   "view-only",
                   self->remote_control_checkbutton,
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

  g_signal_connect (self->remote_control_password_entry,
                    "notify::text",
                    G_CALLBACK (cc_grd_on_vnc_password_entry_notify_text),
                    self);

  networks = cc_sharing_networks_new (self->sharing_proxy, "gnome-remote-desktop");
  gtk_box_pack_end (GTK_BOX (self->remote_control_box), networks, TRUE, TRUE, 0);
  gtk_widget_show (networks);

  w = cc_sharing_switch_new (networks);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->screen_sharing_headerbar), w);
  self->screen_sharing_switch = w;

  cc_sharing_panel_bind_networks_to_label (self, networks,
                                           self->screen_sharing_status_label);
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
  gtk_widget_show (self->screen_sharing_button);
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
    gtk_widget_hide (self->personal_file_sharing_button);

  /* remote login */
  cc_sharing_panel_setup_remote_login_dialog (self);

  /* screen sharing */
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      check_remote_desktop_available (self);
      gtk_widget_hide (self->screen_sharing_button);
    }
  else
#endif
  if (cc_sharing_panel_check_schema_available (self, VINO_SCHEMA_ID))
    cc_sharing_panel_setup_screen_sharing_dialog_vino (self);
  else
    gtk_widget_hide (self->screen_sharing_button);
}

static void
cc_sharing_panel_init (CcSharingPanel *self)
{
  GtkWidget *box;

  g_resources_register (cc_sharing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->main_list_box, "row-activated",
                    G_CALLBACK (cc_sharing_panel_main_list_box_row_activated), self);

  g_signal_connect (self->media_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->personal_file_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->remote_login_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (self->screen_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (self->main_list_box),
                                             TRUE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->main_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  /* create the master switch */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  self->master_switch = gtk_switch_new ();
  gtk_widget_show (self->master_switch);
  atk_object_set_name (ATK_OBJECT (gtk_widget_get_accessible (self->master_switch)), _("Sharing"));
  gtk_widget_set_valign (self->master_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->master_switch, FALSE, FALSE, 4);

  /* start the panel in the disabled state */
  gtk_switch_set_active (GTK_SWITCH (self->master_switch), FALSE);
  gtk_widget_set_sensitive (self->main_list_box, FALSE);
  g_signal_connect (self->master_switch, "notify::active",
                    G_CALLBACK (cc_sharing_panel_master_switch_notify), self);

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
}

CcSharingPanel *
cc_sharing_panel_new (void)
{
  return g_object_new (CC_TYPE_SHARING_PANEL, NULL);
}
