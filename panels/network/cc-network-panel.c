/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
#include "panel-common.h"

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

struct _CcNetworkPanelPrivate
{
	GCancellable	*cancellable;
	gchar		*current_device;
	GDBusProxy	*proxy;
	GPtrArray	*devices;
	GSettings	*proxy_settings;
	GtkBuilder	*builder;
};


typedef struct {
	CcNetworkPanel	*panel;
	gchar		*active_access_point;
	gchar		*device_id;
	gchar		*modem_imei;
	gchar		*operator_name;
	gchar		*udi;
	GDBusProxy	*proxy;
	GDBusProxy	*proxy_additional;
	guint		 type;
} PanelDeviceItem;

typedef struct {
	gchar		*access_point;
	gchar		*active_access_point;
	guint		 strength;
	guint		 mode;
	CcNetworkPanel	*panel;
} PanelAccessPointItem;

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

static void	panel_device_refresh_item_ui		(PanelDeviceItem *item);

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
panel_free_device_item (PanelDeviceItem *item)
{
	g_object_unref (item->panel);
	g_object_unref (item->proxy);
	if (item->proxy_additional != NULL)
		g_object_unref (item->proxy_additional);
	g_free (item->device_id);
	g_free (item->active_access_point);
	g_free (item->udi);
	g_free (item->operator_name);
	g_free (item->modem_imei);
	g_free (item);
}

/**
 * panel_add_device_to_listview:
 **/
static void
panel_add_device_to_listview (PanelDeviceItem *item)
{
	GtkListStore *liststore_devices;
	GtkTreeIter iter;
	gchar *title = NULL;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	g_debug ("device %s type %i", item->device_id, item->type);

	/* make title a bit bigger */
	title = g_strdup_printf ("<span size=\"large\">%s</span>",
				 panel_device_type_to_localized_string (item->type));

	liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
					    "liststore_devices"));
	gtk_list_store_append (liststore_devices, &iter);
	gtk_list_store_set (liststore_devices,
			    &iter,
			    PANEL_DEVICES_COLUMN_ICON, panel_device_type_to_icon_name (item->type),
			    PANEL_DEVICES_COLUMN_SORT, panel_device_type_to_sortable_string (item->type),
			    PANEL_DEVICES_COLUMN_TITLE, title,
			    PANEL_DEVICES_COLUMN_ID, item->device_id,
			    PANEL_DEVICES_COLUMN_TOOLTIP, NULL,
			    PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, item,
			    -1);
	g_free (title);
}

/**
 * panel_got_proxy_access_point_cb:
 **/
static void
panel_got_proxy_access_point_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	gchar *ssid = NULL;
	gchar tmp;
	GDBusProxy *proxy;
	GError *error = NULL;
	gsize len;
	GtkListStore *liststore_wireless_network;
	GtkTreeIter treeiter;
	GtkWidget *widget;
	guint i = 0;
	GVariantIter iter;
	GVariant *variant_mode = NULL;
	GVariant *variant_ssid = NULL;
	GVariant *variant_strength = NULL;
	PanelAccessPointItem *ap_item = (PanelAccessPointItem *) user_data;
	CcNetworkPanelPrivate *priv = ap_item->panel->priv;

	proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the strength */
	variant_strength = g_dbus_proxy_get_cached_property (proxy, "Strength");
	ap_item->strength = g_variant_get_byte (variant_strength);

	/* get the mode */
	variant_mode = g_dbus_proxy_get_cached_property (proxy, "Mode");
	ap_item->mode = g_variant_get_uint32 (variant_mode);

	/* get the (non NULL terminated, urgh) SSID */
	variant_ssid = g_dbus_proxy_get_cached_property (proxy, "Ssid");
	len = g_variant_iter_init (&iter, variant_ssid);
	if (len == 0) {
		g_warning ("invalid ssid?!");
		goto out;
	}

	/* decode each byte */
	ssid = g_new0 (gchar, len + 1);
	while (g_variant_iter_loop (&iter, "y", &tmp))
		ssid[i++] = tmp;
	g_debug ("adding access point %s (%i%%) [%i]", ssid, ap_item->strength, ap_item->mode);

	/* add to the model */
	liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
						     "liststore_wireless_network"));
	gtk_list_store_append (liststore_wireless_network, &treeiter);
	gtk_list_store_set (liststore_wireless_network,
			    &treeiter,
			    PANEL_WIRELESS_COLUMN_ID, ap_item->access_point,
			    PANEL_WIRELESS_COLUMN_TITLE, ssid,
			    PANEL_WIRELESS_COLUMN_SORT, ssid,
			    PANEL_WIRELESS_COLUMN_STRENGTH, ap_item->strength,
			    PANEL_WIRELESS_COLUMN_MODE, ap_item->mode,
			    -1);

	/* is this what we're on already? */
	if (g_strcmp0 (ap_item->access_point,
		       ap_item->active_access_point) == 0) {
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
							     "combobox_network_name"));
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &treeiter);
	}
