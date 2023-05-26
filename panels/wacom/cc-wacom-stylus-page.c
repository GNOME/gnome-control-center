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

#include <adwaita.h>
#include <glib/gi18n.h>
#include "cc-wacom-stylus-page.h"
#include <gtk/gtk.h>
#include <gdesktop-enums.h>

#include <string.h>

struct _CcWacomStylusPage
{
	GtkBox          parent_instance;

	GtkWidget      *stylus_section;
	GtkWidget      *stylus_icon;
	GtkWidget      *stylus_button1_action;
	GtkWidget      *stylus_button2_action;
	GtkWidget      *stylus_button3_action;
	GtkWidget      *stylus_eraser_pressure;
	GtkWidget      *stylus_tip_pressure_scale;
	GtkWidget      *stylus_eraser_pressure_scale;
	GtkAdjustment  *stylus_tip_pressure_adjustment;
	GtkAdjustment  *stylus_eraser_pressure_adjustment;
	CcWacomTool    *stylus;
	GSettings      *stylus_settings;
};

G_DEFINE_TYPE (CcWacomStylusPage, cc_wacom_stylus_page, GTK_TYPE_BOX)

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
on_tip_pressure_value_changed (CcWacomStylusPage *page)
{
	set_pressurecurve (GTK_RANGE (page->stylus_tip_pressure_scale), page->stylus_settings, "pressure-curve");
}

static void
on_eraser_pressure_value_changed (CcWacomStylusPage *page)
{
	set_pressurecurve (GTK_RANGE (page->stylus_eraser_pressure_scale), page->stylus_settings, "eraser-pressure-curve");
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
on_stylus_button1_action_selected (CcWacomStylusPage *page)
{
	gint idx;

	idx = adw_combo_row_get_selected (ADW_COMBO_ROW (page->stylus_button1_action));
	g_settings_set_enum (page->stylus_settings, "button-action", idx);
}

static void
on_stylus_button2_action_selected (CcWacomStylusPage *page)
{
	gint idx;

	idx = adw_combo_row_get_selected (ADW_COMBO_ROW (page->stylus_button2_action));
	g_settings_set_enum (page->stylus_settings, "secondary-button-action", idx);
}

static void
on_stylus_button3_action_selected (CcWacomStylusPage *page)
{
	gint idx;

	idx = adw_combo_row_get_selected (ADW_COMBO_ROW (page->stylus_button3_action));
	g_settings_set_enum (page->stylus_settings, "tertiary-button-action", idx);
}

static void
cc_wacom_stylus_page_class_init (CcWacomStylusPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = cc_wacom_stylus_page_get_property;
	object_class->set_property = cc_wacom_stylus_page_set_property;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wacom/cc-wacom-stylus-page.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_section);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_icon);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_button1_action);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_button2_action);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_button3_action);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_eraser_pressure);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_tip_pressure_scale);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_eraser_pressure_scale);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_tip_pressure_adjustment);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusPage, stylus_eraser_pressure_adjustment);

	gtk_widget_class_bind_template_callback (widget_class, on_stylus_button1_action_selected);
	gtk_widget_class_bind_template_callback (widget_class, on_stylus_button2_action_selected);
	gtk_widget_class_bind_template_callback (widget_class, on_stylus_button3_action_selected);
	gtk_widget_class_bind_template_callback (widget_class, on_tip_pressure_value_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_eraser_pressure_value_changed);
}

static void
add_marks (GtkScale *scale)
{
#if 0
	gint i;

	for (i = 0; i < N_PRESSURE_CURVES; i++)
		gtk_scale_add_mark (scale, i, GTK_POS_BOTTOM, NULL);
#endif
}

static void
cc_wacom_stylus_page_init (CcWacomStylusPage *page)
{
	gtk_widget_init_template (GTK_WIDGET (page));

	add_marks (GTK_SCALE (page->stylus_tip_pressure_scale));
	add_marks (GTK_SCALE (page->stylus_eraser_pressure_scale));
}

static void
set_icon_name (CcWacomStylusPage *page,
	       const char        *icon_name)
{
	g_autofree gchar *resource = NULL;

	resource = g_strdup_printf ("/org/gnome/control-center/wacom/%s.svg", icon_name);
	gtk_picture_set_resource (GTK_PICTURE (page->stylus_icon), resource);
}

GtkWidget *
cc_wacom_stylus_page_new (CcWacomTool *stylus)
{
	CcWacomStylusPage *page;
	guint num_buttons;
	gboolean has_eraser;

	g_return_val_if_fail (CC_IS_WACOM_TOOL (stylus), NULL);

	page = g_object_new (CC_TYPE_WACOM_STYLUS_PAGE, NULL);

	page->stylus = stylus;

	/* Stylus name */
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (page->stylus_section),
					 cc_wacom_tool_get_name (stylus));
	adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (page->stylus_section),
					       cc_wacom_tool_get_description (stylus));

	/* Icon */
	set_icon_name (page, cc_wacom_tool_get_icon_name (stylus));

	/* Settings */
	page->stylus_settings = cc_wacom_tool_get_settings (stylus);
	has_eraser = cc_wacom_tool_get_has_eraser (stylus);

	num_buttons = cc_wacom_tool_get_num_buttons (stylus);
	gtk_widget_set_visible (page->stylus_button3_action,
				num_buttons >= 3);
	gtk_widget_set_visible (page->stylus_button2_action,
				num_buttons >= 2);
	gtk_widget_set_visible (page->stylus_button1_action,
				num_buttons >= 1);
	gtk_widget_set_visible (page->stylus_eraser_pressure,
				has_eraser);

        adw_combo_row_set_selected (ADW_COMBO_ROW (page->stylus_button1_action),
				    g_settings_get_enum (page->stylus_settings, "button-action"));
        adw_combo_row_set_selected (ADW_COMBO_ROW (page->stylus_button2_action),
				    g_settings_get_enum (page->stylus_settings, "secondary-button-action"));
        adw_combo_row_set_selected (ADW_COMBO_ROW (page->stylus_button3_action),
				    g_settings_get_enum (page->stylus_settings, "tertiary-button-action"));

	set_feel_from_gsettings (page->stylus_tip_pressure_adjustment,
				 page->stylus_settings, "pressure-curve");
	set_feel_from_gsettings (page->stylus_eraser_pressure_adjustment,
				 page->stylus_settings, "eraser-pressure-curve");

	return GTK_WIDGET (page);
}

CcWacomTool *
cc_wacom_stylus_page_get_tool (CcWacomStylusPage *page)
{
	return page->stylus;
}
