/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>
#include <glib/gi18n.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <stdlib.h>

#include "cc-network-panel.h"

#include "nm-remote-settings.h"
#include "nm-client.h"
#include "nm-device.h"
#include "nm-device-ethernet.h"
#include "nm-device-modem.h"
#include "nm-device-wifi.h"
#include "nm-utils.h"
#include "nm-active-connection.h"
#include "nm-vpn-connection.h"
#include "nm-setting-wireless.h"
#include "nm-setting-ip4-config.h"
#include "nm-setting-ip6-config.h"
#include "nm-setting-connection.h"
#include "nm-setting-vpn.h"
#include "nm-setting-wireless.h"

#include "net-object.h"
#include "net-device.h"
#include "net-vpn.h"

#include "panel-common.h"
#include "panel-cell-renderer-mode.h"
#include "panel-cell-renderer-signal.h"
#include "panel-cell-renderer-security.h"

#include "network-dialogs.h"

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

typedef enum {
        OPERATION_NULL,
        OPERATION_SHOW_DEVICE,
        OPERATION_CREATE_WIFI,
        OPERATION_CONNECT_HIDDEN,
        OPERATION_CONNECT_8021X,
        OPERATION_CONNECT_MOBILE
} CmdlineOperation;

struct _CcNetworkPanelPrivate
{
        GCancellable     *cancellable;
        GSettings        *proxy_settings;
        GtkBuilder       *builder;
        NMClient         *client;
        NMRemoteSettings *remote_settings;
        gboolean          updating_device;
        guint             add_header_widgets_idle;
        guint             nm_warning_idle;
        guint             refresh_idle;
        GtkWidget        *kill_switch_header;

        /* wireless dialog stuff */
        CmdlineOperation  arg_operation;
        gchar            *arg_device;
        gchar            *arg_access_point;
        gboolean          operation_done;
};

enum {
        PANEL_DEVICES_COLUMN_ICON,
        PANEL_DEVICES_COLUMN_TITLE,
        PANEL_DEVICES_COLUMN_SORT,
        PANEL_DEVICES_COLUMN_OBJECT,
        PANEL_DEVICES_COLUMN_LAST
};

enum {
        PANEL_WIRELESS_COLUMN_ID,
        PANEL_WIRELESS_COLUMN_TITLE,
        PANEL_WIRELESS_COLUMN_SORT,
        PANEL_WIRELESS_COLUMN_STRENGTH,
        PANEL_WIRELESS_COLUMN_MODE,
        PANEL_WIRELESS_COLUMN_SECURITY,
        PANEL_WIRELESS_COLUMN_LAST
};

enum {
        PROP_0,
        PROP_ARGV
};

static void     refresh_ui      (CcNetworkPanel *panel);
static NetObject *find_in_model_by_id (CcNetworkPanel *panel, const gchar *id);
static gboolean find_model_iter_by_object (GtkTreeModel *model, const NetObject *object, GtkTreeIter *iter);

static void
cc_network_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
        switch (property_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static CmdlineOperation
cmdline_operation_from_string (const gchar *string)
{
        if (g_strcmp0 (string, "create-wifi") == 0)
                return OPERATION_CREATE_WIFI;
        if (g_strcmp0 (string, "connect-hidden-wifi") == 0)
                return OPERATION_CONNECT_HIDDEN;
        if (g_strcmp0 (string, "connect-8021x-wifi") == 0)
                return OPERATION_CONNECT_8021X;
        if (g_strcmp0 (string, "connect-3g") == 0)
                return OPERATION_CONNECT_MOBILE;
        if (g_strcmp0 (string, "show-device") == 0)
                return OPERATION_SHOW_DEVICE;

        g_warning ("Invalid additional argument %s", string);
        return OPERATION_NULL;
}

static void
cc_network_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);
        CcNetworkPanelPrivate *priv = self->priv;

        switch (property_id) {
        case PROP_ARGV: {
                gchar **args;

                priv->arg_operation = OPERATION_NULL;
                g_free (priv->arg_device);
                priv->arg_device = NULL;
                g_free (priv->arg_access_point);
                priv->arg_access_point = NULL;

                args = g_value_get_boxed (value);

                if (args) {
                        g_debug ("Invoked with operation %s", args[0]);

                        if (args[0])
                                priv->arg_operation = cmdline_operation_from_string (args[0]);
                        if (args[0] && args[1])
                                priv->arg_device = g_strdup (args[1]);
                        if (args[0] && args[1] && args[2])
                                priv->arg_access_point = g_strdup (args[2]);
                }
                break;
        }
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
cc_network_panel_dispose (GObject *object)
{
        CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (object)->priv;

        if (priv->proxy_settings) {
                g_object_unref (priv->proxy_settings);
                priv->proxy_settings = NULL;
        }
        if (priv->cancellable != NULL) {
                g_cancellable_cancel (priv->cancellable);
                g_object_unref (priv->cancellable);
                priv->cancellable = NULL;
        }
        if (priv->builder != NULL) {
                g_object_unref (priv->builder);
                priv->builder = NULL;
        }
        if (priv->client != NULL) {
                g_object_unref (priv->client);
                priv->client = NULL;
        }
        if (priv->remote_settings != NULL) {
                g_object_unref (priv->remote_settings);
                priv->remote_settings = NULL;
        }
        if (priv->kill_switch_header != NULL) {
                g_clear_object (&priv->kill_switch_header);
        }
        if (priv->refresh_idle != 0) {
                g_source_remove (priv->refresh_idle);
                priv->refresh_idle = 0;
        }
        if (priv->nm_warning_idle != 0) {
                g_source_remove (priv->nm_warning_idle);
                priv->nm_warning_idle = 0;
        }
        if (priv->add_header_widgets_idle != 0) {
                g_source_remove (priv->add_header_widgets_idle);
                priv->add_header_widgets_idle = 0;
        }

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (object)->priv;
        g_free (priv->arg_device);
        g_free (priv->arg_access_point);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private (klass, sizeof (CcNetworkPanelPrivate));

        object_class->get_property = cc_network_panel_get_property;
        object_class->set_property = cc_network_panel_set_property;
        object_class->dispose = cc_network_panel_dispose;
        object_class->finalize = cc_network_panel_finalize;

        g_object_class_override_property (object_class, PROP_ARGV, "argv");
}

static void
cc_network_panel_class_finalize (CcNetworkPanelClass *klass)
{
}

static void
check_wpad_warning (CcNetworkPanel *panel)
{
        GtkWidget *widget;
        gchar *autoconfig_url = NULL;
        GString *string = NULL;
        gboolean ret = FALSE;
        guint mode;

        string = g_string_new ("");

        /* check we're using 'Automatic' */
        mode = g_settings_get_enum (panel->priv->proxy_settings, "mode");
        if (mode != 2)
                goto out;

        /* see if the PAC is blank */
        autoconfig_url = g_settings_get_string (panel->priv->proxy_settings,
                                                "autoconfig-url");
        ret = autoconfig_url == NULL ||
              autoconfig_url[0] == '\0';
        if (!ret)
                goto out;

        g_string_append (string, "<small>");

        /* TRANSLATORS: this is when the use leaves the PAC textbox blank */
        g_string_append (string, _("Web Proxy Autodiscovery is used when a Configuration URL is not provided."));

        g_string_append (string, "\n");

        /* TRANSLATORS: WPAD is bad: if you enable it on an untrusted
         * network, then anyone else on that network can tell your
         * machine that it should proxy all of your web traffic
         * through them. */
        g_string_append (string, _("This is not recommended for untrusted public networks."));
        g_string_append (string, "</small>");
out:
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "label_proxy_warning"));
        gtk_label_set_markup (GTK_LABEL (widget), string->str);
        g_free (autoconfig_url);
        g_string_free (string, TRUE);
}

static void
panel_settings_changed (GSettings      *settings,
                        const gchar    *key,
                        CcNetworkPanel *panel)
{
        check_wpad_warning (panel);
}

static NetObject *
get_selected_object (CcNetworkPanel *panel)
{
        GtkWidget *widget;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        NetObject *object = NULL;

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "treeview_devices"));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return NULL;
        }

        gtk_tree_model_get (model, &iter,
                            PANEL_DEVICES_COLUMN_OBJECT, &object,
                            -1);

        return object;
}

static void
panel_proxy_mode_combo_setup_widgets (CcNetworkPanel *panel, guint value)
{
        GtkWidget *widget;

        /* hide or show the PAC text box */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "heading_proxy_url"));
        gtk_widget_set_visible (widget, value == 2);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_url"));
        gtk_widget_set_visible (widget, value == 2);

        /* hide or show the manual entry text boxes */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "heading_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "spinbutton_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "heading_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "spinbutton_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "heading_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "spinbutton_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "heading_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "spinbutton_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);

        /* perhaps show the wpad warning */
        check_wpad_warning (panel);
}

static void
panel_proxy_mode_combo_changed_cb (GtkWidget *widget, CcNetworkPanel *panel)
{
        gboolean ret;
        gint value;
        GtkTreeIter iter;
        GtkTreeModel *model;

        /* no selection */
        ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
        if (!ret)
                return;

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        gtk_tree_model_get (model, &iter,
                            1, &value,
                            -1);

        /* set */
        g_settings_set_enum (panel->priv->proxy_settings, "mode", value);

        /* hide or show the correct widgets */
        panel_proxy_mode_combo_setup_widgets (panel, value);
}

static void
panel_set_value_for_combo (CcNetworkPanel *panel, GtkComboBox *combo_box, gint value)
{
        gboolean ret;
        gint value_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;

        /* get entry */
        model = gtk_combo_box_get_model (combo_box);
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;

        /* try to make the UI match the setting */
        do {
                gtk_tree_model_get (model, &iter,
                                    1, &value_tmp,
                                    -1);
                if (value == value_tmp) {
                        gtk_combo_box_set_active_iter (combo_box, &iter);
                        break;
                }
        } while (gtk_tree_model_iter_next (model, &iter));

        /* hide or show the correct widgets */
        panel_proxy_mode_combo_setup_widgets (panel, value);
}

static void
select_first_device (CcNetworkPanel *panel)
{
        GtkTreePath *path;
        GtkWidget *widget;
        GtkTreeSelection *selection;

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "treeview_devices"));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

        /* select the first device */
        path = gtk_tree_path_new_from_string ("0");
        gtk_tree_selection_select_path (selection, path);
        gtk_tree_path_free (path);
}

static void
select_tree_iter (CcNetworkPanel *panel, GtkTreeIter *iter)
{
        GtkTreeView *widget;
        GtkTreeSelection *selection;

        widget = GTK_TREE_VIEW (gtk_builder_get_object (panel->priv->builder,
                                                        "treeview_devices"));
        selection = gtk_tree_view_get_selection (widget);

        gtk_tree_selection_select_iter (selection, iter);
}

static void
panel_device_got_modem_manager_cb (GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = NULL;
        GDBusProxy *proxy;
        NMDevice *device = (NMDevice *) user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (proxy == NULL) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        g_object_set_data_full (G_OBJECT (device),
                                "ControlCenter::EquipmentIdentifier",
                                g_variant_dup_string (result, NULL),
                                g_free);
out:
        if (result != NULL)
                g_variant_unref (result);
        if (proxy != NULL)
                g_object_unref (proxy);
        return;
}

static void
panel_get_registration_info_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        gchar *operator_code = NULL;
        GError *error = NULL;
        guint registration_status;
        GVariant *result = NULL;
        gchar *operator_name = NULL;
        gchar *operator_name_safe = NULL;
        NMDevice *device = (NMDevice *) user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting registration info: %s\n",
                           error->message);
                g_error_free (error);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &operator_name);
        if (operator_name != NULL && operator_name[0] != '\0')
                operator_name_safe = g_strescape (operator_name, NULL);

        /* save */
        g_object_set_data_full (G_OBJECT (device),
                                "ControlCenter::OperatorName",
                                operator_name_safe,
                                g_free);

        g_free (operator_name);
        g_free (operator_code);
        g_variant_unref (result);
}

