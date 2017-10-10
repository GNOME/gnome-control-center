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

#include <glib/gi18n.h>
#include "cc-wacom-stylus-page.h"
#include "cc-wacom-nav-button.h"
#include <gtk/gtk.h>
#include <gdesktop-enums.h>

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)
#define CWID(x) (GtkContainer *) gtk_builder_get_object (priv->builder, x)

G_DEFINE_TYPE (CcWacomStylusPage, cc_wacom_stylus_page, GTK_TYPE_BOX)

#define WACOM_STYLUS_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_STYLUS_PAGE, CcWacomStylusPagePrivate))

struct _CcWacomStylusPagePrivate
{
	CcWacomTool    *stylus;
	GtkBuilder     *builder;
	GtkWidget      *nav;
	GSettings      *stylus_settings;
};

/* Button combo box storage columns */
enum {
	BUTTONNUMBER_COLUMN,
	BUTTONNAME_COLUMN,
	N_BUTTONCOLUMNS
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
set_pressurecurve (GtkRange *range, GSettings *settings, const gchar *key)
{
	gint		slider_val = gtk_range_get_value (range);
	GVariant	*values[4],
			*array;
	int		i;

	for (i = 0; i < G_N_ELEMENTS (values); i++)
		values[i] = g_variant_new_int32 (PRESSURE_CURVES[slider_val][i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, values, G_N_ELEMENTS (values));

	g_settings_set_value (settings, key, array);
}

static void
tip_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
	set_pressurecurve (range, CC_WACOM_STYLUS_PAGE(user_data)->priv->stylus_settings, "pressure-curve");
}

static void
eraser_feel_value_changed_cb (GtkRange *range, gpointer user_data)
{
	set_pressurecurve (range, CC_WACOM_STYLUS_PAGE(user_data)->priv->stylus_settings, "eraser-pressure-curve");
}

static void
set_feel_from_gsettings (GtkAdjustment *adjustment, GSettings *settings, const gchar *key)
{
	GVariant	*variant;
	const gint32	*values;
	gsize		nvalues;
	int		i;

	variant = g_settings_get_value (settings, key);
	values = g_variant_get_fixed_array (variant, &nvalues, sizeof (gint32));

	if (nvalues != 4) {
		g_warning ("Invalid pressure curve format, expected 4 values (got %"G_GSIZE_FORMAT")", nvalues);
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
set_button_mapping_from_gsettings (GtkComboBox *combo, GSettings* settings, const gchar *key)
{
	GDesktopStylusButtonAction action;
	GtkTreeModel	*model;
	GtkTreeIter	 iter;
	gboolean	 valid;

	action = g_settings_get_enum (settings, key);
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

		if (button == action) {
			gtk_combo_box_set_active_iter (combo, &iter);
			break;
		}

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
button_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomStylusPagePrivate	*priv = CC_WACOM_STYLUS_PAGE(user_data)->priv;
	GtkTreeIter		iter;
	GtkListStore		*liststore;
	gint			mapping_b2,
				mapping_b3,
				mapping_b4;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("combo-bottombutton")), &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-buttons"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    BUTTONNUMBER_COLUMN, &mapping_b2,
			    -1);

	if (cc_wacom_tool_get_num_buttons (priv->stylus) > 1) {
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("combo-topbutton")), &iter))
			return;

		gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
				    BUTTONNUMBER_COLUMN, &mapping_b3,
				    -1);
	} else {
		mapping_b3 = 0;
	}

	if (cc_wacom_tool_get_num_buttons (priv->stylus) > 2) {
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("combo-thirdbutton")), &iter))
			return;

		gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
				    BUTTONNUMBER_COLUMN, &mapping_b4,
				    -1);
	} else {
		mapping_b4 = 0;
	}

	g_settings_set_enum (priv->stylus_settings, "button-action", mapping_b2);
	g_settings_set_enum (priv->stylus_settings, "secondary-button-action", mapping_b3);
	g_settings_set_enum (priv->stylus_settings, "tertiary-button-action", mapping_b4);
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
cc_wacom_stylus_page_get_property (GObject    *object,
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
cc_wacom_stylus_page_set_property (GObject      *object,
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
cc_wacom_stylus_page_dispose (GObject *object)
{
	CcWacomStylusPagePrivate *priv = CC_WACOM_STYLUS_PAGE (object)->priv;

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}


	G_OBJECT_CLASS (cc_wacom_stylus_page_parent_class)->dispose (object);
}

static void
cc_wacom_stylus_page_class_init (CcWacomStylusPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomStylusPagePrivate));

	object_class->get_property = cc_wacom_stylus_page_get_property;
	object_class->set_property = cc_wacom_stylus_page_set_property;
	object_class->dispose = cc_wacom_stylus_page_dispose;
}

