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
#include "shell/cc-hostname-entry.h"

#include "cc-sharing-resources.h"
#include "vino-preferences.h"
#include "cc-remote-login.h"
#include "file-share-properties.h"
#include "cc-media-sharing.h"

#include <glib/gi18n.h>
#include <config.h>

#ifdef HAVE_BLUETOOTH
#include <bluetooth-killswitch.h>
#endif

CC_PANEL_REGISTER (CcSharingPanel, cc_sharing_panel)

#define PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SHARING_PANEL, CcSharingPanelPrivate))


static GtkWidget *
_gtk_builder_get_widget (GtkBuilder  *builder,
                         const gchar *name)
{
  GtkWidget *w;

  w = (GtkWidget*) gtk_builder_get_object (builder, name);

  g_assert (w != NULL);

  return w;
}

#define WID(y) _gtk_builder_get_widget (priv->builder, y)

#define VINO_SCHEMA_ID "org.gnome.Vino"
#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"

struct _CcSharingPanelPrivate
{
  GtkBuilder *builder;

  GtkWidget *master_switch;
  GtkWidget *hostname_entry;

  GtkWidget *bluetooth_sharing_dialog;
  GtkWidget *media_sharing_dialog;
  GtkWidget *personal_file_sharing_dialog;
  GtkWidget *remote_login_dialog;
  GCancellable *remote_login_cancellable;
  GCancellable *hostname_cancellable;
  GtkWidget *screen_sharing_dialog;

#ifdef HAVE_BLUETOOTH
  BluetoothKillswitch *bluetooth_killswitch;
#endif
};

static void
cc_sharing_panel_master_switch_notify (GtkSwitch      *gtkswitch,
                                       GParamSpec     *pspec,
                                       CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  gboolean active;

  active = gtk_switch_get_active (gtkswitch);

  if (!active)
    {
      /* disable all services if the master switch is not active */
      gtk_switch_set_active (GTK_SWITCH (WID ("remote-view-switch")), FALSE);
      gtk_switch_set_active (GTK_SWITCH (WID ("remote-login-switch")), FALSE);
      gtk_switch_set_active (GTK_SWITCH (WID ("share-public-folder-on-network-switch")),
                             FALSE);
      gtk_switch_set_active (GTK_SWITCH (WID ("share-media-switch")), FALSE);
      gtk_switch_set_active (GTK_SWITCH (WID ("share-public-folder-switch")),
                             FALSE);
    }

  gtk_widget_set_sensitive (WID ("main-list-box"), active);
}

static void
cc_sharing_panel_constructed (GObject *object)
{
  CcSharingPanelPrivate *priv = CC_SHARING_PANEL (object)->priv;

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->constructed (object);

  /* add the master switch */
  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                   priv->master_switch);
}

static void
cc_sharing_panel_dispose (GObject *object)
{
  CcSharingPanelPrivate *priv = CC_SHARING_PANEL (object)->priv;

#ifdef HAVE_BLUETOOTH
  g_clear_object (&priv->bluetooth_killswitch);
#endif

  g_clear_object (&priv->builder);

  if (priv->bluetooth_sharing_dialog)
    {
      gtk_widget_destroy (priv->bluetooth_sharing_dialog);
      priv->bluetooth_sharing_dialog = NULL;
    }

  if (priv->media_sharing_dialog)
    {
      gtk_widget_destroy (priv->media_sharing_dialog);
      priv->media_sharing_dialog = NULL;
    }

  if (priv->personal_file_sharing_dialog)
    {
      gtk_widget_destroy (priv->personal_file_sharing_dialog);
      priv->personal_file_sharing_dialog = NULL;
    }

  if (priv->remote_login_cancellable)
    {
      g_cancellable_cancel (priv->remote_login_cancellable);
      g_clear_object (&priv->remote_login_cancellable);
    }

  if (priv->hostname_cancellable)
    {
      g_cancellable_cancel (priv->hostname_cancellable);
      g_clear_object (&priv->hostname_cancellable);
    }

  if (priv->remote_login_dialog)
    {
      gtk_widget_destroy (priv->remote_login_dialog);
      priv->remote_login_dialog = NULL;
    }

  if (priv->screen_sharing_dialog)
    {
      gtk_widget_destroy (priv->screen_sharing_dialog);
      priv->screen_sharing_dialog = NULL;
    }

  G_OBJECT_CLASS (cc_sharing_panel_parent_class)->dispose (object);
}