static void
panel_device_got_modem_manager_gsm_cb (GObject *source_object,
                                       GAsyncResult *res,
                                       gpointer user_data)
{
        GError *error = NULL;
        GDBusProxy *proxy;
        NMDevice *device = (NMDevice *) user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                g_error_free (error);
                goto out;
        }

        g_dbus_proxy_call (proxy,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           panel_get_registration_info_cb,
                           device);
out:
        if (proxy != NULL)
                g_object_unref (proxy);
        return;
}

static void
device_state_notify_changed_cb (NMDevice *device,
                                GParamSpec *pspec,
                                CcNetworkPanel *panel)
{
        refresh_ui (panel);
}

static void
object_changed_cb (NetObject *object, CcNetworkPanel *panel)
{
        refresh_ui (panel);
}

static void
object_removed_cb (NetObject *object, CcNetworkPanel *panel)
{
        gboolean ret;
        NetObject *object_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *widget;
        GtkTreeSelection *selection;

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "treeview_devices"));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

        /* remove device from model */
        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;

        /* get the other elements */
        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (g_strcmp0 (net_object_get_id (object),
                               net_object_get_id (object_tmp)) == 0) {
                        g_object_unref (object_tmp);
                        if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
                                gtk_tree_model_get_iter_first (model, &iter);
                        gtk_tree_selection_select_iter (selection, &iter);

                        break;
                }
                g_object_unref (object_tmp);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
register_object_interest (CcNetworkPanel *panel, NetObject *object)
{
        g_signal_connect (object,
                          "changed",
                          G_CALLBACK (object_changed_cb),
                          panel);
        g_signal_connect (object,
                          "removed",
                          G_CALLBACK (object_removed_cb),
                          panel);
}

static void
panel_refresh_killswitch_visibility (CcNetworkPanel *panel)
{
        gboolean ret;
        gboolean show_flight_toggle = FALSE;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NetObject *object_tmp;
        NMDeviceModemCapabilities caps;
        NMDevice *nm_device;

        /* find any wireless devices in model */
        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;
        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (NET_IS_DEVICE (object_tmp)) {
                        nm_device = net_device_get_nm_device (NET_DEVICE (object_tmp));
                        switch (nm_device_get_device_type (nm_device)) {
                        case NM_DEVICE_TYPE_WIFI:
                        case NM_DEVICE_TYPE_WIMAX:
                                show_flight_toggle = TRUE;
                                break;
                        case NM_DEVICE_TYPE_MODEM:
                                {
                                caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));
                                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                                    (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO))
                                        show_flight_toggle = TRUE;
                                }
                                break;
                        default:
                                break;
                        }
                }
                g_object_unref (object_tmp);
        } while (!show_flight_toggle && gtk_tree_model_iter_next (model, &iter));

        /* only show toggle if there are wireless devices */
        gtk_widget_set_visible (panel->priv->kill_switch_header,
                                show_flight_toggle);
}

static gboolean
panel_add_device (CcNetworkPanel *panel, NMDevice *device)
{
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        gchar *title = NULL;
        NMDeviceType type;
        NetDevice *net_device;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* do we have an existing object with this id? */
        if (find_in_model_by_id (panel, nm_device_get_udi (device)) != NULL)
                goto out;

        /* we don't support bluetooth devices yet -- no mockup */
        type = nm_device_get_device_type (device);

        if (type == NM_DEVICE_TYPE_BT)
                goto out;

        g_debug ("device %s type %i",
                 nm_device_get_udi (device),
                 nm_device_get_device_type (device));
        g_signal_connect (G_OBJECT (device), "notify::state",
                          (GCallback) device_state_notify_changed_cb, panel);

        /* do we have to get additonal data from ModemManager */
        if (type == NM_DEVICE_TYPE_MODEM) {
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          nm_device_get_udi (device),
                                          "org.freedesktop.ModemManager.Modem",
                                          panel->priv->cancellable,
                                          panel_device_got_modem_manager_cb,
                                          device);
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          nm_device_get_udi (device),
                                          "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                          panel->priv->cancellable,
                                          panel_device_got_modem_manager_gsm_cb,
                                          device);
        }

        /* make title a bit bigger */
        title = g_strdup_printf ("%s", panel_device_to_localized_string (device));

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        net_device = net_device_new ();
        net_device_set_nm_device (net_device, device);
        net_object_set_id (NET_OBJECT (net_device), nm_device_get_udi (device));
        register_object_interest (panel, NET_OBJECT (net_device));
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, panel_device_to_icon_name (device),
                            PANEL_DEVICES_COLUMN_SORT, panel_device_to_sortable_string (device),
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_OBJECT, net_device,
                            -1);

        if (priv->arg_operation != OPERATION_NULL) {
                if (type == NM_DEVICE_TYPE_WIFI &&
                    (priv->arg_operation == OPERATION_CREATE_WIFI ||
                     priv->arg_operation == OPERATION_CONNECT_HIDDEN)) {
                        g_debug ("Selecting wifi device");
                        select_tree_iter (panel, &iter);

                        if (priv->arg_operation == OPERATION_CREATE_WIFI)
                                cc_network_panel_create_wifi_network (panel, priv->client, priv->remote_settings);
                        else
                                cc_network_panel_connect_to_hidden_network (panel, priv->client, priv->remote_settings);

                        priv->arg_operation = OPERATION_NULL; /* done */
                        return TRUE;
                } else if (g_strcmp0 (nm_object_get_path (NM_OBJECT (device)), priv->arg_device) == 0) {
                        if (priv->arg_operation == OPERATION_CONNECT_MOBILE) {
                                cc_network_panel_connect_to_3g_network (panel, priv->client, priv->remote_settings, device);

                                priv->arg_operation = OPERATION_NULL; /* done */
                                select_tree_iter (panel, &iter);
                                return TRUE;
                        } else if (priv->arg_operation == OPERATION_CONNECT_8021X
                                   || priv->arg_operation == OPERATION_SHOW_DEVICE) {
                                select_tree_iter (panel, &iter);

                                /* 802.11 wireless stuff must be handled in add_access_point, but
                                   we still select the right page here, whereas if we're just showing
                                   the device, we're done right away */
                                if (priv->arg_operation == OPERATION_SHOW_DEVICE)
                                        priv->arg_operation = OPERATION_NULL;
                                return TRUE;
                        }
                }
        }

out:
        g_free (title);
        return FALSE;
}

static void
panel_remove_device (CcNetworkPanel *panel, NMDevice *device)
{
        gboolean ret;
        NetObject *object_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;

        /* remove device from model */
        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;

        /* get the other elements */
        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (g_strcmp0 (net_object_get_id (object_tmp),
                               nm_device_get_udi (device)) == 0) {
                        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
                        g_object_unref (object_tmp);
                        break;
                }
                g_object_unref (object_tmp);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
panel_add_devices_columns (CcNetworkPanel *panel, GtkTreeView *treeview)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        GtkCellRenderer *renderer;
        GtkListStore *liststore_devices;
        GtkTreeViewColumn *column;

        /* image */
        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "stock-size", gtk_icon_size_from_name ("cc-sidebar-list"), NULL);
        gtk_cell_renderer_set_padding (renderer, 4, 4);

        column = gtk_tree_view_column_new_with_attributes ("", renderer,
                                                           "icon-name", PANEL_DEVICES_COLUMN_ICON,
                                                           NULL);
        gtk_tree_view_append_column (treeview, column);

        /* column for text */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer,
                      "wrap-mode", PANGO_WRAP_WORD,
                      NULL);
        column = gtk_tree_view_column_new_with_attributes ("", renderer,
                                                           "markup", PANEL_DEVICES_COLUMN_TITLE,
                                                           NULL);
        gtk_tree_view_column_set_sort_column_id (column, PANEL_DEVICES_COLUMN_SORT);
        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore_devices),
                                              PANEL_DEVICES_COLUMN_SORT,
                                              GTK_SORT_ASCENDING);
        gtk_tree_view_append_column (treeview, column);
        gtk_tree_view_column_set_expand (column, TRUE);
}

static void
panel_set_widget_data (CcNetworkPanel *panel,
                       const gchar *sub_pane,
                       const gchar *widget_suffix,
                       const gchar *value)
{
        gchar *heading_id;
        gchar *label_id = NULL;
        GtkWidget *heading;
        GtkWidget *widget;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* hide the row if there is no value */
        heading_id = g_strdup_printf ("heading_%s_%s", sub_pane, widget_suffix);
        label_id = g_strdup_printf ("label_%s_%s", sub_pane, widget_suffix);
        heading = GTK_WIDGET (gtk_builder_get_object (priv->builder, heading_id));
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, label_id));
        if (heading == NULL || widget == NULL) {
                g_critical ("no widgets %s, %s found", heading_id, label_id);
                return;
        }
        g_free (heading_id);
        g_free (label_id);

        if (value == NULL) {
                gtk_widget_hide (heading);
                gtk_widget_hide (widget);
        } else {
                /* there exists a value */
                gtk_widget_show (heading);
                gtk_widget_show (widget);
                gtk_label_set_label (GTK_LABEL (widget), value);
        }
}

static void
panel_set_widget_heading (CcNetworkPanel *panel,
                          const gchar *sub_pane,
                          const gchar *widget_suffix,
                          const gchar *heading)
{
        gchar *label_id = NULL;
        GtkWidget *widget;

        label_id = g_strdup_printf ("heading_%s_%s", sub_pane, widget_suffix);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, label_id));
        if (widget)
                gtk_label_set_label (GTK_LABEL (widget), heading);
        g_free (label_id);
}

static guint
get_access_point_security (NMAccessPoint *ap)
{
        NM80211ApFlags flags;
        NM80211ApSecurityFlags wpa_flags;
        NM80211ApSecurityFlags rsn_flags;
        guint type;

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
add_access_point (CcNetworkPanel *panel, NMAccessPoint *ap, NMAccessPoint *active, NMDevice *device)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        const GByteArray *ssid;
        const gchar *ssid_text;
        const gchar *object_path;
        GtkListStore *liststore_wireless_network;
        GtkTreeIter treeiter;
        GtkWidget *widget;

        ssid = nm_access_point_get_ssid (ap);
        if (ssid == NULL)
                return;
        ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);

        liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                     "liststore_wireless_network"));

        object_path = nm_object_get_path (NM_OBJECT (ap));
        gtk_list_store_append (liststore_wireless_network, &treeiter);
        gtk_list_store_set (liststore_wireless_network,
                            &treeiter,
                            PANEL_WIRELESS_COLUMN_ID, object_path,
                            PANEL_WIRELESS_COLUMN_TITLE, ssid_text,
                            PANEL_WIRELESS_COLUMN_SORT, ssid_text,
                            PANEL_WIRELESS_COLUMN_STRENGTH, nm_access_point_get_strength (ap),
                            PANEL_WIRELESS_COLUMN_MODE, nm_access_point_get_mode (ap),
                            PANEL_WIRELESS_COLUMN_SECURITY, get_access_point_security (ap),
                            -1);

        /* is this what we're on already? */
        if (active && nm_utils_same_ssid (ssid, nm_access_point_get_ssid (active), TRUE)) {
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "combobox_wireless_network_name"));
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &treeiter);
        }

        if (priv->arg_operation == OPERATION_CONNECT_8021X &&
            g_strcmp0(priv->arg_device, nm_object_get_path (NM_OBJECT (device))) == 0 &&
            g_strcmp0(priv->arg_access_point, object_path) == 0) {
                cc_network_panel_connect_to_8021x_network (panel,
                                                           priv->client,
                                                           priv->remote_settings,
                                                           device,
                                                           ap);
                priv->arg_operation = OPERATION_NULL; /* done */
        }
}

