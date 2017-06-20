/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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

#include <config.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "cc-network-panel.h"
#include "cc-network-resources.h"

#include <NetworkManager.h>

#include "net-device.h"
#include "net-device-mobile.h"
#include "net-device-wifi.h"
#include "net-device-ethernet.h"
#include "net-object.h"
#include "net-proxy.h"
#include "net-vpn.h"

#include "panel-common.h"

#include "network-dialogs.h"
#include "connection-editor/net-connection-editor.h"

#include <libmm-glib.h>

CC_PANEL_REGISTER (CcNetworkPanel, cc_network_panel)

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
        GtkBuilder       *builder;
        GtkWidget        *treeview;
        NMClient         *client;
        MMManager        *modem_manager;
        gboolean          updating_device;

        /* Killswitch stuff */
        GDBusProxy       *rfkill_proxy;
        GtkWidget        *kill_switch_header;
        GtkSwitch        *rfkill_switch;

        /* wireless dialog stuff */
        CmdlineOperation  arg_operation;
        gchar            *arg_device;
        gchar            *arg_access_point;
        gboolean          operation_done;
};

enum {
        PANEL_DEVICES_COLUMN_ICON,
        PANEL_DEVICES_COLUMN_OBJECT,
        PANEL_DEVICES_COLUMN_LAST
};

enum {
        PROP_0,
        PROP_PARAMETERS
};

static NetObject *find_in_model_by_id (CcNetworkPanel *panel, const gchar *id, GtkTreeIter *iter_out);
static void handle_argv (CcNetworkPanel *panel);

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
reset_command_line_args (CcNetworkPanel *self)
{
	self->priv->arg_operation = OPERATION_NULL;
	g_clear_pointer (&self->priv->arg_device, g_free);
	g_clear_pointer (&self->priv->arg_access_point, g_free);
}

