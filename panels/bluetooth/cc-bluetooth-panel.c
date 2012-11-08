/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2010  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <shell/cc-shell.h>

#include "cc-bluetooth-panel.h"

#include <bluetooth-client.h>
#include <bluetooth-utils.h>
#include <bluetooth-killswitch.h>
#include <bluetooth-chooser.h>
#include <bluetooth-plugin-manager.h>

CC_PANEL_REGISTER (CcBluetoothPanel, cc_bluetooth_panel)

#define BLUETOOTH_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define KEYBOARD_PREFS		"keyboard"
#define MOUSE_PREFS		"mouse"
#define SOUND_PREFS		"sound"
#define WIZARD			"bluetooth-wizard"

struct CcBluetoothPanelPrivate {
	GtkBuilder          *builder;
	GtkWidget           *chooser;
	char                *selected_bdaddr;
	BluetoothClient     *client;
	BluetoothKillswitch *killswitch;
	gboolean             debug;
	GHashTable          *connecting_devices;
};

static void cc_bluetooth_panel_finalize (GObject *object);

static void
launch_command (const char *command)
{
	GError *error = NULL;

	if (!g_spawn_command_line_async(command, &error)) {
		g_warning ("Couldn't execute command '%s': %s\n", command, error->message);
		g_error_free (error);
	}
}

static const char *
cc_bluetooth_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/bluetooth";
}

static void
cc_bluetooth_panel_class_init (CcBluetoothPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	object_class->finalize = cc_bluetooth_panel_finalize;

	panel_class->get_help_uri = cc_bluetooth_panel_get_help_uri;

	g_type_class_add_private (klass, sizeof (CcBluetoothPanelPrivate));
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	CcBluetoothPanel *self;

	bluetooth_plugin_manager_cleanup ();

	self = CC_BLUETOOTH_PANEL (object);
	g_clear_object (&self->priv->builder);
	g_clear_object (&self->priv->killswitch);
	g_clear_object (&self->priv->client);

	g_clear_pointer (&self->priv->connecting_devices, g_hash_table_destroy);
	g_clear_pointer (&self->priv->selected_bdaddr, g_free);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

enum {
	CONNECTING_NOTEBOOK_PAGE_SWITCH = 0,
	CONNECTING_NOTEBOOK_PAGE_SPINNER = 1
};

static void
set_connecting_page (CcBluetoothPanel *self,
		     int               page)
{
	if (page == CONNECTING_NOTEBOOK_PAGE_SPINNER)
		gtk_spinner_start (GTK_SPINNER (WID ("connecting_spinner")));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (WID ("connecting_notebook")), page);
	if (page == CONNECTING_NOTEBOOK_PAGE_SWITCH)
		gtk_spinner_start (GTK_SPINNER (WID ("connecting_spinner")));
}

static void
remove_connecting (CcBluetoothPanel *self,
		   const char       *bdaddr)
{
	g_hash_table_remove (self->priv->connecting_devices, bdaddr);
}

static void
add_connecting (CcBluetoothPanel *self,
		const char       *bdaddr)
{
	g_hash_table_insert (self->priv->connecting_devices,
			     g_strdup (bdaddr),
			     GINT_TO_POINTER (1));
}

static gboolean
is_connecting (CcBluetoothPanel *self,
	       const char       *bdaddr)
{
	return GPOINTER_TO_INT (g_hash_table_lookup (self->priv->connecting_devices,
						     bdaddr));
}

typedef struct {
	char             *bdaddr;
	CcBluetoothPanel *self;
} ConnectData;

