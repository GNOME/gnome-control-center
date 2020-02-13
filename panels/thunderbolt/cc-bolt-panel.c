/* Copyright © 2018 Red Hat, Inc
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

#include <shell/cc-panel.h>
#include <list-box-helper.h>

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include "cc-bolt-device-dialog.h"
#include "cc-bolt-device-entry.h"

#include "bolt-client.h"
#include "bolt-names.h"
#include "bolt-str.h"

#include "cc-bolt-panel.h"
#include "cc-thunderbolt-resources.h"

struct _CcBoltPanel
{
  CcPanel             parent;

  BoltClient         *client;

  /* headerbar menu */
  GtkBox             *headerbar_box;
  GtkLockButton      *lock_button;

  /* main ui */
  GtkStack           *container;

  /* empty state */
  GtkLabel           *notb_caption;
  GtkLabel           *notb_details;

  /* notifications */
  GtkLabel           *notification_label;
  GtkRevealer        *notification_revealer;

  /* authmode */
  GtkSwitch          *authmode_switch;
  GtkSpinner         *authmode_spinner;
  GtkStack           *authmode_mode;

  /* device list */
  GHashTable         *devices;

  GtkStack           *devices_stack;
  GtkBox             *devices_box;
  GtkBox             *pending_box;

  GtkListBox         *devices_list;
  GtkListBox         *pending_list;

  /* device details dialog */
  CcBoltDeviceDialog *device_dialog;

  /* polkit integration */
  GPermission        *permission;
};

/* initialization */
static void           bolt_client_ready (GObject      *source,
                                         GAsyncResult *res,
                                         gpointer      user_data);

/* panel functions */
static void                cc_bolt_panel_set_no_thunderbolt (CcBoltPanel *panel,
                                                             const char  *custom_msg);

static void                cc_bolt_panel_name_owner_changed (CcBoltPanel *panel);

static CcBoltDeviceEntry * cc_bolt_panel_add_device (CcBoltPanel *panel,
                                                     BoltDevice  *dev);

static void                cc_bolt_panel_del_device_entry (CcBoltPanel       *panel,
                                                           CcBoltDeviceEntry *entry);

static void                cc_bolt_panel_authmode_sync (CcBoltPanel *panel);

static void                cc_panel_list_box_migrate (CcBoltPanel       *panel,
                                                      GtkListBox        *from,
                                                      GtkListBox        *to,
                                                      CcBoltDeviceEntry *entry);

/* bolt client signals */
static void     on_bolt_name_owner_changed_cb (GObject    *object,
                                               GParamSpec *pspec,
                                               gpointer    user_data);

static void     on_bolt_device_added_cb (BoltClient  *cli,
                                         const char  *path,
                                         CcBoltPanel *panel);

static void     on_bolt_device_removed_cb (BoltClient  *cli,
                                           const char  *opath,
                                           CcBoltPanel *panel);

static void     on_bolt_notify_authmode_cb (GObject    *gobject,
                                            GParamSpec *pspec,
                                            gpointer    user_data);

/* panel signals */
static gboolean on_authmode_state_set_cb (CcBoltPanel *panel,
                                          gboolean     state,
                                          GtkSwitch   *toggle);

static void     on_device_entry_row_activated_cb (CcBoltPanel   *panel,
                                                  GtkListBoxRow *row);

static gboolean  on_device_dialog_delete_event_cb (GtkWidget   *widget,
                                                   GdkEvent    *event,
                                                   CcBoltPanel *panel);

static void     on_device_entry_status_changed_cb (CcBoltDeviceEntry *entry,
                                                   BoltStatus         new_status,
                                                   CcBoltPanel       *panel);

static void     on_notification_button_clicked_cb (GtkButton   *button,
                                                   CcBoltPanel *panel);


/* polkit */
static void      on_permission_ready (GObject      *source_object,
                                      GAsyncResult *res,
                                      gpointer      user_data);

static void      on_permission_notify_cb (GPermission *permission,
                                          GParamSpec  *pspec,
                                          CcBoltPanel *panel);

