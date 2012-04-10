/*
 * Copyright © 2012 Wacom.
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
 * Authors: Jason Gerecke <killertofu@gmail.com>
 *
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>

#include <string.h>

#include "gsd-wacom-device.h"
#include "cc-wacom-mapping-panel.h"

G_DEFINE_TYPE (CcWacomMappingPanel, cc_wacom_mapping_panel, GTK_TYPE_BOX)

#define WACOM_MAPPING_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_MAPPING_PANEL, CcWacomMappingPanelPrivate))

struct _CcWacomMappingPanelPrivate
{
	GsdWacomDevice *device;
	GtkWidget      *label;
	GtkWidget      *combobox;
	GtkWidget      *checkbutton;
};

enum {
	MONITOR_NAME_COLUMN,
	MONITOR_NUM_COLUMN,
	MONITOR_NUM_COLUMNS
};

static void combobox_changed_cb (GtkWidget *widget, CcWacomMappingPanel *self);
static void checkbutton_toggled_cb (GtkWidget *widget, CcWacomMappingPanel *self);

static GnomeRROutputInfo**
get_rr_outputs (void)
{
	GError *error = NULL;
	GnomeRRScreen *rr_screen;
	GnomeRRConfig *rr_config;

	/* TODO: Check the value of 'error' */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
	rr_config = gnome_rr_config_new_current (rr_screen, &error);
	return gnome_rr_config_get_outputs (rr_config);
}

static void
set_combobox_sensitive (CcWacomMappingPanel *self,
			gboolean             sensitive)
{
	gtk_widget_set_sensitive (GTK_WIDGET(self->priv->combobox), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET(self->priv->label), sensitive);
}

/* Update the display of available monitors based on the latest
 * information from RandR. At the moment the chooser is just a
 * a combobox crudely listing available outputs. The UI mockup
 * has something more akin to the Display panel, with the ability
 * to do rubber-band selection of multiple outputs (note: the
 * g-s-d backend can only handle a single output at the moment)
 */
static void
update_monitor_chooser (CcWacomMappingPanel *self)
{
	GtkListStore *store;
	GnomeRROutputInfo **outputs;
	GdkRectangle geom;
	gint monitor;
	gboolean single_mon;
	guint i;

	store = gtk_list_store_new (MONITOR_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
	gtk_combo_box_set_model (GTK_COMBO_BOX(self->priv->combobox), GTK_TREE_MODEL(store));

	if (self->priv->device == NULL) {
		set_combobox_sensitive (self, FALSE);
		g_object_unref (store);
		return;
	}

	monitor = gsd_wacom_device_get_display_monitor (self->priv->device);
	single_mon = (monitor != -1);

	g_signal_handlers_block_by_func (G_OBJECT (self->priv->checkbutton), checkbutton_toggled_cb, self);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(self->priv->checkbutton), single_mon);
	g_signal_handlers_unblock_by_func (G_OBJECT (self->priv->checkbutton), checkbutton_toggled_cb, self);

	/* FIXME: does this break screen tablets? What's the default
	 * for unconfigured tablets? */
	if (monitor < 0)
		monitor = 0;
	gdk_screen_get_monitor_geometry (gdk_screen_get_default (), monitor, &geom);

	outputs = get_rr_outputs ();
	if (outputs == NULL)
		goto bail;

	for (i = 0; outputs[i] != NULL; i++) {
		GnomeRROutputInfo *output = outputs[i];

		if (gnome_rr_output_info_is_active (output)) {
			GtkTreeIter iter;
			gchar *name, *disp_name, *text;
			int x, y, w, h;
			int mon_at_point;

			name = gnome_rr_output_info_get_name (output);
			disp_name = gnome_rr_output_info_get_display_name (output);
			text = g_strdup_printf ("%s (%s)", name, disp_name);

			gnome_rr_output_info_get_geometry (output, &x, &y, &w, &h);
			mon_at_point = gdk_screen_get_monitor_at_point (gdk_screen_get_default (), x, y);
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, MONITOR_NAME_COLUMN, text, MONITOR_NUM_COLUMN, mon_at_point, -1);

			if (x == geom.x && y == geom.y && w == geom.width && h == geom.height) {
				g_signal_handlers_block_by_func (G_OBJECT (self->priv->combobox), combobox_changed_cb, self);
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX(self->priv->combobox), &iter);
				g_signal_handlers_unblock_by_func (G_OBJECT (self->priv->combobox), combobox_changed_cb, self);
			}

			g_free (text);
		}
	}