static gboolean
verify_argv (CcNetworkPanel *self,
	     const char    **args)
{
	switch (self->priv->arg_operation) {
	case OPERATION_CONNECT_MOBILE:
	case OPERATION_CONNECT_8021X:
	case OPERATION_SHOW_DEVICE:
		if (self->priv->arg_device == NULL) {
			g_warning ("Operation %s requires an object path", args[0]);
		        return FALSE;
                }
	default:
		return TRUE;
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
        while (g_variant_iter_next (&iter, "v", &v)) {
                g_ptr_array_add (strv, (gpointer *)g_variant_get_string (v, NULL));
                g_variant_unref (v);
        }
        g_ptr_array_add (strv, NULL); /* NULL-terminate the strv data array */
        return strv;
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
        case PROP_PARAMETERS: {
                GVariant *parameters;

                reset_command_line_args (self);

                parameters = g_value_get_variant (value);
                if (parameters) {
                        GPtrArray *array;
                        const gchar **args;
                        array = variant_av_to_string_array (parameters);
                        args = (const gchar **) array->pdata;

                        g_debug ("Invoked with operation %s", args[0]);

                        if (args[0])
                                priv->arg_operation = cmdline_operation_from_string (args[0]);
                        if (args[0] && args[1])
                                priv->arg_device = g_strdup (args[1]);
                        if (args[0] && args[1] && args[2])
                                priv->arg_access_point = g_strdup (args[2]);

                        if (verify_argv (self, (const char **) args) == FALSE) {
                                reset_command_line_args (self);
                                g_ptr_array_unref (array);
                                return;
                        }
                        g_ptr_array_unref (array);
                        g_debug ("Calling handle_argv() after setting property");
                        handle_argv (self);
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

        if (priv->cancellable != NULL)
                g_cancellable_cancel (priv->cancellable);

        g_clear_object (&priv->cancellable);
        g_clear_object (&priv->rfkill_proxy);
        g_clear_object (&priv->builder);
        g_clear_object (&priv->client);
        g_clear_object (&priv->modem_manager);
        g_clear_object (&priv->kill_switch_header);
        priv->rfkill_switch = NULL;

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (object);

        reset_command_line_args (panel);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static const char *
cc_network_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/net";
}

static void
cc_network_panel_notify_enable_active_cb (GtkSwitch *sw,
                                          GParamSpec *pspec,
                                          CcNetworkPanel *panel)
{
        CcNetworkPanelPrivate *priv = panel->priv;
	gboolean enable;
	enable = gtk_switch_get_active (sw);
        g_dbus_proxy_call (priv->rfkill_proxy,
                           "org.freedesktop.DBus.Properties.Set",
                           g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
                                                 "'AirplaneMode', %v)",
                                                 g_variant_new_boolean (enable)),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           NULL, NULL);
}

static void
sync_airplane_mode_switch (CcNetworkPanel *panel)
{
        GVariant *result;
        gboolean enabled, should_show;
        gboolean hw_enabled;

        result = g_dbus_proxy_get_cached_property (panel->priv->rfkill_proxy, "HasAirplaneMode");
        enabled = g_variant_get_boolean (result);

        result = g_dbus_proxy_get_cached_property (panel->priv->rfkill_proxy, "ShouldShowAirplaneMode");
        should_show = g_variant_get_boolean (result);

        gtk_widget_set_visible (GTK_WIDGET (panel->priv->kill_switch_header), enabled && should_show);
        if (!enabled || !should_show)
                return;

        result = g_dbus_proxy_get_cached_property (panel->priv->rfkill_proxy, "AirplaneMode");
        enabled = g_variant_get_boolean (result);

        result = g_dbus_proxy_get_cached_property (panel->priv->rfkill_proxy, "HardwareAirplaneMode");
        hw_enabled = !!g_variant_get_boolean (result);

	enabled |= hw_enabled;

	if (enabled != gtk_switch_get_active (panel->priv->rfkill_switch)) {
		g_signal_handlers_block_by_func (panel->priv->rfkill_switch,
						 cc_network_panel_notify_enable_active_cb,
						 panel);
		gtk_switch_set_active (panel->priv->rfkill_switch, enabled);
		g_signal_handlers_unblock_by_func (panel->priv->rfkill_switch,
						 cc_network_panel_notify_enable_active_cb,
						 panel);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (panel->priv->rfkill_switch), !hw_enabled);
}

static void
on_property_change (GDBusProxy *proxy,
                    GVariant   *changed_properties,
                    GVariant   *invalidated_properties,
                    gpointer    user_data)
{
        sync_airplane_mode_switch (CC_NETWORK_PANEL (user_data));
}

static void
got_rfkill_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        panel->priv->rfkill_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (panel->priv->rfkill_proxy == NULL) {
                g_printerr ("Error creating rfkill proxy: %s\n", error->message);
                g_error_free (error);
                return;
        }

        g_signal_connect (panel->priv->rfkill_proxy, "g-properties-changed",
                          G_CALLBACK (on_property_change), panel);
        sync_airplane_mode_switch (panel);
}

static void
cc_network_panel_constructed (GObject *object)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (object);
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *widget;

        G_OBJECT_CLASS (cc_network_panel_parent_class)->constructed (object);

        /* add kill switch widgets  */
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
        /* TRANSLATORS: this is to disable the radio hardware in the
         * network panel */
        label = gtk_label_new_with_mnemonic (_("Air_plane Mode"));
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
        gtk_widget_set_visible (label, TRUE);
        widget = gtk_switch_new ();
        gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 4);
        gtk_widget_show_all (box);
        panel->priv->rfkill_switch = GTK_SWITCH (widget);
        cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (panel)), box);
        panel->priv->kill_switch_header = g_object_ref (box);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SettingsDaemon.Rfkill",
                                  "/org/gnome/SettingsDaemon/Rfkill",
                                  "org.gnome.SettingsDaemon.Rfkill",
                                  panel->priv->cancellable,
                                  got_rfkill_proxy_cb,
                                  panel);

        g_signal_connect (panel->priv->rfkill_switch, "notify::active",
                          G_CALLBACK (cc_network_panel_notify_enable_active_cb),
                          panel);
}

static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

        g_type_class_add_private (klass, sizeof (CcNetworkPanelPrivate));

	panel_class->get_help_uri = cc_network_panel_get_help_uri;

        object_class->get_property = cc_network_panel_get_property;
        object_class->set_property = cc_network_panel_set_property;
        object_class->dispose = cc_network_panel_dispose;
        object_class->finalize = cc_network_panel_finalize;
        object_class->constructed = cc_network_panel_constructed;

        g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static NetObject *