static void
cc_wacom_stylus_page_init (CcWacomStylusPage *self)
{
	CcWacomStylusPagePrivate *priv;
	GError *error = NULL;
	GtkComboBox *combo;
	GtkWidget *box;
	char *objects[] = {
		"stylus-grid",
		"liststore-buttons",
		"adjustment-tip-feel",
		"adjustment-eraser-feel",
		NULL
	};

	priv = self->priv = WACOM_STYLUS_PAGE_PRIVATE (self);

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_resource (priv->builder,
                                               "/org/gnome/control-center/wacom/wacom-stylus-page.ui",
                                               objects,
                                               &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	box = WID ("stylus-grid");
	gtk_container_add (GTK_CONTAINER (self), box);
	gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);

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

	combo = GTK_COMBO_BOX (WID ("combo-thirdbutton"));
	combobox_text_cellrenderer (combo, BUTTONNAME_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (button_changed_cb), self);

	priv->nav = cc_wacom_nav_button_new ();
        gtk_widget_set_halign (priv->nav, GTK_ALIGN_END);
        gtk_widget_set_margin_start (priv->nav, 10);
	gtk_widget_show (priv->nav);
	gtk_container_add (CWID ("navigation-placeholder"), priv->nav);
}

static void
set_icon_name (CcWacomStylusPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	CcWacomStylusPagePrivate *priv;
	char *resource;

	priv = page->priv;

	resource = g_strdup_printf ("/org/gnome/control-center/wacom/%s.svg", icon_name);
	gtk_image_set_from_resource (GTK_IMAGE (WID (widget_name)), resource);
	g_free (resource);
}

/* Different types of layout for the stylus config */
enum {
	LAYOUT_NORMAL,                      /* eraser, 2 buttons, tip */
	LAYOUT_INKING,                      /* tip */
	LAYOUT_AIRBRUSH,                    /* eraser, 1 button, tip */
	LAYOUT_GENERIC_2_BUTTONS_NO_ERASER, /* 2 buttons, tip, no eraser */
	LAYOUT_OTHER
};

static void
remove_buttons (CcWacomStylusPagePrivate *priv, int n)
{
	if (n < 3) {
		gtk_widget_destroy (WID ("combo-thirdbutton"));
		gtk_widget_destroy (WID ("label-third-button"));
	}
	if (n < 2) {
		gtk_widget_destroy (WID ("combo-topbutton"));
		gtk_widget_destroy (WID ("label-top-button"));
		gtk_label_set_text (GTK_LABEL (WID ("label-lower-button")), _("Button"));
	}
	if (n < 1) {
		gtk_widget_destroy (WID ("combo-bottombutton"));
		gtk_widget_destroy (WID ("label-lower-button"));
	}
}

static void
remove_eraser (CcWacomStylusPagePrivate *priv)
{
	gtk_widget_destroy (WID ("eraser-box"));
	gtk_widget_destroy (WID ("label-eraser-feel"));
}

