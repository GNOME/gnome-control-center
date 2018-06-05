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
	GHashTable       *devices; /* key=GsdDevice, value=CcWacomDevice */
	GHashTable       *pages; /* key=device name, value=GtkWidget */
	GHashTable       *stylus_pages; /* key=CcWacomTool, value=GtkWidget */
	GsdDeviceManager *manager;
	guint             device_added_id;
	guint             device_removed_id;

	CcTabletToolMap  *tablet_tool_map;

	/* DBus */
	GCancellable  *cancellable;
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

static CcWacomPage *
set_device_page (CcWacomPanel *self, const gchar *device_name)
{
	CcWacomPage *page;
	gint current;

	if (device_name == NULL)
		return NULL;

	/* Choose correct device */
	page = g_hash_table_lookup (self->pages, device_name);

	if (page == NULL) {
		g_warning ("Failed to find device '%s', supplied in the command line.", device_name);
		return page;
	}

	current = gtk_notebook_page_num (GTK_NOTEBOOK (self->tablet_notebook), GTK_WIDGET (page));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (self->tablet_notebook), current);

	return page;
}

static void
run_operation_from_params (CcWacomPanel *self, GVariant *parameters)
{
	GVariant *v;
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
		g_variant_unref (v);

		return;
	}

	g_variant_unref (v);

	switch (n_params) {
		case 3:
			page = set_device_page (self, device_name);
			if (page == NULL)
				return;

			g_variant_get_child (parameters, 1, "v", &v);

			if (!g_variant_is_of_type (v, G_VARIANT_TYPE_STRING)) {
				g_warning ("Wrong type for the operation name argument. A string is expected.");
				g_variant_unref (v);
				break;
			}

			operation = g_variant_get_string (v, NULL);
			if (g_strcmp0 (operation, "run-calibration") == 0) {
				if (cc_wacom_page_can_calibrate (page))
					cc_wacom_page_calibrate (page);
				else
					g_warning ("The device %s cannot be calibrated.", device_name);
			} else {
				g_warning ("Ignoring unrecognized operation '%s'", operation);
			}
			g_variant_unref (v);
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

	if (self->builder)
	{
		g_object_unref (self->builder);
		self->builder = NULL;
	}

	if (self->manager)
	{
		g_signal_handler_disconnect (self->manager, self->device_added_id);
		g_signal_handler_disconnect (self->manager, self->device_removed_id);
		self->manager = NULL;
	}

	if (self->devices)
	{
		g_hash_table_destroy (self->devices);
		self->devices = NULL;
	}

	g_clear_object (&self->cancellable);
	g_clear_object (&self->proxy);

	if (self->pages)
	{
		g_hash_table_destroy (self->pages);
		self->pages = NULL;
	}

	if (self->stylus_pages)
	{
		g_hash_table_destroy (self->stylus_pages);
		self->stylus_pages = NULL;
	}

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
}

static void
check_remove_stylus_pages (CcWacomPanel *self)
{
	GHashTableIter iter;
	CcWacomDevice *device;
	CcWacomTool *tool;
	GtkWidget *page;
	GList *tools, *total = NULL;

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

	g_list_free (total);
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
update_current_tool (CcWacomPanel  *panel,
		     GdkDevice     *device,
		     GdkDeviceTool *tool)
{
	GsdDeviceManager *device_manager;
	CcWacomDevice *wacom_device;
	CcWacomTool *stylus;
	GsdDevice *gsd_device;
	guint64 serial, id;

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
	stylus = cc_tablet_tool_map_lookup_tool (panel->tablet_tool_map,
						 wacom_device, serial);

	if (!stylus) {
		gboolean added;

		id = gdk_device_tool_get_hardware_id (tool);
		stylus = cc_wacom_tool_new (serial, id, wacom_device);
		if (!stylus)
			return;

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
	}

	cc_tablet_tool_map_add_relation (panel->tablet_tool_map,
					 wacom_device, stylus);
}

static gboolean
on_shell_event_cb (GtkWidget    *wigdet,
		   GdkEvent     *event,
		   CcWacomPanel *panel)
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

	cc_shell_embed_widget_in_header (shell, button);

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
				 G_CALLBACK (on_shell_event_cb), self, 0);
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
remove_page (GtkNotebook *notebook,
	     GtkWidget   *widget)
{
	int num_pages, i;

	num_pages = gtk_notebook_get_n_pages (notebook);
	g_return_if_fail (num_pages > 1);
	for (i = 1; i < num_pages; i++) {
		if (gtk_notebook_get_nth_page (notebook, i) == widget) {
			gtk_notebook_remove_page (notebook, i);
			return;
		}
	}
}