get_selected_object (CcNetworkPanel *panel)
{
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        NetObject *object = NULL;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->priv->treeview));
        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return NULL;
        }

        gtk_tree_model_get (model, &iter,
                            PANEL_DEVICES_COLUMN_OBJECT, &object,
                            -1);

        return object;
}

static void
select_first_device (CcNetworkPanel *panel)
{
        GtkTreePath *path;
        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->priv->treeview));

        /* select the first device */
        path = gtk_tree_path_new_from_string ("0");
        gtk_tree_selection_select_path (selection, path);
        gtk_tree_path_free (path);
}

static void
select_tree_iter (CcNetworkPanel *panel, GtkTreeIter *iter)
{
        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->priv->treeview));

        gtk_tree_selection_select_iter (selection, iter);
}

static void
object_removed_cb (NetObject *object, CcNetworkPanel *panel)
{
        gboolean ret;
        NetObject *object_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->priv->treeview));

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
                        if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter)) {
                                if (gtk_tree_model_get_iter_first (model, &iter))
                                        gtk_tree_selection_select_iter (selection, &iter);
                        }
                        break;
                }
                g_object_unref (object_tmp);
        } while (gtk_tree_model_iter_next (model, &iter));
}

GPtrArray *
cc_network_panel_get_devices (CcNetworkPanel *panel)
{
        GPtrArray *devices;
        GtkTreeModel *model;
        GtkTreeIter iter;
        NetObject *object;

        devices = g_ptr_array_new_with_free_func (g_object_unref);

        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        if (!gtk_tree_model_get_iter_first (model, &iter))
                return devices;

        do {
                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object,
                                    -1);
                if (NET_IS_DEVICE (object))
                        g_ptr_array_add (devices, object);
                else
                        g_object_unref (object);
        } while (gtk_tree_model_iter_next (model, &iter));

        return devices;
}

static gint
panel_net_object_get_sort_category (NetObject *net_object)
{
        if (NET_IS_DEVICE (net_object)) {
                return panel_device_get_sort_category (net_device_get_nm_device (NET_DEVICE (net_object)));
        } else if (NET_IS_PROXY (net_object)) {
                return 9;
        } else if (NET_IS_VPN (net_object)) {
                return 5;
        }

        g_assert_not_reached ();
}

static gint
panel_net_object_sort_func (GtkTreeModel *model, GtkTreeIter *a,
                            GtkTreeIter *b, void *data)
{
        g_autoptr(NetObject) obj_a = NULL;
        g_autoptr(NetObject) obj_b = NULL;
        gint cat_a, cat_b;
        const char *title_a, *title_b;

        gtk_tree_model_get (model, a,
                            PANEL_DEVICES_COLUMN_OBJECT, &obj_a,
                            -1);
        gtk_tree_model_get (model, b,
                            PANEL_DEVICES_COLUMN_OBJECT, &obj_b,
                            -1);

        cat_a = panel_net_object_get_sort_category (obj_a);
        cat_b = panel_net_object_get_sort_category (obj_b);

        if (cat_a != cat_b)
                return cat_a - cat_b;

        title_a = net_object_get_title (obj_a);
        title_b = net_object_get_title (obj_b);

        if (title_a == title_b)
                return 0;
        if (title_a == NULL)
                return -1;
        if (title_b == NULL)
                return 1;

        return g_utf8_collate (title_a, title_b);
}

static void
panel_net_object_notify_title_cb (NetObject *net_object, GParamSpec *pspec, CcNetworkPanel *panel)
{
        GtkTreeIter iter;
        GtkListStore *liststore;

        if (!find_in_model_by_id (panel, net_object_get_id (net_object), &iter))
                return;

        liststore = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                                            "liststore_devices"));

        /* gtk_tree_model_row_changed would not cause the list store to resort.
         * Instead set the object column to the current value.
         * See https://bugzilla.gnome.org/show_bug.cgi?id=782737 */
        gtk_list_store_set (liststore, &iter,
                            PANEL_DEVICES_COLUMN_OBJECT, net_object,
                           -1);
}