out:
	g_free (ap_item->access_point);
	g_free (ap_item->active_access_point);
	g_object_unref (ap_item->panel);
	g_free (ap_item);
	g_free (ssid);
	if (variant_ssid != NULL)
		g_variant_unref (variant_ssid);
	if (variant_strength != NULL)
		g_variant_unref (variant_strength);
	if (variant_mode != NULL)
		g_variant_unref (variant_mode);
	if (proxy != NULL)
		g_object_unref (proxy);
	return;
}


/**
 * panel_get_access_point_data:
 **/
static void
panel_get_access_point_data (PanelDeviceItem *item, const gchar *access_point_id)
{
	PanelAccessPointItem *ap_item;

	ap_item = g_new0 (PanelAccessPointItem, 1);
	ap_item->access_point = g_strdup (access_point_id);
	ap_item->active_access_point = g_strdup (item->active_access_point);
	ap_item->panel = g_object_ref (item->panel);

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.freedesktop.NetworkManager",
				  access_point_id,
				  "org.freedesktop.NetworkManager.AccessPoint",
				  item->panel->priv->cancellable,
				  panel_got_proxy_access_point_cb,
				  ap_item);
}

/**
 * panel_get_access_points_cb:
 **/
static void
panel_get_access_points_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	const gchar *object_path;
	GError *error = NULL;
	gsize len;
	GVariantIter iter;
	GVariant *result = NULL;
	GVariant *test;
	GtkListStore *liststore_wireless_network;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (result == NULL) {
		g_printerr ("Error getting access points: %s\n", error->message);
		g_error_free (error);
		return;
	}

	/* clear list of access points */
	liststore_wireless_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
						     "liststore_wireless_network"));
	gtk_list_store_clear (liststore_wireless_network);

	test = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, test);
	if (len == 0) {
		g_warning ("no access points?!");
		goto out;
	}

	/* for each entry in the array */
	while (g_variant_iter_loop (&iter, "o", &object_path)) {
		g_debug ("adding access point %s", object_path);
		panel_get_access_point_data (item, object_path);
	}
out:
	g_variant_unref (result);
	g_variant_unref (test);
}

/**
 * panel_got_device_proxy_additional_cb:
 **/
static void
panel_got_device_proxy_additional_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *result = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	item->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy_additional == NULL) {
		g_printerr ("Error creating additional proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* async populate the list of access points */
	if (item->type == NM_DEVICE_TYPE_WIFI) {

		/* get the currently active access point */
		result = g_dbus_proxy_get_cached_property (item->proxy_additional, "ActiveAccessPoint");
		item->active_access_point = g_variant_dup_string (result, NULL);

		g_dbus_proxy_call (item->proxy_additional,
				   "GetAccessPoints",
				   NULL,
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   item->panel->priv->cancellable,
				   panel_get_access_points_cb,
				   item);
	}

	panel_add_device_to_listview (item);
out:
	if (result != NULL)
		g_variant_unref (result);
	return;
}

/**
 * panel_get_registration_info_cb:
 **/
static void
panel_get_registration_info_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	gchar *operator_code = NULL;
	GError *error = NULL;
	guint registration_status;
	GVariant *result = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (result == NULL) {
		g_printerr ("Error getting registration info: %s\n", error->message);
		g_error_free (error);
		return;
	}

	/* get values */
	g_variant_get (result, "((uss))",
		       &registration_status,
		       &operator_code,
		       &item->operator_name);

	g_free (operator_code);
	g_variant_unref (result);
}

/**
 * panel_got_device_proxy_modem_manager_gsm_network_cb:
 **/
