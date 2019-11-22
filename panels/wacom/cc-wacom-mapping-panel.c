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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include "cc-wacom-device.h"
#include "cc-wacom-mapping-panel.h"

struct _CcWacomMappingPanel
{
	GtkBox          parent_instance;

	CcWacomDevice  *device;
	GtkWidget      *label;
	GtkWidget      *combobox;
	GtkWidget      *checkbutton;
	GtkWidget      *aspectlabel;
	GtkWidget      *aspectswitch;

	GnomeRRScreen  *rr_screen;
};

G_DEFINE_TYPE (CcWacomMappingPanel, cc_wacom_mapping_panel, GTK_TYPE_BOX)

enum {
	MONITOR_NAME_COLUMN,
	MONITOR_PTR_COLUMN,
	MONITOR_NUM_COLUMNS
};

static void combobox_changed_cb (CcWacomMappingPanel *self);
static void checkbutton_toggled_cb (CcWacomMappingPanel *self);
static void aspectswitch_toggled_cb (CcWacomMappingPanel *self);

static void
set_combobox_sensitive (CcWacomMappingPanel *self,
			gboolean             sensitive)
{
	gtk_widget_set_sensitive (GTK_WIDGET(self->combobox), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET(self->label), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET(self->aspectswitch), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET(self->aspectlabel), sensitive);
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
	g_autoptr(GtkListStore) store = NULL;
	GnomeRROutput **outputs;
	GSettings *settings;
	GnomeRROutput *cur_output;
	guint i;

	store = gtk_list_store_new (MONITOR_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_combo_box_set_model (GTK_COMBO_BOX(self->combobox), GTK_TREE_MODEL(store));

	if (self->device == NULL) {
		set_combobox_sensitive (self, FALSE);
		return;
	}

	settings = cc_wacom_device_get_settings (self->device);
	cur_output = cc_wacom_device_get_output (self->device,
						 self->rr_screen);

	g_signal_handlers_block_by_func (G_OBJECT (self->checkbutton), checkbutton_toggled_cb, self);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(self->checkbutton), cur_output != NULL);
	g_signal_handlers_unblock_by_func (G_OBJECT (self->checkbutton), checkbutton_toggled_cb, self);

	g_signal_handlers_block_by_func (G_OBJECT (self->aspectswitch), aspectswitch_toggled_cb, self);
	gtk_switch_set_active (GTK_SWITCH(self->aspectswitch), g_settings_get_boolean (settings, "keep-aspect"));
	g_signal_handlers_unblock_by_func (G_OBJECT (self->aspectswitch), aspectswitch_toggled_cb, self);

	if (!self->rr_screen) {
		set_combobox_sensitive (self, FALSE);
		return;
	}

	outputs = gnome_rr_screen_list_outputs (self->rr_screen);

	for (i = 0; outputs[i] != NULL; i++) {
		GnomeRROutput *output = outputs[i];
		GnomeRRCrtc *crtc = gnome_rr_output_get_crtc (output);

		/* Output is turned on? */
		if (crtc && gnome_rr_crtc_get_current_mode (crtc) != NULL) {
			GtkTreeIter iter;
			const gchar *name, *disp_name;
			g_autofree gchar *text = NULL;

			name = gnome_rr_output_get_name (output);
			disp_name = gnome_rr_output_get_display_name (output);
			text = g_strdup_printf ("%s (%s)", name, disp_name);

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, MONITOR_NAME_COLUMN, text, MONITOR_PTR_COLUMN, output, -1);

			if (i == 0 || output == cur_output) {
				g_signal_handlers_block_by_func (G_OBJECT (self->combobox), combobox_changed_cb, self);
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX(self->combobox), &iter);
				g_signal_handlers_unblock_by_func (G_OBJECT (self->combobox), combobox_changed_cb, self);
			}
		}
	}

	set_combobox_sensitive (self, cur_output != NULL);
}

static void
update_ui (CcWacomMappingPanel *self)
{
	if (self->device == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET(self->checkbutton), FALSE);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(self->checkbutton), TRUE);
	} else {
		gboolean is_screen_tablet;

		is_screen_tablet =
			cc_wacom_device_get_integration_flags (self->device) &
			WACOM_DEVICE_INTEGRATED_DISPLAY;

		gtk_widget_set_sensitive (GTK_WIDGET(self->checkbutton), !is_screen_tablet);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(self->checkbutton), FALSE);
	}

	update_monitor_chooser (self);
}