static void
panel_refresh_device_titles (CcNetworkPanel *panel)
{
        GPtrArray *ndarray, *nmdarray;
        NetDevice **devices;
        NMDevice **nm_devices, *nm_device;
        gchar **titles;
        gint i, num_devices;

        ndarray = cc_network_panel_get_devices (panel);
        if (!ndarray->len) {
                g_ptr_array_free (ndarray, TRUE);
                return;
        }

        nmdarray = g_ptr_array_new ();
        for (i = 0; i < ndarray->len; i++) {
                nm_device = net_device_get_nm_device (ndarray->pdata[i]);
                if (nm_device)
                        g_ptr_array_add (nmdarray, nm_device);
                else
                        g_ptr_array_remove_index (ndarray, i--);
        }

        devices = (NetDevice **)ndarray->pdata;
        nm_devices = (NMDevice **)nmdarray->pdata;
        num_devices = ndarray->len;

        titles = nm_device_disambiguate_names (nm_devices, num_devices);
        for (i = 0; i < num_devices; i++) {
                net_object_set_title (NET_OBJECT (devices[i]), titles[i]);
                g_free (titles[i]);
        }
        g_free (titles);
        g_ptr_array_free (ndarray, TRUE);
        g_ptr_array_free (nmdarray, TRUE);
}

static gboolean
handle_argv_for_device (CcNetworkPanel *panel,
			NMDevice       *device,
			GtkTreeIter    *iter)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        NMDeviceType type;
        GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));

        if (priv->arg_operation == OPERATION_NULL)
                return TRUE;

        type = nm_device_get_device_type (device);

        if (type == NM_DEVICE_TYPE_WIFI &&
            (priv->arg_operation == OPERATION_CREATE_WIFI ||
             priv->arg_operation == OPERATION_CONNECT_HIDDEN)) {
                g_debug ("Selecting wifi device");
                select_tree_iter (panel, iter);

                if (priv->arg_operation == OPERATION_CREATE_WIFI)
                        cc_network_panel_create_wifi_network (toplevel, priv->client);
                else
                        cc_network_panel_connect_to_hidden_network (toplevel, priv->client);

                reset_command_line_args (panel); /* done */
                return TRUE;
        } else if (g_strcmp0 (nm_object_get_path (NM_OBJECT (device)), priv->arg_device) == 0) {
                if (priv->arg_operation == OPERATION_CONNECT_MOBILE) {
                        cc_network_panel_connect_to_3g_network (toplevel, priv->client, device);

                        reset_command_line_args (panel); /* done */
                        select_tree_iter (panel, iter);
                        return TRUE;
                } else if (priv->arg_operation == OPERATION_CONNECT_8021X) {
                        cc_network_panel_connect_to_8021x_network (toplevel, priv->client, device, priv->arg_access_point);
                        reset_command_line_args (panel); /* done */
                        select_tree_iter (panel, iter);
                        return TRUE;
                }
                else if (priv->arg_operation == OPERATION_SHOW_DEVICE) {
                        select_tree_iter (panel, iter);
                        reset_command_line_args (panel); /* done */
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
handle_argv_for_connection (CcNetworkPanel *panel,
                            NMConnection   *connection,
                            GtkTreeIter    *iter)
{
        CcNetworkPanelPrivate *priv = panel->priv;

        if (priv->arg_operation == OPERATION_NULL)
                return TRUE;
        if (priv->arg_operation != OPERATION_SHOW_DEVICE)
                return FALSE;

        if (g_strcmp0 (nm_connection_get_path (connection), priv->arg_device) == 0) {
                reset_command_line_args (panel);
                select_tree_iter (panel, iter);
                return TRUE;
        }

        return FALSE;
}


static void
handle_argv (CcNetworkPanel *panel)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        gboolean ret;

        if (panel->priv->arg_operation == OPERATION_NULL)
                return;

        model = GTK_TREE_MODEL (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        while (ret) {
                GObject *object_tmp;
                NMDevice *device;
                NMConnection *connection;
                gboolean done = FALSE;

                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (NET_IS_DEVICE (object_tmp)) {
                        g_object_get (object_tmp, "nm-device", &device, NULL);
                        done = handle_argv_for_device (panel, device, &iter);
                        g_object_unref (device);
                } else if (NET_IS_VPN (object_tmp)) {
                        g_object_get (object_tmp, "connection", &connection, NULL);
                        done = handle_argv_for_connection (panel, connection, &iter);
                        g_object_unref (connection);
                }

                g_object_unref (object_tmp);

                if (done)
                        return;

                ret = gtk_tree_model_iter_next (model, &iter);
        }

        g_debug ("Could not handle argv operation, no matching device yet?");
}

static void
state_changed_cb (NMDevice *device,
                  NMDeviceState new_state,
                  NMDeviceState old_state,
                  NMDeviceStateReason reason,
                  CcNetworkPanel *panel)
{
        GtkListStore *store;
        GtkTreeIter iter;

        if (!find_in_model_by_id (panel, nm_device_get_udi (device), &iter)) {
                return;
        }

        store = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                                        "liststore_devices"));

        gtk_list_store_set (store, &iter,
                            PANEL_DEVICES_COLUMN_ICON, panel_device_to_icon_name (device, TRUE),
                           -1);
}

