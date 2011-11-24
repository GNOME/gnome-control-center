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

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

G_DEFINE_DYNAMIC_TYPE (CcWacomPanel, cc_wacom_panel, CC_TYPE_PANEL)

#define WACOM_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PANEL, CcWacomPanelPrivate))

struct _CcWacomPanelPrivate
{
	GtkBuilder  *builder;
	GtkWidget   *notebook;
	/* The UI doesn't support cursor/pad at the moment */
	GdkDeviceManager *manager;
	guint             device_added_id;
	guint             device_removed_id;
};

enum {
	PLUG_IN_PAGE,
	WACOM_PAGE
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

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
}

static void
cc_wacom_panel_finalize (GObject *object)
{
	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->finalize (object);
}

static void
cc_wacom_panel_class_init (CcWacomPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomPanelPrivate));

	object_class->get_property = cc_wacom_panel_get_property;
	object_class->set_property = cc_wacom_panel_set_property;
	object_class->dispose = cc_wacom_panel_dispose;
	object_class->finalize = cc_wacom_panel_finalize;
}

static void
cc_wacom_panel_class_finalize (CcWacomPanelClass *klass)
{
}

static gboolean
has_wacom_tablet (CcWacomPanel *self)
{
	GList *list, *l;
	gboolean retval;

	retval = FALSE;
	list = gdk_device_manager_list_devices (self->priv->manager,
						GDK_DEVICE_TYPE_SLAVE);
	for (l = list; l != NULL; l = l->next)
	{
		GdkDevice *device = l->data;
		GdkInputSource source;

		source = gdk_device_get_source (device);
		if (source == GDK_SOURCE_PEN ||
		    source == GDK_SOURCE_ERASER)
		{
			retval = TRUE;
			break;
		}
	}
	g_list_free (list);
	return retval;
}

static void
update_current_page (CcWacomPanel *self)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook),
				       has_wacom_tablet (self) ? WACOM_PAGE : PLUG_IN_PAGE);
}

static void
device_changed_cb (GdkDeviceManager *manager,
		   GdkDevice        *device,
		   CcWacomPanel     *self)
{
	update_current_page (self);
}

static gboolean
link_activated (GtkLinkButton *button,
		CcWacomPanel  *self)
{
	CcShell *shell;
	GError *error = NULL;

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (cc_shell_set_active_panel_from_id (shell, "bluetooth", NULL, &error) == FALSE)
	{
		g_warning ("Failed to activate Bluetooth panel: %s", error->message);
		g_error_free (error);
	}
	return TRUE;
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
	GtkWidget *widget, *wacom_page;
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

	notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	priv->notebook = GTK_WIDGET (notebook);
	gtk_notebook_set_show_tabs (notebook, FALSE);
	gtk_widget_show (priv->notebook);

	widget = WID ("main-box");
	gtk_notebook_append_page (notebook, widget, NULL);

	wacom_page = cc_wacom_page_new ();
	gtk_widget_show (wacom_page);
	gtk_notebook_append_page (notebook, wacom_page, NULL);

	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (notebook));
	gtk_widget_set_vexpand (GTK_WIDGET (notebook), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 24);

	enbiggen_label (GTK_LABEL (WID ("advice-label1")));
	priv->manager = gdk_display_get_device_manager (gdk_display_get_default ());
	priv->device_added_id = g_signal_connect (G_OBJECT (priv->manager), "device-added",
						  G_CALLBACK (device_changed_cb), self);
	priv->device_removed_id = g_signal_connect (G_OBJECT (priv->manager), "device-removed",
						    G_CALLBACK (device_changed_cb), self);
	g_signal_connect (G_OBJECT (WID ("linkbutton")), "activate-link",
			  G_CALLBACK (link_activated), self);
	update_current_page (self);
}

void
cc_wacom_panel_register (GIOModule *module)
{
	cc_wacom_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_WACOM_PANEL, "wacom", 0);
}

