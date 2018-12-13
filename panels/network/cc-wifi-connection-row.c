/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "cc-wifi-connection-row.h"

struct _CcWifiConnectionRow
{
  GtkListBoxRow    parent_instance;

  gboolean         checkable;
  gboolean         checked;

  NMDeviceWifi    *device;
  NMAccessPoint   *ap;
  NMConnection    *connection;

  GtkCheckButton  *checkbutton;
  GtkImage        *active_icon;
  GtkLabel        *name_label;
  GtkStack        *button_stack;
  GtkButton       *configure_button;
  GtkSpinner      *connecting_spinner;
  GtkImage        *encrypted_icon;
  GtkImage        *strength_icon;
};

enum {
  PROP_0,
  PROP_CHECKABLE,
  PROP_CHECKED,
  PROP_DEVICE,
  PROP_AP,
  PROP_CONNECTION,
  PROP_LAST
};

typedef enum {
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2
} NMAccessPointSecurity;

G_DEFINE_TYPE (CcWifiConnectionRow, cc_wifi_connection_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *props[PROP_LAST];

static NMAccessPointSecurity
get_access_point_security (NMAccessPoint *ap)
{
        NM80211ApFlags flags;
        NM80211ApSecurityFlags wpa_flags;
        NM80211ApSecurityFlags rsn_flags;
        NMAccessPointSecurity type;

        flags = nm_access_point_get_flags (ap);
        wpa_flags = nm_access_point_get_wpa_flags (ap);
        rsn_flags = nm_access_point_get_rsn_flags (ap);

        if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
            wpa_flags == NM_802_11_AP_SEC_NONE &&
            rsn_flags == NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_NONE;
        else if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
                 wpa_flags == NM_802_11_AP_SEC_NONE &&
                 rsn_flags == NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_WEP;
        else if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
                 wpa_flags != NM_802_11_AP_SEC_NONE &&
                 rsn_flags != NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_WPA;
        else
                type = NM_AP_SEC_WPA2;

        return type;
}

static void
update_ui (CcWifiConnectionRow *self)
{
  GBytes *ssid;
  g_autofree gchar *title = NULL;
  gboolean active;
  gboolean connecting;
  NMAccessPointSecurity security;
  guint8 strength;
  NMDeviceState state;
  NMAccessPoint *active_ap;

  g_assert (self->device);
  g_assert (self->connection || self->ap);

  active_ap = nm_device_wifi_get_active_access_point (self->device);

  if (self->connection)
    {
      NMSettingWireless *sw;
      sw = nm_connection_get_setting_wireless (self->connection);

      ssid = nm_setting_wireless_get_ssid (sw);
    }
  else
    {
      ssid = nm_access_point_get_ssid (self->ap);
    }

  if (self->ap != NULL)
    {
      state = nm_device_get_state (NM_DEVICE (self->device));

      active = (self->ap == active_ap) && (state == NM_DEVICE_STATE_ACTIVATED);
      connecting = (self->ap == active_ap) &&
                   (state == NM_DEVICE_STATE_PREPARE ||
                    state == NM_DEVICE_STATE_CONFIG ||
                    state == NM_DEVICE_STATE_IP_CONFIG ||
                    state == NM_DEVICE_STATE_IP_CHECK ||
                    state == NM_DEVICE_STATE_NEED_AUTH);
      security = get_access_point_security (self->ap);
      strength = nm_access_point_get_strength (self->ap);
    }
  else
    {
      active = FALSE;
      connecting = FALSE;
      security = NM_AP_SEC_UNKNOWN;
      strength = 0;
    }


  title = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
  gtk_label_set_text (self->name_label, title);

  /* TODO: Active */
  gtk_widget_set_visible (GTK_WIDGET (self->active_icon), FALSE);

  if (connecting)
    {
      gtk_stack_set_visible_child_name (self->button_stack, "connecting");
      gtk_spinner_start (self->connecting_spinner);
    }
  else
  	{
      gtk_spinner_stop (self->connecting_spinner);

      if (self->connection)
        gtk_stack_set_visible_child_name (self->button_stack, "configure");
      else
        gtk_stack_set_visible_child_name (self->button_stack, "empty");
  	}

  gtk_widget_set_visible (GTK_WIDGET (self->active_icon), active);

  if (security != NM_AP_SEC_UNKNOWN && security != NM_AP_SEC_NONE)
    gtk_widget_set_opacity (GTK_WIDGET (self->encrypted_icon), 1.0);
  else
    gtk_widget_set_opacity (GTK_WIDGET (self->encrypted_icon), 0.0);

  if (self->ap)
    {
      gchar *icon_name;

      if (strength < 20)
        icon_name = "network-wireless-signal-none-symbolic";
      else if (strength < 40)
        icon_name = "network-wireless-signal-weak-symbolic";
      else if (strength < 50)
        icon_name = "network-wireless-signal-ok-symbolic";
      else if (strength < 80)
        icon_name = "network-wireless-signal-good-symbolic";
      else
        icon_name = "network-wireless-signal-excellent-symbolic";

      g_object_set (self->strength_icon, "icon-name", icon_name, NULL);
      gtk_widget_set_opacity (GTK_WIDGET (self->strength_icon), 1.0);
    }
  else
    gtk_widget_set_opacity (GTK_WIDGET (self->strength_icon), 0.0);
}

static void
cc_wifi_connection_row_constructed (GObject *object)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);
  G_OBJECT_CLASS (cc_wifi_connection_row_parent_class)->constructed (object);

  /* Reparent the label into the checkbox */
  if (self->checkable)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->checkbutton), TRUE);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (self->name_label))),
                            GTK_WIDGET (self->name_label));
      gtk_container_add (GTK_CONTAINER (self->checkbutton), GTK_WIDGET (self->name_label));
      gtk_widget_show (GTK_WIDGET (self->name_label));
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->checkbutton), FALSE);
    }

  update_ui (CC_WIFI_CONNECTION_ROW (object));
}