static const char *
cc_sharing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/settings-sharing";
}

static void
cc_sharing_panel_class_init (CcSharingPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcSharingPanelPrivate));

  object_class->constructed = cc_sharing_panel_constructed;
  object_class->dispose = cc_sharing_panel_dispose;

  panel_class->get_help_uri = cc_sharing_panel_get_help_uri;
}

static void
cc_sharing_panel_run_dialog (CcSharingPanel *self,
                             const gchar    *dialog_name)
{
  CcSharingPanelPrivate *priv = self->priv;
  GtkWidget *dialog, *parent;

  dialog = WID (dialog_name);

  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static void
cc_sharing_panel_main_list_box_row_activated (GtkListBox     *listbox,
                                              GtkListBoxRow  *row,
                                              CcSharingPanel *self)
{
  gchar *widget_name, *found;

  widget_name = g_strdup (gtk_buildable_get_name (GTK_BUILDABLE (row)));

  if (!widget_name)
    return;

  gtk_list_box_select_row (listbox, NULL);

  /* replace "button" with "dialog" */
  found = g_strrstr (widget_name, "button");

  if (!found)
    goto out;

  memcpy (found, "dialog", 6);

  cc_sharing_panel_run_dialog (self, widget_name);

out:
  g_free (widget_name);
}

static void
cc_sharing_panel_main_list_box_update_header (GtkListBoxRow  *row,
                                              GtkListBoxRow  *before,
                                              gpointer    user_data)
{
  GtkWidget *current;

  if (before == NULL)
    return;

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
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
    gtk_switch_set_active (GTK_SWITCH (self->priv->master_switch), TRUE);

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

static gboolean
bluetooth_get_accept_files (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  gboolean bonded;

  bonded = g_str_equal (g_variant_get_string (variant, NULL), "bonded");

  g_value_set_boolean (value, bonded);

  return TRUE;
}

static GVariant *
bluetooth_set_accept_files (const GValue       *value,
                            const GVariantType *type,
                            gpointer            user_data)
{
  if (g_value_get_boolean (value))
    return g_variant_new_string ("bonded");
  else
    return g_variant_new_string ("always");
}

#ifdef HAVE_BLUETOOTH

static void
bluetooth_state_changed (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  BluetoothKillswitchState killswitch_state;

  killswitch_state = BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER;

  if (priv->bluetooth_killswitch)
    killswitch_state = bluetooth_killswitch_get_state (priv->bluetooth_killswitch);

  if (killswitch_state == BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER)
    gtk_widget_hide (WID ("bluetooth-sharing-button"));
  else
    gtk_widget_show (WID ("bluetooth-sharing-button"));
}

#endif

static void
cc_sharing_panel_setup_bluetooth_sharing_dialog (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  GSettings *settings;

#ifdef HAVE_BLUETOOTH
  priv->bluetooth_killswitch = bluetooth_killswitch_new ();

  /* get the initial state */
  bluetooth_state_changed (self);

  g_signal_connect_swapped (priv->bluetooth_killswitch, "state-changed",
                            G_CALLBACK (bluetooth_state_changed), self);
#endif


  cc_sharing_panel_bind_switch_to_label (self,
                                         WID ("share-public-folder-switch"),
                                         WID ("bluetooth-sharing-status-label"));

  cc_sharing_panel_bind_switch_to_widgets (WID ("share-public-folder-switch"),
                                           WID ("only-share-trusted-devices-box"),
                                           NULL);

  cc_sharing_panel_bind_switch_to_widgets (WID ("save-received-files-to-downloads-switch"),
                                           WID ("receive-files-grid"),
                                           NULL);

  settings = g_settings_new (FILE_SHARING_SCHEMA_ID);
  g_settings_bind (settings, "bluetooth-enabled",
                   WID ("share-public-folder-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "bluetooth-require-pairing",
                   WID ("only-share-trusted-devices-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "bluetooth-obexpush-enabled",
                   WID ("save-received-files-to-downloads-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (settings, "bluetooth-accept-files",
                                WID ("only-receive-from-trusted-devices-switch"),
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                bluetooth_get_accept_files,
                                bluetooth_set_accept_files, NULL, NULL);

}

static void
cc_sharing_panel_add_folder (GtkWidget      *button,
                             CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  GtkWidget *dialog;
  GtkListStore *store;
  gchar *folder;

  dialog = gtk_file_chooser_dialog_new (_("Choose a Folder"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_hide (dialog);

  store = (GtkListStore *) gtk_builder_get_object (priv->builder,
                                                   "shared-folders-liststore");

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));

  if (folder && !g_str_equal (folder, ""))
    gtk_list_store_insert_with_values (store, NULL, -1, 0, folder, -1);

  g_free (folder);

  gtk_widget_destroy (dialog);
}

static void
cc_sharing_panel_remove_folder (GtkButton      *button,
                                CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("shared-folders-treeview")));

  gtk_tree_selection_get_selected (selection, &model, &iter);

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
cc_sharing_panel_selection_changed (GtkTreeSelection *selection,
                                    CcSharingPanel   *self)
{
  CcSharingPanelPrivate *priv = self->priv;

  if (gtk_tree_selection_count_selected_rows (selection) > 0)
    gtk_widget_set_sensitive (WID ("remove-button"), TRUE);
  else
    gtk_widget_set_sensitive (WID ("remove-button"), FALSE);
}

static void
cc_sharing_panel_media_sharing_dialog_response (GtkDialog      *dialog,
                                                gint            reponse_id,
                                                CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;
  GPtrArray *folders;

  model = (GtkTreeModel *) gtk_builder_get_object (priv->builder,
                                                   "shared-folders-liststore");

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
  folders = g_ptr_array_new_with_free_func (g_free);

  while (valid)
    {
      gchar *folder;

      gtk_tree_model_get (model, &iter, 0, &folder, -1);

      g_ptr_array_add (folders, folder);

      valid = gtk_tree_model_iter_next (model, &iter);
    }

  g_ptr_array_add (folders, NULL);

  cc_media_sharing_set_preferences (gtk_switch_get_active (GTK_SWITCH (WID ("share-media-switch"))),
                                    (gchar **) folders->pdata);


  g_ptr_array_free (folders, TRUE);
}

static void
cc_sharing_panel_setup_media_sharing_dialog (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  gchar **folders, **list;
  gboolean enabled;
  GtkListStore *store;
  char *path;

  path = g_find_program_in_path ("rygel");
  if (path == NULL)
    {
      gtk_widget_hide (WID ("media-sharing-button"));
      return;
    }
  g_free (path);

  cc_sharing_panel_bind_switch_to_label (self, WID ("share-media-switch"),
                                         WID ("media-sharing-status-label"));


  cc_sharing_panel_bind_switch_to_widgets (WID ("share-media-switch"),
                                           WID ("shared-folders-box"),
                                           NULL);

  g_signal_connect (WID ("add-button"), "clicked",
                    G_CALLBACK (cc_sharing_panel_add_folder), self);
  g_signal_connect (WID ("remove-button"), "clicked",
                    G_CALLBACK (cc_sharing_panel_remove_folder), self);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("shared-folders-treeview"))),
                    "changed", G_CALLBACK (cc_sharing_panel_selection_changed), self);

  g_signal_connect (WID ("media-sharing-dialog"), "response",
                    G_CALLBACK (cc_sharing_panel_media_sharing_dialog_response),
                    self);

  cc_media_sharing_get_preferences (&enabled, &folders);

  gtk_switch_set_active (GTK_SWITCH (WID ("share-media-switch")), enabled);

  list = folders;
  store = (GtkListStore *) gtk_builder_get_object (priv->builder,
                                                   "shared-folders-liststore");
  while (list && *list)
    {
      gtk_list_store_insert_with_values (store, NULL, -1, 0, *list, -1);
      list++;
    }

  g_strfreev (folders);
}

static gboolean
cc_sharing_panel_label_activate_link (GtkLabel *label,
                                      gchar    *uri,
                                      GtkMenu  *menu)
{
  gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time ());

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
cc_sharing_panel_setup_label (GtkLabel    *label,
                              const gchar *hostname)
{
  gchar *text;

  hostname = g_strdup (hostname);

  text = g_strdup_printf (gtk_label_get_label (GTK_LABEL (label)), hostname, hostname);

  gtk_label_set_label (GTK_LABEL (label), text);

  g_free (text);
}

typedef struct
{
  CcSharingPanel *panel;
  GtkWidget *label;
} GetHostNameData;

static void
cc_sharing_panel_get_host_name_fqdn_done (GDBusConnection *connection,
                                          GAsyncResult    *res,
                                          GetHostNameData *data)
{
  GError *error = NULL;
  GVariant *variant;
  const gchar *fqdn;

  variant = g_dbus_connection_call_finish (connection, res, &error);

  if (variant == NULL)
    {
      /* Avahi service may not be available */
      g_debug ("Error calling GetHostNameFqdn: %s", error->message);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          gchar *hostname;

          hostname = cc_hostname_entry_get_hostname (CC_HOSTNAME_ENTRY (data->panel->priv->hostname_entry));

          cc_sharing_panel_setup_label (GTK_LABEL (data->label), hostname);

          g_free (hostname);
        }

      g_free (data);
      g_error_free (error);
      return;
    }

  g_variant_get (variant, "(&s)", &fqdn);

  cc_sharing_panel_setup_label (GTK_LABEL (data->label), fqdn);

  g_variant_unref (variant);
  g_object_unref (connection);
  g_free (data);
}

