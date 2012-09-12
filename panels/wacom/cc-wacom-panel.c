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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include "gsd-wacom-device.h"

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

CC_PANEL_REGISTER (CcWacomPanel, cc_wacom_panel)

#define WACOM_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PANEL, CcWacomPanelPrivate))

struct _CcWacomPanelPrivate
{
	GtkBuilder       *builder;
	GtkWidget        *notebook;
	GHashTable       *devices; /* key=GdkDevice, value=GsdWacomDevice */
	GHashTable       *pages; /* key=device name, value=GtkWidget */
	GdkDeviceManager *manager;
	guint             device_added_id;
	guint             device_removed_id;
};

typedef struct {
	const char *name;
	GsdWacomDevice *stylus;
	GsdWacomDevice *eraser;
	GsdWacomDevice *pad;
} Tablet;

enum {
	WACOM_PAGE = -1,
	PLUG_IN_PAGE = 0,
};

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
	switch (property_id)
	{
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

	if (priv->pages)
	{
		g_hash_table_destroy (priv->pages);
		priv->pages = NULL;
	}

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
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

	panel_class->get_help_uri = cc_wacom_panel_get_help_uri;
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
		case WACOM_TYPE_ERASER:
			tablet->eraser = device;
			break;
		case WACOM_TYPE_PAD:
			tablet->pad = device;
			break;
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
		if (tablet->stylus == NULL ||
		    tablet->eraser == NULL) {
			page = g_hash_table_lookup (priv->pages, tablet->name);
			if (page != NULL) {
				remove_page (GTK_NOTEBOOK (priv->notebook), page);
				g_hash_table_remove (priv->pages, tablet->name);

				changed = TRUE;
			}
			continue;
		}
		/* this code is called once the stylus + eraser were set up, but the pad does not exist yet */
		page = g_hash_table_lookup (priv->pages, tablet->name);
		if (page == NULL) {
			page = cc_wacom_page_new (self, tablet->stylus, tablet->eraser, tablet->pad);
			cc_wacom_page_set_navigation (CC_WACOM_PAGE (page), GTK_NOTEBOOK (priv->notebook), TRUE);
			gtk_widget_show (page);
			gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), page, NULL);
			g_hash_table_insert (priv->pages, g_strdup (tablet->name), page);

			changed = TRUE;
		} else {
			cc_wacom_page_update_tools (CC_WACOM_PAGE (page), tablet->stylus, tablet->eraser, tablet->pad);
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

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_file (priv->builder,
					   GNOMECC_UI_DIR "/gnome-wacom-properties.ui",
					   objects,
					   &error);
	if (error != NULL)
	{
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	/* Notebook */
	notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	priv->notebook = GTK_WIDGET (notebook);

	gtk_notebook_set_show_tabs (notebook, FALSE);
	gtk_widget_set_vexpand (GTK_WIDGET (notebook), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 0);
	g_object_set (G_OBJECT (notebook),
		      "margin-top", 0,
		      "margin-right", 24,
		      "margin-left", 24,
		      "margin-bottom", 24,
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

void
cc_wacom_panel_register (GIOModule *module)
{
	cc_wacom_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_WACOM_PANEL, "wacom", 0);
}