CC_PANEL_REGISTER (CcBoltPanel, cc_bolt_panel);

static void
bolt_client_ready (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(CcBoltPanel) panel = NULL;
  BoltClient *client;

  panel = CC_BOLT_PANEL (user_data);
  client = bolt_client_new_finish (res, &err);

  if (client == NULL)
    {
      const char *text;

      /* operation got cancelled because the panel got destroyed */
      if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
          g_error_matches (err, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        return;

      g_warning ("Could not create client: %s", err->message);
      text = _("The Thunderbolt subsystem (boltd) is not installed or "
               "not set up properly.");

      gtk_label_set_label (panel->notb_details, text);
      gtk_stack_set_visible_child_name (panel->container, "no-thunderbolt");

      return;
    }

  g_signal_connect_object (client,
                           "notify::g-name-owner",
                           G_CALLBACK (on_bolt_name_owner_changed_cb),
                           panel,
                           0);

  g_signal_connect_object (client,
                           "device-added",
                           G_CALLBACK (on_bolt_device_added_cb),
                           panel,
                           0);

  g_signal_connect_object (client,
                           "device-removed",
                           G_CALLBACK (on_bolt_device_removed_cb),
                           panel,
                           0);

  g_signal_connect_object (client,
                           "notify::auth-mode",
                           G_CALLBACK (on_bolt_notify_authmode_cb),
                           panel,
                           0);

  /* Treat security-level changes, which should rarely happen, as
   * if the name owner changed, i.e. as if boltd got restarted */
  g_signal_connect_object (client,
                           "notify::security-level",
                           G_CALLBACK (on_bolt_name_owner_changed_cb),
                           panel,
                           0);

  panel->client = client;

  cc_bolt_device_dialog_set_client (panel->device_dialog, client);

  cc_bolt_panel_authmode_sync (panel);

  g_object_bind_property (panel->authmode_switch,
                          "active",
                          panel->devices_box,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property (panel->authmode_switch,
                          "active",
                          panel->pending_box,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  gtk_stack_set_visible_child_name (panel->devices_stack, "no-devices");
  cc_bolt_panel_name_owner_changed (panel);
}

static gboolean
devices_table_transfer_entry (GHashTable   *from,
                              GHashTable   *to,
                              gconstpointer key)
{
  gpointer k, v;
  gboolean found;

  found = g_hash_table_lookup_extended (from, key, &k, &v);

  if (found)
    {
      g_hash_table_steal (from, key);
      g_hash_table_insert (to, k, v);
    }

  return found;
}

static void
devices_table_clear_entries (GHashTable  *table,
                             CcBoltPanel *panel)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      CcBoltDeviceEntry *entry = value;

      cc_bolt_panel_del_device_entry (panel, entry);
      g_hash_table_iter_remove (&iter);
    }
}

static void
devices_table_synchronize (CcBoltPanel *panel)
{
  g_autoptr(GHashTable) old = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  g_autoptr(GError) err = NULL;
  guint i;

  devices = bolt_client_list_devices (panel->client, cc_panel_get_cancellable (CC_PANEL (panel)), &err);

  if (!devices)
    {
      g_warning ("Could not list devices: %s", err->message);
      devices = g_ptr_array_new_with_free_func (g_object_unref);
    }

  old = panel->devices;
  panel->devices = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; i < devices->len; i++)
    {
      BoltDevice *dev = g_ptr_array_index (devices, i);
      const char *path;
      gboolean found;

      path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (dev));
      found = devices_table_transfer_entry (old, panel->devices, path);

      if (found)
        continue;

      cc_bolt_panel_add_device (panel, dev);
    }

  devices_table_clear_entries (old, panel);
  gtk_stack_set_visible_child_name (panel->container, "devices-listing");
}

static gboolean
list_box_sync_visible (GtkListBox *lstbox)
{
  g_autoptr(GList) children = NULL;
  gboolean show;

  children = gtk_container_get_children (GTK_CONTAINER (lstbox));
  show = g_list_length (children) > 0;

  gtk_widget_set_visible (GTK_WIDGET (lstbox), show);

  return show;
}

