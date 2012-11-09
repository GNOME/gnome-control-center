/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
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
#include <stdlib.h>

#include "cc-network-panel.h"

#include "nm-remote-settings.h"
#include "nm-client.h"
#include "nm-device.h"
#include "nm-device-modem.h"

#include "net-device.h"
#include "net-device-mobile.h"
#include "net-device-wifi.h"
#include "net-device-wired.h"
#include "net-object.h"
#include "net-proxy.h"
#include "net-vpn.h"

#include "rfkill-glib.h"

#include "panel-common.h"

#include "network-dialogs.h"

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
        NMRemoteSettings *remote_settings;
        gboolean          updating_device;
        guint             add_header_widgets_idle;
        guint             nm_warning_idle;
        guint             refresh_idle;

        /* Killswitch stuff */
        GtkWidget        *kill_switch_header;
        CcRfkillGlib       *rfkill;
        GtkSwitch        *rfkill_switch;
        GHashTable       *killswitches;

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
        PROP_0,
        PROP_ARGV
};

static NetObject *find_in_model_by_id (CcNetworkPanel *panel, const gchar *id);
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

                reset_command_line_args (self);

                args = g_value_get_boxed (value);

                if (args) {
                        g_debug ("Invoked with operation %s", args[0]);

                        if (args[0])
                                priv->arg_operation = cmdline_operation_from_string (args[0]);
                        if (args[0] && args[1])
                                priv->arg_device = g_strdup (args[1]);
                        if (args[0] && args[1] && args[2])
                                priv->arg_access_point = g_strdup (args[2]);

                        if (verify_argv (self, (const char **) args) == FALSE) {
                                reset_command_line_args (self);
                                return;
                        }

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
        g_clear_object (&priv->builder);
        g_clear_object (&priv->client);
        g_clear_object (&priv->remote_settings);
        g_clear_object (&priv->kill_switch_header);
        g_clear_object (&priv->rfkill);
        g_clear_pointer (&priv->killswitches, g_hash_table_destroy);
        priv->rfkill_switch = NULL;

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

        g_object_class_override_property (object_class, PROP_ARGV, "argv");
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
                        if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
                                gtk_tree_model_get_iter_first (model, &iter);
                        gtk_tree_selection_select_iter (selection, &iter);

                        break;
                }
                g_object_unref (object_tmp);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static gboolean