static void
connect_done (GObject      *source_object,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	CcBluetoothPanel *self;
	char *bdaddr;
	gboolean success;
	ConnectData *data = (ConnectData *) user_data;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object),
							   res, NULL);

	self = data->self;

	/* Check whether the same device is now selected, and update the UI */
	bdaddr = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));
	if (g_strcmp0 (bdaddr, data->bdaddr) == 0) {
		GtkSwitch *button;

		button = GTK_SWITCH (WID ("switch_connection"));
		/* Reset the switch if it failed */
		if (success == FALSE)
			gtk_switch_set_active (button, !gtk_switch_get_active (button));
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
	}

	remove_connecting (self, data->bdaddr);

	g_free (bdaddr);
	g_object_unref (data->self);
	g_free (data->bdaddr);
	g_free (data);
}

static void
switch_connected_active_changed (GtkSwitch        *button,
				 GParamSpec       *spec,
				 CcBluetoothPanel *self)
{
	char *proxy;
	GValue value = { 0, };
	ConnectData *data;
	char *bdaddr;

	bdaddr = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));
	if (is_connecting (self, bdaddr)) {
		g_free (bdaddr);
		return;
	}

	if (bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
							"proxy", &value) == FALSE) {
		g_warning ("Could not get D-Bus proxy for selected device");
		return;
	}
	proxy = g_strdup (g_dbus_proxy_get_object_path (g_value_get_object (&value)));
	g_value_unset (&value);

	if (proxy == NULL)
		return;

	data = g_new0 (ConnectData, 1);
	data->bdaddr = bdaddr;
	data->self = g_object_ref (self);

	bluetooth_client_connect_service (self->priv->client,
					  proxy,
					  gtk_switch_get_active (button),
					  NULL,
					  connect_done,
					  data);

	add_connecting (self, data->bdaddr);
	set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);
	g_free (proxy);
}

enum {
	NOTEBOOK_PAGE_EMPTY = 0,
	NOTEBOOK_PAGE_PROPS = 1
};

static void
set_notebook_page (CcBluetoothPanel *self,
		   int               page)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (WID ("props_notebook")), page);
}

static void
add_extra_setup_widgets (CcBluetoothPanel *self,
			 const char       *bdaddr)
{
	GValue value = { 0 };
	char **uuids;
	GList *list, *l;
	GtkWidget *container;

	if (bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
							"uuids", &value) == FALSE)
		return;

	uuids = (char **) g_value_get_boxed (&value);
	list = bluetooth_plugin_manager_get_widgets (bdaddr, (const char **) uuids);
	if (list == NULL) {
		g_value_unset (&value);
		return;
	}

	container = WID ("additional_setup_box");
	for (l = list; l != NULL; l = l->next) {
		GtkWidget *widget = l->data;
		gtk_box_pack_start (GTK_BOX (container), widget,
				    FALSE, FALSE, 0);
		gtk_widget_show_all (widget);
	}
	gtk_widget_show (container);
	g_value_unset (&value);
}

static void
remove_extra_setup_widgets (CcBluetoothPanel *self)
{
	GtkWidget *box;

	box = WID ("additional_setup_box");
	gtk_container_forall (GTK_CONTAINER (box), (GtkCallback) gtk_widget_destroy, NULL);
	gtk_widget_hide (WID ("additional_setup_box"));
}