static void
add_access_point_other (CcNetworkPanel *panel)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        GtkListStore *liststore_wireless_network;
        GtkTreeIter treeiter;

        liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                     "liststore_wireless_network"));

        gtk_list_store_append (liststore_wireless_network, &treeiter);
        gtk_list_store_set (liststore_wireless_network,
                            &treeiter,
                            PANEL_WIRELESS_COLUMN_ID, "ap-other...",
                            /* TRANSLATORS: this is when the access point is not listed
                             * in the dropdown (or hidden) and the user has to select
                             * another entry manually */
                            PANEL_WIRELESS_COLUMN_TITLE, C_("Wireless access point", "Other..."),
                            /* always last */
                            PANEL_WIRELESS_COLUMN_SORT, "",
                            PANEL_WIRELESS_COLUMN_STRENGTH, 0,
                            PANEL_WIRELESS_COLUMN_MODE, NM_802_11_MODE_UNKNOWN,
                            PANEL_WIRELESS_COLUMN_SECURITY, NM_AP_SEC_UNKNOWN,
                            -1);
}

#if 0
static gchar *
ip4_address_as_string (guint32 ip)
{
        char buf[INET_ADDRSTRLEN+1];
        struct in_addr tmp_addr;

        memset (&buf, '\0', sizeof (buf));
        tmp_addr.s_addr = ip;

        if (inet_ntop (AF_INET, &tmp_addr, buf, INET_ADDRSTRLEN)) {
                return g_strdup (buf);
        } else {
                g_warning ("error converting IP4 address 0x%X",
                           ntohl (tmp_addr.s_addr));
                return NULL;
        }
}

static void
panel_show_ip4_config (NMIP4Config *cfg)
{
        gchar *tmp;
        const GArray *array;
        const GPtrArray *ptr_array;
        GSList *iter;
        int i;

        for (iter = (GSList *) nm_ip4_config_get_addresses (cfg); iter; iter = g_slist_next (iter)) {
                NMIP4Address *addr = iter->data;
                guint32 u;

                tmp = ip4_address_as_string (nm_ip4_address_get_address (addr));
                g_debug ("IP4 address: %s", tmp);
                g_free (tmp);

                u = nm_ip4_address_get_prefix (addr);
                tmp = ip4_address_as_string (nm_utils_ip4_prefix_to_netmask (u));
                g_debug ("IP4 prefix: %d (%s)", u, tmp);
                g_free (tmp);

                tmp = ip4_address_as_string (nm_ip4_address_get_gateway (addr));
                g_debug ("IP4 gateway: %s", tmp);
                g_free (tmp);
        }

        array = nm_ip4_config_get_nameservers (cfg);
        if (array) {
                g_debug ("IP4 DNS:");
                for (i = 0; i < array->len; i++) {
                        tmp = ip4_address_as_string (g_array_index (array, guint32, i));
                        g_debug ("\t%s", tmp);
                        g_free (tmp);
                }
        }

        ptr_array = nm_ip4_config_get_domains (cfg);
        if (ptr_array) {
                g_debug ("IP4 domains:");
                for (i = 0; i < ptr_array->len; i++)
                        g_debug ("\t%s", (const char *) g_ptr_array_index (ptr_array, i));
        }

        array = nm_ip4_config_get_wins_servers (cfg);
        if (array) {
                g_debug ("IP4 WINS:");
                for (i = 0; i < array->len; i++) {
                        tmp = ip4_address_as_string (g_array_index (array, guint32, i));
                        g_debug ("\t%s", tmp);
                        g_free (tmp);
                }
        }
}
#endif

static GPtrArray *
panel_get_strongest_unique_aps (const GPtrArray *aps)
{
        const GByteArray *ssid;
        const GByteArray *ssid_tmp;
        GPtrArray *aps_unique = NULL;
        gboolean add_ap;
        guint i;
        guint j;
        NMAccessPoint *ap;
        NMAccessPoint *ap_tmp;

        /* we will have multiple entries for typical hotspots, just
         * filter to the one with the strongest signal */
        aps_unique = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
        if (aps != NULL)
                for (i = 0; i < aps->len; i++) {
                        ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));

                        /* Hidden SSIDs don't get shown in the list */
                        ssid = nm_access_point_get_ssid (ap);
                        if (!ssid)
                                continue;

                        add_ap = TRUE;

                        /* get already added list */
                        for (j=0; j<aps_unique->len; j++) {
                                ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (aps_unique, j));
                                ssid_tmp = nm_access_point_get_ssid (ap_tmp);
                                g_assert (ssid_tmp);
        
                                /* is this the same type and data? */
                                if (nm_utils_same_ssid (ssid, ssid_tmp, TRUE)) {

                                        g_debug ("found duplicate: %s",
                                                 nm_utils_escape_ssid (ssid_tmp->data,
                                                                       ssid_tmp->len));

                                        /* the new access point is stronger */
                                        if (nm_access_point_get_strength (ap) >
                                            nm_access_point_get_strength (ap_tmp)) {
                                                g_debug ("removing %s",
                                                         nm_utils_escape_ssid (ssid_tmp->data,
                                                                               ssid_tmp->len));
                                                g_ptr_array_remove (aps_unique, ap_tmp);
                                                add_ap = TRUE;
                                        } else {
                                                add_ap = FALSE;
                                        }

                                        break;
                                }
                        }
                        if (add_ap) {
                                g_debug ("adding %s",
                                         nm_utils_escape_ssid (ssid->data,
                                                               ssid->len));
                                g_ptr_array_add (aps_unique, g_object_ref (ap));
                        }
                }
        return aps_unique;
}

static gchar *
get_ap_security_string (NMAccessPoint *ap)
{
        NM80211ApSecurityFlags wpa_flags, rsn_flags;
        NM80211ApFlags flags;
        GString *str;

        flags = nm_access_point_get_flags (ap);
        wpa_flags = nm_access_point_get_wpa_flags (ap);
        rsn_flags = nm_access_point_get_rsn_flags (ap);

        str = g_string_new ("");
        if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
            (wpa_flags == NM_802_11_AP_SEC_NONE) &&
            (rsn_flags == NM_802_11_AP_SEC_NONE)) {
                /* TRANSLATORS: this WEP WiFi security */
                g_string_append_printf (str, "%s, ", _("WEP"));
        }
        if (wpa_flags != NM_802_11_AP_SEC_NONE) {
                /* TRANSLATORS: this WPA WiFi security */
                g_string_append_printf (str, "%s, ", _("WPA"));
        }
        if (rsn_flags != NM_802_11_AP_SEC_NONE) {
                /* TRANSLATORS: this WPA WiFi security */
                g_string_append_printf (str, "%s, ", _("WPA2"));
        }
        if ((wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) ||
            (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                /* TRANSLATORS: this Enterprise WiFi security */
                g_string_append_printf (str, "%s, ", _("Enterprise"));
        }
        if (str->len > 0)
                g_string_set_size (str, str->len - 2);
        else {
                /* TRANSLATORS: this no (!) WiFi security */
                g_string_append (str, _("None"));
        }
        return g_string_free (str, FALSE);
}

static gchar *
get_ipv4_config_address_as_string (NMIP4Config *ip4_config, const char *what)
{
        const GSList *list;
        struct in_addr addr;
        gchar *str = NULL;
        gchar tmp[INET_ADDRSTRLEN];
        NMIP4Address *address;

        /* get address */
        list = nm_ip4_config_get_addresses (ip4_config);
        if (list == NULL)
                goto out;

        /* we only care about one address */
        address = list->data;
        if (!strcmp (what, "address"))
                addr.s_addr = nm_ip4_address_get_address (address);
        else if (!strcmp (what, "gateway"))
                addr.s_addr = nm_ip4_address_get_gateway (address);
        else if (!strcmp (what, "netmask"))
                addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip4_address_get_prefix (address));
        else
                goto out;

        if (!inet_ntop (AF_INET, &addr, tmp, sizeof(tmp)))
                goto out;
        str = g_strdup (tmp);
out:
        return str;
}

static gchar *
get_ipv4_config_name_servers_as_string (NMIP4Config *ip4_config)
{
        const GArray *array;
        GString *dns;
        struct in_addr addr;
        gchar tmp[INET_ADDRSTRLEN];
        int i;

        dns = g_string_new (NULL);

        array = nm_ip4_config_get_nameservers (ip4_config);
        if (array) {
                for (i = 0; i < array->len; i++) {
                        addr.s_addr = g_array_index (array, guint32, i);
                        if (inet_ntop (AF_INET, &addr, tmp, sizeof(tmp)))
                                g_string_append_printf (dns, "%s ", tmp);
                }
        }

        return g_string_free (dns, FALSE);
}

static gchar *
get_ipv6_config_address_as_string (NMIP6Config *ip6_config)
{
        const GSList *list;
        const struct in6_addr *addr;
        gchar *str = NULL;
        gchar tmp[INET6_ADDRSTRLEN];
        NMIP6Address *address;

        /* get address */
        list = nm_ip6_config_get_addresses (ip6_config);
        if (list == NULL)
                goto out;

        /* we only care about one address */
        address = list->data;
        addr = nm_ip6_address_get_address (address);
        if (addr == NULL)
                goto out;
        inet_ntop (AF_INET6, addr, tmp, sizeof(tmp));
        str = g_strdup (tmp);
out:
        return str;
}

static NMConnection *
find_connection_for_device (CcNetworkPanel *panel,
                            NMDevice       *device)
{
        const GPtrArray *connections;
        const GPtrArray *devices;
        NMActiveConnection *c;
        gint i;

        connections = nm_client_get_active_connections (panel->priv->client);
        if (connections == NULL) {
                return NULL;
        }

        for (i = 0; i < connections->len; i++) {
                c = (NMActiveConnection *)connections->pdata[i];

                devices = nm_active_connection_get_devices (c);
                if (devices && devices->pdata[0] == device) {
                        return (NMConnection *)nm_remote_settings_get_connection_by_path (panel->priv->remote_settings, nm_active_connection_get_connection (c));
                }
        }

        return NULL;
}

static void
device_off_toggled (GtkSwitch      *sw,
                    GParamSpec     *pspec,
                    CcNetworkPanel *panel)
{
        NMDevice *device;
        gboolean active;
        NetObject *object;

        if (panel->priv->updating_device)
                return;

        active = gtk_switch_get_active (sw);

        object = get_selected_object (panel);
        if (NET_IS_VPN (object)) {

                NMConnection *connection;

                connection = net_vpn_get_connection (NET_VPN (object));
                if (active)
                        nm_client_activate_connection (panel->priv->client,
                                                       connection, NULL, NULL,
                                                       NULL, NULL);
                else {
                        const gchar *path;
                        NMActiveConnection *a;
                        const GPtrArray *acs;
                        gint i;

                        path = nm_connection_get_path (connection);

                        acs = nm_client_get_active_connections (panel->priv->client);
                        for (i = 0; i < acs->len; i++) {
                                a = (NMActiveConnection*)acs->pdata[i];
                                if (strcmp (nm_active_connection_get_connection (a), path) == 0) {
                                        nm_client_deactivate_connection (panel->priv->client, a);
                                        break;
                                }
                        }
                }
        }

        if (NET_IS_DEVICE (object)) {
                device = net_device_get_nm_device (NET_DEVICE (object));
                switch (nm_device_get_device_type (device)) {
                case NM_DEVICE_TYPE_ETHERNET:
                        if (active) {
                                GSList *list, *filtered;

                                /* look for an existing connection we can use */
                                list = nm_remote_settings_list_connections (panel->priv->remote_settings);
                                filtered = nm_device_filter_connections (device, list);
                                if (filtered) {
                                        nm_client_activate_connection (panel->priv->client,
                                                                       (NMConnection *)filtered->data,
                                                                       device,
                                                                       NULL,
                                                                       NULL, NULL);
                                } else {
                                        nm_client_add_and_activate_connection (panel->priv->client,
                                                                               NULL,
                                                                               device,
                                                                               NULL,
                                                                               NULL, NULL);
                                }

                                g_slist_free (list);
                                g_slist_free (filtered);
                        } else {
                                nm_device_disconnect (device, NULL, NULL);
                        }
                        break;
                case NM_DEVICE_TYPE_WIFI:
                        nm_client_wireless_set_enabled (panel->priv->client, active);
                        break;
                case NM_DEVICE_TYPE_WIMAX:
                        nm_client_wimax_set_enabled (panel->priv->client, active);
                        break;
                case NM_DEVICE_TYPE_MODEM:
                        nm_client_wwan_set_enabled (panel->priv->client, active);
                        break;
                default: ;
                        /* FIXME: handle other device types */
                }
        }
}