handle_argv_for_device (CcNetworkPanel *panel,
			NMDevice       *device,
			GtkTreeIter    *iter)
{
        CcNetworkPanelPrivate *priv = panel->priv;
        NMDeviceType type;

        if (priv->arg_operation == OPERATION_NULL)
                return TRUE;

        type = nm_device_get_device_type (device);

        if (type == NM_DEVICE_TYPE_WIFI &&
            (priv->arg_operation == OPERATION_CREATE_WIFI ||
             priv->arg_operation == OPERATION_CONNECT_HIDDEN)) {
                g_debug ("Selecting wifi device");
                select_tree_iter (panel, iter);

                if (priv->arg_operation == OPERATION_CREATE_WIFI)
                        cc_network_panel_create_wifi_network (panel, priv->client, priv->remote_settings);
                else
                        cc_network_panel_connect_to_hidden_network (panel, priv->client, priv->remote_settings);

                reset_command_line_args (panel); /* done */
                return TRUE;
        } else if (g_strcmp0 (nm_object_get_path (NM_OBJECT (device)), priv->arg_device) == 0) {
                if (priv->arg_operation == OPERATION_CONNECT_MOBILE) {
                        cc_network_panel_connect_to_3g_network (panel, priv->client, priv->remote_settings, device);

                        reset_command_line_args (panel); /* done */
                        select_tree_iter (panel, iter);
                        return TRUE;
                } else if (priv->arg_operation == OPERATION_CONNECT_8021X) {
                        cc_network_panel_connect_to_8021x_network (panel, priv->client, priv->remote_settings, device, priv->arg_access_point);
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
                gboolean done = FALSE;

                gtk_tree_model_get (model, &iter,
                                    PANEL_DEVICES_COLUMN_OBJECT, &object_tmp,
                                    -1);
                if (g_object_class_find_property (G_OBJECT_GET_CLASS (object_tmp), "nm-device") != NULL) {
                        g_object_get (object_tmp, "nm-device", &device, NULL);
                        done = handle_argv_for_device (panel, device, &iter);
                        g_object_unref (device);
                }

                g_object_unref (object_tmp);

                if (done)
                        return;

                ret = gtk_tree_model_iter_next (model, &iter);
        }

        g_debug ("Could not handle argv operation, no matching device yet?");
}

static gboolean
panel_add_device (CcNetworkPanel *panel, NMDevice *device)
{
        const gchar *title;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NMDeviceType type;
        NetDevice *net_device;
        CcNetworkPanelPrivate *priv = panel->priv;
        GtkNotebook *notebook;
        GtkSizeGroup *size_group;
        GType device_g_type;

        /* do we have an existing object with this id? */
        if (find_in_model_by_id (panel, nm_device_get_udi (device)) != NULL)
                goto out;

        type = nm_device_get_device_type (device);

        g_debug ("device %s type %i path %s",
                 nm_device_get_udi (device), type, nm_object_get_path (NM_OBJECT (device)));

        /* map the NMDeviceType to the GType */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
                device_g_type = NET_TYPE_DEVICE_WIRED;
                break;
        case NM_DEVICE_TYPE_MODEM:
                device_g_type = NET_TYPE_DEVICE_MOBILE;
                break;
        case NM_DEVICE_TYPE_WIFI:
                device_g_type = NET_TYPE_DEVICE_WIFI;
                break;
        default:
                goto out;
        }

        /* create device */
        title = panel_device_to_localized_string (device);
        net_device = g_object_new (device_g_type,
                                   "panel", panel,
                                   "removable", FALSE,
                                   "cancellable", panel->priv->cancellable,
                                   "client", panel->priv->client,
                                   "remote-settings", panel->priv->remote_settings,
                                   "nm-device", device,
                                   "id", nm_device_get_udi (device),
                                   "title", title,
                                   NULL);

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
                            PANEL_DEVICES_COLUMN_ICON, panel_device_to_icon_name (device),
                            PANEL_DEVICES_COLUMN_SORT, panel_device_to_sortable_string (device),
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_OBJECT, net_device,
                            -1);

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
        column = gtk_tree_view_column_new_with_attributes ("title", renderer,
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
out:
        g_list_free (panels);
}

static void
panel_add_proxy_device (CcNetworkPanel *panel)
{
        gchar *title;
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
        title = g_strdup_printf ("%s", _("Network proxy"));
        gtk_list_store_append (liststore_devices, &iter);
        gtk_list_store_set (liststore_devices,
                            &iter,
                            PANEL_DEVICES_COLUMN_ICON, "preferences-system-network",
                            PANEL_DEVICES_COLUMN_TITLE, title,
                            PANEL_DEVICES_COLUMN_SORT, "9",
                            PANEL_DEVICES_COLUMN_OBJECT, proxy,
                            -1);
        g_free (title);
        g_object_unref (proxy);
}

static void
cc_network_panel_notify_enable_active_cb (GtkSwitch *sw,
                                          GParamSpec *pspec,
                                          CcNetworkPanel *panel)
{
	gboolean enable;
	struct rfkill_event event;

	enable = gtk_switch_get_active (sw);
	g_debug ("Setting killswitch to %d", enable);

	memset (&event, 0, sizeof(event));
	event.op = RFKILL_OP_CHANGE_ALL;
	event.type = RFKILL_TYPE_ALL;
	event.soft = enable ? 1 : 0;
	if (cc_rfkill_glib_send_event (panel->priv->rfkill, &event) < 0)
		g_warning ("Setting the killswitch %s failed", enable ? "on" : "off");
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
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object (connection));

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

        g_debug ("Calling handle_argv() after cold-plugging devices");
        handle_argv (panel);
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