static void
cc_bluetooth_panel_update_properties (CcBluetoothPanel *self)
{
	char *bdaddr;
	GtkSwitch *button;

	button = GTK_SWITCH (WID ("switch_connection"));
	g_signal_handlers_block_by_func (button, switch_connected_active_changed, self);

	/* Hide all the buttons now, and show them again if we need to */
	gtk_widget_hide (WID ("keyboard_box"));
	gtk_widget_hide (WID ("sound_box"));
	gtk_widget_hide (WID ("mouse_box"));
	gtk_widget_hide (WID ("browse_box"));
	gtk_widget_hide (WID ("send_box"));

	bdaddr = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));

	/* Remove the extra setup widgets */
	if (g_strcmp0 (self->priv->selected_bdaddr, bdaddr) != 0)
		remove_extra_setup_widgets (self);

	if (bdaddr == NULL) {
		gtk_widget_set_sensitive (WID ("properties_vbox"), FALSE);
		gtk_switch_set_active (button, FALSE);
		gtk_widget_set_sensitive (WID ("button_delete"), FALSE);
		set_notebook_page (self, NOTEBOOK_PAGE_EMPTY);
	} else {
		BluetoothType type;
		gboolean connected;
		GValue value = { 0 };
		GHashTable *services;

		if (self->priv->debug)
			bluetooth_chooser_dump_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));

		gtk_widget_set_sensitive (WID ("properties_vbox"), TRUE);

		if (is_connecting (self, bdaddr)) {
			gtk_switch_set_active (button, TRUE);
			set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);
		} else {
			connected = bluetooth_chooser_get_selected_device_is_connected (BLUETOOTH_CHOOSER (self->priv->chooser));
			gtk_switch_set_active (button, connected);
			set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
		}

		/* Paired */
		bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
							    "paired", &value);
		gtk_label_set_text (GTK_LABEL (WID ("paired_label")),
				    g_value_get_boolean (&value) ? _("Yes") : _("No"));
		g_value_unset (&value);

		/* Connection */
		bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
							    "services", &value);
		services = g_value_get_boxed (&value);
		gtk_widget_set_sensitive (GTK_WIDGET (button), (services != NULL));
		g_value_unset (&value);

		/* UUIDs */
		if (bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
								"uuids", &value)) {
			const char **uuids;
			guint i;

			uuids = (const char **) g_value_get_boxed (&value);
			for (i = 0; uuids && uuids[i] != NULL; i++) {
				if (g_str_equal (uuids[i], "OBEXObjectPush"))
					gtk_widget_show (WID ("send_box"));
				else if (g_str_equal (uuids[i], "OBEXFileTransfer"))
					gtk_widget_show (WID ("browse_box"));
			}
			g_value_unset (&value);
		}

		/* Type */
		type = bluetooth_chooser_get_selected_device_type (BLUETOOTH_CHOOSER (self->priv->chooser));
		gtk_label_set_text (GTK_LABEL (WID ("type_label")), bluetooth_type_to_string (type));
		switch (type) {
		case BLUETOOTH_TYPE_KEYBOARD:
			gtk_widget_show (WID ("keyboard_box"));
			break;
		case BLUETOOTH_TYPE_MOUSE:
		case BLUETOOTH_TYPE_TABLET:
			gtk_widget_show (WID ("mouse_box"));
			break;
		case BLUETOOTH_TYPE_HEADSET:
		case BLUETOOTH_TYPE_HEADPHONES:
		case BLUETOOTH_TYPE_OTHER_AUDIO:
			gtk_widget_show (WID ("sound_box"));
		default:
			/* others? */
			;
		}

		/* Extra widgets */
		if (g_strcmp0 (self->priv->selected_bdaddr, bdaddr) != 0)
			add_extra_setup_widgets (self, bdaddr);

		gtk_label_set_text (GTK_LABEL (WID ("address_label")), bdaddr);

		gtk_widget_set_sensitive (WID ("button_delete"), TRUE);
		set_notebook_page (self, NOTEBOOK_PAGE_PROPS);
	}

	g_free (self->priv->selected_bdaddr);
	self->priv->selected_bdaddr = bdaddr;

	g_signal_handlers_unblock_by_func (button, switch_connected_active_changed, self);
}

static void
power_callback (GObject          *object,
		GParamSpec       *spec,
		CcBluetoothPanel *self)
{
	gboolean state;

	state = gtk_switch_get_active (GTK_SWITCH (WID ("switch_bluetooth")));
	g_debug ("Power switched to %s", state ? "off" : "on");
	bluetooth_killswitch_set_state (self->priv->killswitch,
					state ? BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED : BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED);
}