static void
update_current_page (CcWacomPanel  *self,
		     CcWacomDevice *removed_device)
{
	GHashTable *ht;
	GList *tablets, *l;
	gboolean changed;
	GHashTableIter iter;
	GsdDevice *gsd_device;
	CcWacomDevice *device;

	changed = FALSE;

	ht = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	if (removed_device) {
		Tablet *tablet = g_new0 (Tablet, 1);
		tablet->name = cc_wacom_device_get_name (removed_device);
		g_hash_table_insert (ht, (gpointer) tablet->name, tablet);
	}

	g_hash_table_iter_init (&iter, self->devices);

	while (g_hash_table_iter_next (&iter, (gpointer*) &gsd_device,
				       (gpointer*) &device)) {
		Tablet *tablet;
		GsdDeviceType device_type;

		device_type = gsd_device_get_device_type (gsd_device);
		tablet = g_hash_table_lookup (ht, cc_wacom_device_get_name (device));
		if (tablet == NULL) {
			tablet = g_new0 (Tablet, 1);
			tablet->name = cc_wacom_device_get_name (device);
			g_hash_table_insert (ht, (gpointer) tablet->name, tablet);
		}

		if (device_type & GSD_DEVICE_TYPE_PAD) {
			tablet->pad = device;
		} else if (device_type & GSD_DEVICE_TYPE_TABLET) {
			tablet->stylus = device;
		}
	}

	/* We now have a list of Tablet structs,
	 * see which ones are full tablets */
	tablets = g_hash_table_get_values (ht);
	for (l = tablets; l; l = l->next) {
		Tablet *tablet;
		GtkWidget *page;

		tablet = l->data;
		if (tablet->stylus == NULL) {
			page = g_hash_table_lookup (self->pages, tablet->name);
			if (page != NULL) {
				remove_page (GTK_NOTEBOOK (self->tablet_notebook), page);
				g_hash_table_remove (self->pages, tablet->name);

				changed = TRUE;
			}
			continue;
		}
		/* this code is called once the stylus is set up, but the pad does not exist yet */
		page = g_hash_table_lookup (self->pages, tablet->name);
		if (page == NULL) {
			page = cc_wacom_page_new (self, tablet->stylus, tablet->pad);
			cc_wacom_page_set_navigation (CC_WACOM_PAGE (page), GTK_NOTEBOOK (self->tablet_notebook), TRUE);
			gtk_widget_show (page);
			gtk_notebook_append_page (GTK_NOTEBOOK (self->tablet_notebook), page, NULL);
			g_hash_table_insert (self->pages, g_strdup (tablet->name), page);

			changed = TRUE;
		} else {
			cc_wacom_page_update_tools (CC_WACOM_PAGE (page), tablet->stylus, tablet->pad);
		}
	}
	g_list_free (tablets);

	g_hash_table_destroy (ht);

	if (changed == TRUE) {
		int num_pages;

		num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (self->tablet_notebook));
		if (num_pages > 1)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (self->tablet_notebook), 1);
	}
}

static void
add_known_device (CcWacomPanel *self,
		  GsdDevice    *gsd_device)
{
	CcWacomDevice *device;
	GsdDeviceType device_type;
	GList *tools, *l;

	device_type = gsd_device_get_device_type (gsd_device);

	if ((device_type & GSD_DEVICE_TYPE_TABLET) == 0)
		return;

	if ((device_type &
	     (GSD_DEVICE_TYPE_TOUCHSCREEN | GSD_DEVICE_TYPE_TOUCHPAD)) != 0) {
		return;
	}

	device = cc_wacom_device_new (gsd_device);
	if (!device)
		return;

	g_hash_table_insert (self->devices, gsd_device, device);

	/* Only trigger tool lookup on pen devices */
	if ((device_type & GSD_DEVICE_TYPE_TABLET) != 0) {
		tools = cc_tablet_tool_map_list_tools (self->tablet_tool_map, device);

		for (l = tools; l != NULL; l = l->next) {
			add_stylus (self, l->data);
		}

		g_list_free (tools);
	}
}