static GtkWidget *
cc_bolt_panel_box_for_listbox (CcBoltPanel *panel,
                               GtkListBox  *lstbox)
{
  if ((gpointer) lstbox == panel->devices_list)
    return GTK_WIDGET (panel->devices_box);
  else if ((gpointer) lstbox == panel->pending_list)
    return GTK_WIDGET (panel->pending_box);

  g_return_val_if_reached (NULL);
}

static CcBoltDeviceEntry *
cc_bolt_panel_add_device (CcBoltPanel *panel,
                          BoltDevice  *dev)
{
  CcBoltDeviceEntry *entry;
  BoltDeviceType type;
  BoltStatus status;
  const char *path;

  type = bolt_device_get_device_type (dev);

  if (type != BOLT_DEVICE_PERIPHERAL)
    return FALSE;

  entry = cc_bolt_device_entry_new (dev, FALSE);
  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (dev));

  /* add to the list box */
  gtk_widget_show (GTK_WIDGET (entry));

  status = bolt_device_get_status (dev);

  if (bolt_status_is_pending (status))
    {
      gtk_container_add (GTK_CONTAINER (panel->pending_list), GTK_WIDGET (entry));
      gtk_widget_show (GTK_WIDGET (panel->pending_list));
      gtk_widget_show (GTK_WIDGET (panel->pending_box));
    }
  else
    {
      gtk_container_add (GTK_CONTAINER (panel->devices_list), GTK_WIDGET (entry));
      gtk_widget_show (GTK_WIDGET (panel->devices_list));
      gtk_widget_show (GTK_WIDGET (panel->devices_box));
    }

  g_signal_connect_object (entry,
                           "status-changed",
                           G_CALLBACK (on_device_entry_status_changed_cb),
                           panel,
                           0);

  gtk_stack_set_visible_child_name (panel->devices_stack, "have-devices");
  g_hash_table_insert (panel->devices, (gpointer) path, entry);

  return entry;
}

static void
cc_bolt_panel_del_device_entry (CcBoltPanel       *panel,
                                CcBoltDeviceEntry *entry)
{
  BoltDevice *dev;
  GtkWidget *box;
  GtkWidget *p;
  gboolean show;

  dev = cc_bolt_device_entry_get_device (entry);
  if (cc_bolt_device_dialog_device_equal (panel->device_dialog, dev))
    {
      gtk_widget_hide (GTK_WIDGET (panel->device_dialog));
      cc_bolt_device_dialog_set_device (panel->device_dialog, NULL, NULL);
    }

  p = gtk_widget_get_parent (GTK_WIDGET (entry));
  gtk_widget_destroy (GTK_WIDGET (entry));

  box = cc_bolt_panel_box_for_listbox (panel, GTK_LIST_BOX (p));
  show = list_box_sync_visible (GTK_LIST_BOX (p));
  gtk_widget_set_visible (box, show);

  if (!gtk_widget_is_visible (GTK_WIDGET (panel->pending_list)) &&
      !gtk_widget_is_visible (GTK_WIDGET (panel->devices_list)))
    {
      gtk_stack_set_visible_child_name (panel->devices_stack, "no-devices");
    }
}

static void
cc_bolt_panel_authmode_sync (CcBoltPanel *panel)
{
  BoltClient *client = panel->client;
  BoltAuthMode mode;
  gboolean enabled;
  const char *name;

  mode = bolt_client_get_authmode (client);
  enabled = (mode & BOLT_AUTH_ENABLED) != 0;

  g_signal_handlers_block_by_func (panel->authmode_switch, on_authmode_state_set_cb, panel);

  gtk_switch_set_state (panel->authmode_switch, enabled);

  g_signal_handlers_unblock_by_func (panel->authmode_switch, on_authmode_state_set_cb, panel);

  name = enabled ? "enabled" : "disabled";
  gtk_stack_set_visible_child_name (panel->authmode_mode, name);
}