static void
wireless_enabled_toggled (NMClient       *client,
                          GParamSpec     *pspec,
                          CcNetworkPanel *panel)
{
        gboolean enabled;
        GtkSwitch *sw;
        NMDevice *device;
        NetObject *object;

        object = get_selected_object (panel);
        if (object == NULL)
                return;
        device = net_device_get_nm_device (NET_DEVICE (object));

        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_WIFI)
                return;

        enabled = nm_client_wireless_get_enabled (client);
        sw = GTK_SWITCH (gtk_builder_get_object (panel->priv->builder,
                                                 "device_wireless_off_switch"));

        panel->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        panel->priv->updating_device = FALSE;
}

static void
wimax_enabled_toggled (NMClient       *client,
                       GParamSpec     *pspec,
                       CcNetworkPanel *panel)
{
        gboolean enabled;
        GtkSwitch *sw;
        NMDevice *device;
        NetObject *object;

        object = get_selected_object (panel);
        if (object == NULL)
                return;
        device = net_device_get_nm_device (NET_DEVICE (object));

        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_WIMAX)
                return;

        enabled = nm_client_wimax_get_enabled (client);
        sw = GTK_SWITCH (gtk_builder_get_object (panel->priv->builder,
                                                 "device_wimax_off_switch"));

        panel->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        panel->priv->updating_device = FALSE;
}

static void
mobilebb_enabled_toggled (NMClient       *client,
                          GParamSpec     *pspec,
                          CcNetworkPanel *panel)
{
        gboolean enabled;
        GtkSwitch *sw;
        NMDevice *device;
        NetObject *object;

        object = get_selected_object (panel);
        if (object == NULL)
                return;
        device = net_device_get_nm_device (NET_DEVICE (object));

        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_MODEM)
                return;

        enabled = nm_client_wwan_get_enabled (client);
        sw = GTK_SWITCH (gtk_builder_get_object (panel->priv->builder,
                                                 "device_mobilebb_off_switch"));

        panel->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        panel->priv->updating_device = FALSE;
}

static void
update_off_switch_from_device_state (GtkSwitch *sw, NMDeviceState state, CcNetworkPanel *panel)
{
        panel->priv->updating_device = TRUE;
        switch (state) {
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                        gtk_switch_set_active (sw, FALSE);
                        break;
                default:
                        gtk_switch_set_active (sw, TRUE);
                        break;
        }
        panel->priv->updating_device = FALSE;
}

static gboolean
device_is_hotspot (CcNetworkPanel *panel,
                   NMDevice *device)
{
        NMConnection *c;
        NMSettingIP4Config *s_ip4;

        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_WIFI) {
                return FALSE;
        }

        c = find_connection_for_device (panel, device);
        if (c == NULL) {
                return FALSE;
        }

        s_ip4 = nm_connection_get_setting_ip4_config (c);
        if (g_strcmp0 (nm_setting_ip4_config_get_method (s_ip4),
                       NM_SETTING_IP4_CONFIG_METHOD_SHARED) != 0) {
                return FALSE;
        }

        return TRUE;
}

static const GByteArray *
device_get_hotspot_ssid (CcNetworkPanel *panel,
                         NMDevice *device)
{
        NMConnection *c;
        NMSettingWireless *sw;

        c = find_connection_for_device (panel, device);
        if (c == NULL) {
                return FALSE;
        }

        sw = nm_connection_get_setting_wireless (c);
        return nm_setting_wireless_get_ssid (sw);
}

static void
get_secrets_cb (NMRemoteConnection *c,
                GHashTable         *secrets,
                GError             *error,
                gpointer            data)
{
        CcNetworkPanel *panel = data;
        NMSettingWireless *sw;

        sw = nm_connection_get_setting_wireless (NM_CONNECTION (c));

        nm_connection_update_secrets (NM_CONNECTION (c),
                                      nm_setting_wireless_get_security (sw),
                                      secrets, NULL);

        refresh_ui (panel);
}

static void
device_get_hotspot_security_details (CcNetworkPanel *panel,
                                     NMDevice *device,
                                     gchar **secret,
                                     gchar **security)
{
        NMConnection *c;
        NMSettingWireless *sw;
        NMSettingWirelessSecurity *sws;
        const gchar *key_mgmt;
        const gchar *tmp_secret;
        const gchar *tmp_security;

        c = find_connection_for_device (panel, device);
        if (c == NULL) {
                return;
        }

        sw = nm_connection_get_setting_wireless (c);
        sws = nm_connection_get_setting_wireless_security (c);
        if (sw == NULL || sws == NULL) {
                return;
        }

        tmp_secret = NULL;
        tmp_security = _("None");

        key_mgmt = nm_setting_wireless_security_get_key_mgmt (sws);
        if (strcmp (key_mgmt, "none") == 0) {
                tmp_secret = nm_setting_wireless_security_get_wep_key (sws, 0);
                tmp_security = _("WEP");
        }
        else if (strcmp (key_mgmt, "wpa-none") == 0) {
                tmp_secret = nm_setting_wireless_security_get_psk (sws);
                tmp_security = _("WPA");
        } else {
                g_warning ("unhandled security key-mgmt: %s", key_mgmt);
        }

        /* If we don't have secrets, request them from NM and bail.
         * We'll refresh the UI when secrets arrive.
         */
        if (tmp_secret == NULL) {
                nm_remote_connection_get_secrets ((NMRemoteConnection*)c,
                                                  nm_setting_wireless_get_security (sw),
                                                  get_secrets_cb,
                                                  panel);
                return;
        }

        if (secret) {
                *secret = g_strdup (tmp_secret);
        }

        if (security) {
                *security = g_strdup (tmp_security);
        }
}

static void
refresh_header_ui (CcNetworkPanel *panel, NMDevice *device, const char *page_name)
{
        GtkWidget *widget;
        char *wid_name;
        GString *str;
        NMDeviceState state;
        NMDeviceType type;
        gboolean is_hotspot;
        guint speed = 0;

        type = nm_device_get_device_type (device);
        state = nm_device_get_state (device);
        is_hotspot = device_is_hotspot (panel, device);

        /* set header icon */
        wid_name = g_strdup_printf ("image_%s_device", page_name);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, wid_name));
        g_free (wid_name);
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      panel_device_to_icon_name (device),
                                      GTK_ICON_SIZE_DIALOG);

        /* set device kind */
        wid_name = g_strdup_printf ("label_%s_device", page_name);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, wid_name));
        g_free (wid_name);
        gtk_label_set_label (GTK_LABEL (widget),
                             panel_device_to_localized_string (device));

        /* set up the device on/off switch */
        wid_name = g_strdup_printf ("device_%s_off_switch", page_name);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, wid_name));
        g_free (wid_name);

        /* keep this in sync with the signal handler setup in cc_network_panel_init */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
                gtk_widget_set_visible (widget,
                                        state != NM_DEVICE_STATE_UNAVAILABLE
                                        && state != NM_DEVICE_STATE_UNMANAGED);
                update_off_switch_from_device_state (GTK_SWITCH (widget), state, panel);
                if (state != NM_DEVICE_STATE_UNAVAILABLE)
                        speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (device));
                break;
        case NM_DEVICE_TYPE_WIFI:
                gtk_widget_show (widget);
                wireless_enabled_toggled (panel->priv->client, NULL, panel);
                if (state != NM_DEVICE_STATE_UNAVAILABLE)
                        speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (device));
                speed /= 1000;
                break;
        case NM_DEVICE_TYPE_WIMAX:
                gtk_widget_show (widget);
                wimax_enabled_toggled (panel->priv->client, NULL, panel);
                break;
        case NM_DEVICE_TYPE_MODEM:
                gtk_widget_show (widget);
                mobilebb_enabled_toggled (panel->priv->client, NULL, panel);
                break;
        default:
                gtk_widget_hide (widget);
                break;
        }

        /* set device state, with status and optionally speed */
        wid_name = g_strdup_printf ("label_%s_status", page_name);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, wid_name));
        g_free (wid_name);
        if (is_hotspot) {
                str = g_string_new (_("Hotspot"));
        } else {
                str = g_string_new (panel_device_state_to_localized_string (device));
        }
        if (speed  > 0) {
                g_string_append (str, " - ");
                /* Translators: network device speed */
                g_string_append_printf (str, _("%d Mb/s"), speed);
        }
        gtk_label_set_label (GTK_LABEL (widget), str->str);

        /* set up options button */
        wid_name = g_strdup_printf ("button_%s_options", page_name);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, wid_name));
        g_free (wid_name);
        if (widget != NULL) {
                gtk_widget_set_sensitive (widget, find_connection_for_device (panel, device) != NULL);
        }
        g_string_free (str, TRUE);
}

static void
device_refresh_ethernet_ui (CcNetworkPanel *panel, NetDevice *device)
{
        const char *str;
        NMDevice *nm_device;

        nm_device = net_device_get_nm_device (device);

        refresh_header_ui (panel, nm_device, "wired");

        /* device MAC */
        str = nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (nm_device));
        panel_set_widget_data (panel,
                               "wired",
                               "mac",
                               str);

}

static void
device_refresh_wifi_ui (CcNetworkPanel *panel, NetDevice *device)
{
        GtkWidget *widget;
        GtkWidget *sw;
        const GPtrArray *aps;
        GPtrArray *aps_unique = NULL;
        GtkWidget *heading;
        NMDeviceState state;
        NMAccessPoint *ap;
        NMAccessPoint *active_ap;
        const char *str;
        gchar *str_tmp;
        GtkListStore *liststore_wireless_network;
        guint i;
        NMDevice *nm_device;
        NMClientPermissionResult perm;
        gboolean is_hotspot;
        gchar *hotspot_ssid;
        gchar *hotspot_secret;
        gchar *hotspot_security;
        gboolean can_start_hotspot;

        nm_device = net_device_get_nm_device (device);
        state = nm_device_get_state (nm_device);

        refresh_header_ui (panel, nm_device, "wireless");

        /* sort out hotspot ui */
        is_hotspot = device_is_hotspot (panel, nm_device);
        hotspot_ssid = NULL;
        hotspot_secret = NULL;
        hotspot_security = NULL;
        if (is_hotspot) {
                const GByteArray *ssid;
                ssid = device_get_hotspot_ssid (panel, nm_device);
                if (ssid) {
                        hotspot_ssid = nm_utils_ssid_to_utf8 (ssid);
                }
                device_get_hotspot_security_details (panel, nm_device, &hotspot_secret, &hotspot_security);
        }

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "start_hotspot_button"));
        gtk_widget_set_visible (widget, !is_hotspot);

        sw = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                 "device_wireless_off_switch"));
        perm = nm_client_get_permission_result (panel->priv->client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
        can_start_hotspot = gtk_switch_get_active (GTK_SWITCH (sw)) &&
                            (perm == NM_CLIENT_PERMISSION_RESULT_YES ||
                             perm == NM_CLIENT_PERMISSION_RESULT_AUTH);
        gtk_widget_set_sensitive (widget, can_start_hotspot);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "stop_hotspot_button"));
        gtk_widget_set_visible (widget, is_hotspot);

        panel_set_widget_data (panel, "hotspot", "network_name", hotspot_ssid);
        g_free (hotspot_ssid);

        panel_set_widget_data (panel, "hotspot", "security_key", hotspot_secret);
        g_free (hotspot_secret);

        /* device MAC */
        str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (nm_device));
        panel_set_widget_data (panel,
                               "wireless",
                               "mac",
                               str);
        /* security */
        active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (nm_device));
        if (state == NM_DEVICE_STATE_UNAVAILABLE)
                str_tmp = NULL;
        else if (is_hotspot)
                str_tmp = hotspot_security;
        else if (active_ap != NULL)
                str_tmp = get_ap_security_string (active_ap);
        else
                str_tmp = g_strdup ("");
        panel_set_widget_data (panel,
                               "wireless",
                               "security",
                               str_tmp);
        g_free (str_tmp);

        heading = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                      "heading_wireless_network_name"));
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "combobox_wireless_network_name"));
        /* populate access point dropdown */
        if (is_hotspot || state == NM_DEVICE_STATE_UNAVAILABLE) {
                gtk_widget_hide (heading);
                gtk_widget_hide (widget);
        } else {
                gtk_widget_show (heading);
                gtk_widget_show (widget);
                liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                                                                     "liststore_wireless_network"));
                panel->priv->updating_device = TRUE;
                gtk_list_store_clear (liststore_wireless_network);
                aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (nm_device));
                aps_unique = panel_get_strongest_unique_aps (aps);

                for (i = 0; i < aps_unique->len; i++) {
                        ap = NM_ACCESS_POINT (g_ptr_array_index (aps_unique, i));
                        add_access_point (panel, ap, active_ap, nm_device);
                }
                add_access_point_other (panel);
                if (active_ap == NULL) {
                        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                                     "combobox_wireless_network_name"));
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), NULL);
                        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))), "");
                }

                panel->priv->updating_device = FALSE;

                g_ptr_array_unref (aps_unique);
        }

        /* setup wireless button */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_wireless_button"));
        gtk_widget_set_visible (widget, active_ap != NULL);
}