static void
device_removed_cb (GsdDeviceManager *manager,
		   GsdDevice        *gsd_device,
		   CcWacomPanel     *self)
{
	CcWacomDevice *device;

	device = g_hash_table_lookup (self->devices, gsd_device);
	if (!device)
		return;

	g_hash_table_steal (self->devices, gsd_device);
	update_current_page (self, device);
	check_remove_stylus_pages (self);
	g_object_unref (device);
}

static void
device_added_cb (GsdDeviceManager *manager,
		 GsdDevice        *device,
		 CcWacomPanel     *self)
{
	add_known_device (self, device);
	update_current_page (self, NULL);
}

static gboolean
link_activated (GtkLinkButton *button,
		CcWacomPanel  *self)
{
	cc_wacom_panel_switch_to_panel (self, "bluetooth");
	return TRUE;
}

void
cc_wacom_panel_switch_to_panel (CcWacomPanel *self,
				const char   *panel)
{
	CcShell *shell;
	GError *error = NULL;

	g_return_if_fail (self);

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (cc_shell_set_active_panel_from_id (shell, panel, NULL, &error) == FALSE)
	{
		g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
		g_error_free (error);
	}
}

static void
got_osd_proxy_cb (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      data)
{
	GError              *error = NULL;
	CcWacomPanel        *self;

	self = CC_WACOM_PANEL (data);
	self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	g_clear_object (&self->cancellable);

	if (self->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		g_error_free (error);
		return;
	}
}

static void
enbiggen_label (GtkLabel *label)
{
	const char *str;
	char *new_str;

	str = gtk_label_get_text (label);
	new_str = g_strdup_printf ("<big>%s</big>", str);
	gtk_label_set_markup (label, new_str);
	g_free (new_str);
}

static void
on_stack_visible_child_notify_cb (GObject      *object,
				  GParamSpec   *pspec,
				  CcWacomPanel *panel)
{
	GtkWidget *child;

	child = gtk_stack_get_visible_child (GTK_STACK (object));

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
	GList *devices, *l;
	GError *error = NULL;
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
	if (error != NULL)
	{
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (self->builder);
		g_error_free (error);
		return;
	}

	self->tablet_tool_map = cc_tablet_tool_map_new ();

	self->cancellable = g_cancellable_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.gnome.Shell",
				  "/org/gnome/Shell/Wacom",
				  "org.gnome.Shell.Wacom.PadOsd",
				  self->cancellable,
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

	g_signal_connect (self->stack, "notify::visible-child",
			  G_CALLBACK (on_stack_visible_child_notify_cb), self);

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

	g_signal_connect (G_OBJECT (WID ("linkbutton")), "activate-link",
			  G_CALLBACK (link_activated), self);

	self->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	self->pages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	self->stylus_pages = g_hash_table_new (NULL, NULL);

	self->manager = gsd_device_manager_get ();
	self->device_added_id = g_signal_connect (G_OBJECT (self->manager), "device-added",
						  G_CALLBACK (device_added_cb), self);
	self->device_removed_id = g_signal_connect (G_OBJECT (self->manager), "device-removed",
						    G_CALLBACK (device_removed_cb), self);

	devices = gsd_device_manager_list_devices (self->manager,
						   GSD_DEVICE_TYPE_TABLET);
	for (l = devices; l ; l = l->next)
		add_known_device (self, l->data);
	g_list_free (devices);

	update_current_page (self, NULL);
}

GDBusProxy *
cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self)
{
	g_return_val_if_fail (CC_IS_WACOM_PANEL (self), NULL);

	return self->proxy;
}