static void
panel_got_device_proxy_modem_manager_gsm_network_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *result = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	item->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy_additional == NULL) {
		g_printerr ("Error creating additional proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the currently active access point */
	result = g_dbus_proxy_get_cached_property (item->proxy_additional, "AccessTechnology");
//	item->active_access_point = g_variant_dup_string (result, NULL);

	g_dbus_proxy_call (item->proxy_additional,
			   "GetRegistrationInfo",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   item->panel->priv->cancellable,
			   panel_get_registration_info_cb,
			   item);

	panel_add_device_to_listview (item);
out:
	if (result != NULL)
		g_variant_unref (result);
	return;
}

/**
 * panel_got_device_proxy_modem_manager_cb:
 **/
static void
panel_got_device_proxy_modem_manager_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *result = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	item->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy_additional == NULL) {
		g_printerr ("Error creating additional proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the IMEI */
	result = g_dbus_proxy_get_cached_property (item->proxy_additional, "EquipmentIdentifier");
	item->modem_imei = g_variant_dup_string (result, NULL);
out:
	if (result != NULL)
		g_variant_unref (result);
	return;
}

/**
 * panel_device_properties_changed_cb:
 **/
static void
panel_device_properties_changed_cb (GDBusProxy *proxy,
				    GVariant *changed_properties,
				    const gchar* const *invalidated_properties,
				    gpointer user_data)
{
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	/* only refresh the selected device */
	if (g_strcmp0 (priv->current_device, item->device_id) == 0)
		panel_device_refresh_item_ui (item);
//xxx
}

/**
 * panel_got_device_proxy_cb:
 **/
static void
panel_got_device_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *variant_type = NULL;
	GVariant *variant_udi = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	item->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the UDI, so we can query ModemManager devices */
	variant_udi = g_dbus_proxy_get_cached_property (item->proxy, "Udi");
	g_variant_get (variant_udi, "s", &item->udi);

	/* get the additional interface for this device type */
	variant_type = g_dbus_proxy_get_cached_property (item->proxy, "DeviceType");
	g_variant_get (variant_type, "u", &item->type);
	if (item->type == NM_DEVICE_TYPE_ETHERNET) {
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  "org.freedesktop.NetworkManager",
					  item->device_id,
					  "org.freedesktop.NetworkManager.Device.Wired",
					  item->panel->priv->cancellable,
					  panel_got_device_proxy_additional_cb,
					  item);
	} else if (item->type == NM_DEVICE_TYPE_WIFI) {
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  "org.freedesktop.NetworkManager",
					  item->device_id,
					  "org.freedesktop.NetworkManager.Device.Wireless",
					  item->panel->priv->cancellable,
					  panel_got_device_proxy_additional_cb,
					  item);
	} else if (item->type == NM_DEVICE_TYPE_GSM ||
		   item->type == NM_DEVICE_TYPE_CDMA) {
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  "org.freedesktop.ModemManager",
					  item->udi,
					  "org.freedesktop.ModemManager.Modem",
					  item->panel->priv->cancellable,
					  panel_got_device_proxy_modem_manager_cb,
					  item);
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  "org.freedesktop.ModemManager",
					  item->udi,
					  "org.freedesktop.ModemManager.Modem.Gsm.Network",
					  item->panel->priv->cancellable,
					  panel_got_device_proxy_modem_manager_gsm_network_cb,
					  item);
	} else {
		panel_add_device_to_listview (item);
	}

	/* we want to update the UI */
	g_signal_connect (item->proxy, "g-properties-changed",
			  G_CALLBACK (panel_device_properties_changed_cb),
			  item);
out:
	if (variant_udi != NULL)
		g_variant_unref (variant_udi);
	if (variant_type != NULL)
		g_variant_unref (variant_type);
	return;
}

/**
 * panel_add_device:
 **/
