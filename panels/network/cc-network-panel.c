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

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

struct _CcNetworkPanelPrivate
{
	GSettings	*proxy_settings;
	GCancellable	*cancellable;
	GtkBuilder	*builder;
	GDBusProxy	*proxy;
};

enum {
	PANEL_COLUMN_ICON,
	PANEL_COLUMN_TITLE,
	PANEL_COLUMN_ID,
	PANEL_COLUMN_TOOLTIP,
	PANEL_COLUMN_COMPOSITE_DEVICE,
	PANEL_COLUMN_LAST
};

typedef enum {
	NM_DEVICE_TYPE_UNKNOWN,
	NM_DEVICE_TYPE_ETHERNET,
	NM_DEVICE_TYPE_WIFI,
	NM_DEVICE_TYPE_GSM,
	NM_DEVICE_TYPE_CDMA,
	NM_DEVICE_TYPE_BLUETOOTH,
	NM_DEVICE_TYPE_MESH
} NMDeviceType;

typedef enum {
	NM_DEVICE_STATE_UNKNOWN,
	NM_DEVICE_STATE_UNMANAGED,
	NM_DEVICE_STATE_UNAVAILABLE,
	NM_DEVICE_STATE_DISCONNECTED,
	NM_DEVICE_STATE_PREPARE,
	NM_DEVICE_STATE_CONFIG,
	NM_DEVICE_STATE_NEED_AUTH,
	NM_DEVICE_STATE_IP_CONFIG,
	NM_DEVICE_STATE_ACTIVATED,
	NM_DEVICE_STATE_FAILED
} NMDeviceState;

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
	g_cancellable_cancel (priv->cancellable);
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

/**
 * panel_refresh:
 **/
static void
panel_refresh (CcNetworkPanelPrivate *priv)
{
	return;
}

/**
 * panel_dbus_signal_cb:
 **/
static void
panel_dbus_signal_cb (GDBusProxy *proxy,
		   gchar      *sender_name,
		   gchar      *signal_name,
		   GVariant   *parameters,
		   gpointer    user_data)
{
//	CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (user_data)->priv;

	/* get the new state */
	if (g_strcmp0 (signal_name, "StateChanged") == 0) {
		g_debug ("ensure devices are correct");
		return;
	}
}

/**
 * panel_device_type_to_icon_name:
 **/
static const gchar *
panel_device_type_to_icon_name (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_TYPE_ETHERNET:
		value = "network-wired";
		break;
	case NM_DEVICE_TYPE_WIFI:
	case NM_DEVICE_TYPE_GSM:
	case NM_DEVICE_TYPE_CDMA:
	case NM_DEVICE_TYPE_BLUETOOTH:
	case NM_DEVICE_TYPE_MESH:
		value = "network-wireless";
		break;
	default:
		break;
	}
	return value;
}

/**
 * panel_device_type_to_localized_string:
 **/
static const gchar *
panel_device_type_to_localized_string (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_TYPE_UNKNOWN:
		/* TRANSLATORS: device type */
		value = _("Unknown");
		break;
	case NM_DEVICE_TYPE_ETHERNET:
		/* TRANSLATORS: device type */
		value = _("Wired");
		break;
	case NM_DEVICE_TYPE_WIFI:
		/* TRANSLATORS: device type */
		value = _("Wireless");
		break;
	case NM_DEVICE_TYPE_GSM:
	case NM_DEVICE_TYPE_CDMA:
		/* TRANSLATORS: device type */
		value = _("Mobile broadband");
		break;
	case NM_DEVICE_TYPE_BLUETOOTH:
		/* TRANSLATORS: device type */
		value = _("Bluetooth");
		break;
	case NM_DEVICE_TYPE_MESH:
		/* TRANSLATORS: device type */
		value = _("Mesh");
		break;

	default:
		break;
	}
	return value;
}

/**
 * panel_device_state_to_localized_string:
 **/
static const gchar *
panel_device_state_to_localized_string (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_STATE_UNKNOWN:
		/* TRANSLATORS: device status */
		value = _("Status unknown");
		break;
	case NM_DEVICE_STATE_UNMANAGED:
		/* TRANSLATORS: device status */
		value = _("Unmanaged");
		break;
	case NM_DEVICE_STATE_UNAVAILABLE:
		/* TRANSLATORS: device status */
		value = _("Unavailable");
		break;
	case NM_DEVICE_STATE_DISCONNECTED:
		/* TRANSLATORS: device status */
		value = _("Disconnected");
		break;
	case NM_DEVICE_STATE_PREPARE:
		/* TRANSLATORS: device status */
		value = _("Preparing connection");
		break;
	case NM_DEVICE_STATE_CONFIG:
		/* TRANSLATORS: device status */
		value = _("Configuring connection");
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		/* TRANSLATORS: device status */
		value = _("Authenticating");
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		/* TRANSLATORS: device status */
		value = _("Getting network address");
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		/* TRANSLATORS: device status */
		value = _("Connected");
		break;
	case NM_DEVICE_STATE_FAILED:
		/* TRANSLATORS: device status */
		value = _("Failed to connect");
		break;
	default:
		break;
	}
	return value;
}

