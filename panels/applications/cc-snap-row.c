/* cc-snap-row.c
 *
 * Copyright 2019 Canonical Ltd.
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

#include <config.h>
#include <glib/gi18n.h>

#include "cc-snap-row.h"
#include "cc-applications-resources.h"

struct _CcSnapRow
{
  GtkListBoxRow parent;

  GtkLabel     *title_label;
  GtkSwitch    *slot_toggle;
  GtkComboBox  *slots_combo;
  GtkListStore *slots_combo_model;

  GCancellable *cancellable;

  SnapdPlug    *plug;
  SnapdSlot    *connected_slot;
  GPtrArray    *slots;
};

G_DEFINE_TYPE (CcSnapRow, cc_snap_row, GTK_TYPE_LIST_BOX_ROW)

typedef struct
{
  CcSnapRow *self;
  SnapdSlot *slot;
} ConnectData;

static const gchar *
get_label (const gchar *interface_name)
{
  if (strcmp (interface_name, "account-control") == 0)
    return _("Add user accounts and change passwords");
  else if (strcmp (interface_name, "alsa") == 0)
    return _("Play and record sound");
  else if (strcmp (interface_name, "avahi-observe") == 0)
    return _("Detect network devices using mDNS/DNS-SD (Bonjour/zeroconf)");
  else if (strcmp (interface_name, "bluetooth-control") == 0)
    return _("Access bluetooth hardware directly");
  else if (strcmp (interface_name, "bluez") == 0)
    return _("Use bluetooth devices");
  else if (strcmp (interface_name, "camera") == 0)
    return _("Use your camera");
  else if (strcmp (interface_name, "cups-control") == 0)
    return _("Print documents");
  else if (strcmp (interface_name, "joystick") == 0)
    return _("Use any connected joystick");
  else if (strcmp (interface_name, "docker") == 0)
    return _("Allow connecting to the Docker service");
  else if (strcmp (interface_name, "gsettings") == 0)
    return _("Can change settings");
  else if (strcmp (interface_name, "firewall-control") == 0)
    return _("Configure network firewall");
  else if (strcmp (interface_name, "fuse-support") == 0)
    return _("Setup and use privileged FUSE filesystems");
  else if (strcmp (interface_name, "fwupd") == 0)
    return _("Update firmware on this device");
  else if (strcmp (interface_name, "hardware-observe") == 0)
    return _("Access hardware information");
  else if (strcmp (interface_name, "hardware-random-control") == 0)
    return _("Provide entropy to hardware random number generator");
  else if (strcmp (interface_name, "hardware-random-observe") == 0)
    return _("Use hardware-generated random numbers");
  else if (strcmp (interface_name, "home") == 0)
    return _("Access files in your home folder");
  else if (strcmp (interface_name, "libvirt") == 0)
    return _("Access libvirt service");
  else if (strcmp (interface_name, "locale-control") == 0)
    return _("Change system language and region settings");
  else if (strcmp (interface_name, "location-control") == 0)
    return _("Change location settings and providers");
  else if (strcmp (interface_name, "location-observe") == 0)
    return _("Access your location");
  else if (strcmp (interface_name, "log-observe") == 0)
    return _("Read system and application logs");
  else if (strcmp (interface_name, "lxd") == 0)
    return _("Access LXD service");
  else if (strcmp (interface_name, "media-hub") == 0)
    return _("access the media-hub service");
  else if (strcmp (interface_name, "modem-manager") == 0)
    return _("Use and configure modems");
  else if (strcmp (interface_name, "mount-observe") == 0)
    return _("Read system mount information and disk quotas");
  else if (strcmp (interface_name, "mpris") == 0)
    return _("Control music and video players");
  else if (strcmp (interface_name, "network") == 0)
    return _("Has network access");
  else if (strcmp (interface_name, "network-control") == 0)
    return _("Change low-level network settings");
  else if (strcmp (interface_name, "network-manager") == 0)
    return _("Access the NetworkManager service to read and change network settings");
  else if (strcmp (interface_name, "network-observe") == 0)
    return _("Read access to network settings");
  else if (strcmp (interface_name, "network-setup-control") == 0)
    return _("Change network settings");
  else if (strcmp (interface_name, "network-setup-observe") == 0)
    return _("Read network settings");
  else if (strcmp (interface_name, "ofono") == 0)
    return _("Access the ofono service to read and change network settings for mobile telephony");
  else if (strcmp (interface_name, "openvtswitch") == 0)
    return _("Control Open vSwitch hardware");
  else if (strcmp (interface_name, "optical-drive") == 0)
    return _("Read from CD/DVD");
  else if (strcmp (interface_name, "password-manager-service") == 0)
    return _("Read, add, change, or remove saved passwords");
  else if (strcmp (interface_name, "ppp") == 0)
    return _("Access pppd and ppp devices for configuring Point-to-Point Protocol connections");
  else if (strcmp (interface_name, "process-control") == 0)
    return _("Pause or end any process on the system");
  else if (strcmp (interface_name, "pulseaudio") == 0)
    return _("Play and record sound");
  else if (strcmp (interface_name, "raw-usb") == 0)
    return _("Access USB hardware directly");
  else if (strcmp (interface_name, "removable-media") == 0)
    return _("Read/write files on removable storage devices");
  else if (strcmp (interface_name, "screen-inhibit-control") == 0)
    return _("Prevent screen sleep/lock");
  else if (strcmp (interface_name, "serial-port") == 0)
    return _("Access serial port hardware");
  else if (strcmp (interface_name, "shutdown") == 0)
    return _("Restart or power off the device");
  else if (strcmp (interface_name, "snapd-control") == 0)
    return _("Install, remove and configure software");
  else if (strcmp (interface_name, "storage-framework-service") == 0)
    return _("Access Storage Framework service");
  else if (strcmp (interface_name, "system-observe") == 0)
    return _("Read process and system information");
  else if (strcmp (interface_name, "system-trace") == 0)
    return _("Monitor and control any running program");
  else if (strcmp (interface_name, "time-control") == 0)
    return _("Change the date and time");
  else if (strcmp (interface_name, "timeserver-control") == 0)
    return _("Change time server settings");
  else if (strcmp (interface_name, "timezone-control") == 0)
    return _("Change the time zone");
  else if (strcmp (interface_name, "udisks2") == 0)
    return _("Access the UDisks2 service for configuring disks and removable media");
  else if (strcmp (interface_name, "unity8-calendar") == 0)
    return _("Read/change shared calendar events in Ubuntu Unity 8");
  else if (strcmp (interface_name, "unity8-contacts") == 0)
    return _("Read/change shared contacts in Ubuntu Unity 8");
  else if (strcmp (interface_name, "upower-observe") == 0)
    return _("Access energy usage data");
  else if (strcmp (interface_name, "u2f-devices") == 0)
    return _("Read/write access to U2F devices exposed");
  else
    return interface_name;
}

static void
update_state (CcSnapRow *self)
{
  gboolean have_single_option;
  GtkTreeIter iter;

  have_single_option = self->slots->len == 1;
  gtk_widget_set_visible (GTK_WIDGET (self->slot_toggle), have_single_option);
  gtk_widget_set_visible (GTK_WIDGET (self->slots_combo), !have_single_option);

  gtk_switch_set_active (self->slot_toggle, self->connected_slot != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->slots_combo_model), &iter))
    {
      do
        {
          SnapdSlot *slot;

          gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
          if (slot == self->connected_slot)
            gtk_combo_box_set_active_iter (self->slots_combo, &iter);
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->slots_combo_model), &iter));
    }
}

static void
disable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), FALSE);
}

static void
enable_controls (CcSnapRow *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->slot_toggle), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->slots_combo), TRUE);
}

static ConnectData *
connect_data_new (CcSnapRow *self, SnapdSlot *slot)
{
  ConnectData *data;

  data = g_new0 (ConnectData, 1);
  data->self = self;
  data->slot = g_object_ref (slot);

  return data;
}

static void
connect_data_free (ConnectData *data)
{
  g_clear_object (&data->slot);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ConnectData, connect_data_free)

static void
connect_interface_cb (GObject *client, GAsyncResult *result, gpointer user_data)
{
  g_autoptr(ConnectData) data = user_data;
  CcSnapRow *self = data->self;
  SnapdSlot *slot = data->slot;
  g_autoptr(GError) error = NULL;

  if (snapd_client_connect_interface_finish (SNAPD_CLIENT (client), result, &error))
    {
      g_clear_object (&self->connected_slot);
      self->connected_slot = g_object_ref (slot);
    }
  else
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      g_warning ("Failed to connect interface: %s", error->message);
    }

  update_state (self);
  enable_controls (self);
}

static void
connect_plug (CcSnapRow *self, SnapdSlot *slot)
{
  g_autoptr(SnapdClient) client = NULL;

  /* already connected */
  if (self->connected_slot == slot)
    return;

  disable_controls (self);

  client = snapd_client_new ();
  snapd_client_connect_interface_async (client,
                                        snapd_plug_get_snap (self->plug), snapd_plug_get_name (self->plug),
                                        snapd_slot_get_snap (slot), snapd_slot_get_name (slot),
                                        NULL, NULL,
                                        self->cancellable,
                                        connect_interface_cb, connect_data_new (self, slot));
}

