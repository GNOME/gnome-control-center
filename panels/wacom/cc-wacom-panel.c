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

#include "cc-wacom-panel.h"
#include "cc-wacom-page.h"
#include "cc-wacom-resources.h"
#include "cc-drawing-area.h"
#include "gsd-wacom-device.h"

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

CC_PANEL_REGISTER (CcWacomPanel, cc_wacom_panel)

#define WACOM_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PANEL, CcWacomPanelPrivate))

struct _CcWacomPanelPrivate
{
	GtkBuilder       *builder;
	GtkWidget        *notebook;
	GtkWidget        *test_popover;
	GtkWidget        *test_draw_area;
	GHashTable       *devices; /* key=GdkDevice, value=GsdWacomDevice */
	GHashTable       *pages; /* key=device name, value=GtkWidget */
	GdkDeviceManager *manager;
	guint             device_added_id;
	guint             device_removed_id;

	/* DBus */
	GCancellable  *cancellable;
	GDBusProxy    *proxy;
};

typedef struct {
	const char *name;
	GsdWacomDevice *stylus;
	GsdWacomDevice *pad;
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
	CcWacomPanelPrivate *priv;
	CcWacomPage *page;
	gint current;

	priv = self->priv;

	if (device_name == NULL)
		return NULL;

	/* Choose correct device */
	page = g_hash_table_lookup (priv->pages, device_name);

	if (page == NULL) {
		g_warning ("Failed to find device '%s', supplied in the command line.", device_name);
		return page;
	}

	current = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), GTK_WIDGET (page));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), current);

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
	CcWacomPanelPrivate *priv = CC_WACOM_PANEL (object)->priv;

	if (priv->builder)
	{
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->manager)
	{
		g_signal_handler_disconnect (priv->manager, priv->device_added_id);
		g_signal_handler_disconnect (priv->manager, priv->device_removed_id);
		priv->manager = NULL;
	}

	if (priv->devices)
	{
		g_hash_table_destroy (priv->devices);
		priv->devices = NULL;
	}

	g_clear_object (&priv->cancellable);
	g_clear_object (&priv->proxy);

	if (priv->pages)
	{
		g_hash_table_destroy (priv->pages);
		priv->pages = NULL;
	}

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
}

static void
cc_wacom_panel_constructed (GObject *object)
{
	CcWacomPanel *self = CC_WACOM_PANEL (object);
	CcWacomPanelPrivate *priv = self->priv;
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

	priv->test_popover = gtk_popover_new (button);
	gtk_container_set_border_width (GTK_CONTAINER (priv->test_popover), 6);

	priv->test_draw_area = cc_drawing_area_new ();
	gtk_widget_set_size_request (priv->test_draw_area, 400, 300);
	gtk_container_add (GTK_CONTAINER (priv->test_popover),
			   priv->test_draw_area);
	gtk_widget_show (priv->test_draw_area);

	g_object_bind_property (button, "active",
				priv->test_popover, "visible",
				G_BINDING_BIDIRECTIONAL);
}

static const char *
cc_wacom_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/wacom";
}

static void
cc_wacom_panel_class_init (CcWacomPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomPanelPrivate));

	object_class->get_property = cc_wacom_panel_get_property;
	object_class->set_property = cc_wacom_panel_set_property;
	object_class->dispose = cc_wacom_panel_dispose;
	object_class->constructed = cc_wacom_panel_constructed;

	panel_class->get_help_uri = cc_wacom_panel_get_help_uri;

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
update_current_page (CcWacomPanel *self)
{
	GHashTable *ht;
	GList *devices, *tablets, *l;
	gboolean changed;
	CcWacomPanelPrivate *priv;

	priv = self->priv;
	changed = FALSE;

	ht = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	devices = g_hash_table_get_values (priv->devices);
	for (l = devices; l; l = l->next) {
		Tablet *tablet;
		GsdWacomDevice *device;

		device = l->data;
		tablet = g_hash_table_lookup (ht, gsd_wacom_device_get_name (device));
		if (tablet == NULL) {
			tablet = g_new0 (Tablet, 1);
			tablet->name = gsd_wacom_device_get_name (device);
			g_hash_table_insert (ht, (gpointer) tablet->name, tablet);
		}

		switch (gsd_wacom_device_get_device_type (device)) {
		case WACOM_TYPE_STYLUS:
			tablet->stylus = device;
			break;
		case WACOM_TYPE_PAD:
			tablet->pad = device;
			break;
		case WACOM_TYPE_ERASER:
		default:
			/* Nothing */
			;
		}
	}
	g_list_free (devices);

	/* We now have a list of Tablet structs,
	 * see which ones are full tablets */
	tablets = g_hash_table_get_values (ht);
	for (l = tablets; l; l = l->next) {
		Tablet *tablet;
		GtkWidget *page;

		tablet = l->data;
		if (tablet->stylus == NULL) {
			page = g_hash_table_lookup (priv->pages, tablet->name);
			if (page != NULL) {
				remove_page (GTK_NOTEBOOK (priv->notebook), page);
				g_hash_table_remove (priv->pages, tablet->name);

				changed = TRUE;
			}
			continue;
		}
		/* this code is called once the stylus is set up, but the pad does not exist yet */
		page = g_hash_table_lookup (priv->pages, tablet->name);
		if (page == NULL) {
			page = cc_wacom_page_new (self, tablet->stylus, tablet->pad);
			cc_wacom_page_set_navigation (CC_WACOM_PAGE (page), GTK_NOTEBOOK (priv->notebook), TRUE);
			gtk_widget_show (page);
			gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), page, NULL);
			g_hash_table_insert (priv->pages, g_strdup (tablet->name), page);

			changed = TRUE;
		} else {
			cc_wacom_page_update_tools (CC_WACOM_PAGE (page), tablet->stylus, tablet->pad);
		}
	}
	g_list_free (tablets);

	g_hash_table_destroy (ht);

	if (changed == TRUE) {
		int num_pages;

		num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
		if (num_pages > 1)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), WACOM_PAGE);
	}
}