typedef struct {
	CcNetworkPanel		*panel;
	guint			 type;
	gchar			*device_id;
	GDBusProxy		*proxy;
	GDBusProxy		*proxy_additional;
} PanelDeviceItem;

static void
panel_free_device_item (PanelDeviceItem *item)
{
	g_object_unref (item->panel);
	g_object_unref (item->proxy);
	if (item->proxy_additional != NULL)
		g_object_unref (item->proxy_additional);
	g_free (item->device_id);
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
			    PANEL_COLUMN_ICON, panel_device_type_to_icon_name (item->type),
			    PANEL_COLUMN_TITLE, title,
			    PANEL_COLUMN_ID, item->device_id,
			    PANEL_COLUMN_TOOLTIP, "tooltip - FIXME!",
			    PANEL_COLUMN_COMPOSITE_DEVICE, item,
			    -1);
	g_free (title);
}

/**
 * panel_got_device_proxy_additional_cb2:
 **/
static void
panel_got_device_proxy_additional_cb2 (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;

	item->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy_additional == NULL) {
		g_printerr ("Error creating additional proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	panel_add_device_to_listview (item);
out:
	return;
}

/**
 * panel_got_device_proxy_cb:
 **/
static void
panel_got_device_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *variant_type = NULL;
	const gchar *addition_interface = NULL;
	PanelDeviceItem *item = (PanelDeviceItem *) user_data;
	CcNetworkPanelPrivate *priv = item->panel->priv;

	item->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (item->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the additional interface for this device type */
	variant_type = g_dbus_proxy_get_cached_property (item->proxy, "DeviceType");
	g_variant_get (variant_type, "u", &item->type);
	if (item->type == NM_DEVICE_TYPE_ETHERNET) {
		addition_interface = "org.freedesktop.NetworkManager.Device.Wired";
	} else if (item->type == NM_DEVICE_TYPE_WIFI) {
		addition_interface = "org.freedesktop.NetworkManager.Device.Wireless";
	}
	if (addition_interface != NULL) {
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  "org.freedesktop.NetworkManager",
					  item->device_id,
					  addition_interface,
					  item->panel->priv->cancellable,
					  panel_got_device_proxy_additional_cb2,
					  item);
	} else {
		panel_add_device_to_listview (item);
	}
out:
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
 * panel_get_devices_cb:
 **/
static void
panel_get_devices_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);
//	CcNetworkPanelPrivate *priv = panel->priv;
	const gchar *object_path;
	GError *error = NULL;
	gsize len;
	GVariantIter iter;
	GVariant *result;
	GVariant *test;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (result == NULL) {
		g_printerr ("Error getting devices: %s\n", error->message);
		g_error_free (error);
		return;
	}

	test = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, test);
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
	g_variant_unref (test);
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
			  G_CALLBACK (panel_dbus_signal_cb),
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

	panel_refresh (priv);
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
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", PANEL_COLUMN_ICON,
							   NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", PANEL_COLUMN_TITLE,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, PANEL_COLUMN_TITLE);
	liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
					    "liststore_devices"));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore_devices),
					      PANEL_COLUMN_TITLE,
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
		tmp = g_strdup_printf (_("%iMb/s"), speed);
	} else {
		tmp = g_strdup_printf (_("%iGb/s"), speed / 1000);
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
		tmp = g_strdup_printf (_("%ikb/s"), bitrate);
	} else {
		tmp = g_strdup_printf (_("%iMb/s"), bitrate / 1000);
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
panel_populate_wired_device (CcNetworkPanel *panel, PanelDeviceItem *item)
{
	GtkWidget *widget;
	GVariant *ip4;
	GVariant *hw_address;
	GVariant *speed;

	/* set IP */
	ip4 = g_dbus_proxy_get_cached_property (item->proxy, "Ip4Address");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_wired_ip"));
	panel_set_label_for_variant_ipv4 (widget, ip4);

	/* set MAC */
	hw_address = g_dbus_proxy_get_cached_property (item->proxy_additional, "HwAddress");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_wired_mac"));
	panel_set_label_for_variant_string (widget, hw_address);

	/* set speed */
	speed = g_dbus_proxy_get_cached_property (item->proxy_additional, "Speed");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
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
panel_populate_wireless_device (CcNetworkPanel *panel, PanelDeviceItem *item)
{
	GtkWidget *widget;
	GVariant *bitrate;
	GVariant *hw_address;
	GVariant *ip4;

	/* set IP */
	ip4 = g_dbus_proxy_get_cached_property (item->proxy, "Ip4Address");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_wireless_ip"));
	panel_set_label_for_variant_ipv4 (widget, ip4);

	/* set MAC */
	hw_address = g_dbus_proxy_get_cached_property (item->proxy_additional, "HwAddress");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_wireless_mac"));
	panel_set_label_for_variant_string (widget, hw_address);

	/* set speed */
	bitrate = g_dbus_proxy_get_cached_property (item->proxy_additional, "Bitrate");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_wireless_speed"));
	panel_set_label_for_variant_bitrate (widget, bitrate);

	g_variant_unref (ip4);
	g_variant_unref (hw_address);
	g_variant_unref (bitrate);
}

/**
 * panel_devices_treeview_clicked_cb:
 **/
static void
panel_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcNetworkPanel *panel)
{
	gchar *id = NULL;
	PanelDeviceItem *item;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *widget;
	guint state;
	GVariant *variant_id = NULL;
	GVariant *variant_state = NULL;

	/* will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		goto out;
	}

	/* get id */
	gtk_tree_model_get (model, &iter,
			    PANEL_COLUMN_ID, &id,
			    PANEL_COLUMN_COMPOSITE_DEVICE, &item,
			    -1);

	/* this is the proxy settings device */
	if (item == NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
							     "hbox_device_header"));
		gtk_widget_set_visible (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
							     "notebook_types"));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 2);
		goto out;
	}

	/* we have a new device */
	g_debug ("selected device is: %s", id);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "hbox_device_header"));
	gtk_widget_set_visible (widget, TRUE);

	variant_id = g_dbus_proxy_get_cached_property (item->proxy, "Interface");
	g_variant_get (variant_id, "s", &id);

	variant_state = g_dbus_proxy_get_cached_property (item->proxy, "State");
	g_variant_get (variant_state, "u", &state);

	g_debug ("device %s type %i", id, item->type);

	/* set device icon */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "image_device"));
	gtk_image_set_from_icon_name (GTK_IMAGE (widget),
				      panel_device_type_to_icon_name (item->type),
				      GTK_ICON_SIZE_DIALOG);

	/* set device kind */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_device"));
	gtk_label_set_label (GTK_LABEL (widget),
			     panel_device_type_to_localized_string (item->type));

	/* set device state */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_status"));
	gtk_label_set_label (GTK_LABEL (widget),
			     panel_device_state_to_localized_string (state));

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "notebook_types"));
	if (item->type == NM_DEVICE_TYPE_ETHERNET) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
		panel_populate_wired_device (panel, item);
	} else if (item->type == NM_DEVICE_TYPE_WIFI) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
		panel_populate_wireless_device (panel, item);
	}