static gboolean
panel_add_device (CcNetworkPanel *panel, NMDevice *device)
{
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NMDeviceType type;
        NetDevice *net_device;
        CcNetworkPanelPrivate *priv = panel->priv;
        GtkNotebook *notebook;
        GtkSizeGroup *size_group;
        GType device_g_type;
        const char *udi;

        if (!nm_device_get_managed (device))
                goto out;

        /* do we have an existing object with this id? */
        udi = nm_device_get_udi (device);
        if (find_in_model_by_id (panel, udi, NULL) != NULL)
                goto out;

        type = nm_device_get_device_type (device);

        g_debug ("device %s type %i path %s",
                 udi, type, nm_object_get_path (NM_OBJECT (device)));

        /* map the NMDeviceType to the GType, or ignore */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
                device_g_type = NET_TYPE_DEVICE_ETHERNET;
                break;
        case NM_DEVICE_TYPE_MODEM:
                device_g_type = NET_TYPE_DEVICE_MOBILE;
                break;
        case NM_DEVICE_TYPE_WIFI:
                device_g_type = NET_TYPE_DEVICE_WIFI;
                break;
        /* not going to set up a cluster in GNOME */
        case NM_DEVICE_TYPE_VETH:
        /* enterprise features */
        case NM_DEVICE_TYPE_BOND:
        case NM_DEVICE_TYPE_TEAM:
        /* Don't need the libvirtd bridge */
        case NM_DEVICE_TYPE_BRIDGE:
        /* Don't add VPN devices */
        case NM_DEVICE_TYPE_TUN:
                goto out;
        default:
                device_g_type = NET_TYPE_DEVICE_SIMPLE;
                break;
        }

        /* create device */
        net_device = g_object_new (device_g_type,
                                   "panel", panel,
                                   "removable", FALSE,
                                   "cancellable", panel->priv->cancellable,
                                   "client", panel->priv->client,
                                   "nm-device", device,
                                   "id", nm_device_get_udi (device),
                                   NULL);

        if (type == NM_DEVICE_TYPE_MODEM &&
            g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager1/Modem/")) {
                GDBusObject *modem_object;

                if (priv->modem_manager == NULL) {
                        g_warning ("Cannot grab information for modem at %s: No ModemManager support",
                                   nm_device_get_udi (device));
                        goto out;
                }

                modem_object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (priv->modem_manager),
                                                                 nm_device_get_udi (device));
                if (modem_object == NULL) {
                        g_warning ("Cannot grab information for modem at %s: Not found",
                                   nm_device_get_udi (device));
                        goto out;
                }

                /* Set the modem object in the NetDeviceMobile */
                g_object_set (net_device,
                              "mm-object", modem_object,
                              NULL);
                g_object_unref (modem_object);
        }

        /* add as a panel */
        if (device_g_type != NET_TYPE_DEVICE) {
                notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->priv->builder,
                                                                 "notebook_types"));
                size_group = GTK_SIZE_GROUP (gtk_builder_get_object (panel->priv->builder,
                                                                     "sizegroup1"));
                net_object_add_to_notebook (NET_OBJECT (net_device),
                                            notebook,
                                            size_group);
        }

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        g_signal_connect_object (net_device, "removed",
                                 G_CALLBACK (object_removed_cb), panel, 0);
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, panel_device_to_icon_name (device, TRUE),
                            PANEL_DEVICES_COLUMN_OBJECT, net_device,
                            -1);
        g_signal_connect (net_device, "notify::title",
                          G_CALLBACK (panel_net_object_notify_title_cb), panel);

        g_object_unref (net_device);
        g_signal_connect (device, "state-changed",
                          G_CALLBACK (state_changed_cb), panel);

