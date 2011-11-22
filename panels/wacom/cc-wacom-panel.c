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
 *
 */

#include <config.h>

#include "cc-wacom-panel.h"
#include <gtk/gtk.h>

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

G_DEFINE_DYNAMIC_TYPE (CcWacomPanel, cc_wacom_panel, CC_TYPE_PANEL)

#define WACOM_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PANEL, CcWacomPanelPrivate))

#define WACOM_SCHEMA "org.gnome.settings-daemon.peripherals.wacom"
#define WACOM_STYLUS_SCHEMA WACOM_SCHEMA ".stylus"
#define WACOM_ERASER_SCHEMA WACOM_SCHEMA ".eraser"

struct _CcWacomPanelPrivate
{
	GtkBuilder  *builder;
	GtkWidget   *notebook;
	GSettings   *wacom_settings;
	GSettings   *stylus_settings;
	GSettings   *eraser_settings;
	/* The UI doesn't support cursor/pad at the moment */
	GdkDeviceManager *manager;
	guint             device_added_id;
	guint             device_removed_id;
};

enum {
	PLUG_IN_PAGE,
	WACOM_PAGE
};

/* Button combo box storage columns */
enum {
	BUTTONNUMBER_COLUMN,
	BUTTONNAME_COLUMN,
	N_BUTTONCOLUMNS
};

/* Tablet mode combo box storage columns */
enum {
	MODENUMBER_COLUMN,
	MODELABEL_COLUMN,
	N_MODECOLUMNS
};

/* Tablet mode options - keep in sync with .ui */
enum {
	MODE_ABSOLUTE, /* stylus + eraser absolute */
	MODE_RELATIVE, /* stylus + eraser relative */
};

/* GSettings stores pressurecurve as 4 values like the driver. We map slider
 * scale to these values given the array below. These settings were taken from
 * wacomcpl, where they've been around for years.
 */
#define N_PRESSURE_CURVES 7
static const gint32 PRESSURE_CURVES[N_PRESSURE_CURVES][4] = {
		{	0,	75,	25,	100	},	/* soft */
		{	0,	50,	50,	100	},
		{	0,	25,	75,	100	},
		{	0,	0,	100,	100	},	/* neutral */
		{	25,	0,	100,	75	},
		{	50,	0,	100,	50	},
		{	75,	0,	100,	25	}	/* firm */
};

static void
set_pressurecurve (GtkRange *range, GSettings *settings)
{
	gint		slider_val = gtk_range_get_value (range);
	GVariant	*values[4],
			*array;
	int		i;

	for (i = 0; i < 4; i++)
		values[i] = g_variant_new_int32 (PRESSURE_CURVES[slider_val][i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, values, 4);

	g_settings_set_value (settings, "pressurecurve", array);

	g_variant_unref (array);
}

static void
tip_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
    set_pressurecurve (range, CC_WACOM_PANEL(user_data)->priv->stylus_settings);
}

static void
eraser_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
    set_pressurecurve (range, CC_WACOM_PANEL(user_data)->priv->eraser_settings);
}

static void
set_feel_from_gsettings (GtkAdjustment *adjustment, GSettings *settings)
{
	GVariant	*variant;
	const gint32	*values;
	gsize		nvalues;
	int		i;

	variant = g_settings_get_value (settings, "pressurecurve");
	values = g_variant_get_fixed_array (variant, &nvalues, sizeof (gint32));

	if (nvalues != 4) {
		g_warning ("Invalid pressure curve format, expected 4 values (got %ld)", nvalues);
		return;
	}

	for (i = 0; i < N_PRESSURE_CURVES; i++) {
		if (memcmp (PRESSURE_CURVES[i], values, sizeof (gint32) * 4) == 0) {
			gtk_adjustment_set_value (adjustment, i);
			break;
		}
	}
}

static void
tabletmode_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPanelPrivate	*priv	= CC_WACOM_PANEL(user_data)->priv;
	GtkListStore		*liststore;
	GtkTreeIter		iter;
	gint			mode;
	gboolean		is_absolute;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-tabletmode"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    MODENUMBER_COLUMN, &mode,
			    -1);

	is_absolute = (mode == MODE_ABSOLUTE);
	g_settings_set_boolean (priv->wacom_settings, "is-absolute", is_absolute);
}

static void
left_handed_toggled_cb (GtkSwitch *sw, GParamSpec *pspec, gpointer *user_data)
{
	CcWacomPanelPrivate	*priv = CC_WACOM_PANEL(user_data)->priv;
	const gchar*		rotation;

	rotation = gtk_switch_get_active (sw) ? "half" : "none";

	g_settings_set_string (priv->wacom_settings, "rotation", rotation);
}

static void
set_left_handed_from_gsettings (CcWacomPanel *panel)
{
	CcWacomPanelPrivate	*priv = CC_WACOM_PANEL(panel)->priv;
	const gchar*		rotation;

	rotation = g_settings_get_string (priv->wacom_settings, "rotation");
	if (strcmp (rotation, "half") == 0)
		gtk_switch_set_active (GTK_SWITCH (WID ("switch-left-handed")), TRUE);
}

static void
set_mode_from_gsettings (GtkComboBox *combo, CcWacomPanel *panel)
{
	CcWacomPanelPrivate	*priv = CC_WACOM_PANEL(panel)->priv;
	gboolean		is_absolute;

	is_absolute = g_settings_get_boolean (priv->wacom_settings, "is-absolute");

	/* this must be kept in sync with the .ui file */
	gtk_combo_box_set_active (combo, is_absolute ? MODE_ABSOLUTE : MODE_RELATIVE);
}