static void
cc_bluetooth_panel_update_treeview_message (CcBluetoothPanel *self,
					    const char       *message)
{
	if (message != NULL) {
		gtk_widget_hide (self->priv->chooser);
		gtk_widget_show (WID ("message_scrolledwindow"));

		gtk_label_set_text (GTK_LABEL (WID ("message_label")),
				    message);
	} else {
		gtk_widget_hide (WID ("message_scrolledwindow"));
		gtk_widget_show (self->priv->chooser);
	}
}

static void
cc_bluetooth_panel_update_power (CcBluetoothPanel *self)
{
	BluetoothKillswitchState state;
	char *path;
	gboolean powered, sensitive;

	g_object_get (G_OBJECT (self->priv->client),
		      "default-adapter", &path,
		      "default-adapter-powered", &powered,
		      NULL);
	state = bluetooth_killswitch_get_state (self->priv->killswitch);

	g_debug ("Updating power, default adapter: %s (powered: %s), killswitch: %s",
		 path ? path : "(none)",
		 powered ? "on" : "off",
		 bluetooth_killswitch_state_to_string (state));

	if (path == NULL &&
	    bluetooth_killswitch_has_killswitches (self->priv->killswitch) &&
	    state != BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED) {
		g_debug ("Default adapter is unpowered, but should be available");
		sensitive = TRUE;
		cc_bluetooth_panel_update_treeview_message (self, _("Bluetooth is disabled"));
	} else if (path == NULL &&
		   state == BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED) {
		g_debug ("Bluetooth is Hard blocked");
		sensitive = FALSE;
		cc_bluetooth_panel_update_treeview_message (self, _("Bluetooth is disabled by hardware switch"));
	} else if (path == NULL) {
		sensitive = FALSE;
		g_debug ("No Bluetooth available");
		cc_bluetooth_panel_update_treeview_message (self, _("No Bluetooth adapters found"));
	} else {
		sensitive = TRUE;
		g_debug ("Bluetooth is available and powered");
		cc_bluetooth_panel_update_treeview_message (self, NULL);
	}

	g_free (path);
	gtk_widget_set_sensitive (WID ("box_power") , sensitive);
	gtk_widget_set_sensitive (WID ("box_vis") , sensitive);
}

static void
switch_panel (CcBluetoothPanel *self,
	      const char       *panel)
{
  CcShell *shell;
  GError *error = NULL;

  shell = cc_panel_get_shell (CC_PANEL (self));
  if (cc_shell_set_active_panel_from_id (shell, panel, NULL, &error) == FALSE)
    {
      g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
      g_error_free (error);
    }
}

static gboolean
keyboard_callback (GtkButton        *button,
		   CcBluetoothPanel *self)
{
	switch_panel (self, KEYBOARD_PREFS);
	return TRUE;
}

static gboolean
mouse_callback (GtkButton        *button,
		CcBluetoothPanel *self)
{
	switch_panel (self, MOUSE_PREFS);
	return TRUE;
}

static gboolean
sound_callback (GtkButton        *button,
		CcBluetoothPanel *self)
{
	switch_panel (self, SOUND_PREFS);
	return TRUE;
}

static void
send_callback (GtkButton        *button,
	       CcBluetoothPanel *self)
{
	char *bdaddr, *alias;

	bdaddr = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));
	alias = bluetooth_chooser_get_selected_device_name (BLUETOOTH_CHOOSER (self->priv->chooser));

	bluetooth_send_to_address (bdaddr, alias);

	g_free (bdaddr);
	g_free (alias);
}