out:
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
get_object_title (GtkTreeViewColumn *column,
                  GtkCellRenderer   *cell,
                  GtkTreeModel      *model,
                  GtkTreeIter       *iter,
                  gpointer           data)
{
        NetObject *object;

        gtk_tree_model_get (model, iter,
                            PANEL_DEVICES_COLUMN_OBJECT, &object,
                            -1);
        if (!object)
                return;

        g_object_set (cell, "text", net_object_get_title (object), NULL);
        g_object_unref (object);
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
        g_object_set (renderer,
                      "width", 32,
                      "xalign", 1.0,
                      "stock-size", GTK_ICON_SIZE_MENU,
                      "follow-state", TRUE,
                      NULL);
        gtk_cell_renderer_set_padding (renderer, 4, 10);

        column = gtk_tree_view_column_new_with_attributes ("icon", renderer,
                                                           "icon-name", PANEL_DEVICES_COLUMN_ICON,
                                                           NULL);
        gtk_tree_view_append_column (treeview, column);

        /* column for text */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer,
                      "wrap-mode", PANGO_WRAP_WORD,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);
        column = gtk_tree_view_column_new_with_attributes ("title", renderer, NULL);
        gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (column),
                                                 renderer,
                                                 get_object_title,
                                                 NULL, NULL);
        gtk_tree_view_column_set_sort_column_id (column, PANEL_DEVICES_COLUMN_OBJECT);
        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_devices"));
        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (liststore_devices),
                                         PANEL_DEVICES_COLUMN_OBJECT,
                                         panel_net_object_sort_func, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore_devices),
                                              PANEL_DEVICES_COLUMN_OBJECT,
                                              GTK_SORT_ASCENDING);
        gtk_tree_view_append_column (treeview, column);
        gtk_tree_view_column_set_expand (column, TRUE);
}

static void
nm_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        const gchar *id_tmp;
        const gchar *needle;
        GList *l;
        GList *panels = NULL;
        GtkNotebook *notebook;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkWidget *widget;
        guint i = 0;
        NetObject *object = NULL;

        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                g_debug ("no row selected");
                goto out;
        }

        /* find the widget in the notebook that matches the object ID */
        object = get_selected_object (panel);
        needle = net_object_get_id (object);
        notebook = GTK_NOTEBOOK (gtk_builder_get_object (priv->builder,
                                                         "notebook_types"));
        panels = gtk_container_get_children (GTK_CONTAINER (notebook));
        for (l = panels; l != NULL; l = l->next) {
                widget = GTK_WIDGET (l->data);
                id_tmp = g_object_get_data (G_OBJECT (widget), "NetObject::id");
                if (g_strcmp0 (needle, id_tmp) == 0) {
                        gtk_notebook_set_current_page (notebook, i);

                        /* object is deletable? */
                        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                                     "remove_toolbutton"));
                        gtk_widget_set_sensitive (widget,
                                                  net_object_get_removable (object));
                        break;
                }
                i++;
        }
        g_object_unref (object);
out:
        g_list_free (panels);
}

static void
panel_add_proxy_device (CcNetworkPanel *panel)
{
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NetProxy *proxy;
        GtkNotebook *notebook;
        GtkSizeGroup *size_group;

        /* add proxy to notebook */
        proxy = net_proxy_new ();
        notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->priv->builder,
                                                         "notebook_types"));
        size_group = GTK_SIZE_GROUP (gtk_builder_get_object (panel->priv->builder,
                                                             "sizegroup1"));
        net_object_add_to_notebook (NET_OBJECT (proxy),
                                    notebook,
                                    size_group);

        /* add proxy to device list */
        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                            "liststore_devices"));
        net_object_set_title (NET_OBJECT (proxy), _("Network proxy"));
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "preferences-system-network-symbolic",
                            PANEL_DEVICES_COLUMN_OBJECT, proxy,
                            -1);

        /* NOTE: No connect to notify::title here as it is guaranteed to not
         *       be changed by anyone.*/

        g_object_unref (proxy);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, CcNetworkPanel *panel)
{
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
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object_path (connection));

                if (g_object_get_data (G_OBJECT (connection), "has-state-changed-handler") == NULL) {
                        g_signal_connect_object (connection, "notify::state",
                                                 G_CALLBACK (connection_state_changed), panel, 0);
                        g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (TRUE));
                }
        }
}

