/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
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
#include <shell/cc-object-storage.h>
#include <bluetooth-settings-widget.h>

#include "cc-bluetooth-panel.h"
#include "cc-bluetooth-resources.h"

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->builder, s))

#define BLUETOOTH_NO_DEVICES_PAGE    "no-devices-page"
#define BLUETOOTH_DISABLED_PAGE      "disabled-page"
#define BLUETOOTH_AIRPLANE_PAGE      "airplane-page"
#define BLUETOOTH_HW_AIRPLANE_PAGE   "hw-airplane-page"
#define BLUETOOTH_WORKING_PAGE       "working-page"

struct _CcBluetoothPanel {
	CcPanel parent_instance;

	GtkBuilder          *builder;
	GtkWidget           *stack;
	GtkWidget           *widget;
	GCancellable        *cancellable;

	/* Killswitch */
	GtkWidget           *kill_switch_header;
	GDBusProxy          *rfkill, *properties;
	gboolean             airplane_mode;
	gboolean             bt_airplane_mode;
	gboolean             hardware_airplane_mode;
	gboolean             has_airplane_mode;
};

CC_PANEL_REGISTER (CcBluetoothPanel, cc_bluetooth_panel)

static void cc_bluetooth_panel_finalize (GObject *object);
static void cc_bluetooth_panel_constructed (GObject *object);

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

	object_class->constructed = cc_bluetooth_panel_constructed;
	object_class->finalize = cc_bluetooth_panel_finalize;

	panel_class->get_help_uri = cc_bluetooth_panel_get_help_uri;
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	CcBluetoothPanel *self;

	self = CC_BLUETOOTH_PANEL (object);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);

	g_clear_object (&self->properties);
	g_clear_object (&self->rfkill);
	g_clear_object (&self->kill_switch_header);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

static void
cc_bluetooth_panel_constructed (GObject *object)
{
	CcBluetoothPanel *self = CC_BLUETOOTH_PANEL (object);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->constructed (object);

	/* add kill switch widgets  */
	self->kill_switch_header = g_object_ref (WID ("box_power"));
	cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)),
					 self->kill_switch_header);
	gtk_widget_show_all (self->kill_switch_header);
}

static void
power_callback (GObject          *object,
		GParamSpec       *spec,
		CcBluetoothPanel *self)
{
	gboolean state;

	state = gtk_switch_get_active (GTK_SWITCH (WID ("switch_bluetooth")));
	g_debug ("Power switched to %s", state ? "on" : "off");
	g_dbus_proxy_call (self->properties,
			   "Set",
			   g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', %v)",
						 g_variant_new_boolean (!state)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   self->cancellable,
			   NULL, NULL);
}

static void
cc_bluetooth_panel_update_power (CcBluetoothPanel *self)
{
	GtkAlign valign;
	GObject *toggle;
	gboolean sensitive, powered, change_powered;
	const char *page;

	g_debug ("Updating airplane mode: BluetoothHasAirplaneMode %d, BluetoothHardwareAirplaneMode %d, BluetoothAirplaneMode %d, AirplaneMode %d",
		 self->has_airplane_mode, self->hardware_airplane_mode, self->bt_airplane_mode, self->airplane_mode);

	change_powered = TRUE;
  valign = GTK_ALIGN_CENTER;

	if (self->has_airplane_mode == FALSE) {
		g_debug ("No Bluetooth available");
		sensitive = FALSE;
		powered = FALSE;
		page = BLUETOOTH_NO_DEVICES_PAGE;
	} else if (self->hardware_airplane_mode) {
		g_debug ("Bluetooth is Hard blocked");
		sensitive = FALSE;
		powered = FALSE;
		page = BLUETOOTH_HW_AIRPLANE_PAGE;
	} else if (self->airplane_mode) {
		g_debug ("Airplane mode is on, Wi-Fi and Bluetooth are disabled");
		sensitive = FALSE;
		powered = FALSE;
		page = BLUETOOTH_AIRPLANE_PAGE;
	} else if (self->bt_airplane_mode ||
		   !bluetooth_settings_widget_get_default_adapter_powered (BLUETOOTH_SETTINGS_WIDGET (self->widget))) {
		g_debug ("Default adapter is unpowered, but should be available");
		sensitive = TRUE;
		change_powered = FALSE;
		page = BLUETOOTH_DISABLED_PAGE;
	} else {
		g_debug ("Bluetooth is available and powered");
		sensitive = TRUE;
		powered = TRUE;
		page = BLUETOOTH_WORKING_PAGE;
		valign = GTK_ALIGN_FILL;
	}

	gtk_widget_set_valign (self->stack, valign);
	gtk_widget_set_sensitive (WID ("box_power") , sensitive);

	toggle = G_OBJECT (WID ("switch_bluetooth"));
	if (change_powered) {
		g_signal_handlers_block_by_func (toggle, power_callback, self);
		gtk_switch_set_active (GTK_SWITCH (toggle), powered);
		g_signal_handlers_unblock_by_func (toggle, power_callback, self);
	}

	gtk_stack_set_visible_child_name (GTK_STACK (self->stack), page);
}

