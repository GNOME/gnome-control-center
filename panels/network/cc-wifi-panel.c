/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#include "cc-network-resources.h"
#include "cc-wifi-panel.h"
#include "net-device-wifi.h"
#include "network-dialogs.h"

#include "shell/cc-application.h"
#include "shell/cc-debug.h"
#include "shell/cc-object-storage.h"

#include <glib/gi18n.h>
#include <NetworkManager.h>

typedef enum
{
  OPERATION_NULL,
  OPERATION_SHOW_DEVICE,
  OPERATION_CREATE_WIFI,
  OPERATION_CONNECT_HIDDEN,
  OPERATION_CONNECT_8021X
} CmdlineOperation;

struct _CcWifiPanel
{
  CcPanel             parent;

  /* RFKill (Airplane Mode) */
  GDBusProxy         *rfkill_proxy;
  GtkSwitch          *rfkill_switch;
  GtkWidget          *rfkill_widget;

  /* Main widgets */
  GtkStack           *center_stack;
  GtkStack           *header_stack;
  GtkStack           *main_stack;
  GtkWidget          *spinner;
  GtkStack           *stack;

  NMClient           *client;

  GPtrArray          *devices;

  GBinding           *spinner_binding;

  /* Command-line arguments */
  CmdlineOperation    arg_operation;
  gchar              *arg_device;
  gchar              *arg_access_point;
};

static void          rfkill_switch_notify_activate_cb            (GtkSwitch          *rfkill_switch,
                                                                  GParamSpec         *pspec,
                                                                  CcWifiPanel        *self);

static void          update_devices_names                        (CcWifiPanel        *self);

G_DEFINE_TYPE (CcWifiPanel, cc_wifi_panel, CC_TYPE_PANEL)

enum
{
  PROP_0,
  PROP_PARAMETERS,
  N_PROPS
};

/* Static init function */

static void
update_panel_visibility (NMClient *client)
{
  const GPtrArray *devices;
  CcApplication *application;
  gboolean visible;
  guint i;

  CC_TRACE_MSG ("Updating Wi-Fi panel visibility");

  devices = nm_client_get_devices (client);
  visible = FALSE;

  for (i = 0; devices && i < devices->len; i++)
    {
      NMDevice *device = g_ptr_array_index (devices, i);

      visible |= NM_IS_DEVICE_WIFI (device);

      if (visible)
        break;
    }

  /* Set the new visibility */
  application = CC_APPLICATION (g_application_get_default ());
  cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                       "wifi",
                                       visible ? CC_PANEL_VISIBLE : CC_PANEL_VISIBLE_IN_SEARCH);

  g_debug ("Wi-Fi panel visible: %s", visible ? "yes" : "no");
}

void
cc_wifi_panel_static_init_func (void)
{
  g_autoptr(NMClient) client = NULL;

  g_debug ("Monitoring NetworkManager for Wi-Fi devices");

  /* Create and store a NMClient instance if it doesn't exist yet */
  if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT))
    {
      g_autoptr(NMClient) new_client = nm_client_new (NULL, NULL);
      cc_object_storage_add_object (CC_OBJECT_NMCLIENT, new_client);
    }

  client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

  /* Update the panel visibility and monitor for changes */

  g_signal_connect (client, "device-added", G_CALLBACK (update_panel_visibility), NULL);
  g_signal_connect (client, "device-removed", G_CALLBACK (update_panel_visibility), NULL);

  update_panel_visibility (client);
}

/* Auxiliary methods */

static void
add_wifi_device (CcWifiPanel *self,
                 NMDevice    *device)
{
  GtkWidget *header_widget;
  NetDeviceWifi *net_device;

  /* Only manage Wi-Fi devices */
  if (!NM_IS_DEVICE_WIFI (device) || !nm_device_get_managed (device))
    return;

  /* Create the NetDevice */
  net_device = net_device_wifi_new (CC_PANEL (self),
                                    self->client,
                                    device);
  gtk_widget_show (GTK_WIDGET (net_device));

  /* And add to the header widgets */
  header_widget = net_device_wifi_get_header_widget (net_device);

  gtk_stack_add_named (self->header_stack, header_widget, nm_device_get_udi (device));

  /* Setup custom title properties */
  g_ptr_array_add (self->devices, net_device);

  update_devices_names (self);

  /* Needs to be added after the device is added to the self->devices array */
  gtk_stack_add_titled (self->stack, GTK_WIDGET (net_device),
                        nm_device_get_udi (device),
                        nm_device_get_description (device));
}

