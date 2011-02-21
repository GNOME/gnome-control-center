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

#include <glib/gi18n.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "cc-network-panel.h"

#include "nm-remote-settings.h"
#include "nm-client.h"
#include "nm-device.h"
#include "nm-device-ethernet.h"
#include "nm-device-wifi.h"
#include "nm-utils.h"
#include "nm-active-connection.h"
#include "nm-vpn-connection.h"
#include "nm-setting-ip4-config.h"

#include "panel-common.h"
#include "panel-cell-renderer-mode.h"
#include "panel-cell-renderer-signal.h"

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

struct _CcNetworkPanelPrivate
{
        GCancellable    *cancellable;
        gchar           *current_device;
        GPtrArray       *devices;
        GSettings       *proxy_settings;
        GtkBuilder      *builder;
        NMClient        *client;
};

enum {
        PANEL_DEVICES_COLUMN_ICON,
        PANEL_DEVICES_COLUMN_TITLE,
        PANEL_DEVICES_COLUMN_ID,
        PANEL_DEVICES_COLUMN_SORT,
        PANEL_DEVICES_COLUMN_TOOLTIP,
        PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE,
        PANEL_DEVICES_COLUMN_LAST
};

enum {
        PANEL_WIRELESS_COLUMN_ID,
        PANEL_WIRELESS_COLUMN_TITLE,
        PANEL_WIRELESS_COLUMN_SORT,
        PANEL_WIRELESS_COLUMN_STRENGTH,
        PANEL_WIRELESS_COLUMN_MODE,
        PANEL_WIRELESS_COLUMN_LAST
};

static void     nm_device_refresh_item_ui               (CcNetworkPanel *panel, NMDevice *device);

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

static void
cc_network_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        switch (property_id) {
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

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (object)->priv;

        g_free (priv->current_device);
        g_ptr_array_unref (priv->devices);

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
}

static void
cc_network_panel_class_finalize (CcNetworkPanelClass *klass)
{
}

static void
panel_settings_changed (GSettings      *settings,
                        const gchar    *key,
                        CcNetworkPanel *panel)
{
}

static void
panel_proxy_mode_combo_setup_widgets (CcNetworkPanel *panel, guint value)
{
        GtkWidget *widget;

        /* hide or show the PAC text box */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "hbox_proxy_url"));
        gtk_widget_set_visible (widget, value == 2);

        /* hide or show the manual entry text boxes */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "hbox_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "hbox_proxy_shttp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "hbox_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "hbox_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
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

        /* save */
        g_object_set_data_full (G_OBJECT (device),
                                "ControlCenter::OperatorName",
                                g_strdup (operator_name),
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
panel_add_device (CcNetworkPanel *panel, NMDevice *device)
{
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        gchar *title = NULL;
        NMDeviceType type;
        CcNetworkPanelPrivate *priv = panel->priv;

        g_debug ("device %s type %i",
                 nm_device_get_udi (device),
                 nm_device_get_device_type (device));

        g_ptr_array_add (panel->priv->devices,
                         g_object_ref (device));

        /* do we have to get additonal data from ModemManager */
        type = nm_device_get_device_type (device);
        if (type == NM_DEVICE_TYPE_GSM ||
            type == NM_DEVICE_TYPE_CDMA) {
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
        title = g_strdup_printf ("<span size=\"large\">%s</span>",
                                 panel_device_type_to_localized_string (nm_device_get_device_type (device)));

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, panel_device_type_to_icon_name (nm_device_get_device_type (device)),
                            PANEL_DEVICES_COLUMN_SORT, panel_device_type_to_sortable_string (nm_device_get_device_type (device)),
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_ID, nm_device_get_udi (device),
                            PANEL_DEVICES_COLUMN_TOOLTIP, NULL,
                            PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, device,
                            -1);
        g_free (title);
}

static void
panel_remove_device (CcNetworkPanel *panel, NMDevice *device)
{
        gboolean ret;
        gchar *id_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;
        guint i;
        NMDevice *device_tmp = NULL;

        /* remove device from array */
        for (i=0; i<panel->priv->devices->len; i++) {
                device_tmp = g_ptr_array_index (panel->priv->devices, i);
                if (g_strcmp0 (nm_device_get_udi (device),
                               nm_device_get_udi (device_tmp)) == 0) {
                        g_ptr_array_remove_index_fast (panel->priv->devices, i);
                        break;
                }
        }
        if (device_tmp == NULL)
                return;

        /* remove device from model */
        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;

        /* get the other elements */
        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_ID, &id_tmp,
                                    -1);
                if (g_strcmp0 (id_tmp,
                               nm_device_get_udi (device_tmp)) == 0) {
                        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
                        g_free (id_tmp);
                        break;
                }
                g_free (id_tmp);
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
        g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
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
        gchar *hbox_id;
        gchar *label_id = NULL;
        GtkWidget *widget;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* hide the parent bix if there is no value */
        hbox_id = g_strdup_printf ("hbox_%s_%s", sub_pane, widget_suffix);
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, hbox_id));
        if (value == NULL) {
                gtk_widget_hide (widget);
                goto out;
        }

        /* there exists a value */
        gtk_widget_show (widget);
        label_id = g_strdup_printf ("label_%s_%s", sub_pane, widget_suffix);
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, label_id));
        gtk_label_set_label (GTK_LABEL (widget), value);
