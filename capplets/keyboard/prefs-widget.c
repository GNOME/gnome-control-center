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

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

enum {
	ARG_0,
	ARG_PREFERENCES
};

static CappletWidgetClass *parent_class;

static void prefs_widget_init             (PrefsWidget *prefs_widget);
static void prefs_widget_class_init       (PrefsWidgetClass *class);

static void prefs_widget_set_arg          (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);
static void prefs_widget_get_arg          (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);

static void read_preferences              (PrefsWidget *prefs_widget,
					   Preferences *prefs);

static void repeat_toggled_cb             (GtkToggleButton *button,
					   PrefsWidget *prefs_widget);
static void rate_changed_cb               (GtkToggleButton *toggle,
					   PrefsWidget *prefs_widget);
static void delay_changed_cb              (GtkToggleButton *toggle,
					   PrefsWidget *prefs_widget);
static void click_toggled_cb              (GtkToggleButton *button,
					   PrefsWidget *prefs_widget);
static void click_volume_changed_cb       (GtkAdjustment *adjustment,
					   PrefsWidget *prefs_widget);

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
	char delays[] = "delay0";
	char rates[] = "repeat0";
	int i;

	GtkWidget *widget;
	GtkAdjustment *adjustment;

	prefs_widget->dialog_data = 
		glade_xml_new (GNOMECC_GLADE_DIR "/keyboard-properties.glade",
			       "prefs_widget");

	glade_xml_signal_connect_data
		(prefs_widget->dialog_data, "repeat_toggled_cb",
		 repeat_toggled_cb, prefs_widget);

	widget = glade_xml_get_widget (prefs_widget->dialog_data, 
				       "prefs_widget");
	gtk_container_add (GTK_CONTAINER (prefs_widget), widget);

	glade_xml_signal_connect_data
		(prefs_widget->dialog_data, "click_toggled_cb",
		 click_toggled_cb, prefs_widget);

	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (WID ("click_volume_entry")));
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    click_volume_changed_cb, prefs_widget);


	for (i = 0; i < 4; i++) {
		rates[6] = delays[5] = '0' + i;
		prefs_widget->delay[i] = WID (delays);
		prefs_widget->rate[i]  = WID (rates);

		gtk_signal_connect (GTK_OBJECT (prefs_widget->delay[i]), "toggled",
				    GTK_SIGNAL_FUNC (delay_changed_cb), prefs_widget);

		gtk_signal_connect (GTK_OBJECT (prefs_widget->rate[i]), "toggled",
				    GTK_SIGNAL_FUNC (rate_changed_cb), prefs_widget);
	}
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
	GtkAdjustment *adjustment;
	int i;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (WID ("repeat_toggle")), prefs->repeat);

	i = CLAMP (prefs->rate * 3 / 255, 0, 3);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->rate[i]), TRUE);

	i = CLAMP ((10000 - prefs->delay) * 3 / 10000, 0, 3);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->delay[i]), TRUE);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (WID ("click_toggle")), prefs->click);
		 
	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (WID ("click_volume_entry")));
	gtk_adjustment_set_value (adjustment, prefs->volume);

	gtk_widget_set_sensitive (WID ("click_frame"), prefs_widget->prefs->click);
	gtk_widget_set_sensitive (WID ("delay_frame"), prefs_widget->prefs->repeat);
	gtk_widget_set_sensitive (WID ("repeat_frame"), prefs_widget->prefs->repeat);
}

static void
repeat_toggled_cb (GtkToggleButton *button, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

	prefs_widget->prefs->repeat = 
		gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (WID ("delay_frame"), prefs_widget->prefs->repeat);
	gtk_widget_set_sensitive (WID ("repeat_frame"), prefs_widget->prefs->repeat);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static int
set_scale (GtkToggleButton *toggle, GtkWidget **arr)
{
	int i, retval = 0;

	for (i = 0; i < 4; i++) {
		if (arr[i] == GTK_WIDGET (toggle)) retval = i;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arr[i]), arr[i] == GTK_WIDGET (toggle));
	}
	
	return retval;
}

static void 
rate_changed_cb (GtkToggleButton *toggle, PrefsWidget *prefs_widget) 
{
	int i;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (toggle != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));

	if (!toggle->active)
		return;

	i = set_scale (toggle, prefs_widget->rate);
	prefs_widget->prefs->rate = i * 255 / 3;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
delay_changed_cb (GtkToggleButton *toggle, PrefsWidget *prefs_widget) 
{
	int i;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (toggle != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));

	if (!toggle->active)
		return;

	i = set_scale (toggle, prefs_widget->delay);
	prefs_widget->prefs->delay = (3 - i) * 10000 / 3;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
click_toggled_cb (GtkToggleButton *button, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

	prefs_widget->prefs->click = 
		gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (WID ("click_frame"), prefs_widget->prefs->click);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
click_volume_changed_cb (GtkAdjustment *adjustment, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

	prefs_widget->prefs->volume = adjustment->value;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}