static void
cc_sharing_panel_bus_ready (GObject         *object,
                            GAsyncResult    *res,
                            GetHostNameData *data)
{
  GDBusConnection *connection;
  GError *error = NULL;

  connection = g_bus_get_finish (res, &error);

  if (connection == NULL)
    {
      g_warning ("Could not connect to system bus: %s", error->message);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          gchar *hostname;

          hostname = cc_hostname_entry_get_hostname (CC_HOSTNAME_ENTRY (data->panel->priv->hostname_entry));

          cc_sharing_panel_setup_label (GTK_LABEL (data->label), hostname);

          g_free (hostname);
        }

      g_error_free (error);
      g_free (data);
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
                          NULL,
                          (GAsyncReadyCallback) cc_sharing_panel_get_host_name_fqdn_done,
                          data);
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
  g_bus_get (G_BUS_TYPE_SYSTEM, NULL,
             (GAsyncReadyCallback) cc_sharing_panel_bus_ready,
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
  CcSharingPanelPrivate *priv = self->priv;
  GSettings *settings;


  cc_sharing_panel_bind_switch_to_label (self,
                                         WID ("share-public-folder-on-network-switch"),
                                         WID ("personal-file-sharing-status-label"));

  cc_sharing_panel_bind_switch_to_widgets (WID ("share-public-folder-on-network-switch"),
                                           WID ("require-password-grid"),
                                           NULL);

  cc_sharing_panel_bind_switch_to_widgets (WID ("personal-file-sharing-require-password-switch"),
                                           WID ("personal-file-sharing-password-entry"),
                                           WID ("personal-file-sharing-password-label"),
                                           NULL);

  cc_sharing_panel_setup_label_with_hostname (self,
                                              WID ("personal-file-sharing-label"));

  /* the password cannot be read, so just make sure the entry is not empty */
  gtk_entry_set_text (GTK_ENTRY (WID ("personal-file-sharing-password-entry")),
                      "password");

  settings = g_settings_new (FILE_SHARING_SCHEMA_ID);
  g_settings_bind (settings, "enabled",
                   WID ("share-public-folder-on-network-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (settings, "require-password",
                                WID ("personal-file-sharing-require-password-switch"),
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                file_sharing_get_require_password,
                                file_sharing_set_require_password, NULL, NULL);

  g_signal_connect (WID ("personal-file-sharing-password-entry"),
                    "notify::text", G_CALLBACK (file_sharing_password_changed),
                    NULL);
}

static void
remote_login_switch_activate (GtkSwitch      *remote_login_switch,
                              GParamSpec     *pspec,
                              CcSharingPanel *self)
{
  cc_remote_login_set_enabled (self->priv->remote_login_cancellable, remote_login_switch);
}

static void
cc_sharing_panel_setup_remote_login_dialog (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;

  cc_sharing_panel_bind_switch_to_label (self, WID ("remote-login-switch"),
                                         WID ("remote-login-status-label"));

  cc_sharing_panel_setup_label_with_hostname (self, WID ("remote-login-label"));

  g_signal_connect (WID ("remote-login-switch"), "notify::active",
                    G_CALLBACK (remote_login_switch_activate), self);
  gtk_widget_set_sensitive (WID ("remote-login-switch"), FALSE);

  cc_remote_login_get_enabled (self->priv->remote_login_cancellable,
                               GTK_SWITCH (WID ("remote-login-switch")),
                               WID ("remote-login-button"));
}

static gboolean
cc_sharing_panel_check_schema_available (CcSharingPanel *self,
                                         const gchar *schema_id)
{
  const gchar * const* schema_list;

  if (schema_id == NULL)
    return FALSE;

  schema_list = g_settings_list_schemas ();

  while (*schema_list)
    {
      if (g_str_equal (*schema_list, schema_id))
        return TRUE;

      schema_list++;
    }

  return FALSE;
}

static void
screen_sharing_show_cb (GtkWidget *widget, CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("show-password-checkbutton")),
                                FALSE);
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
cc_sharing_panel_setup_screen_sharing_dialog (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv;
  GSettings *settings;

  cc_sharing_panel_bind_switch_to_label (self, WID ("remote-view-switch"),
                                         WID ("screen-sharing-status-label"));

  cc_sharing_panel_bind_switch_to_widgets (WID ("remote-view-switch"),
                                           WID ("remote-control-box"),
                                           NULL);

  cc_sharing_panel_bind_switch_to_widgets (WID ("remote-control-switch"),
                                           WID ("remote-control-grid"),
                                           NULL);

  cc_sharing_panel_bind_switch_to_widgets (WID ("remote-control-require-password-switch"),
                                           WID ("remote-control-password-entry"),
                                           WID ("remote-control-password-label"),
                                           NULL);

  cc_sharing_panel_setup_label_with_hostname (self,
                                              WID ("screen-sharing-label"));

  /* settings bindings */
  settings = g_settings_new (VINO_SCHEMA_ID);
  g_settings_bind (settings, "enabled", WID ("remote-view-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "view-only", WID ("remote-control-switch"),
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (settings, "prompt-enabled",
                   WID ("approve-all-connections-switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (settings, "authentication-methods",
                                WID ("remote-control-require-password-switch"),
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                vino_get_authtype, vino_set_authtype, NULL, NULL);

  g_settings_bind_with_mapping (settings, "vnc-password",
                                WID ("remote-control-password-entry"),
                                "text",
                                G_SETTINGS_BIND_DEFAULT,
                                vino_get_password, vino_set_password, NULL, NULL);

  g_object_bind_property (WID ("show-password-checkbutton"), "active",
                          WID ("remote-control-password-entry"), "visibility",
                          G_BINDING_SYNC_CREATE);

  /* make sure the password entry is hidden by default */
  g_signal_connect (priv->screen_sharing_dialog, "show",
                    G_CALLBACK (screen_sharing_show_cb), self);

  /* accept at most 8 bytes in password entry */
  g_signal_connect (WID ("remote-control-password-entry"), "insert-text",
                    G_CALLBACK (screen_sharing_password_insert_text_cb), self);
}

static void
cc_sharing_panel_init (CcSharingPanel *self)
{
  CcSharingPanelPrivate *priv = self->priv = PANEL_PRIVATE (self);
  GError *err = NULL;
  gchar *objects[] = {
      "sharing-panel",
      "bluetooth-sharing-dialog",
      "shared-folders-liststore",
      "media-sharing-dialog",
      "personal-file-sharing-dialog",
      "remote-login-dialog",
      "screen-sharing-dialog",
      NULL };

  g_resources_register (cc_sharing_get_resource ());

  priv->builder = gtk_builder_new ();

  gtk_builder_add_objects_from_resource (priv->builder,
                                         "/org/gnome/control-center/sharing/sharing.ui",
                                         objects, &err);

  if (err)
    g_error ("Error loading CcSharingPanel user interface: %s", err->message);

  priv->hostname_entry = WID ("hostname-entry");

  gtk_container_add (GTK_CONTAINER (self), WID ("sharing-panel"));

  g_signal_connect (WID ("main-list-box"), "row-activated",
                    G_CALLBACK (cc_sharing_panel_main_list_box_row_activated), self);

  priv->hostname_cancellable = g_cancellable_new ();

  priv->bluetooth_sharing_dialog = WID ("bluetooth-sharing-dialog");
  priv->media_sharing_dialog = WID ("media-sharing-dialog");
  priv->personal_file_sharing_dialog = WID ("personal-file-sharing-dialog");
  priv->remote_login_dialog = WID ("remote-login-dialog");
  priv->remote_login_cancellable = g_cancellable_new ();
  priv->screen_sharing_dialog = WID ("screen-sharing-dialog");

  g_signal_connect (priv->bluetooth_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (priv->media_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (priv->personal_file_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (priv->remote_login_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (priv->screen_sharing_dialog, "response",
                    G_CALLBACK (gtk_widget_hide), NULL);

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (WID ("main-list-box")),
                                             TRUE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (WID ("main-list-box")),
                                cc_sharing_panel_main_list_box_update_header,
                                NULL, NULL);

  /* create the master switch */
  priv->master_switch = gtk_switch_new ();
  gtk_widget_show (priv->master_switch);

  /* start the panel in the disabled state */
  gtk_switch_set_active (GTK_SWITCH (priv->master_switch), FALSE);
  gtk_widget_set_sensitive (WID ("main-list-box"), FALSE);
  g_signal_connect (priv->master_switch, "notify::active",
                    G_CALLBACK (cc_sharing_panel_master_switch_notify), self);


  /* bluetooth */
  if (cc_sharing_panel_check_schema_available (self, FILE_SHARING_SCHEMA_ID))
    cc_sharing_panel_setup_bluetooth_sharing_dialog (self);
  else
    gtk_widget_hide (WID ("bluetooth-sharing-button"));

  /* media sharing */
  cc_sharing_panel_setup_media_sharing_dialog (self);

  /* personal file sharing */
  if (cc_sharing_panel_check_schema_available (self, FILE_SHARING_SCHEMA_ID))
    cc_sharing_panel_setup_personal_file_sharing_dialog (self);
  else
    gtk_widget_hide (WID ("personal-file-sharing-button"));

  /* remote login */
  cc_sharing_panel_setup_remote_login_dialog (self);

  /* screen sharing */
  if (cc_sharing_panel_check_schema_available (self, VINO_SCHEMA_ID))
    cc_sharing_panel_setup_screen_sharing_dialog (self);
  else
    gtk_widget_hide (WID ("screen-sharing-button"));

  /* make sure the hostname entry isn't focused by default */
  g_signal_connect_swapped (self, "map", G_CALLBACK (gtk_widget_grab_focus),
                            WID ("main-list-box"));
}

CcSharingPanel *
cc_sharing_panel_new (void)
{
  return g_object_new (CC_TYPE_SHARING_PANEL, NULL);
}
