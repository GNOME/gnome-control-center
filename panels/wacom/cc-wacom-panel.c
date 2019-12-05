/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "shell/cc-application.h"
#include "shell/cc-debug.h"
#include "cc-wacom-panel.h"
#include "cc-wacom-page.h"
#include "cc-wacom-stylus-page.h"
#include "cc-wacom-resources.h"
#include "cc-drawing-area.h"
#include "cc-tablet-tool-map.h"
#include "gsd-device-manager.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define WID(x) (GtkWidget *) gtk_builder_get_object (self->builder, x)

struct _CcWacomPanel
{
	CcPanel           parent_instance;

	GtkBuilder       *builder;
	GtkWidget        *stack;
	GtkWidget        *switcher;
	GtkWidget        *tablet_notebook;
	GtkWidget        *stylus_notebook;
	GtkWidget        *test_popover;
	GtkWidget        *test_draw_area;
	GtkWidget        *test_button;
	GHashTable       *devices; /* key=GsdDevice, value=CcWacomDevice */
	GHashTable       *pages; /* key=CcWacomDevice, value=GtkWidget */
	GHashTable       *stylus_pages; /* key=CcWacomTool, value=GtkWidget */

	CcTabletToolMap  *tablet_tool_map;

	/* DBus */
	GDBusProxy    *proxy;
};

CC_PANEL_REGISTER (CcWacomPanel, cc_wacom_panel)

typedef struct {
	const char *name;
	CcWacomDevice *stylus;
	CcWacomDevice *pad;
} Tablet;

enum {
	WACOM_PAGE = -1,
	PLUG_IN_PAGE = 0,
};

enum {
	PROP_0,
	PROP_PARAMETERS
};

/* Static init function */
static void
update_visibility (GsdDeviceManager *manager,
		   GsdDevice        *device,
		   gpointer          user_data)
{
	CcApplication *application;
	g_autoptr(GList) devices = NULL;
	guint i;

	devices = gsd_device_manager_list_devices (manager, GSD_DEVICE_TYPE_TABLET);
	i = g_list_length (devices);

	/* Set the new visibility */
	application = CC_APPLICATION (g_application_get_default ());
	cc_shell_model_set_panel_visibility (cc_application_get_model (application),
					     "wacom",
					     i > 0 ? CC_PANEL_VISIBLE : CC_PANEL_VISIBLE_IN_SEARCH);

	g_debug ("Wacom panel visible: %s", i > 0 ? "yes" : "no");
}

void
cc_wacom_panel_static_init_func (void)
{
	GsdDeviceManager *manager;

	manager = gsd_device_manager_get ();
	g_signal_connect (G_OBJECT (manager), "device-added",
			  G_CALLBACK (update_visibility), NULL);
	g_signal_connect (G_OBJECT (manager), "device-removed",
			  G_CALLBACK (update_visibility), NULL);
	update_visibility (manager, NULL, NULL);
}

static CcWacomDevice *
lookup_wacom_device (CcWacomPanel *self,
		     const gchar  *name)
{
	GHashTableIter iter;
	CcWacomDevice *wacom_device;

	g_hash_table_iter_init (&iter, self->devices);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &wacom_device)) {
		if (g_strcmp0 (cc_wacom_device_get_name (wacom_device), name) == 0)
			return wacom_device;
	}

	return NULL;
}

static CcWacomPage *
set_device_page (CcWacomPanel *self, const gchar *device_name)
{
	CcWacomPage *page;
	CcWacomDevice *wacom_device;
	gint current;

	if (device_name == NULL)
		return NULL;

	wacom_device = lookup_wacom_device (self, device_name);
	if (!wacom_device) {
		g_warning ("Failed to find device '%s', supplied in the command line.", device_name);
		return NULL;
	}

	page = g_hash_table_lookup (self->pages, wacom_device);
	current = gtk_notebook_page_num (GTK_NOTEBOOK (self->tablet_notebook), GTK_WIDGET (page));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (self->tablet_notebook), current);

	return page;
}

