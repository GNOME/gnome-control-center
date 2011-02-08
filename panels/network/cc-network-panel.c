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

#include "cc-network-panel.h"

#include "panel-cell-renderer-mode.h"
#include "panel-cell-renderer-signal.h"
#include "nm-device.h"

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

struct _CcNetworkPanelPrivate
{
        GCancellable    *cancellable;
        gchar           *current_device;
        GDBusProxy      *proxy;
        GPtrArray       *devices;
        GSettings       *proxy_settings;
        GtkBuilder      *builder;
        guint            devices_add_refcount;
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

static void     nm_device_refresh_item_ui               (CcNetworkPanel *panel, NmDevice *device);
static void     panel_refresh_devices                   (CcNetworkPanel *panel);

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
        if (priv->proxy != NULL) {
                g_object_unref (priv->proxy);
                priv->proxy = NULL;
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

/**
 * panel_settings_changed:
 **/
static void
panel_settings_changed (GSettings      *settings,
                        const gchar    *key,
                        CcNetworkPanel *panel)
{
}

/**
 * panel_proxy_mode_combo_setup_widgets:
 **/
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

/**
 * panel_proxy_mode_combo_changed_cb:
 **/
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

/**
 * panel_set_value_for_combo:
 **/
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

/**
 * nm_device_ready_to_add_cb:
 **/
static void
nm_device_ready_to_add_cb (NmDevice *device, CcNetworkPanel *panel)
{
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        gchar *title = NULL;
        CcNetworkPanelPrivate *priv = panel->priv;

        g_debug ("device %s type %i",
                 nm_device_get_object_path (device),
                 nm_device_get_kind (device));

        /* make title a bit bigger */
        title = g_strdup_printf ("<span size=\"large\">%s</span>",
                                 nm_device_kind_to_localized_string (nm_device_get_kind (device)));

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, nm_device_kind_to_icon_name (nm_device_get_kind (device)),
                            PANEL_DEVICES_COLUMN_SORT, nm_device_kind_to_sortable_string (nm_device_get_kind (device)),
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_ID, nm_device_get_object_path (device),
                            PANEL_DEVICES_COLUMN_TOOLTIP, NULL,
                            PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, device,
                            -1);
        g_free (title);

        if (--panel->priv->devices_add_refcount == 0)
                select_first_device (panel);
}

/**
 * nm_device_changed_cb:
 **/
static void
nm_device_changed_cb (NmDevice *device,
                      CcNetworkPanel *panel)
{
        /* only refresh the selected device */
        if (g_strcmp0 (panel->priv->current_device,
                       nm_device_get_object_path (device)) == 0) {
                nm_device_refresh_item_ui (panel, device);
        }
}

/**
 * panel_add_device:
 **/
static void
panel_add_device (CcNetworkPanel *panel, const gchar *device_id)
{
        NmDevice *device;

        /* create device */
        device = nm_device_new ();
        g_signal_connect (device, "ready",
                          G_CALLBACK (nm_device_ready_to_add_cb), panel);
        g_signal_connect (device, "changed",
                          G_CALLBACK (nm_device_changed_cb), panel);
        nm_device_refresh (device, device_id, panel->priv->cancellable);
        g_ptr_array_add (panel->priv->devices, device);

        /* get initial device state */
        panel->priv->devices_add_refcount++;
}

/**
 * panel_remove_device:
 **/
static void
panel_remove_device (CcNetworkPanel *panel, const gchar *device_id)
{
        gboolean ret;
        gchar *id_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;
        guint i;
        NmDevice *device;

        /* remove device from array */
        for (i=0; i<panel->priv->devices->len; i++) {
                device = g_ptr_array_index (panel->priv->devices, i);
                if (g_strcmp0 (nm_device_get_object_path (device), device_id) == 0) {
                        g_ptr_array_remove_index_fast (panel->priv->devices, i);
                        break;
                }
        }

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
                if (g_strcmp0 (id_tmp, device_id) == 0) {
                        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
                        g_free (id_tmp);
                        break;
                }
                g_free (id_tmp);
        } while (gtk_tree_model_iter_next (model, &iter));
}

/**
 * panel_get_devices_cb:
 **/
static void
panel_get_devices_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
        const gchar *object_path;
        GError *error = NULL;
        gsize len;
        GVariantIter iter;
        GVariant *child;
        GVariant *result;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_printerr ("Error getting devices: %s\n", error->message);
                g_error_free (error);
                return;
        }

        child = g_variant_get_child_value (result, 0);
        len = g_variant_iter_init (&iter, child);
        if (len == 0) {
                g_warning ("no devices?!");
                goto out;
        }

        /* for each entry in the array */
        while (g_variant_iter_loop (&iter, "o", &object_path)) {
                g_debug ("adding network device %s", object_path);
                panel_add_device (panel, object_path);
        }