static void
device_added_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("New device added");
        panel_add_device (panel, device);
        panel_refresh_device_titles (panel);
}

static void
device_removed_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("Device removed");
        panel_remove_device (panel, device);
        panel_refresh_device_titles (panel);
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
        if (!nm_client_get_nm_running (client)) {
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

        panel_refresh_device_titles (panel);

        g_debug ("Calling handle_argv() after cold-plugging devices");
        handle_argv (panel);
}

static NetObject *
find_in_model_by_id (CcNetworkPanel *panel, const gchar *id, GtkTreeIter *iter_out)
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
        if (iter_out)
                *iter_out = iter;
        return object;
}

static void
panel_add_vpn_device (CcNetworkPanel *panel, NMConnection *connection)
{
        gchar *title;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NetVpn *net_vpn;
        const gchar *id;
        GtkNotebook *notebook;
        GtkSizeGroup *size_group;

        /* does already exist */
        id = nm_connection_get_path (connection);
        if (find_in_model_by_id (panel, id, NULL) != NULL)
                return;

        /* add as a VPN object */
        net_vpn = g_object_new (NET_TYPE_VPN,
                                "panel", panel,
                                "removable", TRUE,
                                "id", id,
                                "connection", connection,
                                "client", panel->priv->client,
                                NULL);
        g_signal_connect_object (net_vpn, "removed",
                                 G_CALLBACK (object_removed_cb), panel, 0);

        /* add as a panel */
        notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->priv->builder,
                                                         "notebook_types"));
        size_group = GTK_SIZE_GROUP (gtk_builder_get_object (panel->priv->builder,
                                                             "sizegroup1"));
        net_object_add_to_notebook (NET_OBJECT (net_vpn),
                                    notebook,
                                    size_group);

        liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (panel->priv->builder,
                                            "liststore_devices"));
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (connection));

        net_object_set_title (NET_OBJECT (net_vpn), title);
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "network-vpn-symbolic",
                            PANEL_DEVICES_COLUMN_OBJECT, net_vpn,
                            -1);
        g_signal_connect (net_vpn, "notify::title",
                          G_CALLBACK (panel_net_object_notify_title_cb), panel);

        g_free (title);
        g_object_unref (net_vpn);
}

static void
add_connection (CcNetworkPanel *panel,
                NMConnection *connection)
{
        NMSettingConnection *s_con;
        const gchar *type, *iface;

        s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection,
                                                                  NM_TYPE_SETTING_CONNECTION));
        type = nm_setting_connection_get_connection_type (s_con);
        iface = nm_connection_get_interface_name (connection);
        if (g_strcmp0 (type, "vpn") != 0 && iface == NULL)
                return;

        /* Don't add the libvirtd bridge to the UI */
        if (g_strcmp0 (nm_setting_connection_get_interface_name (s_con), "virbr0") == 0)
                return;

        g_debug ("add %s/%s remote connection: %s",
                 type, g_type_name_from_instance ((GTypeInstance*)connection),
                 nm_connection_get_path (connection));
        if (!iface)
                panel_add_vpn_device (panel, connection);
}

static void
notify_connection_added_cb (NMClient           *client,
                            NMRemoteConnection *connection,
                            CcNetworkPanel     *panel)
{
        add_connection (panel, NM_CONNECTION (connection));
}

static void
panel_check_network_manager_version (CcNetworkPanel *panel)
{
        GtkWidget *box;
        GtkWidget *label;
        gchar *markup;
        const gchar *version;

        /* parse running version */
        version = nm_client_get_version (panel->priv->client);
        if (version == NULL) {
                gtk_container_remove (GTK_CONTAINER (panel), gtk_bin_get_child (GTK_BIN (panel)));

                box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
                gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
                gtk_widget_set_vexpand (box, TRUE);
                gtk_container_add (GTK_CONTAINER (panel), box);

                label = gtk_label_new (_("Oops, something has gone wrong. Please contact your software vendor."));
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_widget_set_valign (label, GTK_ALIGN_END);
                gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

                markup = g_strdup_printf ("<small><tt>%s</tt></small>",
                                          _("NetworkManager needs to be running."));
                label = gtk_label_new (NULL);
                gtk_label_set_markup (GTK_LABEL (label), markup);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_widget_set_valign (label, GTK_ALIGN_START);
                gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

                gtk_widget_show_all (box);
                g_free (markup);
        } else {
                manager_running (panel->priv->client, NULL, panel);
        }
}