static void
device_refresh_wimax_ui (CcNetworkPanel *panel, NetDevice *device)
{
        NMDevice *nm_device;

        nm_device = net_device_get_nm_device (device);
        refresh_header_ui (panel, nm_device, "wimax");
}

static void
device_refresh_modem_ui (CcNetworkPanel *panel, NetDevice *device)
{
        NMDeviceModemCapabilities caps;
        NMDevice *nm_device;
        const char *str;

        nm_device = net_device_get_nm_device (device);

        refresh_header_ui (panel, nm_device, "mobilebb");

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));

        if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                /* IMEI */
                str = g_object_get_data (G_OBJECT (nm_device),
                                         "ControlCenter::EquipmentIdentifier");
                panel_set_widget_data (panel,
                                       "mobilebb",
                                       "imei",
                                       str);

                /* operator name */
                str = g_object_get_data (G_OBJECT (nm_device),
                                         "ControlCenter::OperatorName");
                panel_set_widget_data (panel,
                                       "mobilebb",
                                       "provider",
                                       str);

                /* device speed */
                panel_set_widget_data (panel,
                                       "mobilebb",
                                       "speed",
                                       NULL);
        }
}

static void
nm_device_refresh_device_ui (CcNetworkPanel *panel, NetDevice *device)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        const gchar *sub_pane = NULL;
        gchar *str_tmp;
        GtkWidget *widget;
        NMDeviceType type;
        NMIP4Config *ip4_config = NULL;
        NMIP6Config *ip6_config = NULL;
        NMDevice *nm_device;
        gboolean has_ip4;
        gboolean has_ip6;
        gboolean is_hotspot;

        /* we have a new device */
        nm_device = net_device_get_nm_device (device);
        type = nm_device_get_device_type (nm_device);
        is_hotspot = device_is_hotspot (panel, nm_device);
        g_debug ("device %s type %i", nm_device_get_udi (nm_device), type);

        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "notebook_types"));

        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
                sub_pane = "wired";
                device_refresh_ethernet_ui (panel, device);
                break;
        case NM_DEVICE_TYPE_WIFI:
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
                sub_pane = "wireless";
                device_refresh_wifi_ui (panel, device);
                break;
        case NM_DEVICE_TYPE_WIMAX:
                device_refresh_wimax_ui (panel, device);
                break;
        case NM_DEVICE_TYPE_MODEM:
                {
                        NMDeviceModemCapabilities caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));
                        if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                            (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 4);
                                sub_pane = "mobilebb";
                                device_refresh_modem_ui (panel, device);
                        }
                }
                break;
        default:
                g_assert_not_reached ();
                break;
        }

        if (sub_pane == NULL)
                goto out;

        /* get IP4 parameters */
        ip4_config = nm_device_get_ip4_config (nm_device);
        if (!is_hotspot && ip4_config != NULL) {
                /* IPv4 address */

                str_tmp = get_ipv4_config_address_as_string (ip4_config, "address");
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "ipv4",
                                       str_tmp);
                has_ip4 = str_tmp != NULL;
                g_free (str_tmp);

                /* IPv4 DNS */
                str_tmp = get_ipv4_config_name_servers_as_string (ip4_config);
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "dns",
                                       str_tmp);
                g_free (str_tmp);

                /* IPv4 route */
                str_tmp = get_ipv4_config_address_as_string (ip4_config, "gateway");
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "route",
                                       str_tmp);
                g_free (str_tmp);

                /* IPv4 netmask */
                if (type == NM_DEVICE_TYPE_ETHERNET) {
                        str_tmp = get_ipv4_config_address_as_string (ip4_config, "netmask");
                        panel_set_widget_data (panel,
                                               sub_pane,
                                               "subnet",
                                               str_tmp);
                        g_free (str_tmp);
                }
        } else {
                /* IPv4 address */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "ipv4",
                                       NULL);
                has_ip4 = FALSE;

                /* IPv4 DNS */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "dns",
                                       NULL);

                /* IPv4 route */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "route",
                                       NULL);

                /* IPv4 netmask */
                if (type == NM_DEVICE_TYPE_ETHERNET) {
                        panel_set_widget_data (panel,
                                               sub_pane,
                                               "subnet",
                                               NULL);
                }
        }

        /* get IP6 parameters */
        ip6_config = nm_device_get_ip6_config (nm_device);
        if (!is_hotspot && ip6_config != NULL) {

                /* IPv6 address */
                str_tmp = get_ipv6_config_address_as_string (ip6_config);
                panel_set_widget_data (panel, sub_pane, "ipv6", str_tmp);
                has_ip6 = str_tmp != NULL;
                g_free (str_tmp);
        } else {
                panel_set_widget_data (panel, sub_pane, "ipv6", NULL);
                has_ip6 = FALSE;
        }

        if (has_ip4 && has_ip6) {
                panel_set_widget_heading (panel, sub_pane, "ipv4", _("IPv4 Address"));
                panel_set_widget_heading (panel, sub_pane, "ipv6", _("IPv6 Address"));
        }
        else if (has_ip4) {
                panel_set_widget_heading (panel, sub_pane, "ipv4", _("IP Address"));
        }
        else if (has_ip6) {
                panel_set_widget_heading (panel, sub_pane, "ipv6", _("IP Address"));
        }
out: ;
}

static void
nm_device_refresh_vpn_ui (CcNetworkPanel *panel, NetVpn *vpn)
{
        GtkWidget *widget;
        GtkWidget *sw;
        const gchar *sub_pane = "vpn";
        const gchar *status;
        CcNetworkPanelPrivate *priv = panel->priv;
        const GPtrArray *acs;
        NMActiveConnection *a;
        gint i;
        const gchar *path;
        const gchar *apath;
        NMVPNConnectionState state;
        gchar *title;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;

        sw = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                 "device_vpn_off_switch"));
        gtk_widget_set_visible (sw, TRUE);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_vpn_options"));
        gtk_widget_set_visible (widget, TRUE);
        gtk_widget_set_sensitive (widget, TRUE);

        /* use proxy note page */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "notebook_types"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 3);

        /* set VPN icon */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "image_vpn_device"));
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      "network-vpn",
                                      GTK_ICON_SIZE_DIALOG);

        /* update title */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_vpn_device"));
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (net_vpn_get_connection (vpn)));
        net_object_set_title (NET_OBJECT (vpn), title);
        gtk_label_set_label (GTK_LABEL (widget), title);

        /* update list store title */
        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                                                    "liststore_devices"));
        if (find_model_iter_by_object (GTK_TREE_MODEL (liststore_devices), NET_OBJECT (vpn), &iter)) {
                gtk_list_store_set (liststore_devices,
                                    &iter,
                                    PANEL_DEVICES_COLUMN_TITLE, title,
                                    -1);
        }
        g_free (title);

        /* use status */
        state = net_vpn_get_state (vpn);

        acs = nm_client_get_active_connections (priv->client);
        if (acs != NULL) {
                path = nm_connection_get_path (net_vpn_get_connection (vpn));
                for (i = 0; i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];

                        apath = nm_active_connection_get_connection (a);
                        if (NM_IS_VPN_CONNECTION (a) && strcmp (apath, path) == 0) {
                                state = nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (a));
                                break;
                        }
                }
        }

        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_vpn_status"));
        status = panel_vpn_state_to_localized_string (state);
        gtk_label_set_label (GTK_LABEL (widget), status);
        priv->updating_device = TRUE;
        gtk_switch_set_active (GTK_SWITCH (sw),
                               state != NM_VPN_CONNECTION_STATE_FAILED &&
                               state != NM_VPN_CONNECTION_STATE_DISCONNECTED);
        priv->updating_device = FALSE;

        /* service type */
        panel_set_widget_data (panel,
                               sub_pane,
                               "service_type",
                               net_vpn_get_service_type (vpn));

        /* gateway */
        panel_set_widget_data (panel,
                               sub_pane,
                               "gateway",
                               net_vpn_get_gateway (vpn));

        /* groupname */
        panel_set_widget_data (panel,
                               sub_pane,
                               "group_name",
                               net_vpn_get_id (vpn));

        /* username */
        panel_set_widget_data (panel,
                               sub_pane,
                               "username",
                               net_vpn_get_username (vpn));

        /* password */
        panel_set_widget_data (panel,
                               sub_pane,
                               "group_password",
                               net_vpn_get_password (vpn));
}

static gboolean
refresh_ui_idle (gpointer data)
{
        CcNetworkPanel *panel = data;
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *widget;
        NetObject *object = NULL;
        CcNetworkPanelPrivate *priv = panel->priv;

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "treeview_devices"));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

        /* will only work in single or browse selection mode! */
        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                g_debug ("no row selected");
                goto out;
        }

        object = get_selected_object (panel);

        /* this is the proxy settings device */
        if (object == NULL) {

                /* set header to something sane */
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "image_proxy_device"));
                gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                              "preferences-system-network",
                                              GTK_ICON_SIZE_DIALOG);
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "label_proxy_device"));
                gtk_label_set_label (GTK_LABEL (widget),
                                     _("Proxy"));
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "label_proxy_status"));
                gtk_label_set_label (GTK_LABEL (widget), "");

                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "notebook_types"));
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 2);

                /* hide the switch until we get some more detail in the mockup */
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "device_proxy_off_switch"));
                if (widget != NULL)
                        gtk_widget_hide (widget);

                /* we shoulnd't be able to delete the proxy device */
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "remove_toolbutton"));
                gtk_widget_set_sensitive (widget, FALSE);
                goto out;
        }

        /* VPN */
        if (NET_IS_VPN (object)) {
                nm_device_refresh_vpn_ui (panel, NET_VPN (object));

                /* we're able to remove the VPN connection */
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "remove_toolbutton"));
                gtk_widget_set_sensitive (widget, TRUE);
                goto out;
        }

        /* device */
        if (NET_IS_DEVICE (object)) {

                /* we're not yet able to remove the connection */
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "remove_toolbutton"));
                gtk_widget_set_sensitive (widget, FALSE);

                /* refresh device */
                nm_device_refresh_device_ui (panel, NET_DEVICE (object));
        }
out:
        priv->refresh_idle = 0;

        return FALSE;
}

static void
refresh_ui (CcNetworkPanel *panel)
{
        if (panel->priv->refresh_idle != 0)
                return;

        panel->priv->refresh_idle = g_idle_add (refresh_ui_idle, panel);
}