static void
mount_finish_cb (GObject *source_object,
		 GAsyncResult *res,
		 gpointer user_data)
{
	GError *error = NULL;

	if (bluetooth_browse_address_finish (source_object, res, &error) == FALSE) {
		g_printerr ("Failed to mount OBEX volume: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
browse_callback (GtkButton        *button,
		 CcBluetoothPanel *self)
{
	char *bdaddr;

	bdaddr = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));

	bluetooth_browse_address (G_OBJECT (self), bdaddr,
				  GDK_CURRENT_TIME, mount_finish_cb, NULL);

	g_free (bdaddr);
}

/* Visibility/Discoverable */
static void discoverable_changed (BluetoothClient  *client,
				  GParamSpec       *spec,
				  CcBluetoothPanel *self);

static void
switch_discoverable_active_changed (GtkSwitch        *button,
				    GParamSpec       *spec,
				    CcBluetoothPanel *self)
{
	g_signal_handlers_block_by_func (self->priv->client, discoverable_changed, self);
	g_object_set (G_OBJECT (self->priv->client), "default-adapter-discoverable",
		      gtk_switch_get_active (button), NULL);
	g_signal_handlers_unblock_by_func (self->priv->client, discoverable_changed, self);
}

static void
cc_bluetooth_panel_update_visibility (CcBluetoothPanel *self)
{
	gboolean discoverable;
	GtkSwitch *button;
	char *name;

	button = GTK_SWITCH (WID ("switch_discoverable"));
	g_object_get (G_OBJECT (self->priv->client), "default-adapter-discoverable", &discoverable, NULL);
	g_signal_handlers_block_by_func (button, switch_discoverable_active_changed, self);
	gtk_switch_set_active (button, discoverable);
	g_signal_handlers_unblock_by_func (button, switch_discoverable_active_changed, self);

	g_object_get (G_OBJECT (self->priv->client), "default-adapter-name", &name, NULL);
	if (name == NULL) {
		gtk_widget_set_sensitive (WID ("switch_discoverable"), FALSE);
		gtk_widget_set_sensitive (WID ("visible_label"), FALSE);
		gtk_label_set_text (GTK_LABEL (WID ("visible_label")), _("Visibility"));
	} else {
		char *label;

		label = g_strdup_printf (_("Visibility of “%s”"), name);
		g_free (name);
		gtk_label_set_text (GTK_LABEL (WID ("visible_label")), label);
		g_free (label);

		gtk_widget_set_sensitive (WID ("switch_discoverable"), TRUE);
		gtk_widget_set_sensitive (WID ("visible_label"), TRUE);
	}
}

static void
discoverable_changed (BluetoothClient  *client,
		      GParamSpec       *spec,
		      CcBluetoothPanel *self)
{
	cc_bluetooth_panel_update_visibility (self);
}

static void
name_changed (BluetoothClient  *client,
	      GParamSpec       *spec,
	      CcBluetoothPanel *self)
{
	cc_bluetooth_panel_update_visibility (self);
}

static void
device_selected_changed (BluetoothChooser *chooser,
			 GParamSpec       *spec,
			 CcBluetoothPanel *self)
{
	cc_bluetooth_panel_update_properties (self);
}

static gboolean
show_confirm_dialog (CcBluetoothPanel *self,
		     const char *name)
{
	GtkWidget *dialog, *parent;
	gint response;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Remove '%s' from the list of devices?"), name);
	g_object_set (G_OBJECT (dialog), "secondary-text",
		      _("If you remove the device, you will have to set it up again before next use."),
		      NULL);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_REMOVE, GTK_RESPONSE_ACCEPT);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;

	return FALSE;
}

static gboolean
remove_selected_device (CcBluetoothPanel *self)
{
	GValue value = { 0, };
	char *device, *adapter;
	GDBusProxy *adapter_proxy;
	GError *error = NULL;
	GVariant *ret;

	if (bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (self->priv->chooser),
							"proxy", &value) == FALSE) {
		return FALSE;
	}
	device = g_strdup (g_dbus_proxy_get_object_path (g_value_get_object (&value)));
	g_value_unset (&value);

	g_object_get (G_OBJECT (self->priv->client), "default-adapter", &adapter, NULL);
	adapter_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
						       NULL,
						       "org.bluez",
						       adapter,
						       "org.bluez.Adapter",
						       NULL,
						       &error);
	g_free (adapter);
	if (adapter_proxy == NULL) {
		g_warning ("Failed to create a GDBusProxy for the default adapter: %s", error->message);
		g_error_free (error);
		g_free (device);
		return FALSE;
	}

	ret = g_dbus_proxy_call_sync (G_DBUS_PROXY (adapter_proxy),
				      "RemoveDevice",
				      g_variant_new ("(o)", device),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);
	if (ret == NULL) {
		g_warning ("Failed to remove device '%s': %s", device, error->message);
		g_error_free (error);
	} else {
		g_variant_unref (ret);
	}

	g_object_unref (adapter_proxy);
	g_free (device);

	return (ret != NULL);
}

