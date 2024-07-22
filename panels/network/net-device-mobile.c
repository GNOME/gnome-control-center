/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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

#include <NetworkManager.h>
#include <libmm-glib.h>
#include <nma-mobile-providers.h>

#include "panel-common.h"
#include "network-dialogs.h"
#include "net-device-mobile.h"

static void nm_device_mobile_refresh_ui (NetDeviceMobile *self);

struct _NetDeviceMobile
{
        AdwActionRow  parent;

        GtkLabel     *status_label;

        NMClient     *client;
        NMDevice     *device;
        GDBusObject  *modem;
        GCancellable *cancellable;

        gboolean    updating_device;

        /* Old MM < 0.7 support */
        GDBusProxy *gsm_proxy;
        GDBusProxy *cdma_proxy;

        /* New MM >= 0.7 support */
        MMObject   *mm_object;

        NMAMobileProvidersDatabase *mpd;
};

G_DEFINE_TYPE (NetDeviceMobile, net_device_mobile, ADW_TYPE_ACTION_ROW)

static gchar *
device_mobile_find_provider (NetDeviceMobile *self,
                             const gchar     *mccmnc,
                             guint32          sid)
{
        NMAMobileProvider *provider;
        GString *name = NULL;

        if (self->mpd == NULL) {
                g_autoptr(GError) error = NULL;

                /* Use defaults */
                self->mpd = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);
                if (self->mpd == NULL) {
                        g_debug ("Couldn't load mobile providers database: %s",
                                 error ? error->message : "");
                        return NULL;
                }
        }

        if (mccmnc != NULL) {
                provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (self->mpd, mccmnc);
                if (provider != NULL)
                        name = g_string_new (nma_mobile_provider_get_name (provider));
        }

        if (sid != 0) {
                provider = nma_mobile_providers_database_lookup_cdma_sid (self->mpd, sid);
                if (provider != NULL) {
                        if (name == NULL)
                                name = g_string_new (nma_mobile_provider_get_name (provider));
                        else
                                g_string_append_printf (name, ", %s", nma_mobile_provider_get_name (provider));
                }
        }

        return (name != NULL ? g_string_free (name, FALSE) : NULL);
}

static void
nm_device_mobile_refresh_ui (NetDeviceMobile *self)
{
        g_autofree gchar *status = NULL;

        /* set device state, with status */
        status = panel_device_status_to_localized_string (self->device, NULL);
        gtk_label_set_label (self->status_label, status);
}

static void
device_mobile_device_got_modem_manager_cb (GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GDBusProxy) proxy = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (!proxy) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                return;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        if (result)
                g_object_set_data_full (G_OBJECT (self),
                                        "ControlCenter::EquipmentIdentifier",
                                        g_variant_dup_string (result, NULL),
                                        g_free);
}

static void
device_mobile_save_operator_name (NetDeviceMobile *self,
                                  const gchar     *field,
                                  const gchar     *operator_name)
{
        gchar *operator_name_safe = NULL;

        if (operator_name != NULL && operator_name[0] != '\0')
                operator_name_safe = g_strescape (operator_name, NULL);

        /* save */
        g_object_set_data_full (G_OBJECT (self),
                                field,
                                operator_name_safe,
                                g_free);
}

static void
device_mobile_get_registration_info_cb (GObject      *source_object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
        g_autofree gchar *operator_code = NULL;
        g_autoptr(GError) error = NULL;
        guint registration_status;
        g_autoptr(GVariant) result = NULL;
        g_autofree gchar *operator_name = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting registration info: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none give, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (self, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_gsm_signal_cb (NetDeviceMobile *self,
                             const gchar     *sender_name,
                             const gchar     *signal_name,
                             GVariant        *parameters)
{
        guint registration_status = 0;
        g_autofree gchar *operator_code = NULL;
        g_autofree gchar *operator_name = NULL;

        if (!g_str_equal (signal_name, "RegistrationInfo"))
                return;

        g_variant_get (parameters,
                       "(uss)",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none given, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (self, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_gsm_cb (GObject      *source_object,
                                               GAsyncResult *res,
                                               gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        self->gsm_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (self->gsm_proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                return;
        }

        /* Setup value updates */
        g_signal_connect_object (self->gsm_proxy,
                                 "g-signal",
                                 G_CALLBACK (device_mobile_gsm_signal_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

        /* Load initial value */
        g_dbus_proxy_call (self->gsm_proxy,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_registration_info_cb,
                           self);
}

static void
device_mobile_get_serving_system_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GError) error = NULL;

        guint32 band_class;
        g_autofree gchar *band = NULL;
        guint32 sid;
        gchar *operator_name;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting serving system: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((usu))",
                       &band_class,
                       &band,
                       &sid);

        operator_name = device_mobile_find_provider (self, NULL, sid);

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameCdma",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_cdma_cb (GObject      *source_object,
                                                GAsyncResult *res,
                                                gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        self->cdma_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (self->cdma_proxy == NULL) {
                g_warning ("Error creating ModemManager CDMA proxy: %s\n",
                           error->message);
                return;
        }

        /* Load initial value */
        g_dbus_proxy_call (self->cdma_proxy,
                           "GetServingSystem",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_serving_system_cb,
                           self);
}

static void
net_device_mobile_dispose (GObject *object)
{
        NetDeviceMobile *self = NET_DEVICE_MOBILE (object);

        g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->client);
        g_clear_object (&self->device);
        g_clear_object (&self->modem);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->gsm_proxy);
        g_clear_object (&self->cdma_proxy);
        g_clear_object (&self->mm_object);
        g_clear_object (&self->mpd);

        G_OBJECT_CLASS (net_device_mobile_parent_class)->dispose (object);
}

static void
net_device_mobile_class_init (NetDeviceMobileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = net_device_mobile_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-mobile.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, status_label);
}

static void
net_device_mobile_init (NetDeviceMobile *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();
}

NetDeviceMobile *
net_device_mobile_new (NMClient *client, NMDevice *device, GDBusObject *modem)
{
        NetDeviceMobile *self;
        NMDeviceModemCapabilities caps;

        self = g_object_new (net_device_mobile_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (nm_device_mobile_refresh_ui), self, G_CONNECT_SWAPPED);

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));

        /* Only load proxies if we have broadband modems of the OLD ModemManager interface */
        if (g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager/") &&
            ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_LTE))) {
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          nm_device_get_udi (device),
                                          "org.freedesktop.ModemManager.Modem",
                                          self->cancellable,
                                          device_mobile_device_got_modem_manager_cb,
                                          self);

                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_LTE)) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                                  self->cancellable,
                                                  device_mobile_device_got_modem_manager_gsm_cb,
                                                  self);
                }

                if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Cdma",
                                                  self->cancellable,
                                                  device_mobile_device_got_modem_manager_cdma_cb,
                                                  self);
                }
        }

        nm_device_mobile_refresh_ui (self);

        return self;
}

NMDevice *
net_device_mobile_get_device (NetDeviceMobile *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_MOBILE (self), NULL);
        return self->device;
}