static void
run_operation_from_params (CcWacomPanel *self, GVariant *parameters)
{
	g_autoptr(GVariant) v = NULL;
	g_autoptr(GVariant) v2 = NULL;
	CcWacomPage *page;
	const gchar *operation = NULL;
	const gchar *device_name = NULL;
	gint n_params;

	n_params = g_variant_n_children (parameters);

	g_variant_get_child (parameters, n_params - 1, "v", &v);
	device_name = g_variant_get_string (v, NULL);

	if (!g_variant_is_of_type (v, G_VARIANT_TYPE_STRING)) {
		g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
			   g_variant_get_type_string (v));
		return;
	}

	switch (n_params) {
		case 3:
			page = set_device_page (self, device_name);
			if (page == NULL)
				return;

			g_variant_get_child (parameters, 1, "v", &v2);

			if (!g_variant_is_of_type (v2, G_VARIANT_TYPE_STRING)) {
				g_warning ("Wrong type for the operation name argument. A string is expected.");
				break;
			}

			operation = g_variant_get_string (v2, NULL);
			if (g_strcmp0 (operation, "run-calibration") == 0) {
				if (cc_wacom_page_can_calibrate (page))
					cc_wacom_page_calibrate (page);
				else
					g_warning ("The device %s cannot be calibrated.", device_name);
			} else {
				g_warning ("Ignoring unrecognized operation '%s'", operation);
			}
		case 2:
			set_device_page (self, device_name);
			break;
		case 1:
			g_assert_not_reached ();
		default:
			g_warning ("Unexpected number of parameters found: %d. Request ignored.", n_params);
	}
}

/* Boilerplate code goes below */

static void
cc_wacom_panel_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_panel_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	CcWacomPanel *self;
	self = CC_WACOM_PANEL (object);

	switch (property_id)
	{
		case PROP_PARAMETERS: {
			GVariant *parameters;

			parameters = g_value_get_variant (value);
			if (parameters == NULL || g_variant_n_children (parameters) <= 1)
				return;

			run_operation_from_params (self, parameters);

			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_panel_dispose (GObject *object)
{
	CcWacomPanel *self = CC_WACOM_PANEL (object);

	g_clear_object (&self->builder);

	g_clear_pointer (&self->devices, g_hash_table_unref);
	g_clear_object (&self->proxy);
	g_clear_pointer (&self->pages, g_hash_table_unref);
	g_clear_pointer (&self->stylus_pages, g_hash_table_unref);

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
}

static void
check_remove_stylus_pages (CcWacomPanel *self)
{
	GHashTableIter iter;
	CcWacomDevice *device;
	CcWacomTool *tool;
	GtkWidget *page;
	GList *tools;
	g_autoptr(GList) total = NULL;

	/* First. Iterate known devices and get the tools */
	g_hash_table_iter_init (&iter, self->devices);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &device)) {
		tools = cc_tablet_tool_map_list_tools (self->tablet_tool_map, device);
		total = g_list_concat (total, tools);
	}

	/* Second. Iterate through stylus pages and remove the ones whose
	 * tool is no longer in the list.
	 */
	g_hash_table_iter_init (&iter, self->stylus_pages);
	while (g_hash_table_iter_next (&iter, (gpointer*) &tool, (gpointer*) &page)) {
		if (g_list_find (total, tool))
			continue;
		gtk_widget_destroy (page);
		g_hash_table_iter_remove (&iter);
	}
}

static gboolean
add_stylus (CcWacomPanel *self,
	    CcWacomTool  *tool)
{
	GtkWidget *page;

	if (g_hash_table_lookup (self->stylus_pages, tool))
		return FALSE;

	page = cc_wacom_stylus_page_new (tool);
	cc_wacom_stylus_page_set_navigation (CC_WACOM_STYLUS_PAGE (page),
					     GTK_NOTEBOOK (self->stylus_notebook));
	gtk_widget_show (page);
	gtk_notebook_append_page (GTK_NOTEBOOK (self->stylus_notebook), page, NULL);
	g_hash_table_insert (self->stylus_pages, tool, page);

	if (gtk_notebook_get_current_page (GTK_NOTEBOOK (self->stylus_notebook)) == 0)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (self->stylus_notebook), 1);

	return TRUE;
}

static void
update_test_button (CcWacomPanel *self)
{
	if (!self->test_button)
		return;

	if (g_hash_table_size (self->devices) == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->test_button), FALSE);
		gtk_widget_set_sensitive (self->test_button, FALSE);
	} else {
		gtk_widget_set_sensitive (self->test_button, TRUE);
	}
}