out:
        g_free (hbox_id);
        g_free (label_id);
}

static void
add_access_point (CcNetworkPanel *panel, NMAccessPoint *ap, NMAccessPoint *active)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        const GByteArray *ssid;
        const gchar *ssid_text;
        const gchar *hw_address;
        GtkListStore *liststore_wireless_network;
        GtkTreeIter treeiter;
        GtkWidget *widget;

        ssid = nm_access_point_get_ssid (ap);
        if (ssid == NULL)
                return;
        ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);

        liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                     "liststore_wireless_network"));

        hw_address = nm_access_point_get_hw_address (ap);
        gtk_list_store_append (liststore_wireless_network, &treeiter);
        gtk_list_store_set (liststore_wireless_network,
                            &treeiter,
                            PANEL_WIRELESS_COLUMN_ID, hw_address,
                            PANEL_WIRELESS_COLUMN_TITLE, ssid_text,
                            PANEL_WIRELESS_COLUMN_SORT, ssid_text,
                            PANEL_WIRELESS_COLUMN_STRENGTH, nm_access_point_get_strength (ap),
                            PANEL_WIRELESS_COLUMN_MODE, nm_access_point_get_mode (ap),
                            -1);

        /* is this what we're on already? */
        if (active == NULL)
                return;
        if (g_strcmp0 (hw_address, nm_access_point_get_hw_address (active)) == 0) {
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "combobox_network_name"));
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &treeiter);
        }
}

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