/* Treeview buttons */
static void
delete_clicked (GtkToolButton    *button,
		CcBluetoothPanel *self)
{
	char *address, *name;

	address = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (self->priv->chooser));
	g_assert (address);

	name = bluetooth_chooser_get_selected_device_name (BLUETOOTH_CHOOSER (self->priv->chooser));

	if (show_confirm_dialog (self, name) != FALSE) {
		if (remove_selected_device (self))
			bluetooth_plugin_manager_device_deleted (address);
	}

	g_free (address);
	g_free (name);
}

static void
setup_clicked (GtkToolButton    *button,
	       CcBluetoothPanel *self)
{
	launch_command (WIZARD);
}

/* Overall device state */
static void
cc_bluetooth_panel_update_state (CcBluetoothPanel *self)
{
	char *bdaddr;

	g_object_get (G_OBJECT (self->priv->client),
		      "default-adapter", &bdaddr,
		      NULL);
	gtk_widget_set_sensitive (WID ("toolbar"), (bdaddr != NULL));
	g_free (bdaddr);
}

static void
cc_bluetooth_panel_update_powered_state (CcBluetoothPanel *self)
{
	gboolean powered;

	g_object_get (G_OBJECT (self->priv->client),
		      "default-adapter-powered", &powered,
		      NULL);
	gtk_switch_set_active (GTK_SWITCH (WID ("switch_bluetooth")), powered);
}

static void
default_adapter_power_changed (BluetoothClient  *client,
			       GParamSpec       *spec,
			       CcBluetoothPanel *self)
{
	g_debug ("Default adapter power changed");
	cc_bluetooth_panel_update_powered_state (self);
}

static void
default_adapter_changed (BluetoothClient  *client,
			 GParamSpec       *spec,
			 CcBluetoothPanel *self)
{
	g_debug ("Default adapter changed");
	cc_bluetooth_panel_update_state (self);
	cc_bluetooth_panel_update_power (self);
	cc_bluetooth_panel_update_powered_state (self);
}