static void
update_current_tool (CcWacomPanel  *panel,
		     GdkDevice     *device,
		     GdkDeviceTool *tool)
{
	GsdDeviceManager *device_manager;
	CcWacomDevice *wacom_device;
	CcWacomTool *stylus;
	GsdDevice *gsd_device;
	guint64 serial, id;
	gboolean added;

	if (!tool)
		return;

	/* Work our way to the CcWacomDevice */
	device_manager = gsd_device_manager_get ();
	gsd_device = gsd_device_manager_lookup_gdk_device (device_manager,
							   device);
	if (!gsd_device)
		return;

	wacom_device = g_hash_table_lookup (panel->devices, gsd_device);
	if (!wacom_device)
		return;

	/* Check whether we already know this tool, nothing to do then */
	serial = gdk_device_tool_get_serial (tool);

	/* The wacom driver sends serial-less tools with a serial of
	 * 1, libinput uses 0. No device exists with serial 1, let's reset
	 * it here so everything else works as expected.
	 */
	if (serial == 1)
		serial = 0;

	stylus = cc_tablet_tool_map_lookup_tool (panel->tablet_tool_map,
						 wacom_device, serial);

	if (!stylus) {
		id = gdk_device_tool_get_hardware_id (tool);

		/* The wacom driver sends a hw id of 0x2 for stylus and 0xa
		 * for eraser for devices that don't have a true HW id.
		 * Reset those to 0 so we can use the same code-paths
		 * libinput uses.
		 * The touch ID is 0x3, let's ignore that because we don't
		 * have a touch tool and it only happens when the wacom
		 * driver handles the touch device.
		 */
		if (id == 0x2 || id == 0xa)
			id = 0;
		else if (id == 0x3)
			return;

		stylus = cc_wacom_tool_new (serial, id, wacom_device);
		if (!stylus)
			return;
        }

	added = add_stylus (panel, stylus);

	if (added) {
		if (panel->stylus_notebook ==
		    gtk_stack_get_visible_child (GTK_STACK (panel->stack))) {
			GtkWidget *widget;
			gint page;

			widget = g_hash_table_lookup (panel->stylus_pages, stylus);
			page = gtk_notebook_page_num (GTK_NOTEBOOK (panel->stylus_notebook), widget);
			gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->stylus_notebook), page);
		} else {
			gtk_container_child_set (GTK_CONTAINER (panel->stack),
						 panel->stylus_notebook,
						 "needs-attention", TRUE,
						 NULL);
		}
	}

	cc_tablet_tool_map_add_relation (panel->tablet_tool_map,
					 wacom_device, stylus);
}

static gboolean
on_shell_event_cb (CcWacomPanel *panel,
		   GdkEvent     *event)
{
	if (event->type == GDK_MOTION_NOTIFY) {
		update_current_tool (panel,
				     gdk_event_get_source_device (event),
				     gdk_event_get_device_tool (event));
	}

	return GDK_EVENT_PROPAGATE;
}

static void
cc_wacom_panel_constructed (GObject *object)
{
	CcWacomPanel *self = CC_WACOM_PANEL (object);
	GtkWidget *button;
	CcShell *shell;

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->constructed (object);

	/* Add test area button to shell header. */
	shell = cc_panel_get_shell (CC_PANEL (self));

	button = gtk_toggle_button_new_with_mnemonic (_("Test Your _Settings"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
				     "text-button");
	gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
	gtk_widget_set_visible (button, TRUE);

	cc_shell_embed_widget_in_header (shell, button, GTK_POS_RIGHT);

	self->test_popover = gtk_popover_new (button);
	gtk_container_set_border_width (GTK_CONTAINER (self->test_popover), 6);

	self->test_draw_area = cc_drawing_area_new ();
	gtk_widget_set_size_request (self->test_draw_area, 400, 300);
	gtk_container_add (GTK_CONTAINER (self->test_popover),
			   self->test_draw_area);
	gtk_widget_show (self->test_draw_area);

	g_object_bind_property (button, "active",
				self->test_popover, "visible",
				G_BINDING_BIDIRECTIONAL);

	g_signal_connect_object (shell, "event",
				 G_CALLBACK (on_shell_event_cb), self, G_CONNECT_SWAPPED);

	self->test_button = button;
	update_test_button (self);
}

static const char *
cc_wacom_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/wacom";
}