static GPtrArray *
panel_get_strongest_unique_aps (const GPtrArray *aps)
{
        const GByteArray *ssid;
        const GByteArray *ssid_tmp;
        gboolean add_ap;
        GPtrArray *aps_unique = NULL;
        guint i;
        guint j;
        NMAccessPoint *ap;
        NMAccessPoint *ap_tmp;

        /* we will have multiple entries for typical hotspots, just
         * filter to the one with the strongest signal */
        aps_unique = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
        for (i = 0; i < aps->len; i++) {
                ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));
                ssid = nm_access_point_get_ssid (ap);
                add_ap = TRUE;

                /* get already added list */
                for (j=0; j<aps_unique->len; j++) {
                        ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (aps, j));
                        ssid_tmp = nm_access_point_get_ssid (ap_tmp);

                        /* is this the same type and data? */
                        if (ssid->len != ssid_tmp->len)
                                continue;
                        if (memcmp (ssid->data, ssid_tmp->data, ssid_tmp->len) != 0)
                                continue;
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
                                break;
                        } else {
                                add_ap = FALSE;
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

static void
nm_device_refresh_item_ui (CcNetworkPanel *panel, NMDevice *device)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        const gchar *str;
        const gchar *sub_pane = NULL;
        const GPtrArray *aps;
        GPtrArray *aps_unique = NULL;
        gchar *str_tmp;
        GHashTable *options = NULL;
        GtkListStore *liststore_wireless_network;
        GtkWidget *widget;
        guint i;
        NMAccessPoint *ap;
        NMAccessPoint *active_ap;
        NMDeviceState state;
        NMDeviceType type;
        NMDHCP4Config *config_dhcp4 = NULL;
        NMDHCP6Config *config_dhcp6 = NULL;

        /* we have a new device */
        g_debug ("selected device is: %s", nm_device_get_udi (device));
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "hbox_device_header"));
        gtk_widget_set_visible (widget, TRUE);

        type = nm_device_get_device_type (device);
        g_debug ("device %s type %i",
                 nm_device_get_udi (device),
                 type);

        /* set device icon */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "image_device"));
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      panel_device_type_to_icon_name (type),
                                      GTK_ICON_SIZE_DIALOG);

        /* set device kind */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             panel_device_type_to_localized_string (type));


        /* set device state */
        state = nm_device_get_state (device);
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_status"));
        gtk_label_set_label (GTK_LABEL (widget),
                             panel_device_state_to_localized_string (state));

        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "notebook_types"));
        if (type == NM_DEVICE_TYPE_ETHERNET) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
                sub_pane = "wired";
        } else if (type == NM_DEVICE_TYPE_WIFI) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
                sub_pane = "wireless";
        } else if (type == NM_DEVICE_TYPE_GSM ||
                   type == NM_DEVICE_TYPE_CDMA) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 4);
                sub_pane = "mobilebb";
        }
        if (sub_pane == NULL)
                goto out;

        /* FIXME? should we need to do something with this? */
        if (state == NM_DEVICE_STATE_ACTIVATED)
                panel_show_ip4_config (nm_device_get_ip4_config (device));

        if (type == NM_DEVICE_TYPE_ETHERNET) {

                /* speed */
                str_tmp = g_strdup_printf ("%d Mb/sec",
                                           nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (device)));
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "speed",
                                       str_tmp);
                g_free (str_tmp);

                /* device MAC */
                str = nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (device));
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "mac",
                                       str);

        } else if (type == NM_DEVICE_TYPE_WIFI) {

                /* speed */
                str_tmp = g_strdup_printf ("%d Mb/s",
                                           nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (device)));
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "speed",
                                       str_tmp);
                g_free (str_tmp);

                /* device MAC */
                str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (device));
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "mac",
                                       str);
                /* security */
                str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (device));
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "security",
                                       NULL); /* FIXME */

                /* populate access point dropdown */
                liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                             "liststore_wireless_network"));
                gtk_list_store_clear (liststore_wireless_network);
                active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
                aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (device));
                if (aps == NULL)
                        return;
                aps_unique = panel_get_strongest_unique_aps (aps);
                for (i = 0; i < aps_unique->len; i++) {
                        ap = NM_ACCESS_POINT (g_ptr_array_index (aps_unique, i));
                        add_access_point (panel,
                                          ap,
                                          active_ap);
                }

        } else if (type == NM_DEVICE_TYPE_GSM ||
                   type == NM_DEVICE_TYPE_CDMA) {

                /* IMEI */
                str = g_object_get_data (G_OBJECT (device),
                                         "ControlCenter::EquipmentIdentifier");
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "imei",
                                       str);

                /* operator name */
                str = g_object_get_data (G_OBJECT (device),
                                         "ControlCenter::OperatorName");
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "provider",
                                       str);

                /* device speed */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "speed",
                                       NULL);
        }

        /* get IP4 parameters */
        config_dhcp4 = nm_device_get_dhcp4_config (device);
        if (config_dhcp4 != NULL) {
                g_object_get (G_OBJECT (config_dhcp4),
                              NM_DHCP4_CONFIG_OPTIONS, &options,
                              NULL);

                /* IPv4 address */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "ip4",
                                       nm_dhcp4_config_get_one_option (config_dhcp4,
                                                                       "ip_address"));

                /* IPv4 DNS */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "dns",
                                       nm_dhcp4_config_get_one_option (config_dhcp4,
                                                                       "domain_name_servers"));

                /* IPv4 route */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "route",
                                       nm_dhcp4_config_get_one_option (config_dhcp4,
                                                                       "routers"));

                /* IPv4 netmask */
                if (type == NM_DEVICE_TYPE_ETHERNET) {
                        panel_set_widget_data (panel,
                                               sub_pane,
                                               "subnet",
                                               nm_dhcp4_config_get_one_option (config_dhcp4,
                                                                               "subnet_mask"));
                }
        }

        /* get IP6 parameters */
        if (0) config_dhcp6 = nm_device_get_dhcp6_config (device);
        if (config_dhcp6 != NULL) {

                /* IPv6 address */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "ip6",
                                       nm_dhcp6_config_get_one_option (config_dhcp6,
                                                                       "ip_address"));
        }