bail:
	set_combobox_sensitive (self, single_mon);
	g_object_unref (store);
}

static void
update_ui (CcWacomMappingPanel *self)
{
	if (self->priv->device == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET(self->priv->checkbutton), FALSE);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(self->priv->checkbutton), TRUE);
	} else {
		gboolean is_screen_tablet;

		is_screen_tablet = gsd_wacom_device_is_screen_tablet (self->priv->device);
		gtk_widget_set_sensitive (GTK_WIDGET(self->priv->checkbutton), !is_screen_tablet);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(self->priv->checkbutton), FALSE);
	}

	update_monitor_chooser (self);
}

static void
update_mapping (CcWacomMappingPanel *self)
{
	int monitor = -1;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->checkbutton))) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		char *name;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->priv->combobox));
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->priv->combobox), &iter)) {
			g_warning ("Map to single monitor checked, but no screen selected.");
			return;
		}

		gtk_tree_model_get (model, &iter, MONITOR_NAME_COLUMN, &name, MONITOR_NUM_COLUMN, &monitor, -1);
	}

	gsd_wacom_device_set_display (self->priv->device, monitor);
}

void
cc_wacom_mapping_panel_set_device (CcWacomMappingPanel *self,
                                   GsdWacomDevice *device)
{
	self->priv->device = device;
	update_ui (self);
}

static void
checkbutton_toggled_cb (GtkWidget           *widget,
			CcWacomMappingPanel *self)
{
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	set_combobox_sensitive (self, active);
	update_mapping (self);
}

static void
combobox_changed_cb (GtkWidget           *widget,
                     CcWacomMappingPanel *self)
{
	update_mapping (self);
}

static void
cc_wacom_mapping_panel_init (CcWacomMappingPanel *self)
{
	CcWacomMappingPanelPrivate *priv;
	GtkWidget *vbox, *hbox;
	GtkCellRenderer *renderer;

	priv = self->priv = WACOM_MAPPING_PANEL_PRIVATE (self);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add (GTK_CONTAINER (self), vbox);
	gtk_widget_set_vexpand (GTK_WIDGET (vbox), TRUE);
	gtk_widget_set_hexpand (GTK_WIDGET (vbox), TRUE);

	/* Output Combobox */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	priv->label = gtk_label_new (_("Output:"));
	priv->combobox = gtk_combo_box_new ();
	g_signal_connect (G_OBJECT (priv->combobox), "changed",
	                      G_CALLBACK (combobox_changed_cb), self);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(priv->combobox), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT(priv->combobox), renderer, "text", 0);
	gtk_box_pack_start (GTK_BOX(hbox), GTK_WIDGET(priv->label),
				FALSE, FALSE, 8);
	gtk_box_pack_start (GTK_BOX(hbox), GTK_WIDGET(priv->combobox),
				FALSE, FALSE, 0);

	/* Whole-desktop checkbox */
	priv->checkbutton = gtk_check_button_new_with_label (_("Map to single monitor"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton), FALSE);
	g_signal_connect (G_OBJECT (priv->checkbutton), "toggled",
                      G_CALLBACK (checkbutton_toggled_cb), self);

	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(priv->checkbutton),
				FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(hbox),
				FALSE, FALSE, 8);

	/* Update display */
	cc_wacom_mapping_panel_set_device (self, NULL);
	gtk_widget_show_all(GTK_WIDGET(self));
}

GtkWidget *
cc_wacom_mapping_panel_new (void)
{
	CcWacomMappingPanel *panel;

	panel = CC_WACOM_MAPPING_PANEL(g_object_new (CC_TYPE_WACOM_MAPPING_PANEL, NULL));
	panel->priv->device = NULL;

	return GTK_WIDGET(panel);
}

static void
cc_wacom_mapping_panel_get_property (GObject    *object,
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
cc_wacom_mapping_panel_set_property (GObject      *object,
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
cc_wacom_mapping_panel_dispose (GObject *object)
{
	G_OBJECT_CLASS (cc_wacom_mapping_panel_parent_class)->dispose (object);
}

static void
cc_wacom_mapping_panel_class_init (CcWacomMappingPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomMappingPanelPrivate));

	object_class->get_property = cc_wacom_mapping_panel_get_property;
	object_class->set_property = cc_wacom_mapping_panel_set_property;
	object_class->dispose = cc_wacom_mapping_panel_dispose;
}