static void
update_stylus_ui (CcWacomStylusPage *page,
		  int                layout)
{
	CcWacomStylusPagePrivate *priv = page->priv;

	switch (layout) {
	case LAYOUT_NORMAL:
		remove_buttons (page->priv, 2);
		break;
	case LAYOUT_INKING:
		remove_buttons (page->priv, 0);
		remove_eraser (page->priv);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("label-tip-feel"),
					 "top_attach", 0, NULL);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("box-tip-feel"),
					 "top_attach", 0, NULL);
		break;
	case LAYOUT_AIRBRUSH:
		remove_buttons (page->priv, 1);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("label-lower-button"),
					 "top_attach", 1, NULL);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("combo-bottombutton"),
					 "top_attach", 1, NULL);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("label-tip-feel"),
					 "top_attach", 2, NULL);
		gtk_container_child_set (CWID ("stylus-controls-grid"),
					 WID ("box-tip-feel"),
					 "top_attach", 2, NULL);
		break;
	case LAYOUT_GENERIC_2_BUTTONS_NO_ERASER:
		remove_buttons (page->priv, 2);
		remove_eraser (page->priv);
		break;
	case LAYOUT_OTHER:
		/* We already warn about it in cc_wacom_stylus_page_new () */
		break;
	}
}

GtkWidget *
cc_wacom_stylus_page_new (CcWacomTool *stylus)
{
	CcWacomStylusPage *page;
	CcWacomStylusPagePrivate *priv;
	guint num_buttons;
	int layout;
	gboolean has_eraser;

	g_return_val_if_fail (CC_IS_WACOM_TOOL (stylus), NULL);

	page = g_object_new (CC_TYPE_WACOM_STYLUS_PAGE, NULL);

	priv = page->priv;
	priv->stylus = stylus;

	/* Icon */
	set_icon_name (page, "image-stylus", cc_wacom_tool_get_icon_name (stylus));

	/* Settings */
	priv->stylus_settings = cc_wacom_tool_get_settings (stylus);
	has_eraser = cc_wacom_tool_get_has_eraser (stylus);

	/* Stylus name */
	gtk_label_set_text (GTK_LABEL (WID ("label-stylus")), cc_wacom_tool_get_name (stylus));

	num_buttons = cc_wacom_tool_get_num_buttons (stylus);
	if (num_buttons == 0 && !has_eraser)
		layout = LAYOUT_INKING;
	else if (num_buttons == 2 && has_eraser)
		layout = LAYOUT_NORMAL;
	else if (num_buttons == 1 && has_eraser)
		layout = LAYOUT_AIRBRUSH;
	else if (num_buttons == 2 && !has_eraser)
		layout = LAYOUT_GENERIC_2_BUTTONS_NO_ERASER;
	else {
		layout = LAYOUT_OTHER;
		remove_buttons (priv, num_buttons);

		/* Gray out eraser if not available */
		gtk_widget_set_sensitive (WID ("eraser-box"), has_eraser);
		gtk_widget_set_sensitive (WID ("label-eraser-feel"), has_eraser);

		g_warning ("The layout of this page is not known, %d buttons, %s eraser",
			   num_buttons, has_eraser ? "with" : "without");
	}

	update_stylus_ui (page, layout);

	if (num_buttons >= 3)
		set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-thirdbutton")),
						   priv->stylus_settings, "tertiary-button-action");
	if (num_buttons >= 2)
		set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-topbutton")),
						   priv->stylus_settings, "secondary-button-action");
	if (num_buttons >= 1)
		set_button_mapping_from_gsettings (GTK_COMBO_BOX (WID ("combo-bottombutton")),
						   priv->stylus_settings, "button-action");
	set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-tip-feel")),
				 priv->stylus_settings, "pressure-curve");

	if (has_eraser)
		set_feel_from_gsettings (GTK_ADJUSTMENT (WID ("adjustment-eraser-feel")),
					 priv->stylus_settings, "eraser-pressure-curve");

	return GTK_WIDGET (page);
}

CcWacomTool *
cc_wacom_stylus_page_get_tool (CcWacomStylusPage *page)
{
	return page->priv->stylus;
}

void
cc_wacom_stylus_page_set_navigation (CcWacomStylusPage *page,
				     GtkNotebook *notebook)
{
	CcWacomStylusPagePrivate *priv;

	g_return_if_fail (CC_IS_WACOM_STYLUS_PAGE (page));

	priv = page->priv;

	g_object_set (G_OBJECT (priv->nav),
		      "notebook", notebook,
		      "ignore-first", TRUE,
		      NULL);
}