static void
disconnect_interface_cb (GObject *client, GAsyncResult *result, gpointer user_data)
{
  CcSnapRow *self = user_data;
  g_autoptr(GError) error = NULL;

  if (snapd_client_disconnect_interface_finish (SNAPD_CLIENT (client), result, &error))
    {
      g_clear_object (&self->connected_slot);
    }
  else
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      g_warning ("Failed to disconnect interface: %s", error->message);
    }

  update_state (self);
  enable_controls (self);
}

static void
disconnect_plug (CcSnapRow *self)
{
  g_autoptr(SnapdClient) client = NULL;

  /* already disconnected */
  if (self->connected_slot == NULL)
    return;

  disable_controls (self);

  client = snapd_client_new ();
  snapd_client_disconnect_interface_async (client,
                                           snapd_plug_get_snap (self->plug), snapd_plug_get_name (self->plug),
                                           NULL, NULL,
                                           NULL, NULL,
                                           self->cancellable,
                                           disconnect_interface_cb, self);
}

static void
switch_changed_cb (CcSnapRow *self)
{
  if (gtk_switch_get_active (self->slot_toggle))
    {
      if (self->slots->len == 1)
        connect_plug (self, g_ptr_array_index (self->slots, 0));
    }
  else
    {
      disconnect_plug (self);
    }
}