static void
airplane_mode_changed (GDBusProxy       *proxy,
		       GVariant         *changed_properties,
		       GStrv             invalidated_properties,
		       CcBluetoothPanel *self)
{
	GVariant *v;

	v = g_dbus_proxy_get_cached_property (self->rfkill, "AirplaneMode");
	self->airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	v = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothAirplaneMode");
	self->bt_airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	v = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothHardwareAirplaneMode");
	self->hardware_airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	v = g_dbus_proxy_get_cached_property (self->rfkill, "BluetoothHasAirplaneMode");
	self->has_airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	cc_bluetooth_panel_update_power (self);
}

static void
on_airplane_mode_off_clicked (GtkButton        *button,
			      CcBluetoothPanel *self)
{
	g_debug ("Airplane Mode Off clicked, disabling airplane mode");
	g_dbus_proxy_call (self->rfkill,
			   "org.freedesktop.DBus.Properties.Set",
			   g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
						 "'AirplaneMode', %v)",
						 g_variant_new_boolean (FALSE)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   self->cancellable,
			   NULL, NULL);
}

static void
add_stack_page (CcBluetoothPanel *self,
		const char       *message,
		const char       *explanation,
		const char       *name)
{
	GtkWidget *label, *image, *box;
	char *str;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (G_OBJECT (box), "margin-top", 64, "margin-bottom", 64, NULL);

	if (g_str_equal (name, BLUETOOTH_AIRPLANE_PAGE) ||
	    g_str_equal (name, BLUETOOTH_HW_AIRPLANE_PAGE)) {
		image = gtk_image_new_from_icon_name ("airplane-mode-symbolic",
						      GTK_ICON_SIZE_DIALOG);
	} else {
		image = gtk_image_new_from_icon_name ("bluetooth-active-symbolic",
						      GTK_ICON_SIZE_DIALOG);
	}
	gtk_image_set_pixel_size (GTK_IMAGE (image), 192);
	gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 24);

	str = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>", message);
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	label = gtk_label_new (explanation);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	if (g_str_equal (name, BLUETOOTH_AIRPLANE_PAGE)) {
		GtkWidget *button;

		button = gtk_button_new_with_label (_("Turn Off Airplane Mode"));
		gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
		g_signal_connect (G_OBJECT (button), "clicked",
				  G_CALLBACK (on_airplane_mode_off_clicked), self);
		gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 24);
	}

	gtk_stack_add_named (GTK_STACK (self->stack), box, name);
	gtk_widget_show_all (box);
}

static void
panel_changed (GtkWidget        *settings_widget,
	       const char       *panel,
	       CcBluetoothPanel *self)
{
	CcShell *shell;
	GError *error = NULL;

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (cc_shell_set_active_panel_from_id (shell, panel, NULL, &error) == FALSE) {
		g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
		g_error_free (error);
	}
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	GError *error = NULL;

	g_resources_register (cc_bluetooth_get_resource ());

	self->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (self->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (self->builder,
                                       "/org/gnome/control-center/bluetooth/bluetooth.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Could not load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	self->cancellable = g_cancellable_new ();

	/* RFKill */
	self->rfkill = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
								 G_DBUS_PROXY_FLAGS_NONE,
								 "org.gnome.SettingsDaemon.Rfkill",
								 "/org/gnome/SettingsDaemon/Rfkill",
								 "org.gnome.SettingsDaemon.Rfkill",
								 NULL, NULL);
	self->properties = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
								     G_DBUS_PROXY_FLAGS_NONE,
								     "org.gnome.SettingsDaemon.Rfkill",
								     "/org/gnome/SettingsDaemon/Rfkill",
								     "org.freedesktop.DBus.Properties",
								     NULL, NULL);

	self->stack = gtk_stack_new ();
	gtk_stack_set_homogeneous (GTK_STACK (self->stack), TRUE);
	add_stack_page (self, _("No Bluetooth Found"), _("Plug in a dongle to use Bluetooth."), BLUETOOTH_NO_DEVICES_PAGE);
	add_stack_page (self, _("Bluetooth Turned Off"), _("Turn on to connect devices and receive file transfers."), BLUETOOTH_DISABLED_PAGE);
	add_stack_page (self, _("Airplane Mode is on"), _("Bluetooth is disabled when airplane mode is on."), BLUETOOTH_AIRPLANE_PAGE);
	add_stack_page (self, _("Hardware Airplane Mode is on"), _("Turn off the Airplane mode switch to enable Bluetooth."), BLUETOOTH_HW_AIRPLANE_PAGE);

	self->widget = bluetooth_settings_widget_new ();
	g_signal_connect (G_OBJECT (self->widget), "panel-changed",
			  G_CALLBACK (panel_changed), self);
	gtk_stack_add_named (GTK_STACK (self->stack),
			     self->widget, BLUETOOTH_WORKING_PAGE);
	gtk_widget_show (self->widget);
	gtk_widget_show (self->stack);

	gtk_container_add (GTK_CONTAINER (self), self->stack);

	airplane_mode_changed (NULL, NULL, NULL, self);
	g_signal_connect_object (self->rfkill, "g-properties-changed",
                                 G_CALLBACK (airplane_mode_changed), self, 0);
	g_signal_connect_object (G_OBJECT (self->widget), "adapter-status-changed",
                                 G_CALLBACK (cc_bluetooth_panel_update_power), self, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT (WID ("switch_bluetooth")), "notify::active",
			  G_CALLBACK (power_callback), self);
}