static GtkWidget *
cc_wacom_panel_get_title_widget (CcPanel *panel)
{
	CcWacomPanel *self = CC_WACOM_PANEL (panel);

	return self->switcher;
}

static void
cc_wacom_panel_class_init (CcWacomPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	object_class->get_property = cc_wacom_panel_get_property;
	object_class->set_property = cc_wacom_panel_set_property;
	object_class->dispose = cc_wacom_panel_dispose;
	object_class->constructed = cc_wacom_panel_constructed;

	panel_class->get_help_uri = cc_wacom_panel_get_help_uri;
	panel_class->get_title_widget = cc_wacom_panel_get_title_widget;

	g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static void
update_current_page (CcWacomPanel  *self)
{
	int num_pages;

	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (self->tablet_notebook));
	if (num_pages > 1)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (self->tablet_notebook), 1);

	update_test_button (self);
}

static void
add_known_device (CcWacomPanel *self,
		  GsdDevice    *gsd_device)
{
	CcWacomDevice *device;
	GsdDeviceType device_type;
	g_autoptr(GList) tools = NULL;
	GtkWidget *page;
	GList *l;

	device_type = gsd_device_get_device_type (gsd_device);

	if ((device_type & GSD_DEVICE_TYPE_TABLET) == 0)
		return;

	if ((device_type &
	     (GSD_DEVICE_TYPE_PAD |
	      GSD_DEVICE_TYPE_TOUCHSCREEN |
	      GSD_DEVICE_TYPE_TOUCHPAD)) != 0) {
		return;
	}

	device = cc_wacom_device_new (gsd_device);
	if (!device)
		return;

	g_hash_table_insert (self->devices, gsd_device, device);

	tools = cc_tablet_tool_map_list_tools (self->tablet_tool_map, device);

	for (l = tools; l != NULL; l = l->next) {
		add_stylus (self, l->data);
	}

	page = cc_wacom_page_new (self, device);
	cc_wacom_page_set_navigation (CC_WACOM_PAGE (page), GTK_NOTEBOOK (self->tablet_notebook), TRUE);
	gtk_widget_show (page);
	gtk_notebook_append_page (GTK_NOTEBOOK (self->tablet_notebook), page, NULL);
	g_hash_table_insert (self->pages, device, page);
}

static void
device_removed_cb (CcWacomPanel     *self,
		   GsdDevice        *gsd_device)
{
	CcWacomDevice *device;
	GtkWidget *page;

	device = g_hash_table_lookup (self->devices, gsd_device);
	if (!device)
		return;

	page = g_hash_table_lookup (self->pages, device);
	if (page) {
		g_hash_table_remove (self->pages, device);
		gtk_widget_destroy (page);
	}

	g_hash_table_remove (self->devices, gsd_device);
	check_remove_stylus_pages (self);
	update_current_page (self);
}

static void
device_added_cb (CcWacomPanel *self,
		 GsdDevice    *device)
{
	add_known_device (self, device);
	update_current_page (self);
}

static gboolean
link_activated (CcWacomPanel  *self)
{
	cc_wacom_panel_switch_to_panel (self, "bluetooth");
	return TRUE;
}

void
cc_wacom_panel_switch_to_panel (CcWacomPanel *self,
				const char   *panel)
{
	CcShell *shell;
	g_autoptr(GError) error = NULL;

	g_return_if_fail (self);

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (!cc_shell_set_active_panel_from_id (shell, panel, NULL, &error))
		g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
}

static void
got_osd_proxy_cb (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      data)
{
	g_autoptr(GError)    error = NULL;
	CcWacomPanel        *self;

	self = CC_WACOM_PANEL (data);
	self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (self->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		return;
	}
}

static void
enbiggen_label (GtkLabel *label)
{
	const char *str;
	g_autofree char *new_str = NULL;

	str = gtk_label_get_text (label);
	new_str = g_strdup_printf ("<big>%s</big>", str);
	gtk_label_set_markup (label, new_str);
}