out:
        g_variant_unref (result);
        g_variant_unref (child);
}

/**
 * panel_dbus_manager_signal_cb:
 **/
static void
panel_dbus_manager_signal_cb (GDBusProxy *proxy,
                              gchar *sender_name,
                              gchar *signal_name,
                              GVariant *parameters,
                              gpointer user_data)
{
        gchar *object_path = NULL;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        /* get the new state */
        if (g_strcmp0 (signal_name, "StateChanged") == 0) {
                g_debug ("ensure devices are correct");

                /* refresh devices */
                panel_refresh_devices (panel);
                goto out;
        }

        /* device added or removed */
        if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
                g_variant_get (parameters, "(o)", &object_path);
                panel_add_device (panel, object_path);
                goto out;
        }

        /* device added or removed */
        if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
                g_variant_get (parameters, "(o)", &object_path);
                panel_remove_device (panel, object_path);
                goto out;
        }
out:
        g_free (object_path);
}

/**
 * panel_refresh_devices:
 **/
static void
panel_refresh_devices (CcNetworkPanel *panel)
{
        GtkListStore *liststore_devices;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* clear the existing list */
        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        gtk_list_store_clear (liststore_devices);

        /* get the new state */
        g_dbus_proxy_call (priv->proxy,
                           "GetDevices",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           panel_get_devices_cb,
                           panel);

}

/**
 * panel_got_network_proxy_cb:
 **/
static void
panel_got_network_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
        CcNetworkPanelPrivate *priv = panel->priv;

        priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (priv->proxy == NULL) {
                g_printerr ("Error creating proxy: %s\n", error->message);
                g_error_free (error);
                select_first_device (panel);
                goto out;
        }

        /* we want to change the UI in reflection to device events */
        g_signal_connect (priv->proxy,
                          "g-signal",
                          G_CALLBACK (panel_dbus_manager_signal_cb),
                          user_data);

        /* refresh devices */
        panel_refresh_devices (panel);
out:
        return;
}

/**
 * panel_add_devices_columns:
 **/
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

/**
 * panel_set_widget_data:
 **/
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

/**
 * nm_device_refresh_item_access_points:
 **/
static void
nm_device_refresh_item_access_points (CcNetworkPanel *panel, NmDevice *device)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        GPtrArray *access_points;
        GtkListStore *liststore_wireless_network;
        GtkTreeIter treeiter;
        GtkWidget *widget;
        NmAccessPoint *access_point_tmp;
        guint i;

        /* add to the model */
        liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                     "liststore_wireless_network"));
        gtk_list_store_clear (liststore_wireless_network);

        access_points = nm_device_get_access_points (device);
        for (i=0; i<access_points->len; i++) {
                access_point_tmp = g_ptr_array_index (access_points, i);

                gtk_list_store_append (liststore_wireless_network, &treeiter);
                gtk_list_store_set (liststore_wireless_network,
                                    &treeiter,
                                    PANEL_WIRELESS_COLUMN_ID, nm_access_point_get_object_path (access_point_tmp),
                                    PANEL_WIRELESS_COLUMN_TITLE, nm_access_point_get_ssid (access_point_tmp),
                                    PANEL_WIRELESS_COLUMN_SORT, nm_access_point_get_ssid (access_point_tmp),
                                    PANEL_WIRELESS_COLUMN_STRENGTH, nm_access_point_get_strength (access_point_tmp),
                                    PANEL_WIRELESS_COLUMN_MODE, nm_access_point_get_mode (access_point_tmp),
                                    -1);

                /* is this what we're on already? */
                if (g_strcmp0 (nm_access_point_get_object_path (access_point_tmp),
                               nm_device_get_active_access_point (device)) == 0) {
                        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                                     "combobox_network_name"));
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &treeiter);
                }
        }
        g_ptr_array_unref (access_points);
}

/**
 * nm_device_refresh_item_ui:
 **/