static void
add_known_device (CcWacomPanel *self,
		  GdkDevice    *gdk_device)
{
	CcWacomPanelPrivate *priv;
	GsdWacomDevice *device;

	priv = self->priv;

	device = gsd_wacom_device_new (gdk_device);
	if (gsd_wacom_device_get_device_type (device) == WACOM_TYPE_INVALID) {
		g_object_unref (device);
		return;
	}
	g_debug ("Adding device '%s' (type: '%s') to known devices list",
		 gsd_wacom_device_get_tool_name (device),
		 gsd_wacom_device_type_to_string (gsd_wacom_device_get_device_type (device)));
	g_hash_table_insert (priv->devices, (gpointer) gdk_device, device);
}

static void
device_removed_cb (GdkDeviceManager *manager,
		   GdkDevice        *gdk_device,
		   CcWacomPanel     *self)
{
	g_hash_table_remove (self->priv->devices, gdk_device);
	update_current_page (self);
}

static void
device_added_cb (GdkDeviceManager *manager,
		 GdkDevice        *device,
		 CcWacomPanel     *self)
{
	add_known_device (self, device);
	update_current_page (self);
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
got_wacom_proxy_cb (GObject      *source_object,
		    GAsyncResult *res,
		    gpointer      data)
{
	GError              *error = NULL;
	CcWacomPanel        *self;
	CcWacomPanelPrivate *priv;

	self = CC_WACOM_PANEL (data);
	priv = self->priv;
	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	g_clear_object (&priv->cancellable);

	if (priv->proxy == NULL) {
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
cc_wacom_panel_init (CcWacomPanel *self)
{
	CcWacomPanelPrivate *priv;
	GtkNotebook *notebook;
	GtkWidget *widget;
	GList *devices, *l;
	GError *error = NULL;
	char *objects[] = {
		"main-box",
		NULL
	};

	priv = self->priv = WACOM_PANEL_PRIVATE (self);
        g_resources_register (cc_wacom_get_resource ());

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_resource (priv->builder,
                                               "/org/gnome/control-center/wacom/gnome-wacom-properties.ui",
                                               objects,
                                               &error);
	if (error != NULL)
	{
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	priv->cancellable = g_cancellable_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.gnome.SettingsDaemon.Wacom",
				  "/org/gnome/SettingsDaemon/Wacom",
				  "org.gnome.SettingsDaemon.Wacom",
				  priv->cancellable,
				  got_wacom_proxy_cb,
				  self);

	/* Notebook */
	notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	priv->notebook = GTK_WIDGET (notebook);

	gtk_notebook_set_show_tabs (notebook, FALSE);
	gtk_notebook_set_show_border (notebook, FALSE);
	gtk_widget_set_vexpand (GTK_WIDGET (notebook), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 0);
	g_object_set (G_OBJECT (notebook),
		      "margin-top", 6,
		      "margin-end", 30,
		      "margin-start", 30,
		      "margin-bottom", 30,
		      NULL);

	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (notebook));
	gtk_widget_show (priv->notebook);

	/* No tablets page */
	widget = WID ("main-box");
	enbiggen_label (GTK_LABEL (WID ("advice-label1")));
	gtk_notebook_append_page (notebook, widget, NULL);

	g_signal_connect (G_OBJECT (WID ("linkbutton")), "activate-link",
			  G_CALLBACK (link_activated), self);

	priv->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	priv->pages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	priv->manager = gdk_display_get_device_manager (gdk_display_get_default ());
	priv->device_added_id = g_signal_connect (G_OBJECT (priv->manager), "device-added",
						  G_CALLBACK (device_added_cb), self);
	priv->device_removed_id = g_signal_connect (G_OBJECT (priv->manager), "device-removed",
						    G_CALLBACK (device_removed_cb), self);

	devices = gdk_device_manager_list_devices (priv->manager, GDK_DEVICE_TYPE_SLAVE);
	for (l = devices; l ; l = l->next)
		add_known_device (self, l->data);
	g_list_free (devices);

	update_current_page (self);
}

GDBusProxy *
cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self)
{
	g_return_val_if_fail (CC_IS_WACOM_PANEL (self), NULL);

	return self->priv->proxy;
}