static void
panel_add_vpn_device (CcNetworkPanel *panel, NMConnection *connection)
{
        gchar *title;
        gchar *title_markup;
        GtkListStore *liststore_devices;
        GtkTreeIter iter;
        NetVpn *net_vpn;
        const gchar *id;
        GtkNotebook *notebook;
        GtkSizeGroup *size_group;

        /* does already exist */
        id = nm_connection_get_path (connection);
        if (find_in_model_by_id (panel, id) != NULL)
                return;

        /* add as a virtual object */
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
remove_connection (GtkToolButton *button, CcNetworkPanel *panel)
{
        NetObject *object;

        /* get current device */
        object = get_selected_object (panel);
        if (object == NULL)
                return;

        /* delete the object */
        net_object_delete (object);
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
rfkill_changed (CcRfkillGlib     *rfkill,
		GList          *events,
		CcNetworkPanel *panel)
{
	gboolean enabled;
	GList *l;
	GHashTableIter iter;
	gpointer key, value;

	enabled = TRUE;

	for (l = events; l != NULL; l = l->next) {
		struct rfkill_event *event = l->data;

		if (event->op == RFKILL_OP_ADD)
			g_hash_table_insert (panel->priv->killswitches,
					     GINT_TO_POINTER (event->idx),
					     GINT_TO_POINTER (event->soft || event->hard));
		else if (event->op == RFKILL_OP_CHANGE)
			g_hash_table_insert (panel->priv->killswitches,
					     GINT_TO_POINTER (event->idx),
					     GINT_TO_POINTER (event->soft || event->hard));
		else if (event->op == RFKILL_OP_DEL)
			g_hash_table_remove (panel->priv->killswitches,
					     GINT_TO_POINTER (event->idx));
	}

	g_hash_table_iter_init (&iter, panel->priv->killswitches);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		int idx, state;

		idx = GPOINTER_TO_INT (key);
		state = GPOINTER_TO_INT (value);
		g_debug ("Killswitch %d is %s", idx, state ? "enabled" : "disabled");

		/* A single device that's enabled? airplane mode is off */
		if (state == FALSE) {
			enabled = FALSE;
			break;
		}
	}

	if (enabled != gtk_switch_get_active (panel->priv->rfkill_switch)) {
		g_signal_handlers_block_by_func (panel->priv->rfkill_switch,
						 cc_network_panel_notify_enable_active_cb,
						 panel);
		gtk_switch_set_active (panel->priv->rfkill_switch, enabled);
		g_signal_handlers_unblock_by_func (panel->priv->rfkill_switch,
						 cc_network_panel_notify_enable_active_cb,
						 panel);
	}
}

static gboolean
network_add_shell_header_widgets_cb (gpointer user_data)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *widget;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
        /* TRANSLATORS: this is to disable the radio hardware in the
         * network panel */
        label = gtk_label_new_with_mnemonic (_("Air_plane Mode"));
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
        gtk_widget_set_visible (label, TRUE);
        widget = gtk_switch_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
        gtk_widget_show_all (box);
        panel->priv->rfkill_switch = GTK_SWITCH (widget);
        cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (panel)), box);
        panel->priv->kill_switch_header = g_object_ref (box);

        panel->priv->killswitches = g_hash_table_new (g_direct_hash, g_direct_equal);
        panel->priv->rfkill = cc_rfkill_glib_new ();
        g_signal_connect (G_OBJECT (panel->priv->rfkill), "changed",
                          G_CALLBACK (rfkill_changed), panel);
        if (cc_rfkill_glib_open (panel->priv->rfkill) < 0)
                gtk_widget_hide (box);

        g_signal_connect (panel->priv->rfkill_switch, "notify::active",
                          G_CALLBACK (cc_network_panel_notify_enable_active_cb),
                          panel);

        return FALSE;
}

static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        DBusGConnection *bus = NULL;
        GError *error = NULL;
        GtkStyleContext *context;
        GtkTreeSelection *selection;
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
                                                     "add_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (add_connection_cb), panel);

        /* disable for now, until we actually show removable connections */
        widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                                     "remove_toolbutton"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (remove_connection), panel);

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