static void
set_button_mapping_from_gsettings (GtkComboBox *combo, GSettings* settings, gint current_button)
{
	GVariant	*current;
	gsize		 nvalues;
	const gint	*values;
	GtkTreeModel	*model;
	GtkTreeIter	 iter;
	gboolean	 valid;

	current = g_settings_get_value (settings, "buttonmapping");
	values = g_variant_get_fixed_array (current, &nvalues, sizeof (gint32));
	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gint button;

		gtk_tree_model_get (model, &iter,
				    BUTTONNUMBER_COLUMN, &button,
				    -1);

		/* Currently button values match logical X buttons. If we
		 * introduce things like double-click, this code must
		 * change. Recommendation: use negative buttons numbers for
		 * special ones.
		 */

		/* 0 vs 1-indexed array/button numbers */
		if (button == values[current_button - 1]) {
			gtk_combo_box_set_active_iter (combo, &iter);
			break;
		}

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
map_button (GSettings *settings, int button2, int button3)
{
	GVariant	*current; /* current mapping */
	GVariant	*array;   /* new mapping */
	GVariant	**tmp;
	gsize		 nvalues;
	const gint	*values;
	gint		 i;

	current = g_settings_get_value (settings, "buttonmapping");
	values = g_variant_get_fixed_array (current, &nvalues, sizeof (gint32));

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < nvalues; i++) {
		if (i == 1) /* zero indexed array vs one-indexed buttons */
			tmp[i] = g_variant_new_int32 (button2);
		else if (i == 2)
			tmp[i] = g_variant_new_int32 (button3);
		else
			tmp[i] = g_variant_new_int32 (values[i]);
	}

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, tmp, nvalues);
	g_settings_set_value (settings, "buttonmapping", array);

	g_free (tmp);
	g_variant_unref (array);
}

static void
button_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPanelPrivate	*priv = CC_WACOM_PANEL(user_data)->priv;
	GtkTreeIter		iter;
	GtkListStore		*liststore;
	gint			mapping_b2,
				mapping_b3;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("combo-bottombutton")), &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-buttons"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    BUTTONNUMBER_COLUMN, &mapping_b2,
			    -1);

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("combo-topbutton")), &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    BUTTONNUMBER_COLUMN, &mapping_b3,
			    -1);

	map_button (priv->stylus_settings, mapping_b2, mapping_b3);
}

static void
combobox_text_cellrenderer (GtkComboBox *combo, int name_column)
{
	GtkCellRenderer	*renderer;

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", BUTTONNAME_COLUMN, NULL);
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

	if (priv->wacom_settings)
	{
		g_object_unref (priv->wacom_settings);
		priv->wacom_settings = NULL;
	}

	if (priv->stylus_settings)
	{
		g_object_unref (priv->stylus_settings);
		priv->stylus_settings = NULL;
	}

	if (priv->eraser_settings)
	{
		g_object_unref (priv->eraser_settings);
		priv->eraser_settings = NULL;
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
	GtkWidget *notebook;
	GError *error = NULL;
	GtkComboBox *combo;
	GtkSwitch *sw;
	char *objects[] = {
		"main-notebook",
		"liststore-tabletmode",
		"liststore-buttons",
		"adjustment-tip-feel",
		"adjustment-eraser-feel",
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

	notebook = WID ("main-notebook");
	gtk_container_add (GTK_CONTAINER (self), notebook);
	gtk_widget_set_vexpand (GTK_WIDGET (notebook), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 24);
	priv->notebook = notebook;

	priv->wacom_settings  = g_settings_new (WACOM_SCHEMA);
	priv->stylus_settings = g_settings_new (WACOM_STYLUS_SCHEMA);
	priv->eraser_settings = g_settings_new (WACOM_ERASER_SCHEMA);

	g_signal_connect (WID ("scale-tip-feel"), "value-changed",
			  G_CALLBACK (tip_feel_value_changed_cb), self);
	g_signal_connect (WID ("scale-eraser-feel"), "value-changed",
			  G_CALLBACK (eraser_feel_value_changed_cb), self);

	combo = GTK_COMBO_BOX (WID ("combo-topbutton"));
	combobox_text_cellrenderer (combo, BUTTONNAME_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (button_changed_cb), self);

	combo = GTK_COMBO_BOX (WID ("combo-bottombutton"));
	combobox_text_cellrenderer (combo, BUTTONNAME_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (button_changed_cb), self);

	combo = GTK_COMBO_BOX (WID ("combo-tabletmode"));
	combobox_text_cellrenderer (combo, MODELABEL_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (tabletmode_changed_cb), self);

	sw = GTK_SWITCH (WID ("switch-left-handed"));
	g_signal_connect (G_OBJECT (sw), "notify::active",
			  G_CALLBACK (left_handed_toggled_cb), self);

	set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-topbutton")), priv->stylus_settings, 3);
	set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-bottombutton")), priv->stylus_settings, 2);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), self);
	set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-tip-feel")), priv->stylus_settings);
	set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-eraser-feel")), priv->eraser_settings);
	set_left_handed_from_gsettings (self);

	gtk_image_set_from_file (GTK_IMAGE (WID ("image-tablet")), PIXMAP_DIR "/wacom-tablet.svg");
	gtk_image_set_from_file (GTK_IMAGE (WID ("image-stylus")), PIXMAP_DIR "/wacom-stylus.svg");

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