static void
panel_add_device (CcNetworkPanel *panel, const gchar *device_id)
{
	PanelDeviceItem *item;

	/* create temp device */
	item = g_new0 (PanelDeviceItem, 1);
	item->panel = g_object_ref (panel);
	item->device_id = g_strdup (device_id);

	/* add to array */
	g_ptr_array_add (panel->priv->devices, item);

	/* get initial device state */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.freedesktop.NetworkManager",
				  device_id,
				  "org.freedesktop.NetworkManager.Device",
				  panel->priv->cancellable,
				  panel_got_device_proxy_cb,
				  item);
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
	PanelDeviceItem *item;

	/* remove device from array */
	for (i=0; i<panel->priv->devices->len; i++) {
		item = g_ptr_array_index (panel->priv->devices, i);
		if (g_strcmp0 (item->device_id, device_id) == 0) {
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
 * panel_got_network_proxy_cb:
 **/
static void
panel_got_network_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (user_data)->priv;

	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (priv->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* we want to change the UI in reflection to device events */
	g_signal_connect (priv->proxy,
			  "g-signal",
			  G_CALLBACK (panel_dbus_manager_signal_cb),
			  user_data);

	/* get the new state */
	g_dbus_proxy_call (priv->proxy,
			   "GetDevices",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   priv->cancellable,
			   panel_get_devices_cb,
			   user_data);
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
 * panel_set_label_for_variant_string:
 **/
static void
panel_set_label_for_variant_string (GtkWidget *widget, GVariant *variant)
{
	gchar *tmp;
	g_variant_get (variant, "s", &tmp);
	gtk_label_set_label (GTK_LABEL (widget), tmp);
	g_free (tmp);
}

/**
 * panel_set_label_for_variant_speed:
 **/
static void
panel_set_label_for_variant_speed (GtkWidget *widget, GVariant *variant)
{
	guint speed;
	gchar *tmp;

	/* format with correct scale */
	g_variant_get (variant, "u", &speed);
	if (speed < 1000) {
		tmp = g_strdup_printf (_("%i Mb/s"), speed);
	} else {
		tmp = g_strdup_printf (_("%i Gb/s"), speed / 1000);
	}
	gtk_label_set_label (GTK_LABEL (widget), tmp);
	g_free (tmp);
}

/**
 * panel_set_label_for_variant_bitrate:
 **/
static void
panel_set_label_for_variant_bitrate (GtkWidget *widget, GVariant *variant)
{
	guint bitrate;
	gchar *tmp;

	/* format with correct scale */
	g_variant_get (variant, "u", &bitrate);
	if (bitrate < 1000) {
		tmp = g_strdup_printf (_("%i kb/s"), bitrate);
	} else {
		tmp = g_strdup_printf (_("%i Mb/s"), bitrate / 1000);
	}
	gtk_label_set_label (GTK_LABEL (widget), tmp);
	g_free (tmp);
}

/**
 * panel_set_label_for_variant_ipv4:
 **/
static void
panel_set_label_for_variant_ipv4 (GtkWidget *widget, GVariant *variant)
{
	gchar *ip_str;
	guint32 ip;

	g_variant_get (variant, "u", &ip);
	ip_str = g_strdup_printf ("%i.%i.%i.%i",
				    ip & 0x000000ff,
				   (ip & 0x0000ff00) / 0x100,
				   (ip & 0x00ff0000) / 0x10000,
				   (ip & 0xff000000) / 0x1000000);
	gtk_label_set_label (GTK_LABEL (widget), ip_str);
	g_free (ip_str);
}

/**
 * panel_populate_wired_device:
 **/
static void
panel_populate_wired_device (PanelDeviceItem *item)
{
	GtkWidget *widget;
	GVariant *ip4;
	GVariant *hw_address;
	GVariant *speed;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	/* set IP */
	ip4 = g_dbus_proxy_get_cached_property (item->proxy, "Ip4Address");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wired_ip"));
	panel_set_label_for_variant_ipv4 (widget, ip4);

	/* set MAC */
	hw_address = g_dbus_proxy_get_cached_property (item->proxy_additional, "HwAddress");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wired_mac"));
	panel_set_label_for_variant_string (widget, hw_address);

	/* set speed */
	speed = g_dbus_proxy_get_cached_property (item->proxy_additional, "Speed");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wired_speed"));
	panel_set_label_for_variant_speed (widget, speed);

	g_variant_unref (ip4);
	g_variant_unref (hw_address);
	g_variant_unref (speed);
}

/**
 * panel_populate_wireless_device:
 **/
static void
panel_populate_wireless_device (PanelDeviceItem *item)
{
	GtkWidget *widget;
	GVariant *bitrate;
	GVariant *hw_address;
	GVariant *ip4;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	/* set IP */
	ip4 = g_dbus_proxy_get_cached_property (item->proxy, "Ip4Address");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wireless_ip"));
	panel_set_label_for_variant_ipv4 (widget, ip4);

	/* set MAC */
	hw_address = g_dbus_proxy_get_cached_property (item->proxy_additional, "HwAddress");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wireless_mac"));
	panel_set_label_for_variant_string (widget, hw_address);

	/* set speed */
	bitrate = g_dbus_proxy_get_cached_property (item->proxy_additional, "Bitrate");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wireless_speed"));
	panel_set_label_for_variant_bitrate (widget, bitrate);

	g_variant_unref (ip4);
	g_variant_unref (hw_address);
	g_variant_unref (bitrate);
}

/**
 * panel_populate_mobilebb_device:
 **/
static void
panel_populate_mobilebb_device (PanelDeviceItem *item)
{
	GtkWidget *widget;
	GVariant *ip4;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	/* set IP */
	ip4 = g_dbus_proxy_get_cached_property (item->proxy, "Ip4Address");
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_wireless_ip"));
	panel_set_label_for_variant_ipv4 (widget, ip4);

	/* use data from ModemManager */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_mobilebb_provider"));
	gtk_label_set_text (GTK_LABEL (widget), item->operator_name);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_mobilebb_imei"));
	gtk_label_set_text (GTK_LABEL (widget), item->modem_imei);

	/* I'm not sure where to get this data from */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "hbox_mobilebb_speed"));
	gtk_widget_set_visible (widget, FALSE);

	g_variant_unref (ip4);
}

/**
 * panel_device_refresh_item_ui:
 **/
static void
panel_device_refresh_item_ui (PanelDeviceItem *item)
{
	GtkWidget *widget;
	guint state;
	GVariant *variant_id;
	GVariant *variant_state;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	/* we have a new device */
	g_debug ("selected device is: %s", item->device_id);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "hbox_device_header"));
	gtk_widget_set_visible (widget, TRUE);

	variant_id = g_dbus_proxy_get_cached_property (item->proxy, "Interface");