static void
cc_panel_list_box_migrate (CcBoltPanel       *panel,
                           GtkListBox        *from,
                           GtkListBox        *to,
                           CcBoltDeviceEntry *entry)
{
  GtkWidget *from_box;
  GtkWidget *to_box;
  gboolean show;
  GtkWidget *target;

  target = GTK_WIDGET (entry);

  gtk_container_remove (GTK_CONTAINER (from), target);
  gtk_container_add (GTK_CONTAINER (to), target);
  gtk_widget_show (GTK_WIDGET (to));

  from_box = cc_bolt_panel_box_for_listbox (panel, from);
  to_box = cc_bolt_panel_box_for_listbox (panel, to);

  show = list_box_sync_visible (from);
  gtk_widget_set_visible (from_box, show);
  gtk_widget_set_visible (to_box, TRUE);
}

/* bolt client signals */
static void
cc_bolt_panel_set_no_thunderbolt (CcBoltPanel *panel,
                                  const char  *msg)
{
  if (!msg)
    {
      msg = _("Thunderbolt could not be detected.\n"
              "Either the system lacks Thunderbolt support, "
              "it has been disabled in the BIOS or is set to "
              "an unsupported security level in the BIOS.");
    }

  gtk_label_set_label (panel->notb_details, msg);
  gtk_stack_set_visible_child_name (panel->container, "no-thunderbolt");
}

static void
cc_bolt_panel_name_owner_changed (CcBoltPanel *panel)
{
  g_autofree char *name_owner = NULL;
  BoltClient *client = panel->client;
  BoltSecurity sl;
  gboolean notb = TRUE;
  const char *text = NULL;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (panel->client));

  if (name_owner == NULL)
    {
      cc_bolt_panel_set_no_thunderbolt (panel, NULL);
      devices_table_clear_entries (panel->devices, panel);
      gtk_widget_hide (GTK_WIDGET (panel->headerbar_box));
      return;
    }

  gtk_stack_set_visible_child_name (panel->container, "loading");

  sl = bolt_client_get_security (client);

  switch (sl)
    {
    case BOLT_SECURITY_NONE:
    case BOLT_SECURITY_SECURE:
    case BOLT_SECURITY_USER:
      /* we fetch the device list and show them here */
      notb = FALSE;
      break;

    case BOLT_SECURITY_DPONLY:
    case BOLT_SECURITY_USBONLY:
      text = _("Thunderbolt support has been disabled in the BIOS.");
      break;

    case BOLT_SECURITY_UNKNOWN:
      text = _("Thunderbolt security level could not be determined.");;
      break;
    }

  if (notb)
    {
      /* security level is unknown or un-handled */
      cc_bolt_panel_set_no_thunderbolt (panel, text);
      return;
    }

  if (panel->permission)
    {
      gtk_widget_show (GTK_WIDGET (panel->headerbar_box));
    }
  else
    {
      polkit_permission_new ("org.freedesktop.bolt.manage",
                             NULL,
                             cc_panel_get_cancellable (CC_PANEL (panel)),
                             on_permission_ready,
                             g_object_ref (panel));
    }

  devices_table_synchronize (panel);
}

/* bolt client signals */
static void
on_bolt_name_owner_changed_cb (GObject    *object,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  cc_bolt_panel_name_owner_changed (CC_BOLT_PANEL (user_data));
}

static void
on_bolt_device_added_cb (BoltClient  *cli,
                         const char  *path,
                         CcBoltPanel *panel)
{
  g_autoptr(GError) err = NULL;
  GDBusConnection *bus;
  BoltDevice *dev;
  gboolean found;

  found = g_hash_table_contains (panel->devices, path);

  if (found)
    return;

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (panel->client));
  dev = bolt_device_new_for_object_path (bus, path, cc_panel_get_cancellable (CC_PANEL (panel)), &err);

  if (!dev)
    {
      g_warning ("Could not create proxy for %s", path);
      return;
    }

  cc_bolt_panel_add_device (panel, dev);
}

