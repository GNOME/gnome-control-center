/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include "net-device-wifi.h"
#include "panel-common.h"

struct _NetDeviceWifi
{
        AdwActionRow             parent;

        GtkLabel                *state_label;

        NMDevice                *device;
};

enum {
        PROP_0,
        PROP_STATE,
        PROP_LAST,
};

G_DEFINE_TYPE (NetDeviceWifi, net_device_wifi, ADW_TYPE_ACTION_ROW)

static void
update_device_state (NetDeviceWifi *self)
{
        NMDeviceState state;
        NMAccessPoint *access_point;
        g_autofree char *description = NULL;
        guint strength = 0;
        gchar *icon_name = NULL;

        state = nm_device_get_state (self->device);
        if (state == NM_DEVICE_STATE_ACTIVATED)
                gtk_label_set_label (self->state_label, _("Connected"));
        else if (state == NM_DEVICE_STATE_PREPARE)
                gtk_label_set_label (self->state_label, _("Connecting…"));
        else
                gtk_label_set_label (self->state_label, _("Disconnected"));

        access_point = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (self->device));
        if (!access_point) {
                adw_action_row_set_icon_name (ADW_ACTION_ROW (self),
                                              "network-wireless-offline-symbolic");
                return;
        }

        icon_name = net_access_point_get_signal_icon (access_point);
        adw_action_row_set_icon_name (ADW_ACTION_ROW (self), icon_name);

        if (strength > 0) {
                description = g_strdup_printf(_("Signal strength %d%%"), strength);
                gtk_widget_set_tooltip_text (GTK_WIDGET (self), description);
        }
}

static void
net_device_wifi_finalize (GObject *object)
{
        NetDeviceWifi *self = NET_DEVICE_WIFI (object);

        g_clear_object (&self->device);

        G_OBJECT_CLASS (net_device_wifi_parent_class)->finalize (object);
}

static void
net_device_wifi_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        NetDeviceWifi *self = NET_DEVICE_WIFI (object);

        switch (prop_id) {
        case PROP_STATE:
                g_value_set_string (value, gtk_label_get_label (self->state_label));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
net_device_wifi_class_init (NetDeviceWifiClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_device_wifi_finalize;
        object_class->get_property = net_device_wifi_get_property;

        g_object_class_install_property (object_class,
                                         PROP_STATE,
                                         g_param_spec_string ("state",
                                                              "State",
                                                              "The Wifi Device state",
                                                              NULL,
                                                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-wifi.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, state_label);
}

static void
net_device_wifi_init (NetDeviceWifi *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

NetDeviceWifi *
net_device_wifi_new (NMDevice *device)
{
        NetDeviceWifi *self;

        self = g_object_new (net_device_wifi_get_type (), NULL);
        self->device = g_object_ref (device);

        g_signal_connect_swapped (G_OBJECT (self->device), "notify::state",
                                  G_CALLBACK (update_device_state), self);
        update_device_state (self);

        return self;
}

NMDevice *
net_device_wifi_get_device (NetDeviceWifi *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_WIFI (self), NULL);
        return self->device;
}
