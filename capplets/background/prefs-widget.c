/* -*- mode: c; style: linux -*- */

/* prefs-widget.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "prefs-widget.h"
#include "applier.h"

#include <gdk-pixbuf/gdk-pixbuf-xlibrgb.h>

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

enum {
	ARG_0,
	ARG_PREFERENCES
};

static CappletWidgetClass *parent_class;

static void prefs_widget_init               (PrefsWidget *prefs_widget);
static void prefs_widget_class_init         (PrefsWidgetClass *class);

static void prefs_widget_set_arg            (GtkObject *object, 
					     GtkArg *arg, 
					     guint arg_id);
static void prefs_widget_get_arg            (GtkObject *object, 
					     GtkArg *arg, 
					     guint arg_id);
static void prefs_widget_destroy            (GtkObject *object);

static void read_preferences                (PrefsWidget *prefs_widget,
					     Preferences *prefs);

static void color1_select_color_set_cb      (GnomeColorPicker *cp, 
					     guint r, 
					     guint g, 
					     guint b, 
					     guint a, 
					     PrefsWidget *prefs_widget);
static void color2_select_color_set_cb      (GnomeColorPicker *cp, 
					     guint r, 
					     guint g, 
					     guint b, 
					     guint a, 
					     PrefsWidget *prefs_widget);
static void solid_select_toggled_cb         (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void gradient_select_toggled_cb      (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void vertical_select_toggled_cb      (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void horizontal_select_toggled_cb    (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void wallpaper_entry_changed_cb      (GtkEntry *e, 
					     PrefsWidget *prefs_widget);
static void tiled_select_toggled_cb         (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void centered_select_toggled_cb      (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void scaled_aspect_select_toggled_cb (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);

static void scaled_select_toggled_cb        (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void disable_toggled_cb              (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void auto_apply_toggled_cb           (GtkToggleButton *tb, 
					     PrefsWidget *prefs_widget);
static void adjust_brightness_toggled_cb    (GtkToggleButton *tb,
					     PrefsWidget *prefs_widget);
static void brightness_adjust_changed_cb    (GtkAdjustment *adjustment,
					     PrefsWidget *prefs_widget);

static void set_gradient_controls_sensitive   (PrefsWidget *prefs_widget,
					       gboolean s);
static void set_wallpaper_controls_sensitive  (PrefsWidget *prefs_widget,
					       gboolean s);
static void set_background_controls_sensitive (PrefsWidget *prefs_widget,
					       gboolean s);
static void set_brightness_controls_sensitive (PrefsWidget *prefs_widget,
					       gboolean s);

guint
prefs_widget_get_type (void)
{
	static guint prefs_widget_type = 0;

	if (!prefs_widget_type) {
		GtkTypeInfo prefs_widget_info = {
			"PrefsWidget",
			sizeof (PrefsWidget),
			sizeof (PrefsWidgetClass),
			(GtkClassInitFunc) prefs_widget_class_init,
			(GtkObjectInitFunc) prefs_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		prefs_widget_type = 
			gtk_type_unique (capplet_widget_get_type (), 
					 &prefs_widget_info);
	}

	return prefs_widget_type;
}

static void
prefs_widget_init (PrefsWidget *prefs_widget)
{
	GtkWidget *widget;
	GtkAdjustment *adjustment;

	prefs_widget->dialog_data = 
		glade_xml_new (GLADE_DATADIR "/background-properties.glade",
			       "prefs_widget");

	widget = glade_xml_get_widget (prefs_widget->dialog_data, 
				       "prefs_widget");
	gtk_container_add (GTK_CONTAINER (prefs_widget), widget);

	widget = glade_xml_get_widget (prefs_widget->dialog_data,
				       "monitor_frame");
	prefs_widget->preview = applier_class_get_preview_widget ();
	gtk_container_add (GTK_CONTAINER (widget), prefs_widget->preview);

	glade_xml_signal_connect_data (prefs_widget->dialog_data, 
				       "color1_select_color_set_cb",
				       color1_select_color_set_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data, 
				       "color2_select_color_set_cb",
				       color2_select_color_set_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "solid_select_toggled_cb",
				       solid_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "gradient_select_toggled_cb",
				       gradient_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "vertical_select_toggled_cb",
				       vertical_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "horizontal_select_toggled_cb",
				       horizontal_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "wallpaper_entry_changed_cb",
				       wallpaper_entry_changed_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "tiled_select_toggled_cb",
				       tiled_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "centered_select_toggled_cb",
				       centered_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "scaled_aspect_select_toggled_cb",
				       scaled_aspect_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "scaled_select_toggled_cb",
				       scaled_select_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "disable_toggled_cb",
				       disable_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "auto_apply_toggled_cb",
				       auto_apply_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "adjust_brightness_toggled_cb",
				       adjust_brightness_toggled_cb,
				       prefs_widget);

	adjustment = gtk_range_get_adjustment
		(GTK_RANGE (WID ("brightness_adjust")));
	gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
			    GTK_SIGNAL_FUNC (brightness_adjust_changed_cb),
			    prefs_widget);

	gnome_entry_load_history
		(GNOME_ENTRY (gnome_file_entry_gnome_entry 
			      (GNOME_FILE_ENTRY (WID ("wallpaper_entry")))));
}

static void
prefs_widget_class_init (PrefsWidgetClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("PrefsWidget::preferences",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PREFERENCES);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = prefs_widget_destroy;
	object_class->set_arg = prefs_widget_set_arg;
	object_class->get_arg = prefs_widget_get_arg;

	parent_class = CAPPLET_WIDGET_CLASS
		(gtk_type_class (capplet_widget_get_type ()));
}

static void
prefs_widget_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	PrefsWidget *prefs_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (object));

	prefs_widget = PREFS_WIDGET (object);

	switch (arg_id) {
	case ARG_PREFERENCES:
		if (prefs_widget->prefs)
			gtk_object_unref (GTK_OBJECT (prefs_widget->prefs));

		prefs_widget->prefs = GTK_VALUE_POINTER (*arg);

		if (prefs_widget->prefs) {
			gtk_object_ref (GTK_OBJECT (prefs_widget->prefs));
			read_preferences (prefs_widget, prefs_widget->prefs);
		}

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
prefs_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	PrefsWidget *prefs_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (object));

	prefs_widget = PREFS_WIDGET (object);

	switch (arg_id) {
	case ARG_PREFERENCES:
		GTK_VALUE_POINTER (*arg) = prefs_widget->prefs;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
prefs_widget_destroy (GtkObject *object) 
{
	PrefsWidget *prefs_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (object));

	prefs_widget = PREFS_WIDGET (object);

	gnome_entry_save_history
		(GNOME_ENTRY (gnome_file_entry_gnome_entry 
			      (GNOME_FILE_ENTRY (WID ("wallpaper_entry")))));

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
prefs_widget_new (Preferences *prefs) 
{
	g_return_val_if_fail (prefs == NULL || IS_PREFERENCES (prefs), NULL);

	return gtk_widget_new (prefs_widget_get_type (),
			       "preferences", prefs,
			       NULL);
}

void
prefs_widget_set_preferences (PrefsWidget *prefs_widget, Preferences *prefs)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gtk_object_set (GTK_OBJECT (prefs_widget), "preferences", prefs, NULL);
}

static void
read_preferences (PrefsWidget *prefs_widget, Preferences *prefs) 
{
	GtkWidget *widget, *entry;
	GtkAdjustment *adjustment;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gnome_color_picker_set_i16
		(GNOME_COLOR_PICKER (WID ("color1_select")),
		 prefs->color1->red, prefs->color1->green,
		 prefs->color1->blue, 0xffff);
	gnome_color_picker_set_i16
		(GNOME_COLOR_PICKER (WID ("color2_select")),
		 prefs->color2->red, prefs->color2->green,
		 prefs->color2->blue, 0xffff);

	if (prefs->gradient_enabled) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("gradient_select")),
					      TRUE);
		set_gradient_controls_sensitive (prefs_widget, TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("solid_select")),
					      TRUE);
		set_gradient_controls_sensitive (prefs_widget, FALSE);
	}

	if (prefs->orientation == ORIENTATION_VERT)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("vertical_select")),
					      TRUE);
	else if (prefs->orientation == ORIENTATION_HORIZ)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("horizontal_select")),
					      TRUE);

	widget = WID ("wallpaper_entry");

	if (prefs->wallpaper_sel_path)
		gnome_file_entry_set_default_path 
			(GNOME_FILE_ENTRY (widget),
			 prefs->wallpaper_sel_path);

	if (prefs->wallpaper_filename) {
		entry = gnome_file_entry_gtk_entry 
			(GNOME_FILE_ENTRY (widget));
		gtk_entry_set_text (GTK_ENTRY (entry),
				    prefs->wallpaper_filename);
		set_wallpaper_controls_sensitive (prefs_widget, TRUE);
	} else {
		set_wallpaper_controls_sensitive (prefs_widget, FALSE);
	}

	switch (prefs->wallpaper_type) {
	case WPTYPE_TILED:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("tiled_select")),
					      TRUE);
		break;

	case WPTYPE_CENTERED:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("centered_select")),
					      TRUE);
		break;

	case WPTYPE_SCALED_ASPECT:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("scaled_aspect_select")),
					      TRUE);
		break;

	case WPTYPE_SCALED:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("scaled_select")),
					      TRUE);
		break;

	default:
		g_error ("Bad wallpaper type");
		break;
	}

	if (prefs->enabled) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("disable_toggle")),
					      FALSE);
		set_background_controls_sensitive (prefs_widget, TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("disable_toggle")),
					      TRUE);
		set_background_controls_sensitive (prefs_widget, FALSE);
	}

	if (prefs->auto_apply)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("auto_apply")),
					      TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("auto_apply")),
					      FALSE);

	if (prefs->adjust_brightness)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("auto_apply")),
					      TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
					      (WID ("auto_apply")),
					      FALSE);

	if (prefs->adjust_brightness) {
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON (WID ("adjust_brightness_toggle")),
			 TRUE);
		set_brightness_controls_sensitive (prefs_widget, TRUE);
	} else {
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON (WID ("adjust_brightness_toggle")),
			 FALSE);
		set_brightness_controls_sensitive (prefs_widget, FALSE);
	}

	adjustment = gtk_range_get_adjustment
		(GTK_RANGE (WID ("brightness_adjust")));
	gtk_adjustment_set_value (adjustment, prefs->brightness_value);

	preferences_apply_preview (prefs);
}

static void
color1_select_color_set_cb (GnomeColorPicker *cp, guint r, guint g,
			    guint b, guint a, PrefsWidget *prefs_widget) 
{
	guint32 rgb;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->color1->red = r;
	prefs_widget->prefs->color1->green = g;
	prefs_widget->prefs->color1->blue = b;
	rgb = ((r >> 8) << 16) || ((g >> 8) << 8) || (b >> 8);
	prefs_widget->prefs->color1->pixel = xlib_rgb_xpixel_from_rgb (rgb);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
color2_select_color_set_cb (GnomeColorPicker *cp, guint r, guint g,
			    guint b, guint a, PrefsWidget *prefs_widget) 
{
	guint32 rgb;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->color2->red = r;
	prefs_widget->prefs->color2->green = g;
	prefs_widget->prefs->color2->blue = b;
	rgb = ((r >> 8) << 16) || ((g >> 8) << 8) || (b >> 8);
	prefs_widget->prefs->color2->pixel = xlib_rgb_xpixel_from_rgb (rgb);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
solid_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->gradient_enabled = FALSE;
		set_gradient_controls_sensitive (prefs_widget, FALSE);

		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
gradient_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->gradient_enabled = TRUE;
		set_gradient_controls_sensitive (prefs_widget, TRUE);

		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
vertical_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->orientation = ORIENTATION_VERT;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
horizontal_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->orientation = ORIENTATION_HORIZ;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
wallpaper_entry_changed_cb (GtkEntry *e, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs == NULL ||
			  IS_PREFERENCES (prefs_widget->prefs));

	if (prefs_widget->prefs == NULL) return;

	if (prefs_widget->prefs->wallpaper_filename)
		g_free (prefs_widget->prefs->wallpaper_filename);

	prefs_widget->prefs->wallpaper_filename =
		gnome_file_entry_get_full_path 
		(GNOME_FILE_ENTRY (WID ("wallpaper_entry")), TRUE);

	if (!g_file_test (prefs_widget->prefs->wallpaper_filename,
			  G_FILE_TEST_ISFILE)) {
		g_free (prefs_widget->prefs->wallpaper_filename);
		prefs_widget->prefs->wallpaper_filename = NULL;
	}

	if (prefs_widget->prefs->wallpaper_filename &&
	    strlen (prefs_widget->prefs->wallpaper_filename) &&
	    g_strcasecmp (prefs_widget->prefs->wallpaper_filename, "none")) 
	{
		set_wallpaper_controls_sensitive (prefs_widget, TRUE);
		prefs_widget->prefs->wallpaper_enabled = TRUE;
	} else {
		set_wallpaper_controls_sensitive (prefs_widget, FALSE);
		prefs_widget->prefs->wallpaper_enabled = FALSE;
	}

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
tiled_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->wallpaper_type = WPTYPE_TILED;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
centered_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->wallpaper_type = WPTYPE_CENTERED;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
scaled_aspect_select_toggled_cb (GtkToggleButton *tb, 
				 PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->wallpaper_type = WPTYPE_SCALED_ASPECT;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
scaled_select_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->wallpaper_type = WPTYPE_SCALED;
		preferences_changed (prefs_widget->prefs);
	}

	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
disable_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb))
		prefs_widget->prefs->enabled = FALSE;
	else
		prefs_widget->prefs->enabled = TRUE;

	set_background_controls_sensitive (prefs_widget, 
					   prefs_widget->prefs->enabled);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
auto_apply_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb))
		prefs_widget->prefs->auto_apply = TRUE;
	else
		prefs_widget->prefs->auto_apply = FALSE;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
adjust_brightness_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (gtk_toggle_button_get_active (tb)) {
		prefs_widget->prefs->adjust_brightness = TRUE;
		set_brightness_controls_sensitive (prefs_widget, TRUE);
	} else {
		prefs_widget->prefs->adjust_brightness = FALSE;
		set_brightness_controls_sensitive (prefs_widget, FALSE);
	}

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
brightness_adjust_changed_cb (GtkAdjustment *adjustment,
			      PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->brightness_value = adjustment->value;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
set_gradient_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (WID ("vertical_select"), s);
	gtk_widget_set_sensitive (WID ("horizontal_select"), s);
	gtk_widget_set_sensitive (WID ("color2_label"), s);
	gtk_widget_set_sensitive (WID ("color2_select"), s);
}

static void
set_wallpaper_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (WID ("tiled_select"), s);
	gtk_widget_set_sensitive (WID ("centered_select"), s);
	gtk_widget_set_sensitive (WID ("scaled_aspect_select"), s);
	gtk_widget_set_sensitive (WID ("scaled_select"), s);
}

static void
set_background_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (WID ("color_frame"), s);
	gtk_widget_set_sensitive (WID ("wallpaper_frame"), s);
}

static void
set_brightness_controls_sensitive (PrefsWidget *prefs_widget, gboolean s)
{
	gtk_widget_set_sensitive (WID ("brightness_low_label"), s);
	gtk_widget_set_sensitive (WID ("brightness_adjust"), s);
	gtk_widget_set_sensitive (WID ("brightness_high_label"), s);
}