out:
	if (variant_id != NULL)
		g_variant_unref (variant_id);
	g_free (id);
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
				 _("Network Proxy"));

	gtk_list_store_append (liststore_devices, &iter);
	gtk_list_store_set (liststore_devices,
			    &iter,
			    PANEL_COLUMN_ICON, "preferences-system-network",
			    PANEL_COLUMN_TITLE, title,
			    PANEL_COLUMN_ID, NULL,
			    PANEL_COLUMN_TOOLTIP, _("Set the system proxy settings"),
			    PANEL_COLUMN_COMPOSITE_DEVICE, NULL,
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
	GtkTreePath *path;
	GtkTreeSelection *selection;
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
						     "label_proxy_http"));
	g_settings_bind (settings_tmp, "host",
			 widget, "label",
			 G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings_tmp);

	/* bind the proxy values */
	settings_tmp = g_settings_new ("org.gnome.system.proxy.https");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_proxy_https"));
	g_settings_bind (settings_tmp, "host",
			 widget, "label",
			 G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings_tmp);

	/* bind the proxy values */
	settings_tmp = g_settings_new ("org.gnome.system.proxy.ftp");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_proxy_ftp"));
	g_settings_bind (settings_tmp, "host",
			 widget, "label",
			 G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings_tmp);

	/* bind the proxy values */
	settings_tmp = g_settings_new ("org.gnome.system.proxy.socks");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_proxy_socks"));
	g_settings_bind (settings_tmp, "host",
			 widget, "label",
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
						     "togglebutton_flightmode"));
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