static void
check_main_stack_page (CcWifiPanel *self)
{
  const gchar *nm_version;
  gboolean airplane_mode_active;
  gboolean wireless_enabled;

  nm_version = nm_client_get_version (self->client);
  wireless_enabled = nm_client_wireless_get_enabled (self->client);
  airplane_mode_active = gtk_switch_get_active (self->rfkill_switch);

  if (!nm_version)
    gtk_stack_set_visible_child_name (self->main_stack, "nm-not-running");
  else if (!wireless_enabled && airplane_mode_active)
    gtk_stack_set_visible_child_name (self->main_stack, "airplane-mode");
  else if (!wireless_enabled || self->devices->len == 0)
    gtk_stack_set_visible_child_name (self->main_stack, "no-wifi-devices");
  else
    gtk_stack_set_visible_child_name (self->main_stack, "wifi-connections");
}

static void
load_wifi_devices (CcWifiPanel *self)
{
  const GPtrArray *devices;
  guint i;

  devices = nm_client_get_devices (self->client);

  /* Cold-plug existing devices */
  if (devices)
    {
      for (i = 0; i < devices->len; i++)
        add_wifi_device (self, g_ptr_array_index (devices, i));
    }

  check_main_stack_page (self);
}

static inline gboolean
get_cached_rfkill_property (CcWifiPanel *self,
                            const gchar *property)
{
  g_autoptr(GVariant) result = NULL;

  result = g_dbus_proxy_get_cached_property (self->rfkill_proxy, property);
  return result ? g_variant_get_boolean (result) : FALSE;
}

static void
sync_airplane_mode_switch (CcWifiPanel *self)
{
  gboolean enabled, should_show, hw_enabled;

  enabled = get_cached_rfkill_property (self, "HasAirplaneMode");
  should_show = get_cached_rfkill_property (self, "ShouldShowAirplaneMode");

  gtk_widget_set_visible (GTK_WIDGET (self->rfkill_widget), enabled && should_show);
  if (!enabled || !should_show)
    return;

  enabled = get_cached_rfkill_property (self, "AirplaneMode");
  hw_enabled = get_cached_rfkill_property (self, "HardwareAirplaneMode");

  enabled |= hw_enabled;

  if (enabled != gtk_switch_get_active (self->rfkill_switch))
    {
      g_signal_handlers_block_by_func (self->rfkill_switch,
                                       rfkill_switch_notify_activate_cb,
                                       self);
      gtk_switch_set_active (self->rfkill_switch, enabled);
      check_main_stack_page (self);
      g_signal_handlers_unblock_by_func (self->rfkill_switch,
                                         rfkill_switch_notify_activate_cb,
                                         self);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (self->rfkill_switch), !hw_enabled);

  check_main_stack_page (self);
}

static void
update_devices_names (CcWifiPanel *self)
{
  guint number_of_devices = self->devices->len;

  if (number_of_devices == 1)
    {
      GtkWidget *title_widget;
      NetDeviceWifi *net_device;

      net_device = g_ptr_array_index (self->devices, 0);
      title_widget = net_device_wifi_get_title_widget (net_device);

      gtk_stack_add_named (self->center_stack, title_widget, "single");
      gtk_stack_set_visible_child_name (self->center_stack, "single");

      net_device_wifi_set_title (net_device, _("Wi-Fi"));
    }
  else
    {
      GtkWidget *single_page_widget;
      guint i;

      for (i = 0; i < number_of_devices; i++)
        {
          NetDeviceWifi *net_device;
          NMDevice *device;

          net_device = g_ptr_array_index (self->devices, i);
          device = net_device_wifi_get_device (net_device);

          net_device_wifi_set_title (net_device, nm_device_get_description (device));
        }

      /* Remove the widget at the "single" page */
      single_page_widget = gtk_stack_get_child_by_name (self->center_stack, "single");

      if (single_page_widget)
        {
          g_object_ref (single_page_widget);
          gtk_container_remove (GTK_CONTAINER (self->center_stack), single_page_widget);
          g_object_unref (single_page_widget);
        }

      /* Show the stack-switcher page */
      gtk_stack_set_visible_child_name (self->center_stack, "many");
    }
}

/* Command-line arguments */

static void
reset_command_line_args (CcWifiPanel *self)
{
  self->arg_operation = OPERATION_NULL;
  g_clear_pointer (&self->arg_device, g_free);
  g_clear_pointer (&self->arg_access_point, g_free);
}