out:
        if (aps_unique != NULL)
                g_ptr_array_unref (aps_unique);
}

static void
nm_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *widget;
        NMDevice *device;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* will only work in single or browse selection mode! */
        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                g_debug ("no row selected");
                goto out;
        }

        /* get id */
        gtk_tree_model_get (model, &iter,
                            PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, &device,
                            -1);

        /* this is the proxy settings device */
        if (device == NULL) {
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "hbox_device_header"));
                gtk_widget_set_visible (widget, FALSE);
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                             "notebook_types"));
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 2);

                /* save so we ignore */
                g_free (priv->current_device);
                priv->current_device = NULL;
                goto out;
        }

        /* save so we can update */
        g_free (priv->current_device);
        priv->current_device = g_strdup (nm_device_get_udi (device));

        /* refresh device */
        nm_device_refresh_item_ui (panel, device);
out:
        return;
}

static void
panel_add_proxy_device (CcNetworkPanel *panel)
{
        gchar *title;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                            "liststore_devices"));
        title = g_strdup_printf ("<span size=\"large\">%s</span>",
                                 _("Network proxy"));

        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "preferences-system-network",
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_ID, NULL,
                            PANEL_DEVICES_COLUMN_SORT, "9",
                            PANEL_DEVICES_COLUMN_TOOLTIP, _("Set the system proxy settings"),
                            PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, NULL,
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
active_connections_changed (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
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
        }
}

static void
device_state_notify_changed_cb (NMDevice *device,
                                GParamSpec *pspec,
                                gpointer user_data)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        /* only refresh the selected device */
        if (g_strcmp0 (panel->priv->current_device,
                       nm_device_get_udi (device)) == 0) {
                nm_device_refresh_item_ui (panel, device);
        }
}

static void
device_added_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("New device added");
        panel_add_device (panel, device);
        g_signal_connect (G_OBJECT (device), "notify::state",
                          (GCallback) device_state_notify_changed_cb, NULL);
}

static void
device_removed_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("Device removed");
        panel_remove_device (panel, device);
}

static void
manager_running (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
        const GPtrArray *devices;
        int i;
        NMDevice *device_tmp;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        /* TODO: clear all devices we added */
        if (!nm_client_get_manager_running (client)) {
                g_debug ("NM disappeared");
                return;
        }

        g_debug ("coldplugging devices");
        devices = nm_client_get_devices (client);
        if (devices == NULL) {
                g_debug ("No devices to add");
                return;
        }
        for (i = 0; i < devices->len; i++) {
                device_tmp = g_ptr_array_index (devices, i);
                panel_add_device (panel, device_tmp);
        }

        /* select the first device */
        select_first_device (panel);
}

static void
notify_connections_read_cb (NMRemoteSettings *settings,
                            GParamSpec *pspec,
                            gpointer user_data)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
        GSList *list, *iter;

        list = nm_remote_settings_list_connections (settings);
        g_debug ("%p has %i remote connections",
                 panel, g_slist_length (list));
        for (iter = list; iter; iter = g_slist_next (iter)) {
                NMConnection *candidate = NM_CONNECTION (iter->data);
                /* we can't actually test this yet as VPN support in
                 * NetworkManager 0.9 is currently broken */
                g_debug ("TODO: add %s", nm_connection_get_path (candidate));
        }
}