static void
editor_done (NetConnectionEditor *editor,
             gboolean             success,
             gpointer             user_data)
{
        g_object_unref (editor);
}

static void
add_connection_cb (GtkToolButton *button, CcNetworkPanel *panel)
{
        NetConnectionEditor *editor;
        GtkWindow *toplevel;

        toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel)));
        editor = net_connection_editor_new (toplevel, NULL, NULL, NULL,
                                            panel->priv->client);
        g_signal_connect (editor, "done", G_CALLBACK (editor_done), panel);
        net_connection_editor_run (editor);
}

static void
remove_connection (GtkToolButton *button, CcNetworkPanel *panel)
{
        NetObject *object;

        /* get current device */
        object = get_selected_object (panel);
        if (object == NULL)
                return;

        /* delete the object */
        net_object_delete (object);
        g_object_unref (object);
}

static void
on_toplevel_map (GtkWidget      *widget,
                 CcNetworkPanel *panel)
{
        /* is the user compiling against a new version, but not running
         * the daemon? */
        panel_check_network_manager_version (panel);
}

static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        GError *error = NULL;
        GtkStyleContext *context;
        GtkTreeSelection *selection;
        GtkWidget *widget;
        GtkWidget *toplevel;
        GDBusConnection *system_bus;
        GtkCssProvider *provider;
        const GPtrArray *connections;
        guint i;

        panel->priv = NETWORK_PANEL_PRIVATE (panel);
        g_resources_register (cc_network_get_resource ());

        panel->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (panel->priv->builder,
                                       "/org/gnome/control-center/network/network.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        panel->priv->cancellable = g_cancellable_new ();

        panel->priv->treeview = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                                    "treeview_devices"));
        panel_add_devices_columns (panel, GTK_TREE_VIEW (panel->priv->treeview));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->priv->treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
        g_signal_connect (selection, "changed",
                          G_CALLBACK (nm_devices_treeview_clicked_cb), panel);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "devices_scrolledwindow"));
        gtk_widget_set_size_request (widget, 200, -1);
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "devices_toolbar"));
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

        /* add the virtual proxy device */
        panel_add_proxy_device (panel);

        /* use NetworkManager client */
        panel->priv->client = nm_client_new (NULL, NULL);
        g_signal_connect (panel->priv->client, "notify::nm-running" ,
                          G_CALLBACK (manager_running), panel);
        g_signal_connect (panel->priv->client, "notify::active-connections",
                          G_CALLBACK (active_connections_changed), panel);
        g_signal_connect (panel->priv->client, "device-added",
                          G_CALLBACK (device_added_cb), panel);
        g_signal_connect (panel->priv->client, "device-removed",
                          G_CALLBACK (device_removed_cb), panel);

        /* Setup ModemManager client */
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (system_bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
                g_clear_error (&error);
        } else {
                panel->priv->modem_manager = mm_manager_new_sync (system_bus,
                                                                  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                                  NULL,
                                                                  &error);
                if (panel->priv->modem_manager == NULL) {
                        g_warning ("Error connecting to ModemManager: %s",
                                   error->message);
                        g_clear_error (&error);
                }
                g_object_unref (system_bus);
        }

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "add_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (add_connection_cb), panel);

        /* disable for now, until we actually show removable connections */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "remove_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (remove_connection), panel);

        /* add remote settings such as VPN settings as virtual devices */
        g_signal_connect (panel->priv->client, NM_CLIENT_CONNECTION_ADDED,
                          G_CALLBACK (notify_connection_added_cb), panel);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        g_signal_connect_after (toplevel, "map", G_CALLBACK (on_toplevel_map), panel);

        /* hide implementation details */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "notebook_types"));
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);

        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "vbox1"));
        gtk_container_add (GTK_CONTAINER (panel), widget);

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, ".circular-button { border-radius: 20px; -gtk-outline-radius: 20px; }", -1, NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);

        /* Cold-plug existing connections */
        connections = nm_client_get_connections (panel->priv->client);
        for (i = 0; i < connections->len; i++)
                add_connection (panel, connections->pdata[i]);

        g_debug ("Calling handle_argv() after cold-plugging connections");
        handle_argv (panel);
}
