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

#include "cc-wacom-page.h"
#include "cc-wacom-nav-button.h"
#include "gui_gtk.h"
#include <gtk/gtk.h>

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

#define WACOM_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PAGE, CcWacomPagePrivate))

#define WACOM_SCHEMA "org.gnome.settings-daemon.peripherals.wacom"
#define WACOM_STYLUS_SCHEMA WACOM_SCHEMA ".stylus"
#define WACOM_ERASER_SCHEMA WACOM_SCHEMA ".eraser"

struct _CcWacomPagePrivate
{
	GsdWacomDevice *stylus, *eraser;
	GtkBuilder     *builder;
	GtkWidget      *nav;
	GSettings      *wacom_settings;
	GSettings      *stylus_settings;
	GSettings      *eraser_settings;
	/* The UI doesn't support cursor/pad at the moment */
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

	for (i = 0; i < G_N_ELEMENTS (values); i++)
		values[i] = g_variant_new_int32 (PRESSURE_CURVES[slider_val][i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, values, G_N_ELEMENTS (values));

	g_settings_set_value (settings, "pressurecurve", array);
}

static void
tip_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
    set_pressurecurve (range, CC_WACOM_PAGE(user_data)->priv->stylus_settings);
}

static void
eraser_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
    set_pressurecurve (range, CC_WACOM_PAGE(user_data)->priv->eraser_settings);
}

static void
get_calibration (gint      ** cal,
                 gsize      * ncal,
                 GSettings  * settings)
{
	GVariant *current = g_settings_get_value (settings, "area");
	*cal = g_variant_get_fixed_array (current, ncal, sizeof (gint32));
}