static void
on_stack_visible_child_notify_cb (CcWacomPanel *panel)
{
	GtkWidget *child;

	child = gtk_stack_get_visible_child (GTK_STACK (panel->stack));

	if (child == panel->stylus_notebook) {
		gtk_container_child_set (GTK_CONTAINER (panel->stack),
					 panel->stylus_notebook,
					 "needs-attention", FALSE,
					 NULL);
	}
}

static void
cc_wacom_panel_init (CcWacomPanel *self)
{
	GtkWidget *widget;
	GsdDeviceManager *device_manager;
	g_autoptr(GList) devices = NULL;
	GList *l;
	g_autoptr(GError) error = NULL;
	char *objects[] = {
		"main-box",
		"no-stylus-page",
		NULL
	};

        g_resources_register (cc_wacom_get_resource ());

	self->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_resource (self->builder,
                                               "/org/gnome/control-center/wacom/gnome-wacom-properties.ui",
                                               objects,
                                               &error);
	gtk_builder_add_objects_from_resource (self->builder,
                                               "/org/gnome/control-center/wacom/wacom-stylus-page.ui",
                                               objects,
                                               &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		return;
	}

	self->tablet_tool_map = cc_tablet_tool_map_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.gnome.Shell",
				  "/org/gnome/Shell/Wacom",
				  "org.gnome.Shell.Wacom.PadOsd",
				  cc_panel_get_cancellable (CC_PANEL (self)),
				  got_osd_proxy_cb,
				  self);

	/* Stack + Switcher */
	self->stack = gtk_stack_new ();
	g_object_set (G_OBJECT (self->stack),
		      "margin-top", 30,
		      "margin-end", 30,
		      "margin-start", 30,
		      "margin-bottom", 30,
		      NULL);

	g_signal_connect_object (self->stack, "notify::visible-child",
	                         G_CALLBACK (on_stack_visible_child_notify_cb), self, G_CONNECT_SWAPPED);

	self->switcher = gtk_stack_switcher_new ();
	gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (self->switcher),
				      GTK_STACK (self->stack));
	gtk_widget_show (self->switcher);

	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->stack));
	gtk_widget_show (self->stack);

	self->tablet_notebook = gtk_notebook_new ();
	gtk_widget_show (self->tablet_notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (self->tablet_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (self->tablet_notebook), FALSE);
	gtk_widget_set_vexpand (self->tablet_notebook, TRUE);

	self->stylus_notebook = gtk_notebook_new ();
	gtk_widget_show (self->stylus_notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (self->stylus_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (self->stylus_notebook), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (self->stylus_notebook), 0);
	gtk_widget_set_vexpand (self->stylus_notebook, TRUE);

	gtk_stack_add_titled (GTK_STACK (self->stack),
			      self->stylus_notebook, "stylus",
			      _("Stylus"));
	gtk_stack_add_titled (GTK_STACK (self->stack),
			      self->tablet_notebook, "tablet",
			      _("Tablet"));

	/* No styli page */
	widget = WID ("no-stylus-page");
	enbiggen_label (GTK_LABEL (WID ("no-stylus-label1")));
	gtk_notebook_append_page (GTK_NOTEBOOK (self->stylus_notebook), widget, NULL);

	/* No tablets page */
	widget = WID ("main-box");
	enbiggen_label (GTK_LABEL (WID ("advice-label1")));
	gtk_notebook_append_page (GTK_NOTEBOOK (self->tablet_notebook), widget, NULL);

	g_signal_connect_object (WID ("linkbutton"), "activate-link",
			         G_CALLBACK (link_activated), self, G_CONNECT_SWAPPED);

	self->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	self->pages = g_hash_table_new (NULL, NULL);
	self->stylus_pages = g_hash_table_new (NULL, NULL);

	device_manager = gsd_device_manager_get ();
	g_signal_connect_object (device_manager, "device-added",
				 G_CALLBACK (device_added_cb), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (device_manager, "device-removed",
				 G_CALLBACK (device_removed_cb), self, G_CONNECT_SWAPPED);

	devices = gsd_device_manager_list_devices (device_manager,
						   GSD_DEVICE_TYPE_TABLET);
	for (l = devices; l ; l = l->next)
		add_known_device (self, l->data);

	update_current_page (self);
}

GDBusProxy *
cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self)
{
	g_return_val_if_fail (CC_IS_WACOM_PANEL (self), NULL);

	return self->proxy;
}