static void
killswitch_changed (BluetoothKillswitch      *killswitch,
		    BluetoothKillswitchState  state,
		    CcBluetoothPanel         *self)
{
	g_debug ("Killswitch changed to state '%s' (%d)", bluetooth_killswitch_state_to_string (state) , state);
	cc_bluetooth_panel_update_state (self);
	cc_bluetooth_panel_update_power (self);
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	GtkWidget *widget;
	GError *error = NULL;
	GtkStyleContext *context;

	self->priv = BLUETOOTH_PANEL_PRIVATE (self);

	bluetooth_plugin_manager_init ();
	self->priv->killswitch = bluetooth_killswitch_new ();
	self->priv->client = bluetooth_client_new ();
	self->priv->connecting_devices = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								(GDestroyNotify) g_free,
								NULL);
	self->priv->debug = g_getenv ("BLUETOOTH_DEBUG") != NULL;

	self->priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (self->priv->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_file (self->priv->builder,
				   PKGDATADIR "/bluetooth.ui",
				   &error);
	if (error != NULL) {
		g_warning ("Failed to load '%s': %s", PKGDATADIR "/bluetooth.ui", error->message);
		g_error_free (error);
		return;
	}

	widget = WID ("grid");
	gtk_widget_reparent (widget, GTK_WIDGET (self));

	/* Overall device state */
	cc_bluetooth_panel_update_state (self);
	g_signal_connect (G_OBJECT (self->priv->client), "notify::default-adapter",
			  G_CALLBACK (default_adapter_changed), self);
	g_signal_connect (G_OBJECT (self->priv->client), "notify::default-adapter-powered",
			  G_CALLBACK (default_adapter_power_changed), self);

	/* The discoverable button */
	cc_bluetooth_panel_update_visibility (self);
	g_signal_connect (G_OBJECT (self->priv->client), "notify::default-adapter-discoverable",
			  G_CALLBACK (discoverable_changed), self);
	g_signal_connect (G_OBJECT (self->priv->client), "notify::default-adapter-name",
			  G_CALLBACK (name_changed), self);
	g_signal_connect (G_OBJECT (WID ("switch_discoverable")), "notify::active",
			  G_CALLBACK (switch_discoverable_active_changed), self);

	/* The known devices */
	widget = WID ("devices_table");

	context = gtk_widget_get_style_context (WID ("message_scrolledwindow"));
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

	/* Note that this will only ever show the devices on the default
	 * adapter, this is on purpose */
	self->priv->chooser = bluetooth_chooser_new ();
	gtk_box_pack_start (GTK_BOX (WID ("box_devices")), self->priv->chooser, TRUE, TRUE, 0);
	g_object_set (self->priv->chooser,
		      "show-searching", FALSE,
		      "show-device-type", FALSE,
		      "show-device-type-column", FALSE,
		      "show-device-category", FALSE,
		      "show-pairing", FALSE,
		      "show-connected", FALSE,
		      "device-category-filter", BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED,
		      "no-show-all", TRUE,
		      NULL);

	/* Join treeview and buttons */
	widget = bluetooth_chooser_get_scrolled_window (BLUETOOTH_CHOOSER (self->priv->chooser));
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (widget), 250);
	gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (widget), 200);
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
	widget = WID ("toolbar");
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

	g_signal_connect (G_OBJECT (self->priv->chooser), "notify::device-selected",
			  G_CALLBACK (device_selected_changed), self);
	g_signal_connect (G_OBJECT (WID ("button_delete")), "clicked",
			  G_CALLBACK (delete_clicked), self);
	g_signal_connect (G_OBJECT (WID ("button_setup")), "clicked",
			  G_CALLBACK (setup_clicked), self);

	/* Set the initial state of the properties */
	cc_bluetooth_panel_update_properties (self);
	g_signal_connect (G_OBJECT (WID ("mouse_link")), "activate-link",
			  G_CALLBACK (mouse_callback), self);
	g_signal_connect (G_OBJECT (WID ("keyboard_link")), "activate-link",
			  G_CALLBACK (keyboard_callback), self);
	g_signal_connect (G_OBJECT (WID ("sound_link")), "activate-link",
			  G_CALLBACK (sound_callback), self);
	g_signal_connect (G_OBJECT (WID ("browse_button")), "clicked",
			  G_CALLBACK (browse_callback), self);
	g_signal_connect (G_OBJECT (WID ("send_button")), "clicked",
			  G_CALLBACK (send_callback), self);
	g_signal_connect (G_OBJECT (WID ("switch_connection")), "notify::active",
			  G_CALLBACK (switch_connected_active_changed), self);

	/* Set the initial state of power */
	g_signal_connect (G_OBJECT (WID ("switch_bluetooth")), "notify::active",
			  G_CALLBACK (power_callback), self);
	g_signal_connect (G_OBJECT (self->priv->killswitch), "state-changed",
			  G_CALLBACK (killswitch_changed), self);
	cc_bluetooth_panel_update_power (self);

	gtk_widget_show_all (GTK_WIDGET (self));
}

void
cc_bluetooth_panel_register (GIOModule *module)
{
	cc_bluetooth_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_BLUETOOTH_PANEL,
					"bluetooth", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_bluetooth_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