static void
on_bolt_device_removed_cb (BoltClient  *cli,
                           const char  *path,
                           CcBoltPanel *panel)
{
  CcBoltDeviceEntry *entry;

  entry = g_hash_table_lookup (panel->devices, path);

  if (!entry)
    return;

  cc_bolt_panel_del_device_entry (panel, entry);
  g_hash_table_remove (panel->devices, path);
}

static void
on_bolt_notify_authmode_cb (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  cc_bolt_panel_authmode_sync (CC_BOLT_PANEL (user_data));
}

/* panel signals */

static void
on_authmode_ready (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcBoltPanel *panel = CC_BOLT_PANEL (user_data);
  gboolean ok;

  ok = bolt_client_set_authmode_finish (BOLT_CLIENT (source_object), res, &error);
  if (!ok)
    {
      g_autofree char *text = NULL;

      g_warning ("Could not set authmode: %s", error->message);

      text = g_strdup_printf (_("Error switching direct mode: %s"), error->message);
      gtk_label_set_markup (panel->notification_label, text);
      gtk_revealer_set_reveal_child (panel->notification_revealer, TRUE);

      /* make sure we are reflecting the correct state */
      cc_bolt_panel_authmode_sync (panel);
    }

  gtk_spinner_stop (panel->authmode_spinner);
  gtk_widget_set_sensitive (GTK_WIDGET (panel->authmode_switch), TRUE);
}

static gboolean
on_authmode_state_set_cb (CcBoltPanel *panel,
                          gboolean     enable,
                          GtkSwitch   *toggle)
{
  BoltClient *client = panel->client;
  BoltAuthMode mode;

  gtk_widget_set_sensitive (GTK_WIDGET (panel->authmode_switch), FALSE);
  gtk_spinner_start (panel->authmode_spinner);

  mode = bolt_client_get_authmode (client);

  if (enable)
    mode = mode | BOLT_AUTH_ENABLED;
  else
    mode = mode & ~BOLT_AUTH_ENABLED;

  bolt_client_set_authmode_async (client, mode, NULL, on_authmode_ready, panel);

  return TRUE;
}

static void
on_device_entry_row_activated_cb (CcBoltPanel   *panel,
                                  GtkListBoxRow *row)
{
  g_autoptr(GPtrArray) parents = NULL;
  CcBoltDeviceEntry *entry;
  BoltDevice *device;
  BoltDevice *iter;
  const char *parent;

  if (!CC_IS_BOLT_DEVICE_ENTRY (row))
    return;

  entry = CC_BOLT_DEVICE_ENTRY (row);
  device = cc_bolt_device_entry_get_device (entry);

  /* walk up the chain and collect all parents */
  parents = g_ptr_array_new_with_free_func (g_object_unref);
  iter = device;

  parent = bolt_device_get_parent (iter);
  while (parent != NULL)
    {
      g_autofree char *path = NULL;
      CcBoltDeviceEntry *child;
      BoltDevice *dev;

      path = bolt_gen_object_path (BOLT_DBUS_PATH_DEVICES, parent);

      /* NB: the host device is not a peripheral and thus not
       * in the hash table; therefore when get a NULL back, we
       * should have reached the end of the chain */
      child = g_hash_table_lookup (panel->devices, path);
      if (!child)
	break;

      dev = cc_bolt_device_entry_get_device (child);
      g_ptr_array_add (parents, g_object_ref (dev));
      iter = dev;

      parent = bolt_device_get_parent (iter);
    }

  cc_bolt_device_dialog_set_device (panel->device_dialog, device, parents);

  gtk_window_resize (GTK_WINDOW (panel->device_dialog), 1, 1);
  gtk_widget_show (GTK_WIDGET (panel->device_dialog));
}

static gboolean
on_device_dialog_delete_event_cb (GtkWidget   *widget,
                                  GdkEvent    *event,
                                  CcBoltPanel *panel)
{
  CcBoltDeviceDialog *dialog;

  dialog = CC_BOLT_DEVICE_DIALOG (widget);

  cc_bolt_device_dialog_set_device (dialog, NULL, NULL);
  gtk_widget_hide (widget);

  return TRUE;
}