static void
nm_device_refresh_item_ui (CcNetworkPanel *panel, NmDevice *device)
{
        GtkWidget *widget;
        NmDeviceState state;
        CcNetworkPanelPrivate *priv = panel->priv;
        const gchar *sub_pane = NULL;

        /* we have a new device */
        g_debug ("selected device is: %s", nm_device_get_object_path (device));
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "hbox_device_header"));
        gtk_widget_set_visible (widget, TRUE);

        g_debug ("device %s type %i",
                 nm_device_get_object_path (device),
                 nm_device_get_kind (device));

        /* set device icon */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "image_device"));
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      nm_device_kind_to_icon_name (nm_device_get_kind (device)),
                                      GTK_ICON_SIZE_DIALOG);

        /* set device kind */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             nm_device_kind_to_localized_string (nm_device_get_kind (device)));

        /* set device state */
        state = nm_device_get_state (device);
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_status"));
        gtk_label_set_label (GTK_LABEL (widget),
                             nm_device_state_to_localized_string (state));

        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "notebook_types"));
        if (nm_device_get_kind (device) == NM_DEVICE_KIND_ETHERNET) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
                sub_pane = "wired";
        } else if (nm_device_get_kind (device) == NM_DEVICE_KIND_WIFI) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
                sub_pane = "wireless";
        } else if (nm_device_get_kind (device) == NM_DEVICE_KIND_GSM ||
                   nm_device_get_kind (device) == NM_DEVICE_KIND_CDMA) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 4);
                sub_pane = "mobilebb";
        }
        if (sub_pane == NULL)
                goto out;

        /* IPv4 address */
        panel_set_widget_data (panel,
                               sub_pane,
                               "ip4",
                               nm_device_get_ip4_address (device));

        if (nm_device_get_kind (device) == NM_DEVICE_KIND_ETHERNET ||
            nm_device_get_kind (device) == NM_DEVICE_KIND_WIFI) {
                /* IPv6 address */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "ip6",
                                       nm_device_get_ip6_address (device));

                /* IPv4 DNS */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "dns",
                                       nm_device_get_ip4_nameserver (device));

                /* IPv4 route */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "route",
                                       nm_device_get_ip4_route (device));

                /* device MAC*/
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "mac",
                                       nm_device_get_mac_address (device));

                /* device speed */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "speed",
                                       nm_device_get_speed (device));
        }

        if (nm_device_get_kind (device) == NM_DEVICE_KIND_ETHERNET) {

                /* IPv4 netmask */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "subnet",
                                       nm_device_get_ip4_subnet_mask (device));
        }

        if (nm_device_get_kind (device) == NM_DEVICE_KIND_GSM ||
            nm_device_get_kind (device) == NM_DEVICE_KIND_CDMA) {

                /* IMEI */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "imei",
                                       nm_device_get_modem_imei (device));

                /* operator name */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "provider",
                                       nm_device_get_operator_name (device));

                /* device speed: not sure where to get this data from... */
                panel_set_widget_data (panel,
                                       sub_pane,
                                       "speed",
                                       NULL);
        }

        /* refresh access point list too */
        if (nm_device_get_kind (device) == NM_DEVICE_KIND_WIFI) {
                nm_device_refresh_item_access_points (panel, device);
        }
out:
        return;
}

/**
 * nm_devices_treeview_clicked_cb:
 **/
static void
nm_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *widget;
        NmDevice *device;
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
        priv->current_device = g_strdup (nm_device_get_object_path (device));

        /* refresh device */
        nm_device_refresh_item_ui (panel, device);
out:
        return;
}

/**
 * panel_add_proxy_device:
 **/
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

/**
 * panel_enable_cb:
 **/
static void
panel_enable_cb (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
        GError *error = NULL;
        GVariant *result;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_printerr ("Error enabling NetworkManager: %s\n",
                            error->message);
                g_error_free (error);
                goto out;
        }
out:
        if (result != NULL)
                g_variant_unref (result);
}

/**
 * cc_network_panel_notify_enable_active_cb:
 **/
static void
cc_network_panel_notify_enable_active_cb (GtkSwitch *sw,
                                          GParamSpec *pspec,
                                          CcNetworkPanel *panel)
{
        gboolean enable;
        CcNetworkPanelPrivate *priv = panel->priv;

        /* get enabled state */
        enable = !gtk_switch_get_active (sw);

        /* get the new state */
        g_dbus_proxy_call (priv->proxy,
                           "Enable",
                           g_variant_new ("(b)", enable),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           panel_enable_cb,
                           panel);
}

/**
 * cc_network_panel_init:
 **/
static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        GError *error;
        gint value;
        GSettings *settings_tmp;
        GtkAdjustment *adjustment;
        GtkCellRenderer *renderer;
        GtkComboBox *combobox;
        GtkTreeSelection *selection;
        GtkTreeSortable *sortable;
        GtkWidget *widget;
        GtkStyleContext *context;

        panel->priv = NETWORK_PANEL_PRIVATE (panel);

        panel->priv->builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (panel->priv->builder,
                                   GNOMECC_UI_DIR "/network.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        panel->priv->cancellable = g_cancellable_new ();
        panel->priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify )g_object_unref);

        /* get initial icon state */
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.NetworkManager",
                                  "/org/freedesktop/NetworkManager",
                                  "org.freedesktop.NetworkManager",
                                  panel->priv->cancellable,
                                  panel_got_network_proxy_cb,
                                  panel);

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

        /* disable for now */
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
        g_signal_connect (GTK_SWITCH (widget), "notify::active",
                          G_CALLBACK (cc_network_panel_notify_enable_active_cb),
                          panel);

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