static void
combo_changed_cb (CcSnapRow *self)
{
  GtkTreeIter iter;
  SnapdSlot *slot = NULL;

  if (!gtk_combo_box_get_active_iter (self->slots_combo, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->slots_combo_model), &iter, 0, &slot, -1);
  if (slot != NULL)
    connect_plug (self, slot);
  else
    disconnect_plug (self);
}

static void
cc_snap_row_finalize (GObject *object)
{
  CcSnapRow *self = CC_SNAP_ROW (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->plug);
  g_clear_pointer (&self->slots, g_ptr_array_unref);

  G_OBJECT_CLASS (cc_snap_row_parent_class)->finalize (object);
}

static void
cc_snap_row_class_init (CcSnapRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_snap_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-snap-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slot_toggle);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo);
  gtk_widget_class_bind_template_child (widget_class, CcSnapRow, slots_combo_model);

  gtk_widget_class_bind_template_callback (widget_class, combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_changed_cb);
}

static void
cc_snap_row_init (CcSnapRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcSnapRow *
cc_snap_row_new (GCancellable *cancellable, SnapdPlug *plug, GPtrArray *slots)
{
  CcSnapRow *self;
  GPtrArray *connected_slots;
  GtkTreeIter iter;

  g_return_val_if_fail (SNAPD_IS_PLUG (plug), NULL);

  self = CC_SNAP_ROW (g_object_new (CC_TYPE_SNAP_ROW, NULL));

  self->cancellable = g_object_ref (cancellable);
  self->plug = g_object_ref (plug);
  self->slots = g_ptr_array_ref (slots);

  connected_slots = snapd_plug_get_connected_slots (plug);
  if (connected_slots->len > 0)
    {
      SnapdSlotRef *connected_slot_ref = g_ptr_array_index (connected_slots, 0);

      for (int i = 0; i < slots->len; i++)
        {
          SnapdSlot *slot = g_ptr_array_index (slots, i);

          if (g_strcmp0 (snapd_slot_get_snap (slot), snapd_slot_ref_get_snap (connected_slot_ref)) == 0 &&
              g_strcmp0 (snapd_slot_get_name (slot), snapd_slot_ref_get_slot (connected_slot_ref)) == 0)
            self->connected_slot = slot;
        }
    }

  gtk_label_set_label (self->title_label, get_label (snapd_plug_get_interface (plug)));

  /* Add option into combo box */
  gtk_list_store_append (self->slots_combo_model, &iter);
  gtk_list_store_set (self->slots_combo_model, &iter, 1, "--", -1);
  for (int i = 0; i < slots->len; i++)
    {
      SnapdSlot *slot = g_ptr_array_index (slots, i);
      g_autofree gchar *label = NULL;

      label = g_strdup_printf ("%s:%s", snapd_slot_get_snap (slot), snapd_slot_get_name (slot));
      gtk_list_store_append (self->slots_combo_model, &iter);
      gtk_list_store_set (self->slots_combo_model, &iter, 0, slot, 1, label, -1);
    }

  update_state (self);

  return self;
}