static void
on_device_entry_status_changed_cb (CcBoltDeviceEntry *entry,
                                   BoltStatus         new_status,
                                   CcBoltPanel       *panel)
{
  GtkListBox *from = NULL;
  GtkListBox *to = NULL;
  GtkWidget *p;
  gboolean is_pending;
  gboolean parent_pending;

  /* if we are doing some active work, then lets not change
   * the list the entry is in; otherwise we might just hop
   * from one box to the other and back again.
   */
  if (new_status == BOLT_STATUS_CONNECTING || new_status == BOLT_STATUS_AUTHORIZING)
    return;

  is_pending = bolt_status_is_pending (new_status);

  p = gtk_widget_get_parent (GTK_WIDGET (entry));
  parent_pending = (gpointer) p == panel->pending_list;

  /*  */
  if (is_pending && !parent_pending)
    {
      from = panel->devices_list;
      to = panel->pending_list;
    }
  else if (!is_pending && parent_pending)
    {
      from = panel->pending_list;
      to = panel->devices_list;
    }

  if (from && to)
    cc_panel_list_box_migrate (panel, from, to, entry);
}


static void
on_notification_button_clicked_cb (GtkButton   *button,
                                   CcBoltPanel *panel)
{
  gtk_revealer_set_reveal_child (panel->notification_revealer, FALSE);
}

/* polkit */

static void
on_permission_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  g_autoptr(CcBoltPanel) panel = user_data;
  g_autoptr(GError) err = NULL;
  GPermission *permission;
  gboolean is_allowed;
  const char *name;

  permission = polkit_permission_new_finish (res, &err);
  panel->permission = permission;

  if (!panel->permission)
    {
      g_warning ("Could not get polkit permissions: %s", err->message);
      return;
    }

  g_signal_connect_object (permission,
                           "notify",
                           G_CALLBACK (on_permission_notify_cb),
                           panel,
                           G_CONNECT_AFTER);

  is_allowed = g_permission_get_allowed (permission);
  gtk_widget_set_sensitive (GTK_WIDGET (panel->authmode_switch), is_allowed);
  gtk_lock_button_set_permission (panel->lock_button, permission);

  name = gtk_stack_get_visible_child_name (panel->container);

  gtk_widget_set_visible (GTK_WIDGET (panel->headerbar_box),
                          bolt_streq (name, "devices-listing"));
}

static void
on_permission_notify_cb (GPermission *permission,
                         GParamSpec  *pspec,
                         CcBoltPanel *panel)
{
  gboolean is_allowed = g_permission_get_allowed (permission);

  gtk_widget_set_sensitive (GTK_WIDGET (panel->authmode_switch), is_allowed);
}

static gint
device_entries_sort_by_recency_cb (GtkListBoxRow *a_row,
                                   GtkListBoxRow *b_row,
                                   gpointer       user_data)
{
  CcBoltDeviceEntry *a_entry = CC_BOLT_DEVICE_ENTRY (a_row);
  CcBoltDeviceEntry *b_entry = CC_BOLT_DEVICE_ENTRY (b_row);
  BoltDevice *a = cc_bolt_device_entry_get_device (a_entry);
  BoltDevice *b = cc_bolt_device_entry_get_device (b_entry);
  BoltStatus status;
  gint64 a_ts, b_ts;
  gint64 score;

  a_ts = (gint64) bolt_device_get_timestamp (a);
  b_ts = (gint64) bolt_device_get_timestamp (b);

  score = b_ts - a_ts;

  if (score != 0)
    return score;

  status = bolt_device_get_status (a);

  if (bolt_status_is_connected (status))
    {
      const char *a_path;
      const char *b_path;

      a_path = bolt_device_get_syspath (a);
      b_path = bolt_device_get_syspath (b);

      return g_strcmp0 (a_path, b_path);
    }
  else
    {
      const char *a_name;
      const char *b_name;

      a_name = bolt_device_get_name (a);
      b_name = bolt_device_get_name (b);

      return g_strcmp0 (a_name, b_name);
    }

  return 0;
}