static void
set_calibration (gint      *cal,
                 gsize      ncal,
                 GSettings *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	GVariant   **tmp;
	gsize        nvalues;
	const gint  *values;
	int          i;

	current = g_settings_get_value (settings, "area");
	values = g_variant_get_fixed_array (current, &nvalues, sizeof (gint32));
	if ((ncal != 4) || (nvalues != 4))
	{
		g_warning("Unable set set device calibration property. Got %d items to put in %d slots; expected %d items.\n", ncal, nvalues, 4);
		return;
	}

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_int32 (cal[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, tmp, nvalues);
	g_settings_set_value (settings, "area", array);

	g_free (tmp);
	g_variant_unref (array);
}

static gboolean
run_calibration (gint  *cal,
                 gsize  ncal)
{
	gboolean success = FALSE;
	XYinfo axis;
	gboolean swap_xy;
	struct Calib calibrator;

	if (ncal != 4)
	{
		g_warning("Unable to run calibration. Got %d items; expected %d.\n", ncal, 4);
		goto quit_calibration;
	}

	calibrator.threshold_misclick = 15;
	calibrator.threshold_doubleclick = 7;
	calibrator.geometry = NULL;
	calibrator.old_axis.x_min = cal[0];
	calibrator.old_axis.y_min = cal[1];
	calibrator.old_axis.x_max = cal[2];
	calibrator.old_axis.y_max = cal[3];

	/* !!NOTE!! This call blocks on the calibration
         * !!NOTE!! process. It will be several seconds
         * !!NOTE!! before this returns.
         */
	if(run_gui(&calibrator, &axis, &swap_xy))
		success = TRUE;

quit_calibration:
	if (success)
	{
		cal[0] = axis.x_min;
		cal[1] = axis.y_min;
		cal[2] = axis.x_max;
		cal[3] = axis.y_max;
	}

	return success;
}

static void
calibrate_button_clicked_cb (GtkButton *button, gpointer user_data)
{
    GSettings *tablet = CC_WACOM_PAGE(user_data)->priv->wacom_settings;
    gint i, calibration[4];
    gint *current;
    gsize s;

    get_calibration (&current, &s, tablet);
    if (s != 4)
    {
        g_warning("Device calibration property has wrong length. Got %d items; expected %d.\n", s, 4);
        return;
    }

    for (i = 0; i < 4; i++)
        calibration[i] = current[i];

    if (calibration[0] == -1 && calibration[1] == -1 && calibration[2] == -1 && calibration[3] == -1)
        gsd_wacom_device_get_area(CC_WACOM_PAGE(user_data)->priv->stylus, &calibration);

    if (run_calibration(calibration, 4))
        set_calibration(calibration, 4, tablet);
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
	CcWacomPagePrivate	*priv	= CC_WACOM_PAGE(user_data)->priv;
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
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(user_data)->priv;
	const gchar*		rotation;

	rotation = gtk_switch_get_active (sw) ? "half" : "none";

	g_settings_set_string (priv->wacom_settings, "rotation", rotation);
}

static void
set_left_handed_from_gsettings (CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(page)->priv;
	const gchar*		rotation;

	rotation = g_settings_get_string (priv->wacom_settings, "rotation");
	if (strcmp (rotation, "half") == 0)
		gtk_switch_set_active (GTK_SWITCH (WID ("switch-left-handed")), TRUE);
}

static void
set_mode_from_gsettings (GtkComboBox *combo, CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = page->priv;
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
}

static void
button_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(user_data)->priv;
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
cc_wacom_page_get_property (GObject    *object,
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
cc_wacom_page_set_property (GObject      *object,
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
cc_wacom_page_dispose (GObject *object)
{
	CcWacomPagePrivate *priv = CC_WACOM_PAGE (object)->priv;

	if (priv->builder)
	{
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	G_OBJECT_CLASS (cc_wacom_page_parent_class)->dispose (object);
}

static void
cc_wacom_page_class_init (CcWacomPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomPagePrivate));

	object_class->get_property = cc_wacom_page_get_property;
	object_class->set_property = cc_wacom_page_set_property;
	object_class->dispose = cc_wacom_page_dispose;
}

static void
cc_wacom_page_init (CcWacomPage *self)
{
	CcWacomPagePrivate *priv;
	GError *error = NULL;
	GtkComboBox *combo;
	GtkWidget *box;
	GtkSwitch *sw;
	char *objects[] = {
		"main-grid",
		"liststore-tabletmode",
		"liststore-buttons",
		"adjustment-tip-feel",
		"adjustment-eraser-feel",
		NULL
	};

	priv = self->priv = WACOM_PAGE_PRIVATE (self);

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_file (priv->builder,
					   GNOMECC_UI_DIR "/gnome-wacom-properties.ui",
					   objects,
					   &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	box = WID ("main-grid");
	gtk_container_add (GTK_CONTAINER (self), box);
	gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);

	g_signal_connect (WID ("scale-tip-feel"), "value-changed",
			  G_CALLBACK (tip_feel_value_changed_cb), self);
	g_signal_connect (WID ("scale-eraser-feel"), "value-changed",
			  G_CALLBACK (eraser_feel_value_changed_cb), self);
	g_signal_connect (WID ("button-calibrate"), "clicked",
			  G_CALLBACK (calibrate_button_clicked_cb), self);


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

	priv->nav = cc_wacom_nav_button_new ();
	gtk_grid_attach (GTK_GRID (box), priv->nav, 0, 0, 1, 1);
}

static GSettings *
get_first_stylus_setting (GsdWacomDevice *device)
{
	GList *styli;
	GsdWacomStylus *stylus;

	styli = gsd_wacom_device_list_styli (device);
	stylus = styli->data;
	g_list_free (styli);

	return gsd_wacom_stylus_get_settings (stylus);
}

static void
set_icon_name (CcWacomPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	CcWacomPagePrivate *priv;
	char *filename, *path;

	priv = page->priv;

	filename = g_strdup_printf ("%s.svg", icon_name);
	path = g_build_filename (PIXMAP_DIR, filename, NULL);
	g_free (filename);

	gtk_image_set_from_file (GTK_IMAGE (WID (widget_name)), path);
	g_free (path);
}

static void
set_first_stylus_icon (CcWacomPage *page)
{
	GList *styli;
	GsdWacomStylus *stylus;

	styli = gsd_wacom_device_list_styli (page->priv->stylus);
	stylus = styli->data;
	g_list_free (styli);

	set_icon_name (page, "image-stylus", gsd_wacom_stylus_get_icon_name (stylus));
}

GtkWidget *
cc_wacom_page_new (GsdWacomDevice *stylus,
		   GsdWacomDevice *eraser)
{
	CcWacomPage *page;
	CcWacomPagePrivate *priv;

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (stylus), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (stylus) == WACOM_TYPE_STYLUS, NULL);

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (eraser), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (eraser) == WACOM_TYPE_ERASER, NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	priv = page->priv;
	priv->stylus = stylus;
	priv->eraser = eraser;

	/* FIXME move this to construct */
	priv->wacom_settings  = gsd_wacom_device_get_settings (stylus);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), page);

	/* Tablet name */
	gtk_label_set_text (GTK_LABEL (WID ("label-tabletmodel")), gsd_wacom_device_get_name (stylus));

	/* Left-handedness */
	if (gsd_wacom_device_reversible (stylus) == FALSE) {
		gtk_widget_hide (WID ("label-left-handed"));
		gtk_widget_hide (WID ("switch-left-handed"));
	} else {
		set_left_handed_from_gsettings (page);
	}

	/* Calibration for screen tablets */
	if (gsd_wacom_device_is_screen_tablet (stylus) == TRUE) {
		gtk_widget_show (WID ("button-calibrate"));
	}

	/* Tablet icon */
	set_icon_name (page, "image-tablet", gsd_wacom_device_get_icon_name (stylus));

	/* Stylus/Eraser */
	priv->stylus_settings = get_first_stylus_setting (stylus);
	priv->eraser_settings = get_first_stylus_setting (eraser);

	set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-topbutton")), priv->stylus_settings, 3);
	set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-bottombutton")), priv->stylus_settings, 2);
	set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-tip-feel")), priv->stylus_settings);
	set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-eraser-feel")), priv->eraser_settings);
	set_first_stylus_icon (page);

	return GTK_WIDGET (page);
}

void
cc_wacom_page_set_navigation (CcWacomPage *page,
			      GtkNotebook *notebook,
			      gboolean     ignore_first_page)
{
	CcWacomPagePrivate *priv;

	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	priv = page->priv;

	g_object_set (G_OBJECT (priv->nav),
		      "notebook", notebook,
		      "ignore-first", ignore_first_page,
		      NULL);
}