static void
nm_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
        refresh_ui (panel);
}

static void
panel_add_proxy_device (CcNetworkPanel *panel)
{
        gchar *title;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                            "liststore_devices"));
        title = g_strdup_printf ("%s", _("Network proxy"));

        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "preferences-system-network",
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_SORT, "9",
                            PANEL_DEVICES_COLUMN_OBJECT, NULL,
                            -1);
        g_free (title);
}

static void
cc_network_panel_notify_enable_active_cb (GtkSwitch *sw,
                                          GParamSpec *pspec,
                                          CcNetworkPanel *panel)
{
        gboolean enable;

        /* set enabled state */
        enable = !gtk_switch_get_active (sw);
        nm_client_wireless_set_enabled (panel->priv->client, enable);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, CcNetworkPanel *panel)
{
        refresh_ui (panel);
}

static void
active_connections_changed (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
        CcNetworkPanel *panel = user_data;
        const GPtrArray *connections;
        int i, j;

        g_debug ("Active connections changed:");
        connections = nm_client_get_active_connections (client);
        for (i = 0; connections && (i < connections->len); i++) {
                NMActiveConnection *connection;
                const GPtrArray *devices;

                connection = g_ptr_array_index (connections, i);
                g_debug ("    %s", nm_object_get_path (NM_OBJECT (connection)));
                devices = nm_active_connection_get_devices (connection);
                for (j = 0; devices && j < devices->len; j++)
                        g_debug ("           %s", nm_device_get_udi (g_ptr_array_index (devices, j)));
                if (NM_IS_VPN_CONNECTION (connection))
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object (connection));

                if (g_object_get_data (G_OBJECT (connection), "has-state-changed-handler") == NULL) {
                        g_signal_connect_object (connection, "notify::state",
                                                 G_CALLBACK (connection_state_changed), panel, 0);
                        g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (TRUE));
                }
        }

        refresh_ui (panel);
}

static void
device_added_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("New device added");
        panel_add_device (panel, device);
        panel_refresh_killswitch_visibility (panel);
}

static void
device_removed_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("Device removed");
        panel_remove_device (panel, device);
        panel_refresh_killswitch_visibility (panel);
}

static void
manager_running (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
        const GPtrArray *devices;
        int i;
        NMDevice *device_tmp;
        GtkListStore *liststore_devices;
        gboolean selected = FALSE;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        /* clear all devices we added */
        if (!nm_client_get_manager_running (client)) {
                g_debug ("NM disappeared");
                liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                                    "liststore_devices"));
                gtk_list_store_clear (liststore_devices);
                panel_add_proxy_device (panel);
                goto out;
        }

        g_debug ("coldplugging devices");
        devices = nm_client_get_devices (client);
        if (devices == NULL) {
                g_debug ("No devices to add");
                return;
        }
        for (i = 0; i < devices->len; i++) {
                device_tmp = g_ptr_array_index (devices, i);
                selected = panel_add_device (panel, device_tmp) || selected;
        }
out:
        if (!selected) {
                /* select the first device */
                select_first_device (panel);
        }
}

static NetObject *
find_in_model_by_id (CcNetworkPanel *panel, const gchar *id)
{
        gboolean ret;
        NetObject *object_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NetObject *object = NULL;

        /* find in model */
        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                goto out;

        /* get the other elements */
        ret = FALSE;
        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (object_tmp != NULL) {
                        g_debug ("got %s", net_object_get_id (object_tmp));
                        if (g_strcmp0 (net_object_get_id (object_tmp), id) == 0)
                                object = object_tmp;
                        g_object_unref (object_tmp);
                }
        } while (object == NULL && gtk_tree_model_iter_next (model, &iter));
out:
        return object;
}

static gboolean
find_model_iter_by_object (GtkTreeModel *model, const NetObject *object, GtkTreeIter *iter)
{
        gboolean valid;
        NetObject *object_tmp;

        /* find iter in model according to the passed object */
        valid = gtk_tree_model_get_iter_first (model, iter);
        while (valid) {
                gtk_tree_model_get (model, iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (object_tmp != NULL)
                        g_object_unref (object_tmp);
                if (object_tmp == object)
                        return TRUE;
                valid = gtk_tree_model_iter_next (model, iter);
        }

        return FALSE;
}

static void
panel_add_vpn_device (CcNetworkPanel *panel, NMConnection *connection)
{
        gchar *title;
        gchar *title_markup;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NetVpn *net_vpn;
        const gchar *id;

        /* does already exist */
        id = nm_connection_get_path (connection);
        if (find_in_model_by_id (panel, id) != NULL)
                return;

        /* add as a virtual object */
        net_vpn = net_vpn_new ();
        net_vpn_set_connection (net_vpn, connection);
        net_object_set_id (NET_OBJECT (net_vpn), id);
        register_object_interest (panel, NET_OBJECT (net_vpn));

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                            "liststore_devices"));
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (connection));
        title_markup = g_strdup (title);

        net_object_set_title (NET_OBJECT (net_vpn), title);
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "network-vpn",
                            PANEL_DEVICES_COLUMN_TITLE, title_markup,
                            PANEL_DEVICES_COLUMN_SORT, "5",
                            PANEL_DEVICES_COLUMN_OBJECT, net_vpn,
                            -1);
        g_free (title);
        g_free (title_markup);
}

static void
add_connection (CcNetworkPanel *panel,
                NMConnection *connection)
{
        NMSettingConnection *s_con;
        const gchar *type;

        s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection,
                                                                  NM_TYPE_SETTING_CONNECTION));
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_strcmp0 (type, "vpn") != 0)
                return;
        g_debug ("add %s/%s remote connection: %s",
                 type, g_type_name_from_instance ((GTypeInstance*)connection),
                 nm_connection_get_path (connection));
        panel_add_vpn_device (panel, connection);
}

static void
notify_new_connection_cb (NMRemoteSettings *settings,
                          NMRemoteConnection *connection,
                          CcNetworkPanel *panel)
{
        add_connection (panel, NM_CONNECTION (connection));
}

static void
notify_connections_read_cb (NMRemoteSettings *settings,
                            CcNetworkPanel *panel)
{
        GSList *list, *iter;
        NMConnection *connection;

        list = nm_remote_settings_list_connections (settings);
        g_debug ("%p has %i remote connections",
                 panel, g_slist_length (list));
        for (iter = list; iter; iter = g_slist_next (iter)) {
                connection = NM_CONNECTION (iter->data);
                add_connection (panel, connection);
        }
}

static gboolean
display_version_warning_idle (CcNetworkPanel *panel)
{
        GtkWidget  *dialog;
        GtkWidget  *image;
        GtkWindow  *window;
        const char *message;

        /* TRANSLATORS: the user is running a NM that is not API compatible */
        message = _("The system network services are not compatible with this version.");

        window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel)));
        dialog = gtk_message_dialog_new (window,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s",
                                         message);
        image = gtk_image_new_from_icon_name ("computer-fail", GTK_ICON_SIZE_DIALOG);
        gtk_widget_show (image);
        gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        return FALSE;
}

static gboolean
panel_check_network_manager_version (CcNetworkPanel *panel)
{
        const gchar *version;
        gchar **split = NULL;
        guint major = 0;
        guint micro = 0;
        guint minor = 0;
        gboolean ret = TRUE;

        /* parse running version */
        version = nm_client_get_version (panel->priv->client);
        if (version != NULL) {
                split = g_strsplit (version, ".", -1);
                major = atoi (split[0]);
                minor = atoi (split[1]);
                micro = atoi (split[2]);
        }

        /* is it too new or old */
        if (major > 0 || major > 9 || (minor <= 8 && micro < 992)) {
                ret = FALSE;

                /* do modal dialog in idle so we don't block startup */
                panel->priv->nm_warning_idle = g_idle_add ((GSourceFunc)display_version_warning_idle, panel);
        }

        g_strfreev (split);
        return ret;
}

static void
edit_connection (GtkButton *button, CcNetworkPanel *panel)
{
        NMConnection *c;
        const gchar *uuid;
        gchar *cmdline;
        GError *error;
        NetObject *object;
        NMDevice *device;

        object = get_selected_object (panel);
        if (object == NULL)
                return;
        else if (NET_IS_VPN (object)) {
                c = net_vpn_get_connection (NET_VPN (object));
        }
        else {
                device = net_device_get_nm_device (NET_DEVICE (object));
                c = find_connection_for_device (panel, device);
        }

        uuid = nm_connection_get_uuid (c);

        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);

        error = NULL;
        if (!g_spawn_command_line_async (cmdline, &error)) {
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
                g_error_free (error);
        }

        g_free (cmdline);
}

static void
add_connection_cb (GtkToolButton *button, CcNetworkPanel *panel)
{
        GtkWidget *dialog;
        gint response;

        dialog = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "connection_type_dialog"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel))));

        response = gtk_dialog_run (GTK_DIALOG (dialog));

        gtk_widget_hide (dialog);

        if (response == GTK_RESPONSE_OK) {
                GtkComboBox *combo;
                GtkTreeModel *model;
                GtkTreeIter iter;
                gchar *type;
                gchar *cmdline;
                GError *error;

                combo = GTK_COMBO_BOX (gtk_builder_get_object (panel->priv->builder,
                                                               "connection_type_combo"));
                model = gtk_combo_box_get_model (combo);
                gtk_combo_box_get_active_iter (combo, &iter);
                type = NULL;
                gtk_tree_model_get (model, &iter, 1, &type, -1);

                cmdline = g_strdup_printf ("nm-connection-editor --create --type %s", type);
                g_debug ("Launching '%s'\n", cmdline);

                error = NULL;
                if (!g_spawn_command_line_async (cmdline, &error)) {
                        g_warning ("Failed to launch nm-connection-editor: %s", error->message);
                        g_error_free (error);
                }
                g_free (cmdline);
                g_free (type);
        }
}

static void
forget_network_connection_delete_cb (NMRemoteConnection *connection,
                                     GError *error,
                                     gpointer user_data)
{
        if (error == NULL)
                return;
        g_warning ("failed to delete connection %s: %s",
                   nm_object_get_path (NM_OBJECT (connection)),
                   error->message);
}

static void
forget_network_response_cb (GtkWidget *dialog,
                            gint response,
                            CcNetworkPanel *panel)
{
        NetObject *object;
        NMDevice *device;
        NMRemoteConnection *remote_connection;

        if (response != GTK_RESPONSE_OK)
                goto out;

        /* get current device */
        object = get_selected_object (panel);
        if (object == NULL)
                return;
        device = net_device_get_nm_device (NET_DEVICE (object));
        if (device == NULL)
                goto out;

        /* delete the connection */
        remote_connection = NM_REMOTE_CONNECTION (find_connection_for_device (panel, device));
        if (remote_connection == NULL) {
                g_warning ("failed to get remote connection");
                goto out;
        }
        nm_remote_connection_delete (remote_connection,
                                     forget_network_connection_delete_cb,
                                     panel);
out:
        gtk_widget_destroy (dialog);
}

static void
wireless_button_clicked_cb (GtkButton *button, CcNetworkPanel *panel)
{
        gboolean ret;
        gchar *ssid_pretty = NULL;
        gchar *ssid_target = NULL;
        gchar *warning = NULL;
        GtkComboBox *combobox;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *dialog;
        GtkWidget *window;

        combobox = GTK_COMBO_BOX (gtk_builder_get_object (panel->priv->builder,
                                                          "combobox_wireless_network_name"));
        ret = gtk_combo_box_get_active_iter (combobox, &iter);
        if (!ret)
                goto out;

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter,
                            PANEL_WIRELESS_COLUMN_TITLE, &ssid_target,
                            -1);

        ssid_pretty = g_strdup_printf ("<b>%s</b>", ssid_target);
        warning = g_strdup_printf (_("Network details for %s including password and any custom configuration will be lost"), ssid_pretty);
        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         NULL);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), warning);
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                _("Forget"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (forget_network_response_cb), panel);
        gtk_window_present (GTK_WINDOW (dialog));