static gint
device_entries_sort_by_syspath_cb (GtkListBoxRow *a_row,
                                   GtkListBoxRow *b_row,
                                   gpointer       user_data)
{
  CcBoltDeviceEntry *a_entry = CC_BOLT_DEVICE_ENTRY (a_row);
  CcBoltDeviceEntry *b_entry = CC_BOLT_DEVICE_ENTRY (b_row);
  BoltDevice *a = cc_bolt_device_entry_get_device (a_entry);
  BoltDevice *b = cc_bolt_device_entry_get_device (b_entry);

  const char *a_path;
  const char *b_path;

  a_path = bolt_device_get_syspath (a);
  b_path = bolt_device_get_syspath (b);

  return g_strcmp0 (a_path, b_path);
}

/* GObject overrides */

static void
cc_bolt_panel_finalize (GObject *object)
{
  CcBoltPanel *panel = CC_BOLT_PANEL (object);

  g_clear_object (&panel->client);
  g_clear_pointer (&panel->devices, g_hash_table_unref);
  g_clear_object (&panel->permission);

  G_OBJECT_CLASS (cc_bolt_panel_parent_class)->finalize (object);
}

static void
cc_bolt_panel_dispose (GObject *object)
{
  CcBoltPanel *panel = CC_BOLT_PANEL (object);

  /* Must be destroyed in dispose, not finalize. */
  g_clear_pointer ((GtkWidget **) &panel->device_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_bolt_panel_parent_class)->dispose (object);
}

static void
cc_bolt_panel_constructed (GObject *object)
{
  CcBoltPanel *panel = CC_BOLT_PANEL (object);
  GtkWindow *parent;
  CcShell *shell;

  parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));
  gtk_window_set_transient_for (GTK_WINDOW (panel->device_dialog), parent);

  G_OBJECT_CLASS (cc_bolt_panel_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (panel));
  cc_shell_embed_widget_in_header (shell, GTK_WIDGET (panel->headerbar_box), GTK_POS_RIGHT);
}

static void
cc_bolt_panel_class_init (CcBoltPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_bolt_panel_constructed;
  object_class->dispose = cc_bolt_panel_dispose;
  object_class->finalize = cc_bolt_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/thunderbolt/cc-bolt-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, authmode_mode);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, authmode_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, authmode_switch);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, container);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, devices_list);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, devices_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, devices_stack);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, headerbar_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, lock_button);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, notb_caption);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, notb_details);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, pending_box);
  gtk_widget_class_bind_template_child (widget_class, CcBoltPanel, pending_list);

  gtk_widget_class_bind_template_callback (widget_class, on_notification_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_authmode_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_entry_row_activated_cb);
}

static void
cc_bolt_panel_init (CcBoltPanel *panel)
{
  g_resources_register (cc_thunderbolt_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  gtk_stack_set_visible_child_name (panel->container, "loading");

  gtk_list_box_set_header_func (panel->devices_list,
                                cc_list_box_update_header_func,
                                NULL,
                                NULL);

  gtk_list_box_set_header_func (panel->pending_list,
                                cc_list_box_update_header_func,
                                NULL,
                                NULL);

  gtk_list_box_set_sort_func (panel->devices_list,
                              device_entries_sort_by_recency_cb,
                              panel,
                              NULL);

  gtk_list_box_set_sort_func (panel->pending_list,
                              device_entries_sort_by_syspath_cb,
                              panel,
                              NULL);

  panel->devices = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

  panel->device_dialog = cc_bolt_device_dialog_new ();
  g_signal_connect_object (panel->device_dialog,
                           "delete-event",
                           G_CALLBACK (on_device_dialog_delete_event_cb),
                           panel, 0);

  bolt_client_new_async (cc_panel_get_cancellable (CC_PANEL (panel)), bolt_client_ready, g_object_ref (panel));

}