static void
update_mapping (CcWacomMappingPanel *self)
{
	GnomeRROutput *output = NULL;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->checkbutton))) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		char *name;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combobox));
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combobox), &iter)) {
			g_warning ("Map to single monitor checked, but no screen selected.");
			return;
		}

		gtk_tree_model_get (model, &iter, MONITOR_NAME_COLUMN, &name, MONITOR_PTR_COLUMN, &output, -1);
	}

	cc_wacom_device_set_output (self->device, output);
}

void
cc_wacom_mapping_panel_set_device (CcWacomMappingPanel *self,
                                   CcWacomDevice       *device)
{
	self->device = device;
	update_ui (self);
}

static void
checkbutton_toggled_cb (CcWacomMappingPanel *self)
{
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->checkbutton));
	set_combobox_sensitive (self, active);
	if (!active)
		gtk_switch_set_active (GTK_SWITCH(self->aspectswitch), FALSE);
	update_mapping (self);
}

static void
aspectswitch_toggled_cb (CcWacomMappingPanel *self)
{
	GSettings *settings;

	settings = cc_wacom_device_get_settings (self->device);
	g_settings_set_boolean (settings,
				"keep-aspect",
				gtk_switch_get_active (GTK_SWITCH (self->aspectswitch)));
}

static void
combobox_changed_cb (CcWacomMappingPanel *self)
{
	update_mapping (self);
}

static void
cc_wacom_mapping_panel_init (CcWacomMappingPanel *self)
{
	GtkWidget *vbox, *grid;
	GtkCellRenderer *renderer;
	g_autoptr(GError) error = NULL;

	self->rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);

	if (error)
		g_warning ("Could not get RR screen: %s", error->message);

	g_signal_connect_object (self->rr_screen, "changed",
				 G_CALLBACK (update_monitor_chooser), self, G_CONNECT_SWAPPED);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add (GTK_CONTAINER (self), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (self), 12);
	gtk_widget_set_vexpand (GTK_WIDGET (vbox), TRUE);
	gtk_widget_set_hexpand (GTK_WIDGET (vbox), TRUE);

	/* Output Combobox */
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
	self->label = gtk_label_new (_("Output:"));
	gtk_widget_set_halign (self->label, GTK_ALIGN_END);
	self->combobox = gtk_combo_box_new ();
	g_signal_connect_object (self->combobox, "changed",
	                         G_CALLBACK (combobox_changed_cb), self, G_CONNECT_SWAPPED);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(self->combobox), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT(self->combobox), renderer, "text", 0);
	gtk_grid_attach (GTK_GRID(grid), GTK_WIDGET(self->label), 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID(grid), GTK_WIDGET(self->combobox), 1, 0, 1, 1);

	/* Keep ratio switch */
	self->aspectlabel = gtk_label_new (_("Keep aspect ratio (letterbox):"));
	gtk_widget_set_halign (self->aspectlabel, GTK_ALIGN_END);
	self->aspectswitch = gtk_switch_new ();
	gtk_widget_set_halign (self->aspectswitch, GTK_ALIGN_START);
	gtk_switch_set_active (GTK_SWITCH (self->aspectswitch), FALSE);
	g_signal_connect_object (self->aspectswitch, "notify::active",
                                 G_CALLBACK (aspectswitch_toggled_cb), self, G_CONNECT_SWAPPED);
	gtk_grid_attach (GTK_GRID(grid), GTK_WIDGET(self->aspectlabel), 0, 1, 1, 1);
	gtk_grid_attach (GTK_GRID(grid), GTK_WIDGET(self->aspectswitch), 1, 1, 1, 1);

	/* Whole-desktop checkbox */
	self->checkbutton = gtk_check_button_new_with_label (_("Map to single monitor"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->checkbutton), FALSE);
	g_signal_connect_object (self->checkbutton, "toggled",
                                 G_CALLBACK (checkbutton_toggled_cb), self, G_CONNECT_SWAPPED);

	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(self->checkbutton),
				FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(grid),
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
	panel->device = NULL;

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
	CcWacomMappingPanel *self = CC_WACOM_MAPPING_PANEL (object);

	g_clear_object (&self->rr_screen);

	G_OBJECT_CLASS (cc_wacom_mapping_panel_parent_class)->dispose (object);
}

static void
cc_wacom_mapping_panel_class_init (CcWacomMappingPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cc_wacom_mapping_panel_get_property;
	object_class->set_property = cc_wacom_mapping_panel_set_property;
	object_class->dispose = cc_wacom_mapping_panel_dispose;
}