//	g_variant_get (variant_id, "s", &interface);

	variant_state = g_dbus_proxy_get_cached_property (item->proxy, "State");
	g_variant_get (variant_state, "u", &state);

	g_debug ("device %s type %i @ %s", item->device_id, item->type, item->udi);

	/* set device icon */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "image_device"));
	gtk_image_set_from_icon_name (GTK_IMAGE (widget),
				      panel_device_type_to_icon_name (item->type),
				      GTK_ICON_SIZE_DIALOG);

	/* set device kind */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_device"));
	gtk_label_set_label (GTK_LABEL (widget),
			     panel_device_type_to_localized_string (item->type));

	/* set device state */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "label_status"));
	gtk_label_set_label (GTK_LABEL (widget),
			     panel_device_state_to_localized_string (state));

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "notebook_types"));
	if (item->type == NM_DEVICE_TYPE_ETHERNET) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
		panel_populate_wired_device (item);
	} else if (item->type == NM_DEVICE_TYPE_WIFI) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
		panel_populate_wireless_device (item);
	} else if (item->type == NM_DEVICE_TYPE_GSM ||
	           item->type == NM_DEVICE_TYPE_CDMA) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 4);
		panel_populate_mobilebb_device (item);
	}

	g_variant_unref (variant_state);
	g_variant_unref (variant_id);
}

/**
 * panel_devices_treeview_clicked_cb:
 **/
static void
panel_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *widget;
	PanelDeviceItem *item;
	CcNetworkPanelPrivate *priv = panel->priv;

	/* will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		goto out;
	}

	/* get id */
	gtk_tree_model_get (model, &iter,
			    PANEL_DEVICES_COLUMN_COMPOSITE_DEVICE, &item,
			    -1);

	/* this is the proxy settings device */
	if (item == NULL) {
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
	priv->current_device = g_strdup (item->device_id);

	/* refresh item */
	panel_device_refresh_item_ui (item);
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
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GtkTreeSortable *sortable;
	GtkWidget *widget;

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
	panel->priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify )panel_free_device_item);

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
			  G_CALLBACK (panel_devices_treeview_clicked_cb), panel);

	/* add the virtual proxy device */
	panel_add_proxy_device (panel);

	/* select the proxy device */
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

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
						     "button_add"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_remove"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_unlock"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "switch_flight_mode"));
	gtk_widget_set_sensitive (widget, FALSE);

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