out:
        g_free (ssid_target);
        g_free (ssid_pretty);
        g_free (warning);
}

static void
remove_connection (GtkToolButton *button, CcNetworkPanel *panel)
{
        NetObject *object;
        NMConnection *connection;

        /* get current device */
        object = get_selected_object (panel);
        if (object == NULL)
                return;

        /* VPN */
        if (NET_IS_VPN (object)) {
                connection = net_vpn_get_connection (NET_VPN (object));
                nm_remote_connection_delete (NM_REMOTE_CONNECTION (connection), NULL, panel);
                return;
        }
}

static void
on_toplevel_map (GtkWidget      *widget,
                 CcNetworkPanel *panel)
{
        gboolean ret;

        /* is the user compiling against a new version, but running an
         * old daemon version? */
        ret = panel_check_network_manager_version (panel);
        if (ret) {
                manager_running (panel->priv->client, NULL, panel);
        } else {
                /* just select the proxy settings */
                select_first_device (panel);
        }
}

static void
connection_activate_cb (NMClient *client,
                        NMActiveConnection *connection,
                        GError *error,
                        gpointer user_data)
{
        CcNetworkPanel *panel = user_data;

        if (connection == NULL) {
                /* failed to activate */
                refresh_ui (panel);
        }
}

static void
connection_add_activate_cb (NMClient *client,
                            NMActiveConnection *connection,
                            const char *path,
                            GError *error,
                            gpointer user_data)
{
        connection_activate_cb (client, connection, error, user_data);
}

static void
connect_to_hidden_network (CcNetworkPanel *panel)
{
        CcNetworkPanelPrivate *priv = panel->priv;

        cc_network_panel_connect_to_hidden_network (panel, priv->client, priv->remote_settings);
}

static void
wireless_ap_changed_cb (GtkComboBox *combo_box, CcNetworkPanel *panel)
{
        const GByteArray *ssid;
        const gchar *ssid_tmp;
        gboolean ret;
        gchar *object_path = NULL;
        gchar *ssid_target = NULL;
        GSList *list, *l;
        GSList *filtered;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NetObject *object;
        NMConnection *connection;
        NMConnection *connection_activate = NULL;
        NMDevice *device;
        NMSettingWireless *setting_wireless;

        if (panel->priv->updating_device)
                goto out;

        ret = gtk_combo_box_get_active_iter (combo_box, &iter);
        if (!ret)
                goto out;

        object = get_selected_object (panel);
        if (object == NULL)
                goto out;

        device = net_device_get_nm_device (NET_DEVICE (object));
        if (device == NULL)
                goto out;

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
        gtk_tree_model_get (model, &iter,
                            PANEL_WIRELESS_COLUMN_ID, &object_path,
                            PANEL_WIRELESS_COLUMN_TITLE, &ssid_target,
                            -1);
        g_debug ("try to connect to WIFI network %s [%s]",
                 ssid_target, object_path);
        if (g_strcmp0 (object_path, "ap-other...") == 0) {
                connect_to_hidden_network (panel);
                goto out;
        }

        /* look for an existing connection we can use */
        list = nm_remote_settings_list_connections (panel->priv->remote_settings);
        g_debug ("%i existing remote connections available",
                 g_slist_length (list));
        filtered = nm_device_filter_connections (device, list);
        g_debug ("%i suitable remote connections to check",
                 g_slist_length (filtered));
        for (l = filtered; l; l = g_slist_next (l)) {
                connection = NM_CONNECTION (l->data);
                setting_wireless = nm_connection_get_setting_wireless (connection);
                if (!NM_IS_SETTING_WIRELESS (setting_wireless))
                        continue;
                ssid = nm_setting_wireless_get_ssid (setting_wireless);
                if (ssid == NULL)
                        continue;
                ssid_tmp = nm_utils_escape_ssid (ssid->data, ssid->len);
                if (g_strcmp0 (ssid_target, ssid_tmp) == 0) {
                        g_debug ("we found an existing connection %s to activate!",
                                 nm_connection_get_id (connection));
                        connection_activate = connection;
                        break;
                }
        }

        g_slist_free (list);
        g_slist_free (filtered);

        /* activate the connection */
        if (connection_activate != NULL) {
                nm_client_activate_connection (panel->priv->client,
                                               connection_activate,
                                               device, NULL,
                                               connection_activate_cb, panel);
                goto out;
        }

        /* create one, as it's missing */
        g_debug ("no existing connection found for %s, creating",
                 ssid_target);
        nm_client_add_and_activate_connection (panel->priv->client,
                                               NULL,
                                               device, object_path,
                                               connection_add_activate_cb, panel);
out:
        g_free (ssid_target);
        g_free (object_path);
}

static gint
wireless_ap_model_sort_cb (GtkTreeModel *model,
                           GtkTreeIter *a,
                           GtkTreeIter *b,
                           gpointer user_data)
{
        gchar *str_a;
        gchar *str_b;
        gint retval;

        gtk_tree_model_get (model, a,
                            PANEL_WIRELESS_COLUMN_SORT, &str_a,
                            -1);
        gtk_tree_model_get (model, b,
                            PANEL_WIRELESS_COLUMN_SORT, &str_b,
                            -1);

        /* special case blank entries to the bottom */
        if (g_strcmp0 (str_a, "") == 0) {
                retval = 1;
                goto out;
        }
        if (g_strcmp0 (str_b, "") == 0) {
                retval = -1;
                goto out;
        }

        /* case sensitive search like before */
        g_debug ("compare %s with %s", str_a, str_b);
        retval = g_strcmp0 (str_a, str_b);
out:
        g_free (str_a);
        g_free (str_b);
        return retval;
}

static GByteArray *
ssid_to_byte_array (const gchar *ssid)
{
        guint32 len;
        GByteArray *ba;

        len = strlen (ssid);
        ba = g_byte_array_sized_new (len);
        g_byte_array_append (ba, (guchar *)ssid, len);

        return ba;
}

static void
activate_cb (NMClient           *client,
             NMActiveConnection *connection,
             GError             *error,
             CcNetworkPanel     *panel)
{
        if (error) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
                return;
        }

        refresh_ui (panel);
}

static void
activate_new_cb (NMClient           *client,
                 NMActiveConnection *connection,
                 const gchar        *path,
                 GError             *error,
                 CcNetworkPanel     *panel)
{
        activate_cb (client, connection, error, panel);
}

static gchar *
get_hostname (void)
{
        GDBusConnection *bus;
        GVariant *res;
        GVariant *inner;
        gchar *str;
        GError *error;

        error = NULL;
        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (error != NULL) {
                g_warning ("Failed to get system bus connection: %s", error->message);
                g_error_free (error);

                return NULL;
        }
        res = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.hostname1",
                                           "/org/freedesktop/hostname1",
                                           "org.freedesktop.DBus.Properties",
                                           "Get",
                                           g_variant_new ("(ss)",
                                                          "org.freedesktop.hostname1",
                                                          "PrettyHostname"),
                                           (GVariantType*)"(v)",
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);
        g_object_unref (bus);

        if (error != NULL) {
                g_warning ("Getting pretty hostname failed: %s", error->message);
                g_error_free (error);
        }

        str = NULL;

        if (res != NULL) {
                g_variant_get (res, "(v)", &inner);
                str = g_variant_dup_string (inner, NULL);
                g_variant_unref (res);
        }

        if (str == NULL || *str == '\0') {
                str = g_strdup (g_get_host_name ());
        }

        if (str == NULL || *str == '\0') {
                str = g_strdup ("GNOME");
	}

        return str;
}

static GByteArray *
generate_ssid_for_hotspot (CcNetworkPanel *panel)
{
        GByteArray *ssid_array;
        gchar *ssid;

        ssid = get_hostname ();
        ssid_array = ssid_to_byte_array (ssid);
        g_free (ssid);

        return ssid_array;
}

static gchar *
generate_wep_key (CcNetworkPanel *panel)
{
        gchar key[11];
        gint i;
        const gchar *hexdigits = "0123456789abcdef";

        /* generate a 10-digit hex WEP key */
        for (i = 0; i < 10; i++) {
                gint digit;
                digit = g_random_int_range (0, 16);
                key[i] = hexdigits[digit];
        }
        key[10] = 0;

        return g_strdup (key);
}

static gboolean
is_hotspot_connection (NMConnection *connection)
{
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIP4Config *sip;

        sc = nm_connection_get_setting_connection (connection);
        if (g_strcmp0 (nm_setting_connection_get_connection_type (sc), "802-11-wireless") != 0) {
                return FALSE;
        }
        sw = nm_connection_get_setting_wireless (connection);
        if (g_strcmp0 (nm_setting_wireless_get_mode (sw), "adhoc") != 0) {
                return FALSE;
        }
        if (g_strcmp0 (nm_setting_wireless_get_security (sw), "802-11-wireless-security") != 0) {
                return FALSE;
        }
        sip = nm_connection_get_setting_ip4_config (connection);
        if (g_strcmp0 (nm_setting_ip4_config_get_method (sip), "shared") != 0) {
                return FALSE;
        }

        return TRUE;
}

static void
start_shared_connection (CcNetworkPanel *panel)
{
        NMConnection *c;
        NMConnection *tmp;
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIP4Config *sip;
        NMSettingWirelessSecurity *sws;
        NMDevice *device;
        NetObject *object;
        GByteArray *ssid_array;
        gchar *wep_key;
        const gchar *str_mac;
        struct ether_addr *bin_mac;
        GSList *connections;
        GSList *filtered;
        GSList *l;

        object = get_selected_object (panel);
        device = net_device_get_nm_device (NET_DEVICE (object));
        g_assert (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI);

        connections = nm_remote_settings_list_connections (panel->priv->remote_settings);
        filtered = nm_device_filter_connections (device, connections);
        g_slist_free (connections);
        c = NULL;
        for (l = filtered; l; l = l->next) {
                tmp = l->data;
                if (is_hotspot_connection (tmp)) {
                        c = tmp;
                        break;
                }
        }
        g_slist_free (filtered);

        if (c != NULL) {
                g_debug ("activate existing hotspot connection\n");
                nm_client_activate_connection (panel->priv->client,
                                               c,
                                               device,
                                               NULL,
                                               (NMClientActivateFn)activate_cb,
                                               panel);
                return;
        }

        g_debug ("create new hotspot connection\n");
        c = nm_connection_new ();

        sc = (NMSettingConnection *)nm_setting_connection_new ();
        g_object_set (sc,
                      "type", "802-11-wireless",
                      "id", "Hotspot",
                      "autoconnect", FALSE,
                      NULL);
        nm_connection_add_setting (c, (NMSetting *)sc);

        sw = (NMSettingWireless *)nm_setting_wireless_new ();
        g_object_set (sw,
                      "mode", "adhoc",
                      "security", "802-11-wireless-security",
                      NULL);

        str_mac = nm_device_wifi_get_permanent_hw_address (NM_DEVICE_WIFI (device));
        bin_mac = ether_aton (str_mac);
        if (bin_mac) {
                GByteArray *hw_address;

                hw_address = g_byte_array_sized_new (ETH_ALEN);
                g_byte_array_append (hw_address, bin_mac->ether_addr_octet, ETH_ALEN);
                g_object_set (sw,
                              "mac-address", hw_address,
                              NULL);
                g_byte_array_unref (hw_address);
        }
        nm_connection_add_setting (c, (NMSetting *)sw);

        sip = (NMSettingIP4Config*) nm_setting_ip4_config_new ();
        g_object_set (sip, "method", "shared", NULL);
        nm_connection_add_setting (c, (NMSetting *)sip);

        ssid_array = generate_ssid_for_hotspot (panel);
        g_object_set (sw,
                      "ssid", ssid_array,
                      NULL);
        g_byte_array_unref (ssid_array);

        sws = (NMSettingWirelessSecurity*) nm_setting_wireless_security_new ();
        wep_key = generate_wep_key (panel);
        g_object_set (sws,
                      "key-mgmt", "none",
                      "wep-key0", wep_key,
                      "wep-key-type", NM_WEP_KEY_TYPE_KEY,
                      NULL);
        g_free (wep_key);
        nm_connection_add_setting (c, (NMSetting *)sws);

        nm_client_add_and_activate_connection (panel->priv->client,
                                               c,
                                               device,
                                               NULL,
                                               (NMClientAddActivateFn)activate_new_cb,
                                               panel);

        g_object_unref (c);
}