static void
cc_wifi_connection_row_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      g_value_set_boolean (value, self->checkable);
      break;

    case PROP_CHECKED:
      g_value_set_boolean (value, self->checked);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_AP:
      g_value_set_object (value, self->ap);
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      self->checkable = g_value_get_boolean (value);
      break;

    case PROP_CHECKED:
      self->checked = g_value_get_boolean (value);
      break;

    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    case PROP_AP:
      self->ap = g_value_dup_object (value);
      break;

    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_row_finalize (GObject *object)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);

  g_clear_object (&self->device);
  g_clear_object (&self->ap);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (cc_wifi_connection_row_parent_class)->finalize (object);
}


void
cc_wifi_connection_row_class_init (CcWifiConnectionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_wifi_connection_row_constructed;
  object_class->get_property = cc_wifi_connection_row_get_property;
  object_class->set_property = cc_wifi_connection_row_set_property;
  object_class->finalize = cc_wifi_connection_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-wifi-connection-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, checkbutton);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, active_icon);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, button_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, configure_button);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, connecting_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, encrypted_icon);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, strength_icon);

  props[PROP_CHECKABLE] = g_param_spec_boolean ("checkable", "checkable",
                                                "Whether to show a checkbox to select the row",
                                                FALSE,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CHECKED] = g_param_spec_boolean ("checked", "Checked",
                                              "Whether the row is selected by checking it",
                                              FALSE,
                                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_DEVICE] = g_param_spec_object ("device", "WiFi Device",
                                            "The WiFi Device for this connection/ap",
                                            NM_TYPE_DEVICE_WIFI,
                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_AP] = g_param_spec_object ("ap", "Access Point",
                                        "The best access point for this connection  (may be NULL if there is a connection)",
                                        NM_TYPE_ACCESS_POINT,
                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CONNECTION] = g_param_spec_object ("connection", "Connection",
                                                "The NMConnection (may be NULL if there is an AP)",
                                                 NM_TYPE_CONNECTION,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     PROP_LAST,
                                     props);

  g_signal_new ("configure",
                CC_TYPE_WIFI_CONNECTION_ROW,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);

}

static void
configure_clicked_cb (GtkButton *btn, CcWifiConnectionRow *row)
{
  g_signal_emit_by_name (row, "configure");
}

void
cc_wifi_connection_row_init (CcWifiConnectionRow *row)
{
  gtk_widget_init_template (GTK_WIDGET (row));

  g_signal_connect (row->configure_button, "clicked", G_CALLBACK (configure_clicked_cb), row);

  g_object_bind_property (row, "checked",
                          row->checkbutton, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

CcWifiConnectionRow *
cc_wifi_connection_row_new (NMDevice      *device,
                            NMConnection  *connection,
                            NMAccessPoint *ap,
                            gboolean       checkable)
{
  return g_object_new (CC_TYPE_WIFI_CONNECTION_ROW,
                       "device", device,
                       "connection", connection,
                       "ap", ap,
                       "checkable", checkable,
                       NULL);
}

gboolean
cc_wifi_connection_row_get_checkable (CcWifiConnectionRow  *row)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (row), FALSE);

  return row->checkable;
}

gboolean
cc_wifi_connection_row_get_checked (CcWifiConnectionRow  *row)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (row), FALSE);

  return row->checked;
}

NMDeviceWifi*
cc_wifi_connection_row_get_device (CcWifiConnectionRow  *row)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (row), NULL);

  return row->device;
}

NMAccessPoint*
cc_wifi_connection_row_get_access_point (CcWifiConnectionRow  *row)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (row), NULL);

  return row->ap;
}

NMConnection*
cc_wifi_connection_row_get_connection (CcWifiConnectionRow  *row)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (row), NULL);

  return row->connection;
}