static gboolean
panel_check_network_manager_version (CcNetworkPanel *panel)
{
        const gchar *message;
        const gchar *version;
        gchar **split;
        GtkWidget *dialog;
        GtkWindow *window = NULL;
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

        /* is it too new */
        if (major > 0 || major > 9) {

                /* TRANSLATORS: the user is running a NM that is too new and API compatible */
                message = _("The running NetworkManager version is not compatible (too new).");

        /* is it new enough */
        } else if (minor <= 8 && micro < 992) {

                /* TRANSLATORS: the user is running a NM that is too old and API compatible */
                message = _("The running NetworkManager version is not compatible (too old).");
        }

        /* nothing to do */
        if (message == NULL)
                goto out;

        /* do modal dialog */
        ret = FALSE;
        dialog = gtk_message_dialog_new (window,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s",
                                         message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
out:
        g_strfreev (split);
        return ret;
}

static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        DBusGConnection *bus = NULL;
        gboolean ret;
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
        NMRemoteSettings *remote_settings;

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
        panel->priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

        panel->priv->proxy_settings = g_settings_new ("org.gnome.system.proxy");
        g_signal_connect (panel->priv->proxy_settings,
                          "changed",
                          G_CALLBACK (panel_settings_changed),
                          panel);

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

        /* bind the proxy values */
        settings_tmp = g_settings_new ("org.gnome.system.proxy.http");
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

        /* bind the proxy values */
        settings_tmp = g_settings_new ("org.gnome.system.proxy.https");
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

        /* bind the proxy values */
        settings_tmp = g_settings_new ("org.gnome.system.proxy.ftp");
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

        /* bind the proxy values */
        settings_tmp = g_settings_new ("org.gnome.system.proxy.socks");
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
                                                          "combobox_network_name"));

        renderer = panel_cell_renderer_mode_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "mode", PANEL_WIRELESS_COLUMN_MODE,
                                        NULL);

        /* sort networks in drop down */
        sortable = GTK_TREE_SORTABLE (gtk_builder_get_object (panel->priv->builder,
                                                              "liststore_wireless_network"));
        gtk_tree_sortable_set_sort_column_id (sortable,
                                              PANEL_WIRELESS_COLUMN_SORT,
                                              GTK_SORT_ASCENDING);

        renderer = panel_cell_renderer_signal_new ();
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

        /* disable for now, until we can remove connections without
         * segfaulting NM... */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "add_toolbutton"));
        gtk_widget_set_sensitive (widget, FALSE);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "remove_toolbutton"));
        gtk_widget_set_sensitive (widget, FALSE);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "button_unlock"));
        gtk_widget_set_sensitive (widget, FALSE);
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "switch_flight_mode"));
        ret = nm_client_wireless_get_enabled (panel->priv->client);
        gtk_switch_set_active (GTK_SWITCH (widget), !ret);
        g_signal_connect (GTK_SWITCH (widget), "notify::active",
                          G_CALLBACK (cc_network_panel_notify_enable_active_cb),
                          panel);

        /* add remote settings such as VPN settings as virtual devices */
        bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
                g_error_free (error);
        }
        remote_settings = nm_remote_settings_new (bus);
        g_signal_connect (remote_settings, "notify::" NM_REMOTE_SETTINGS_CONNECTIONS_READ,
                          G_CALLBACK (notify_connections_read_cb), panel);

        /* is the user compiling against a new version, but running an
         * old daemon version? */
        ret = panel_check_network_manager_version (panel);
        if (ret) {
                manager_running (panel->priv->client, NULL, panel);
        } else {
                /* just select the proxy settings */
                select_first_device (panel);
        }

        /* hide implementation details */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "notebook_types"));
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "vbox1"));
        gtk_widget_reparent (widget, (GtkWidget *) panel);
}

void
cc_network_panel_register (GIOModule *module)
{
        cc_network_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                        CC_TYPE_NETWORK_PANEL,
                                        "network", 0);
}