static gboolean
handle_argv_for_device (CcWifiPanel *self, NetDeviceWifi *net_device)
{
  GtkWidget *toplevel;
  NMDevice *device;
  gboolean ret;

  toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));
  device = net_device_wifi_get_device (net_device);
  ret = FALSE;

  if (self->arg_operation == OPERATION_CREATE_WIFI)
    {
      cc_network_panel_create_wifi_network (toplevel, self->client);
      ret = TRUE;
    }
  else if (self->arg_operation == OPERATION_CONNECT_HIDDEN)
    {
      cc_network_panel_connect_to_hidden_network (toplevel, self->client);
      ret = TRUE;
    }
  else if (g_str_equal (nm_object_get_path (NM_OBJECT (device)), self->arg_device))
    {
      if (self->arg_operation == OPERATION_CONNECT_8021X)
        {
          cc_network_panel_connect_to_8021x_network (toplevel,
                                                     self->client,
                                                     device,
                                                     self->arg_access_point);
          ret = TRUE;
        }
      else if (self->arg_operation == OPERATION_SHOW_DEVICE)
        {
          gtk_stack_set_visible_child_name (self->stack, nm_device_get_udi (device));
          ret = TRUE;
        }
    }

  if (ret)
    reset_command_line_args (self);

  return ret;
}

static void
handle_argv (CcWifiPanel *self)
{
  guint i;

  if (self->arg_operation == OPERATION_NULL)
    return;

  for (i = 0; i < self->devices->len; i++)
    {
      if (handle_argv_for_device (self, g_ptr_array_index (self->devices, i)))
        break;
    }
}

static GPtrArray *
variant_av_to_string_array (GVariant *array)
{
  GVariantIter iter;
  GVariant *v;
  GPtrArray *strv;
  gsize count;

  count = g_variant_iter_init (&iter, array);
  strv = g_ptr_array_sized_new (count + 1);

  while (g_variant_iter_next (&iter, "v", &v))
    {
      g_ptr_array_add (strv, (gpointer) g_variant_get_string (v, NULL));
      g_variant_unref (v);
    }

  g_ptr_array_add (strv, NULL); /* NULL-terminate the strv data array */
  return strv;
}

static gboolean
verify_argv (CcWifiPanel  *self,
             const char  **args)
{
  switch (self->arg_operation)
    {
    case OPERATION_CONNECT_8021X:
    case OPERATION_SHOW_DEVICE:
      if (!self->arg_device)
        {
          g_warning ("Operation %s requires an object path", args[0]);
          return FALSE;
        }
    default:
      return TRUE;
    }
}

/* Callbacks */

static void
device_added_cb (CcWifiPanel *self, NMDevice *device)
{
  add_wifi_device (self, device);
  check_main_stack_page (self);
}

static void
device_removed_cb (CcWifiPanel *self, NMDevice *device)
{
  GtkWidget *child;
  const gchar *id;
  guint i;

  if (!NM_IS_DEVICE_WIFI (device) || !nm_device_get_managed (device))
    return;

  id = nm_device_get_udi (device);

  /* Destroy all stack pages related to this device */
  child = gtk_stack_get_child_by_name (self->stack, id);
  gtk_widget_destroy (child);

  child = gtk_stack_get_child_by_name (self->header_stack, id);
  gtk_widget_destroy (child);

  /* Remove from the devices list */
  for (i = 0; i < self->devices->len; i++)
    {
      NetDeviceWifi *net_device = g_ptr_array_index (self->devices, i);

      if (net_device_wifi_get_device (net_device) == device)
        {
          g_ptr_array_remove (self->devices, net_device);
          break;
        }
    }

  /* Update the title widget */
  update_devices_names (self);

  /* And check which page should be visible */
  check_main_stack_page (self);
}

static void
wireless_enabled_cb (CcWifiPanel *self)
{
  check_main_stack_page (self);
}

static void
on_rfkill_proxy_properties_changed_cb (CcWifiPanel *self)
{
  g_debug ("Rfkill properties changed");

  sync_airplane_mode_switch (self);
}

static void
rfkill_proxy_acquired_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcWifiPanel *self;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating rfkill proxy: %s\n", error->message);

      return;
    }

  self = CC_WIFI_PANEL (user_data);

  self->rfkill_proxy = proxy;

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_rfkill_proxy_properties_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  sync_airplane_mode_switch (self);
}

static void
rfkill_switch_notify_activate_cb (GtkSwitch   *rfkill_switch,
                                  GParamSpec  *pspec,
                                  CcWifiPanel *self)
{
  gboolean enable;

  enable = gtk_switch_get_active (rfkill_switch);

  g_dbus_proxy_call (self->rfkill_proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
                                           "'AirplaneMode', %v)",
                                           g_variant_new_boolean (enable)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     NULL,
                     NULL);
}

