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

static const graphene_point_t P1MIN = GRAPHENE_POINT_INIT (-75, 75);
static const graphene_point_t P1MAX = GRAPHENE_POINT_INIT (75, -75);
static const graphene_point_t P2MIN = GRAPHENE_POINT_INIT (25, 175);
static const graphene_point_t P2MAX = GRAPHENE_POINT_INIT (175, 25);

static void
map_pressurecurve (unsigned int val, graphene_point_t *p1, graphene_point_t *p2)
{
	g_return_if_fail (val >= 0 && val <= 100);

	p1->x = P1MIN.x + ((P1MAX.x - P1MIN.x) * val/100.0);
	p1->y = P1MIN.y + ((P1MAX.y - P1MIN.y) * val/100.0);
	p2->x = P2MIN.x + ((P2MAX.x - P2MIN.x) * val/100.0);
	p2->y = P2MIN.y + ((P2MAX.y - P2MIN.y) * val/100.0);

	p1->x = (int)CLAMP(p1->x, 0, 100);
	p1->y = (int)CLAMP(p1->y, 0, 100);
	p2->x = (int)CLAMP(p2->x, 0, 100);
	p2->y = (int)CLAMP(p2->y, 0, 100);
}

static void
set_pressurecurve (GtkRange *range, GSettings *settings, const gchar *key)
{
	gint			slider_val = gtk_range_get_value (range);
	graphene_point_t	p1, p2;
	GVariantBuilder		builder;

	g_return_if_fail (slider_val >= 0 && slider_val <= 100);

	/* slider goes from 0 to 100, inclusive, and produces this sequence
	 * of previously hardcoded values:
	 *   0: {       0,      75,     25,     100     },      soft
	 *      {       0,      50,     50,     100     },
	 *      {       0,      25,     75,     100     },
	 *  50: {       0,      0,      100,    100     },      neutral
	 *      {       25,     0,      100,    75      },
	 *      {       50,     0,      100,    50      },
	 * 100: {       75,     0,      100,    25      }       firm
	 * These settings were taken from wacomcpl, where they've been around for years.
	 */

	map_pressurecurve (slider_val, &p1, &p2);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("ai"));
	g_variant_builder_add (&builder, "i", (int)p1.x);
	g_variant_builder_add (&builder, "i", (int)p1.y);
	g_variant_builder_add (&builder, "i", (int)p2.x);
	g_variant_builder_add (&builder, "i", (int)p2.y);

	g_settings_set_value (settings, key, g_variant_builder_end (&builder));

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
	GVariant		*variant;
	const gint32		*values;
	gsize			 nvalues;
	int			 i;
	graphene_point_t	 p1, p2;

	variant = g_settings_get_value (settings, key);
	values = g_variant_get_fixed_array (variant, &nvalues, sizeof (gint32));

	if (nvalues != 4) {
		g_warning ("Invalid pressure curve format, expected 4 values (got %"G_GSIZE_FORMAT")", nvalues);
		return;
	}

	p1 = GRAPHENE_POINT_INIT (values[0], values[1]);
	p2 = GRAPHENE_POINT_INIT (values[2], values[3]);

	/* Our functions in set_pressurecurve() are lossy thanks to CLAMP so
	 * we calculate a (possibly wrong) slider value from our points, compare
	 * what points that value would produce and if they match - hooray!
	 */
	for (i = 0; i < 4; i++) {
		double val;

		switch (i) {
		case 0:
			val = (p1.x - P1MIN.x) / (P1MAX.x - P1MIN.x);
			break;
		case 1:
			val = (p1.y - P1MIN.y) / (P1MAX.y - P1MIN.y);
			break;
		case 2:
			val = (p2.x - P2MIN.x) / (P2MAX.x - P2MIN.x);
			break;
		case 3:
			val = (p2.y - P2MIN.y) / (P2MAX.y - P2MIN.y);
			break;
		}

		if (val >= 0.0 && val <= 1.0) {
			unsigned int slider_val;
			graphene_point_t mapped_p1, mapped_p2;

			slider_val = (int)(val * 100 + 0.5);
			map_pressurecurve (slider_val, &mapped_p1, &mapped_p2);
			if (graphene_point_equal(&p1, &mapped_p1) && graphene_point_equal(&p2, &mapped_p2)) {
				gtk_adjustment_set_value (adjustment, slider_val);
				break;
			}
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