static void
start_hotspot_response_cb (GtkWidget *dialog, gint response, CcNetworkPanel *panel)
{
        if (response == GTK_RESPONSE_OK) {
                start_shared_connection (panel);
        }
        gtk_widget_destroy (dialog);
}

static void
start_hotspot (GtkButton *button, CcNetworkPanel *panel)
{
        NetObject *object;
        NMDevice *device;
        gboolean is_default;
        const GPtrArray *connections;
        const GPtrArray *devices;
        NMActiveConnection *c;
        NMAccessPoint *ap;
        gchar *active_ssid;
        gchar *warning;
        gint i;

        warning = NULL;

        object = get_selected_object (panel);
        device = net_device_get_nm_device (NET_DEVICE (object));
        connections = nm_client_get_active_connections (panel->priv->client);
        if (connections == NULL || connections->len == 0) {
                warning = g_strdup_printf ("%s\n\n%s",
                                           _("Not connected to the internet."),
                                           _("Create the hotspot anyway?"));
        } else {
                is_default = FALSE;
                active_ssid = NULL;
                for (i = 0; i < connections->len; i++) {
                        c = (NMActiveConnection *)connections->pdata[i];
                        devices = nm_active_connection_get_devices (c);
                        if (devices && devices->pdata[0] == device) {
                                ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
                                active_ssid = nm_utils_ssid_to_utf8 (nm_access_point_get_ssid (ap));
                                is_default = nm_active_connection_get_default (c);
                                break;
                        }
                }

                if (active_ssid != NULL) {
                        GString *str;
                        str = g_string_new ("");
                        g_string_append_printf (str, _("Disconnect from %s and create a new hotspot?"), active_ssid);
                        if (is_default) {
                                g_string_append (str, "\n\n");
                                g_string_append (str, _("This is your only connection to the internet."));
                        }
                        warning = g_string_free (str, FALSE);
                }
        }

        if (warning != NULL) {
                GtkWidget *dialog;
                GtkWidget *window;

                window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
                dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_OTHER,
                                                 GTK_BUTTONS_NONE,
                                                 "%s", warning);
                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        _("Create _Hotspot"), GTK_RESPONSE_OK,
                                        NULL);
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (start_hotspot_response_cb), panel);
                gtk_window_present (GTK_WINDOW (dialog));

                return;
          }

        /* if we get here, things look good to go ahead */
        start_shared_connection (panel);
}

static void
stop_shared_connection (CcNetworkPanel *panel)
{
        const GPtrArray *connections;
        const GPtrArray *devices;
        NetObject *object;
        NMDevice *device;
        gint i;
        NMActiveConnection *c;

        object = get_selected_object (panel);
        device = net_device_get_nm_device (NET_DEVICE (object));

        connections = nm_client_get_active_connections (panel->priv->client);
        for (i = 0; i < connections->len; i++) {
                c = (NMActiveConnection *)connections->pdata[i];

                devices = nm_active_connection_get_devices (c);
                if (devices && devices->pdata[0] == device) {
                        nm_client_deactivate_connection (panel->priv->client, c);
                        break;
                }
        }

        refresh_ui (panel);
}

static void
stop_hotspot_response_cb (GtkWidget *dialog, gint response, CcNetworkPanel *panel)
{
        if (response == GTK_RESPONSE_OK) {
                stop_shared_connection (panel);
        }
        gtk_widget_destroy (dialog);
}

static void
stop_hotspot (GtkButton *button, CcNetworkPanel *panel)
{
        GtkWidget *dialog;
        GtkWidget *window;

        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         _("Stop hotspot and disconnect any users?"));
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                _("_Stop Hotspot"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (stop_hotspot_response_cb), panel);
        gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
network_add_shell_header_widgets_cb (gpointer user_data)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
        gboolean ret;
        GtkWidget *box;
        GtkWidget *widget;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
        /* TRANSLATORS: this is to disable the radio hardware in the
         * network panel */
        widget = gtk_label_new (_("Airplane Mode"));
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
        gtk_widget_set_visible (widget, TRUE);
        widget = gtk_switch_new ();
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
        gtk_widget_set_visible (widget, TRUE);
        cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (panel)), box);
        panel->priv->kill_switch_header = g_object_ref (box);

        ret = nm_client_wireless_get_enabled (panel->priv->client);
        gtk_switch_set_active (GTK_SWITCH (widget), !ret);
        g_signal_connect (GTK_SWITCH (widget), "notify::active",
                          G_CALLBACK (cc_network_panel_notify_enable_active_cb),
                          panel);
        panel_refresh_killswitch_visibility (panel);

        return FALSE;
}

static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        DBusGConnection *bus = NULL;
        GError *error = NULL;
        gint value;
        GSettings *settings_tmp;
        GtkAdjustment *adjustment;
        GtkCellRenderer *renderer;
        GtkComboBox *combobox;
        GtkStyleContext *context;
        GtkTreeSelection *selection;
        GtkTreeSortable *sortable;
        GtkWidget *widget;
        GtkWidget *toplevel;

        panel->priv = NETWORK_PANEL_PRIVATE (panel);

        panel->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_file (panel->priv->builder,
                                   GNOMECC_UI_DIR "/network.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        panel->priv->cancellable = g_cancellable_new ();

        panel->priv->proxy_settings = g_settings_new ("org.gnome.system.proxy");
        g_signal_connect (panel->priv->proxy_settings,
                          "changed",
                          G_CALLBACK (panel_settings_changed),
                          panel);

        /* explicitly set this to false as the panel has no way of
         * linking the http and https proxies them together */
        g_settings_set_boolean (panel->priv->proxy_settings,
                                "use-same-proxy",
                                FALSE);

        /* actions */
        value = g_settings_get_enum (panel->priv->proxy_settings, "mode");
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "combobox_proxy_mode"));
        panel_set_value_for_combo (panel, GTK_COMBO_BOX (widget), value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (panel_proxy_mode_combo_changed_cb),
                          panel);

        /* bind the proxy values */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_url"));
        g_settings_bind (panel->priv->proxy_settings, "autoconfig-url",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the HTTP proxy values */
        settings_tmp = g_settings_get_child (panel->priv->proxy_settings, "http");
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_http"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (panel->priv->builder,
                                                             "adjustment_proxy_port_http"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the HTTPS proxy values */
        settings_tmp = g_settings_get_child (panel->priv->proxy_settings, "https");
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_https"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (panel->priv->builder,
                                                             "adjustment_proxy_port_https"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the FTP proxy values */
        settings_tmp = g_settings_get_child (panel->priv->proxy_settings, "ftp");
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_ftp"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (panel->priv->builder,
                                                             "adjustment_proxy_port_ftp"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the SOCKS proxy values */
        settings_tmp = g_settings_get_child (panel->priv->proxy_settings, "socks");
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "entry_proxy_socks"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (panel->priv->builder,
                                                             "adjustment_proxy_port_socks"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "treeview_devices"));
        panel_add_devices_columns (panel, GTK_TREE_VIEW (widget));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
        g_signal_connect (selection, "changed",
                          G_CALLBACK (nm_devices_treeview_clicked_cb), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "devices_scrolledwindow"));
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "devices_toolbar"));
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

        /* add the virtual proxy device */
        panel_add_proxy_device (panel);

        /* setup wireless combobox model */
        combobox = GTK_COMBO_BOX (gtk_builder_get_object (panel->priv->builder,
                                                          "combobox_wireless_network_name"));
        g_signal_connect (combobox, "changed",
                          G_CALLBACK (wireless_ap_changed_cb),
                          panel);

        renderer = panel_cell_renderer_mode_new ();
        gtk_cell_renderer_set_padding (renderer, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "mode", PANEL_WIRELESS_COLUMN_MODE,
                                        NULL);

        renderer = panel_cell_renderer_security_new ();
        gtk_cell_renderer_set_padding (renderer, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "security", PANEL_WIRELESS_COLUMN_SECURITY,
                                        NULL);

        /* sort networks in drop down */
        sortable = GTK_TREE_SORTABLE (gtk_builder_get_object (panel->priv->builder,
                                                              "liststore_wireless_network"));
        gtk_tree_sortable_set_sort_column_id (sortable,
                                              PANEL_WIRELESS_COLUMN_SORT,
                                              GTK_SORT_ASCENDING);
        gtk_tree_sortable_set_sort_func (sortable,
                                         PANEL_WIRELESS_COLUMN_SORT,
                                         wireless_ap_model_sort_cb,
                                         sortable,
                                         NULL);

        renderer = panel_cell_renderer_signal_new ();
        gtk_cell_renderer_set_padding (renderer, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "signal", PANEL_WIRELESS_COLUMN_STRENGTH,
                                        NULL);

        /* use NetworkManager client */
        panel->priv->client = nm_client_new ();
        g_signal_connect (panel->priv->client, "notify::" NM_CLIENT_MANAGER_RUNNING,
                          G_CALLBACK (manager_running), panel);
        g_signal_connect (panel->priv->client, "notify::" NM_CLIENT_ACTIVE_CONNECTIONS,
                          G_CALLBACK (active_connections_changed), panel);
        g_signal_connect (panel->priv->client, "device-added",
                          G_CALLBACK (device_added_cb), panel);
        g_signal_connect (panel->priv->client, "device-removed",
                          G_CALLBACK (device_removed_cb), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "device_wired_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), panel);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "device_wireless_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), panel);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "device_mobilebb_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), panel);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "device_vpn_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), panel);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "device_proxy_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), panel);

        g_signal_connect (panel->priv->client, "notify::wireless-enabled",
                          G_CALLBACK (wireless_enabled_toggled), panel);
        g_signal_connect (panel->priv->client, "notify::wimax-enabled",
                          G_CALLBACK (wimax_enabled_toggled), panel);
        g_signal_connect (panel->priv->client, "notify::wwan-enabled",
                          G_CALLBACK (mobilebb_enabled_toggled), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "start_hotspot_button"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (start_hotspot), panel);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "stop_hotspot_button"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (stop_hotspot), panel);


        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_wired_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_wireless_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_mobilebb_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_vpn_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "add_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (add_connection_cb), panel);

        /* disable for now, until we actually show removable connections */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "remove_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (remove_connection), panel);

        /* setup wireless button */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_wireless_button"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (wireless_button_clicked_cb), panel);

        /* add remote settings such as VPN settings as virtual devices */
        bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
                g_error_free (error);
        }
        panel->priv->remote_settings = nm_remote_settings_new (bus);
        g_signal_connect (panel->priv->remote_settings, NM_REMOTE_SETTINGS_CONNECTIONS_READ,
                          G_CALLBACK (notify_connections_read_cb), panel);
        g_signal_connect (panel->priv->remote_settings, NM_REMOTE_SETTINGS_NEW_CONNECTION,
                          G_CALLBACK (notify_new_connection_cb), panel);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        g_signal_connect_after (toplevel, "map", G_CALLBACK (on_toplevel_map), panel);

        /* hide implementation details */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "notebook_types"));
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "vbox1"));
        gtk_widget_reparent (widget, (GtkWidget *) panel);

        /* add kill switch widgets when dialog activated */
        panel->priv->add_header_widgets_idle = g_idle_add (network_add_shell_header_widgets_cb, panel);
}

void
cc_network_panel_register (GIOModule *module)
{
        cc_network_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                        CC_TYPE_NETWORK_PANEL,
                                        "network", 0);
}