static void
on_stack_visible_child_changed_cb (GtkStack    *stack,
                                   GParamSpec  *pspec,
                                   CcWifiPanel *self)
{
  const gchar *visible_device_id = NULL;
  guint i;

  /* Remove previous bindings */
  g_clear_pointer (&self->spinner_binding, g_binding_unbind);

  visible_device_id = gtk_stack_get_visible_child_name (stack);
  for (i = 0; i < self->devices->len; i++)
    {
      NetDeviceWifi *net_device = g_ptr_array_index (self->devices, i);

      if (g_strcmp0 (nm_device_get_udi (net_device_wifi_get_device (net_device)), visible_device_id) == 0)
        {
          self->spinner_binding = g_object_bind_property (net_device,
                                                          "scanning",
                                                          self->spinner,
                                                          "active",
                                                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
          break;
        }
    }
}

/* Overrides */

static const gchar *
cc_wifi_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/net#connecting";
}

static GtkWidget *
cc_wifi_panel_get_title_widget (CcPanel *panel)
{
  CcWifiPanel *self = CC_WIFI_PANEL (panel);

  return GTK_WIDGET (self->center_stack);
}

static void
cc_wifi_panel_constructed (GObject *object)
{
  CcWifiPanel *self;
  CcShell *shell;

  self = CC_WIFI_PANEL (object);
  shell = cc_panel_get_shell (CC_PANEL (object));

  G_OBJECT_CLASS (cc_wifi_panel_parent_class)->constructed (object);

  cc_shell_embed_widget_in_header (shell, GTK_WIDGET (self->header_stack), GTK_POS_RIGHT);
}

static void
cc_wifi_panel_finalize (GObject *object)
{
  CcWifiPanel *self = (CcWifiPanel *)object;

  g_clear_object (&self->client);
  g_clear_object (&self->rfkill_proxy);

  g_clear_pointer (&self->devices, g_ptr_array_unref);

  reset_command_line_args (self);

  G_OBJECT_CLASS (cc_wifi_panel_parent_class)->finalize (object);
}

static void
cc_wifi_panel_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
cc_wifi_panel_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcWifiPanel *self = CC_WIFI_PANEL (object);
  GVariant *parameters;

  switch (prop_id)
    {
    case PROP_PARAMETERS:
      reset_command_line_args (self);

      parameters = g_value_get_variant (value);

      if (parameters)
        {
          g_autoptr(GPtrArray) array = NULL;
          const gchar **args;

          array = variant_av_to_string_array (parameters);
          args = (const gchar **) array->pdata;

          if (args[0])
            {
              if (g_str_equal (args[0], "create-wifi"))
                self->arg_operation = OPERATION_CREATE_WIFI;
              else if (g_str_equal (args[0], "connect-hidden-wifi"))
                self->arg_operation = OPERATION_CONNECT_HIDDEN;
              else if (g_str_equal (args[0], "connect-8021x-wifi"))
                self->arg_operation = OPERATION_CONNECT_8021X;
              else if (g_str_equal (args[0], "show-device"))
                self->arg_operation = OPERATION_SHOW_DEVICE;
              else
                self->arg_operation = OPERATION_NULL;
            }

          if (args[0] && args[1])
            self->arg_device = g_strdup (args[1]);
          if (args[0] && args[1] && args[2])
            self->arg_access_point = g_strdup (args[2]);

          if (!verify_argv (self, (const char **) args))
            {
              reset_command_line_args (self);
              return;
            }

          handle_argv (self);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_panel_class_init (CcWifiPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_wifi_panel_get_help_uri;
  panel_class->get_title_widget = cc_wifi_panel_get_title_widget;

  object_class->constructed = cc_wifi_panel_constructed;
  object_class->finalize = cc_wifi_panel_finalize;
  object_class->get_property = cc_wifi_panel_get_property;
  object_class->set_property = cc_wifi_panel_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-wifi-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, center_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, header_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, rfkill_switch);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, rfkill_widget);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcWifiPanel, stack);

  gtk_widget_class_bind_template_callback (widget_class, rfkill_switch_notify_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_stack_visible_child_changed_cb);

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static void
cc_wifi_panel_init (CcWifiPanel *self)
{
  g_resources_register (cc_network_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->devices = g_ptr_array_new ();

  /* Create and store a NMClient instance if it doesn't exist yet */
  if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT))
    {
      g_autoptr(NMClient) client = nm_client_new (NULL, NULL);
      cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
    }

  /* Load NetworkManager */
  self->client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

  g_signal_connect_object (self->client,
                           "device-added",
                           G_CALLBACK (device_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->client,
                           "device-removed",
                           G_CALLBACK (device_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->client,
                           "notify::wireless-enabled",
                           G_CALLBACK (wireless_enabled_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Load Wi-Fi devices */
  load_wifi_devices (self);

  /* Acquire Airplane Mode proxy */
  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Rfkill",
                                       "/org/gnome/SettingsDaemon/Rfkill",
                                       "org.gnome.SettingsDaemon.Rfkill",
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       rfkill_proxy_acquired_cb,
                                       self);

  /* Handle comment-line arguments after loading devices */
  handle_argv (self);
}
